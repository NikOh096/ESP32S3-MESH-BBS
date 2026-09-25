package com.meshbbs.setup;
import android.Manifest;
import android.app.*;
import android.bluetooth.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.os.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import org.json.*;
@SuppressWarnings("deprecation")
public class BoardService extends Service {
  public interface Listener {
    void changed();
  }
  public interface Reply {
    void result(JSONObject result);
  }
  public final class LocalBinder extends Binder {
    public BoardService service() {
      return BoardService.this;
    }
  }
  private final Handler h = new Handler(Looper.getMainLooper());
  private final IBinder binder = new LocalBinder();
  private BluetoothAdapter adapter;
  private BluetoothGatt gatt;
  private BluetoothGattCharacteristic info, rpc;
  private static final UUID SERVICE = UUID.fromString("8f3a0001-6ca2-4d15-9d49-75b974ad2716"),
                            INFO = UUID.fromString("8f3a0002-6ca2-4d15-9d49-75b974ad2716"),
                            RPC = UUID.fromString("8f3a0006-6ca2-4d15-9d49-75b974ad2716");
  public Listener listener;
  public String message = "Not connected", deviceId = "", address = "";
  public boolean ready, enrollment, visible;
  public JSONObject status = new JSONObject();
  public JSONObject nodeStatus = new JSONObject();
  private long nodePairUntil;
  public void nodePairStarted() {
    nodePairUntil = SystemClock.elapsedRealtime() + 95000;
    String protocol = RadioProfile.protocol(nodeStatus.optString("protocol"), status.optString("firmware"));
    nodeStatus = object("stage", "searching", "protocol", protocol);
    pollAt = 0;
    changed();
  }
  private boolean nodePairing() {
    return SystemClock.elapsedRealtime() < nodePairUntil;
  }
  public final JSONObject[] threads = new JSONObject[9];
  public String activity = "No activity yet.";
  private final java.util.LinkedHashSet<String> observedActivity = new java.util.LinkedHashSet<>();
  private JSONObject identity;
  private boolean running = true, connecting, refreshing, discovered, blocked;
  public boolean isConnecting() { return connecting && !ready && !enrollment; }
  private int epoch, sequence = 1, mtu = 23, lastRevision = -1;
  private long retryAt, pollAt, refreshAt;
  private boolean foreground;
  private final ArrayDeque<Operation> queue = new ArrayDeque<>();
  private Operation active;
  private static final class Operation {
    JSONObject request;
    Reply callback;
    int id;
    long deadline;
    Operation(JSONObject j, Reply r) {
      request = j;
      callback = r;
    }
  }
  public static JSONObject object(Object... pairs) {
    JSONObject j = new JSONObject();
    try {
      for (int i = 0; i < pairs.length; i += 2) j.put((String) pairs[i], pairs[i + 1]);
    } catch (JSONException ex) {
      throw new IllegalArgumentException(ex);
    }
    return j;
  }
  private void changed() {
    if (listener != null)
      listener.changed();
  }
  private void note(String s) {
    message = s;
    ActivityLog.add("LINK", s);
    changed();
  }
  private android.content.SharedPreferences prefs() {
    return getSharedPreferences("app", 0);
  }
  private boolean permissions() {
    return Build.VERSION.SDK_INT < 31
        || checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
        == PackageManager.PERMISSION_GRANTED;
  }
  @Override
  public void onCreate() {
    super.onCreate();
    adapter = ((BluetoothManager) getSystemService(BLUETOOTH_SERVICE)).getAdapter();
    address = prefs().getString("address", "");
    deviceId = prefs().getString("board", "");
    channels();
    IntentFilter f = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
    if (Build.VERSION.SDK_INT >= 33)
      registerReceiver(bonds, f, Context.RECEIVER_EXPORTED);
    else
      registerReceiver(bonds, f);
    h.post(tick);
  }
  @Override
  public int onStartCommand(Intent intent, int flags, int id) {
    monitorNotification();
    return START_NOT_STICKY;
  }
  private void monitorNotification() {
    foreground = true;
    startForeground(1,
        new Notification.Builder(this, "connection")
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle("MESHBBS nearby connection")
            .setContentText("Reconnects to your enrolled board. Change monitoring in Settings.")
            .setOngoing(true)
            .setContentIntent(openIntent(0))
            .build());
  }
  @Override
  public IBinder onBind(Intent intent) {
    return binder;
  }
  public void select(String value) {
    close();
    Arrays.fill(threads, null);
    status = new JSONObject();
    activity = "No activity yet.";
    address = value;
    blocked = false;
    retryAt = 0;
    connect();
  }
  public void retry() {
    blocked = false;
    close();
    retryAt = 0;
    connect();
  }
  public void setVisible(boolean value) {
    visible = value;
    if (value) {
      blocked = false;
      retryAt = 0;
      if (!foreground)
        monitorNotification();
    }
  }
  private final Runnable tick = new Runnable() {
    public void run() {
      if (!running)
        return;
      long now = SystemClock.elapsedRealtime();
      boolean wanted = visible || prefs().getBoolean("background", true);
      if (!wanted) {
        if (gatt != null)
          close();
        stopForeground(true);
        foreground = false;
        if (listener == null)
          stopSelf();
      } else if (permissions() && adapter != null && adapter.isEnabled()) {
        if (!foreground)
          monitorNotification();
        if (gatt == null && !blocked && !address.isEmpty() && now >= retryAt)
          connect();
        if (ready && active == null && queue.isEmpty() && now >= pollAt) {
          pollAt = now + (nodePairing() ? 500 : 3000);
          poll();
        }
      }
      if (active != null && now >= active.deadline) {
        note("Request timed out. Reconnecting; check Home before repeating a create/delete "
            + "operation.");
        close();
      }
      h.postDelayed(this, 500);
    }
  };
  private void connect() {
    if (connecting || gatt != null || !permissions() || adapter == null || !adapter.isEnabled())
      return;
    try {
      connecting = true;
      discovered = false;
      mtu = 23;
      int ticket = ++epoch;
      note("Connecting to remembered MESHBBS…");
      gatt = adapter.getRemoteDevice(address).connectGatt(
          this, false, callback, BluetoothDevice.TRANSPORT_LE);
      h.postDelayed(() -> {
        if (ticket == epoch && !ready && !enrollment) {
          note("Board not reachable; retrying automatically.");
          close();
        }
      }, 30000);
    } catch (Exception ex) {
      note("Bluetooth unavailable. Check permissions and Bluetooth.");
      close();
    }
  }
  private void close() {
    ++epoch;
    BluetoothGatt old = gatt;
    gatt = null;
    info = rpc = null;
    ready = enrollment = connecting = discovered = false;
    nodeStatus = new JSONObject();
    nodePairUntil = 0;
    active = null;
    queue.clear();
    refreshing = false;
    retryAt = SystemClock.elapsedRealtime() + 5000;
    if (old != null)
      try {
        old.disconnect();
        old.close();
      } catch (Exception ignored) {
      }
    changed();
  }
  private void mtu() {
    if (gatt == null)
      return;
    int ticket = epoch;
    try {
      if (!gatt.requestMtu(517))
        discover();
      else
        h.postDelayed(() -> {
          if (ticket == epoch && !discovered)
            discover();
        }, 5000);
    } catch (Exception ex) {
      close();
    }
  }
  private void discover() {
    if (gatt == null || discovered)
      return;
    discovered = true;
    try {
      if (!gatt.discoverServices()) {
        note("Service discovery failed");
        close();
      }
    } catch (Exception ex) {
      close();
    }
  }
  private final BroadcastReceiver bonds = new BroadcastReceiver() {
    public void onReceive(Context c, Intent i) {
      BluetoothDevice d = i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
      if (gatt != null && d != null && d.equals(gatt.getDevice())) {
        int b = i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, 0);
        if (b == BluetoothDevice.BOND_BONDED)
          mtu();
        else if (b == BluetoothDevice.BOND_NONE) {
          note("Pairing failed. Reopen physical enrollment and retry.");
          close();
        }
      }
    }
  };
  private final BluetoothGattCallback callback = new BluetoothGattCallback() {
    @Override
    public void onConnectionStateChange(BluetoothGatt g, int result, int state) {
      h.post(() -> {
        if (g != gatt)
          return;
        if (result != 0 || state == BluetoothProfile.STATE_DISCONNECTED) {
          note("Board disconnected (" + result + "); automatic reconnect enabled.");
          close();
          return;
        }
        if (state == BluetoothProfile.STATE_CONNECTED) {
          connecting = false;
          if (g.getDevice().getBondState() == BluetoothDevice.BOND_BONDED)
            mtu();
          else {
            note("Pairing with MESHBBS…");
            if (!g.getDevice().createBond()) {
              note("Cannot start phone pairing");
              close();
            }
          }
        }
      });
    }
    @Override
    public void onMtuChanged(BluetoothGatt g, int size, int result) {
      h.post(() -> {
        if (g != gatt)
          return;
        if (result == 0)
          mtu = size;
        discover();
      });
    }
    @Override
    public void onServicesDiscovered(BluetoothGatt g, int result) {
      h.post(() -> {
        if (g != gatt)
          return;
        BluetoothGattService s = g.getService(SERVICE);
        if (result != 0 || s == null) {
          note("MESHBBS service missing. Check firmware and pairing.");
          close();
          return;
        }
        info = s.getCharacteristic(INFO);
        rpc = s.getCharacteristic(RPC);
        if (info == null || rpc == null) {
          blocked = true;
          note("Install firmware 0.4.0, then enroll this phone.");
          close();
          return;
        }
        if (!g.readCharacteristic(info))
          close();
      });
    }
    @Override
    public void onCharacteristicRead(BluetoothGatt g, BluetoothGattCharacteristic c, int result) {
      if (Build.VERSION.SDK_INT < 33)
        read(g, c, c.getValue(), result);
    }
    @Override
    public void onCharacteristicRead(
        BluetoothGatt g, BluetoothGattCharacteristic c, byte[] value, int result) {
      read(g, c, value, result);
    }
    @Override
    public void onCharacteristicWrite(BluetoothGatt g, BluetoothGattCharacteristic c, int result) {
      h.post(() -> {
        if (g != gatt || active == null)
          return;
        if (result != 0) {
          note("Bluetooth write rejected (" + result + ").");
          close();
          return;
        }
        h.postDelayed(() -> readResponse(g), 70);
      });
    }
  };
  private void read(BluetoothGatt g, BluetoothGattCharacteristic c, byte[] bytes, int result) {
    final byte[] value = bytes == null ? new byte[0] : bytes.clone();
    h.post(() -> {
      if (g != gatt)
        return;
      if (result != 0) {
        note("Protected read failed (" + result
            + "). Reopen enrollment if this phone lost its pairing key.");
        close();
        return;
      }
      try {
        JSONObject j = new JSONObject(new String(value, StandardCharsets.UTF_8));
        if (c.getUuid().equals(INFO)) {
          identity = j;
          deviceId = j.getString("id");
          enrollment = j.optBoolean("enroll");
          byte[] key = OwnerKey.load(this, deviceId);
          if (key != null && j.optBoolean("owned")) {
            String proof = OwnerKey.proof(key, j.getString("nonce"), deviceId);
            Arrays.fill(key, (byte) 0);
            enqueue(object("op", "auth", "proof", proof), r -> {
              if (r.optBoolean("ok"))
                authenticated();
              else {
                note("Owner key was replaced. Use the physical sequence, then Enroll this phone.");
                blocked = true;
              }
            });
          } else {
            note(enrollment ? "Physical enrollment open. Tap Enroll this phone."
                            : "This phone is not the owner. Use the physical enrollment sequence.");
            blocked = true;
          }
        } else if (c.getUuid().equals(RPC) && active != null) {
          if (j.optInt("id") != active.id || j.optBoolean("busy")) {
            h.postDelayed(() -> readResponse(g), 80);
            return;
          }
          Operation done = active;
          active = null;
          if (!j.optBoolean("ok"))
            ActivityLog.add("ERROR", j.optString("error", "Request failed"));
          if (done.callback != null)
            done.callback.result(j);
          pump();
        }
      } catch (Exception ex) {
        note("Invalid board response or unavailable owner key. Re-enroll if needed.");
        close();
      }
    });
  }
  private void readResponse(BluetoothGatt g) {
    if (g == gatt && active != null)
      try {
        if (!g.readCharacteristic(rpc))
          h.postDelayed(() -> readResponse(g), 100);
      } catch (Exception ex) {
        close();
      }
  }
  public void enroll() {
    if (identity == null || !enrollment || gatt == null) {
      note("Open physical enrollment first: three BOOT taps, then hold 3 seconds and release.");
      return;
    }
    try {
      byte[] key = OwnerKey.create(this, deviceId);
      String proof = OwnerKey.hex(key);
      Arrays.fill(key, (byte) 0);
      enqueue(object("op", "enroll", "proof", proof), r -> {
        if (r.optBoolean("ok"))
          authenticated();
        else
          note(r.optString("error"));
      });
    } catch (Exception ex) {
      note("Could not protect the owner key in Android Keystore.");
    }
  }
  private void authenticated() {
    ready = true;
    enrollment = false;
    blocked = false;
    lastRevision = -1;
    prefs().edit().putString("address", address).putString("board", deviceId).apply();
    note("Owner connected · " + deviceId);
    enqueue(object("op", "clock", "time", System.currentTimeMillis() / 1000), r -> poll());
  }
  public void request(JSONObject j, Reply reply) {
    if (!ready) {
      if (reply != null)
        reply.result(object("ok", false, "error", "Connect to your enrolled MESHBBS first"));
      return;
    }
    enqueue(j, reply);
  }
  private void enqueue(JSONObject j, Reply callback) {
    try {
      j.put("id", sequence++);
      if (sequence < 1)
        sequence = 1;
      Operation o = new Operation(j, callback);
      o.id = j.getInt("id");
      String op = j.optString("op");
      if (op.equals("pin") || op.equals("pair_cancel") || op.equals("node_select")
          || op.equals("pair"))
        queue.addFirst(o);
      else
        queue.add(o);
      pump();
    } catch (Exception ex) {
      note("Could not prepare request");
    }
  }
  private void pump() {
    if (active != null || gatt == null || rpc == null || queue.isEmpty())
      return;
    Operation o = queue.remove();
    byte[] bytes = o.request.toString().getBytes(StandardCharsets.UTF_8);
    if (bytes.length > 512) {
      if (o.callback != null)
        o.callback.result(object(
            "ok", false, "error", "Text is too large for one Bluetooth request. Shorten it."));
      pump();
      return;
    }
    active = o;
    o.deadline = SystemClock.elapsedRealtime() + 15000;
    try {
      boolean started;
      if (Build.VERSION.SDK_INT >= 33)
        started =
            gatt.writeCharacteristic(rpc, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
            == BluetoothStatusCodes.SUCCESS;
      else {
        rpc.setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        rpc.setValue(bytes.clone());
        started = gatt.writeCharacteristic(rpc);
      }
      if (!started) {
        note("Could not start Bluetooth write");
        close();
      }
    } catch (Exception ex) {
      close();
    }
    Arrays.fill(bytes, (byte) 0);
  }
  private void poll() {
    if (!ready)
      return;
    request(object("op", "node_status"), n -> {
      if (n.optBoolean("ok")) {
        nodeStatus = n;
        if (!NodePairing.active(n.optString("stage")))
          nodePairUntil = 0;
        changed();
      }
    });
    if (nodePairing())
      return;
    request(object("op", "status"), r -> {
      if (!r.optBoolean("ok"))
        return;
      status = r;
      changed();
      int revision = r.optInt("revision");
      if ((revision != lastRevision || SystemClock.elapsedRealtime() >= refreshAt) && !refreshing) {
        lastRevision = revision;
        refreshAt = SystemClock.elapsedRealtime() + 30000;
        refresh();
      }
      request(object("op", "activity"), a -> {
        if (a.optBoolean("ok")) {
          activity = a.optString("text");
          for (String line : activity.split("\\n"))
            if (!line.isEmpty() && observedActivity.add(line))
              ActivityLog.add("BOARD", line);
          while (observedActivity.size() > 300)
            observedActivity.remove(observedActivity.iterator().next());
          changed();
        }
      });
    });
  }
  public void refresh() {
    if (!ready || refreshing)
      return;
    refreshing = true;
    readSlot(1);
  }
  private void readSlot(int slot) {
    request(object("op", "list", "slot", slot), r -> {
      if (!r.optBoolean("ok")) {
        refreshing = false;
        return;
      }
      threads[slot - 1] = r;
      notifyComment(slot, r);
      if (slot < 9 && nodePairing())
        refreshing = false;
      else if (slot < 9)
        readSlot(slot + 1);
      else {
        refreshing = false;
        NotificationManager manager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        java.util.HashSet<String> alive = new java.util.HashSet<>();
        for (JSONObject t : threads)
          if (t != null && t.optBoolean("active"))
            alive.add(deviceId + "_" + t.optInt("generation"));
        for (android.service.notification.StatusBarNotification n :
            manager.getActiveNotifications())
          if (n.getTag() != null && n.getTag().startsWith(deviceId + "_")
              && !alive.contains(n.getTag()))
            manager.cancel(n.getTag(), n.getId());
        changed();
      }
    });
  }
  private void notifyComment(int slot, JSONObject thread) {
    int count = thread.optInt("count"), generation = thread.optInt("generation");
    String key = "seen_count_" + deviceId + "_id_" + generation;
    String state = generation + ":" + count, old = prefs().getString(key, "");
    prefs().edit().putString(key, state).apply();
    if (state.equals(old)
        || (thread.optInt("unread") == 0 && !(old.isEmpty() && thread.optLong("creator") != 0))
        || !thread.optBoolean("active"))
      return;
    int mode = prefs().getInt("notifications", 0);
    if (mode == 3)
      return;
    if (Build.VERSION.SDK_INT >= 33
        && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED)
      return;
    String channel = mode == 1 ? "vibrate" : mode == 2 ? "silent" : "messages";
    Notification n =
        new Notification.Builder(this, channel)
            .setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(thread.optString("thread_id") + ": " + thread.optString("title"))
            .setContentText(thread.optInt("unread") > 0
                    ? thread.optInt("unread") + " unread comment(s)"
                    : "New bulletin")
            .setContentIntent(openIntent(generation))
            .setAutoCancel(true)
            .setOnlyAlertOnce(false)
            .build();
    ((NotificationManager) getSystemService(NOTIFICATION_SERVICE))
        .notify(deviceId + "_" + generation, 2, n);
  }
  public void clearNotification(int generation) {
    ((NotificationManager) getSystemService(NOTIFICATION_SERVICE))
        .cancel(deviceId + "_" + generation, 2);
  }
  private PendingIntent openIntent(int generation) {
    return PendingIntent.getActivity(this, generation,
        new Intent(this, MainActivity.class)
            .setData(android.net.Uri.parse("meshbbs://" + deviceId + "/" + generation))
            .putExtra("thread_id", generation)
            .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
  }
  private void channels() {
    NotificationManager m = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
    NotificationChannel c = new NotificationChannel(
        "connection", "Nearby board connection", NotificationManager.IMPORTANCE_LOW);
    c.setSound(null, null);
    c.enableVibration(false);
    m.createNotificationChannel(c);
    c = new NotificationChannel(
        "messages", "Messages: sound and vibration", NotificationManager.IMPORTANCE_DEFAULT);
    c.enableVibration(true);
    m.createNotificationChannel(c);
    c = new NotificationChannel(
        "vibrate", "Messages: vibration only", NotificationManager.IMPORTANCE_DEFAULT);
    c.setSound(null, null);
    c.enableVibration(true);
    m.createNotificationChannel(c);
    c = new NotificationChannel("silent", "Messages: silent", NotificationManager.IMPORTANCE_LOW);
    c.setSound(null, null);
    c.enableVibration(false);
    m.createNotificationChannel(c);
  }
  @Override
  public void onDestroy() {
    running = false;
    h.removeCallbacksAndMessages(null);
    close();
    try {
      unregisterReceiver(bonds);
    } catch (Exception ignored) {
    }
    super.onDestroy();
  }
}
