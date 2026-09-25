package com.meshbbs.setup;
import android.Manifest;
import android.app.*;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.*;
import android.text.InputType;
import android.view.*;
import android.widget.*;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.util.*;
import org.json.*;
@SuppressWarnings("deprecation")
public class MainActivity extends Activity implements BoardService.Listener {
  private static final int BG = 0xff0c1825, CARD = 0xff14283a, INK = 0xffe7f0f7, MUTED = 0xffaac0cf,
                           TEAL = 0xff58dac2;
  private final Handler ui = new Handler(Looper.getMainLooper());
  private BoardService board;
  private boolean bound, visible;
  private LinearLayout[] pages = new LinearLayout[4];
  private LinearLayout root, homeCards, boardList, nodeList, banList;
  private TextView state, nodeState, homeLog, console, totals;
  private EditText target, banMessage, banNode;
  private CheckBox noPin;
  private int nodeScanToken;
  private final Map<String, Button> nodeButtons = new LinkedHashMap<>();
  private TextView scanState;
  private TextView boardScanState;
  private TextView noPinHelp;
  private Spinner hops;
  private Button enroll, boardConnect, nodeConnect, nodeCancel, nodeFind, createButton;
  private LinearLayout ownerInstructions, radioOptions, hopOptions;
  private TextView radioHeading, hardwareInfo;
  private FrameLayout pageHolder;
  private final Button[] navButtons = new Button[4];
  private String selectedName = "";
  private boolean nodeActionPending, scanningRadios;
  private String protocol() {
    return board == null ? "Meshtastic" : RadioProfile.protocol(board.nodeStatus.optString("protocol"), board.status.optString("firmware"));
  }
  private void showTab(int n) {
    tab = n;
    for (int x = 0; x < 4; x++) {
      pageHolder.getChildAt(x).setVisibility(x == n ? View.VISIBLE : View.GONE);
      navButtons[x].setTextColor(x == n ? BG : MUTED);
      navButtons[x].setBackgroundTintList(ColorStateList.valueOf(x == n ? TEAL : CARD));
      navButtons[x].setSelected(x == n);
      navButtons[x].setContentDescription(new String[]{"Home", "Devices", "Settings", "Activity"}[x] + (x == n ? ", selected" : ""));
    }
    if (n == 2) loadBans();
  }
  private Button secondary(LinearLayout parent, String label, View.OnClickListener click) {
    Button b = button(parent, label, click);
    b.setBackgroundTintList(ColorStateList.valueOf(CARD));
    b.setTextColor(TEAL);
    return b;
  }

  private BluetoothLeScanner scanner;
  private ScanCallback scan;
  private int scanToken;
  private final Set<String> found = new HashSet<>();
  private int tab, openFromNotification;
  private boolean filled;
  private String lastHome = "";
  private AlertDialog pinDialog;
  private long pinPromptedAttempt;
  private int dp(int n) {
    return Math.round(n * getResources().getDisplayMetrics().density);
  }
  private GradientDrawable bg(int color) {
    GradientDrawable d = new GradientDrawable();
    d.setColor(color);
    d.setCornerRadius(dp(14));
    return d;
  }
  private TextView text(String value, int size, int color) {
    TextView t = new TextView(this);
    t.setText(value);
    t.setTextSize(size);
    t.setTextColor(color);
    t.setPadding(0, dp(6), 0, dp(8));
    t.setTextIsSelectable(true);
    return t;
  }
  private LinearLayout vertical() {
    LinearLayout l = new LinearLayout(this);
    l.setOrientation(1);
    return l;
  }
  private LinearLayout card(LinearLayout parent) {
    LinearLayout l = vertical();
    l.setPadding(dp(16), dp(12), dp(16), dp(16));
    l.setBackground(bg(CARD));
    LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
    p.bottomMargin = dp(12);
    parent.addView(l, p);
    return l;
  }
  private Button button(LinearLayout parent, String label, View.OnClickListener click) {
    Button b = new Button(this);
    b.setText(label);
    b.setAllCaps(false);
    b.setMinHeight(dp(48));
    b.setTextSize(16);
    b.setTextColor(BG);
    b.setBackgroundTintList(ColorStateList.valueOf(TEAL));
    b.setOnClickListener(click);
    parent.addView(b, new LinearLayout.LayoutParams(-1, -2));
    return b;
  }
  private EditText input(LinearLayout p, String hint, boolean multiline) {
    EditText e = new EditText(this);
    e.setTextColor(INK);
    e.setHintTextColor(MUTED);
    p.addView(text(hint, 14, MUTED));
    e.setContentDescription(hint);
    e.setHint(hint);
    e.setTextSize(16);
    e.setSingleLine(!multiline);
    e.setSaveEnabled(false);
    e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
    p.addView(e, new LinearLayout.LayoutParams(-1, -2));
    return e;
  }
  private CheckBox check(LinearLayout p, String label) {
    CheckBox c = new CheckBox(this);
    c.setText(label);
    c.setTextColor(INK);
    p.addView(c);
    return c;
  }
  private Spinner spinner(LinearLayout p, String[] values) {
    Spinner s = new Spinner(this);
    ArrayAdapter<String> a = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, values);
    a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    s.setAdapter(a);
    p.addView(s);
    return s;
  }
  private void toast(String s) {
    Toast.makeText(this, s, Toast.LENGTH_LONG).show();
  }
  private android.content.SharedPreferences prefs() {
    return getSharedPreferences("app", 0);
  }
  @Override
  public void onCreate(Bundle saved) {
    super.onCreate(saved);
    root = vertical();
    root.setBackgroundColor(BG);
    root.setPadding(dp(14), dp(8), dp(14), dp(8));
    setContentView(root);
    root.setOnApplyWindowInsetsListener((v, i) -> {
      v.setPadding(dp(14) + i.getSystemWindowInsetLeft(), dp(8) + i.getSystemWindowInsetTop(),
          dp(14) + i.getSystemWindowInsetRight(), dp(8) + i.getSystemWindowInsetBottom());
      return i;
    });
    LinearLayout header = new LinearLayout(this);
    ImageView logo = new ImageView(this);
    logo.setImageResource(R.drawable.ic_meshbbs);
    header.addView(logo, new LinearLayout.LayoutParams(dp(42), dp(42)));
    TextView title = text("  MESHBBS", 24, INK);
    title.setTypeface(null, Typeface.BOLD);
    header.addView(title);
    root.addView(header);
    state = text("Your nearby bulletin board", 13, TEAL);
    root.addView(state);
    FrameLayout holder = new FrameLayout(this);
    pageHolder = holder;
    root.addView(holder, new LinearLayout.LayoutParams(-1, 0, 1));
    for (int i = 0; i < 4; i++) {
      ScrollView s = new ScrollView(this);
      pages[i] = vertical();
      s.addView(pages[i]);
      holder.addView(s);
      s.setTag(i);
      s.setVisibility(i == 0 ? View.VISIBLE : View.GONE);
    }
    LinearLayout nav = new LinearLayout(this);
    String[] tabs = {"Home", "Devices", "Settings", "Activity"};
    for (int i = 0; i < 4; i++) {
      final int n = i;
      Button b = new Button(this);
      b.setText(tabs[i]);
      b.setAllCaps(false);
      b.setTextSize(12);
      navButtons[i] = b;
      b.setOnClickListener(v -> showTab(n));
      nav.addView(b, new LinearLayout.LayoutParams(0, dp(52), 1));
    }
    root.addView(nav);
    home();
    node();
    settings();
    console();
    openFromNotification = getIntent().getIntExtra("thread_id", 0);
    showTab(0);
    ensurePermissions();
  }
  private void home() {
    LinearLayout p = pages[0], intro = card(p);
    intro.addView(text("Your bulletin board", 24, INK));
    intro.addView(text("A place for news, questions and conversation. Up to nine threads stay open at a time.", 15, MUTED));
    createButton = button(intro, "Create a bulletin", v -> {
      if (board == null || !board.ready) { showTab(1); return; }
      for (int i=0; i<9; i++) if (board.threads[i] != null && !board.threads[i].optBoolean("active")) { create(i+1); return; }
      toast("All nine spaces are in use. Open a thread to change its expiry or remove it.");
    });
    secondary(intro, "How to join the conversation", v -> guide());
    secondary(intro, "Waiting bulletins", v -> showQueue());
    totals = text("Connect your board to see what is happening.", 14, MUTED);
    intro.addView(totals);
    homeCards = vertical(); p.addView(homeCards);
    LinearLayout log = card(p);
    log.addView(text("Recent activity", 18, INK));
    homeLog = text("Your messages and board activity will appear here.", 14, MUTED);
    log.addView(homeLog);
    secondary(log, "View activity", v -> showTab(3));
  }
  private void node() {
    LinearLayout c = card(pages[1]);
    c.addView(text("1 · Your MESHBBS board", 21, INK));
    ownerInstructions = vertical(); c.addView(ownerInstructions);
    ownerInstructions.addView(text("First setup: tap BOOT three times, then hold it for three seconds and release. Finish within 12 seconds. Blue pulses mean the board is ready. Leave RST alone.", 15, MUTED));
    boardConnect = button(c, "Find my board", v -> {
      if (board != null && !board.address.isEmpty()) board.retry(); else scan();
    });
    secondary(c, "Choose another board", v -> scan());
    enroll = button(c, "Use this phone", v -> new AlertDialog.Builder(this)
        .setTitle("Make this the owner phone?")
        .setMessage("This gives this phone control of the board and replaces the previous owner's app access. Your bulletins stay on the board.")
        .setPositiveButton("Use this phone", (d,w) -> { if (board != null) board.enroll(); })
        .setNegativeButton("Cancel", null).show());
    enroll.setVisibility(View.GONE);
    boardList = vertical(); c.addView(boardList);
    boardScanState = text("", 14, MUTED); c.addView(boardScanState);
    c = card(pages[1]);
    radioHeading = text("2 · Your mesh radio", 21, INK); c.addView(radioHeading);
    c.addView(text("MESHBBS uses this radio to reach your mesh. Keep it nearby and disconnect its other phone app before connecting.", 15, MUTED));
    nodeFind = secondary(c, "Find nearby radios", v -> scanNodes());
    scanState = text("Choose a radio below. Its model appears after pairing, when available.", 14, MUTED); c.addView(scanState);
    nodeList = vertical(); c.addView(nodeList);
    nodeConnect = button(c, "Connect", v -> selectNode(target.getText().toString().trim(), false));
    nodeCancel = secondary(c, "Cancel connection", v -> request(BoardService.object("op", "pair_cancel"), x -> {
      nodeActionPending = false;
      if (ok(x)) { board.nodePairStarted(); scanState.setText("Connection cancelled. You can connect again when ready."); }
      changed();
    }));
    nodeCancel.setVisibility(View.GONE);
    nodeState = text("Connect your MESHBBS board first.", 15, TEAL); c.addView(nodeState);
    hardwareInfo = text("", 14, MUTED); c.addView(hardwareInfo);
    secondary(c, "Connection options", v -> radioOptions.setVisibility(radioOptions.getVisibility()==View.VISIBLE ? View.GONE : View.VISIBLE));
    radioOptions = vertical(); c.addView(radioOptions);
    target = input(radioOptions, "Radio name or Bluetooth address", false);
    target.addTextChangedListener(new android.text.TextWatcher() {
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
      public void onTextChanged(CharSequence s, int start, int before, int count) { updateRadioConnect(); }
      public void afterTextChanged(android.text.Editable s) {}
    });
    noPin = check(radioOptions, "My radio is explicitly configured with no PIN");
    noPinHelp = text("Leave this off unless you changed the radio to No PIN mode. A fixed PIN still needs pairing.", 14, MUTED);
    radioOptions.addView(noPinHelp);
    hopOptions = vertical(); radioOptions.addView(hopOptions);
    hopOptions.addView(text("Maximum mesh hops", 14, MUTED));
    hops = spinner(hopOptions, new String[]{"0", "1", "2", "3", "4", "5", "6", "7"}); hops.setSelection(3);
    radioOptions.setVisibility(View.GONE);
    secondary(c, "Radio guide & compatibility", v -> radioGuide());
    c.addView(text("Your owner phone reconnects automatically when the app is open and the board is nearby.", 14, MUTED));
  }
  private String hardwareName() {
    if (board == null) return "";
    String name=board.nodeStatus.optString("hardware");
    if (!name.isEmpty()) return name;
    int model=board.nodeStatus.optInt("hw");
    return model == 0 ? "" : RadioModels.name(model);
  }
  private void radioGuide() {
    LinearLayout p=vertical(); p.setPadding(dp(18),0,dp(18),0);
    p.addView(text(RadioProfile.guide(hardwareName(),protocol()),16,INK));
    p.addView(text("A radio's name can be changed. We identify the protocol from its service, then read model details after pairing. Models listed here are documented candidates unless marked as tested.",14,MUTED));
    EditText search=input(p,"Search models, for example Heltec or RAK",false);
    TextView results=text("",14,INK); p.addView(results);
    try {
      java.io.InputStream in=getAssets().open("radio-catalog.json");
      java.io.ByteArrayOutputStream out=new java.io.ByteArrayOutputStream(); byte[] buf=new byte[4096]; int n;
      while((n=in.read(buf))>0) out.write(buf,0,n); in.close();
      final JSONArray entries=new JSONObject(new String(out.toByteArray(),StandardCharsets.UTF_8)).getJSONArray("radios");
      Runnable filter=() -> {
        String q=search.getText().toString().toLowerCase(Locale.ROOT); StringBuilder text=new StringBuilder(); int count=0;
        for(int i=0;i<entries.length();i++) { JSONObject e=entries.optJSONObject(i); if(e==null) continue;
          if(!(e.optString("name")+" "+e.optString("protocol")).toLowerCase(Locale.ROOT).contains(q)) continue;
          if(count++>=40) continue;
          text.append(e.optString("name")).append(" · ").append(e.optString("protocol")).append("\n")
              .append(e.optString("status")).append("\n").append(e.optString("note")).append("\n\n");
        }
        results.setText(count==0 ? "No matching model. A model not listed may still work if it exposes the supported Bluetooth service." : (count>40?"Showing 40 of "+count+" matches. Type a model name to narrow the list.\n\n":"")+text.toString());
      };
      search.addTextChangedListener(new android.text.TextWatcher(){ public void beforeTextChanged(CharSequence s,int start,int count,int after){} public void onTextChanged(CharSequence s,int start,int before,int count){filter.run();} public void afterTextChanged(android.text.Editable e){} }); filter.run();
    } catch(Exception ex) { results.setText("See the compatibility guide included with your download."); }
    ScrollView scroll=new ScrollView(this); scroll.addView(p);
    new AlertDialog.Builder(this).setTitle("Your radio guide").setView(scroll).setPositiveButton("Done",null).show();
  }
  private void settings() {
    LinearLayout p = pages[2], c = card(p);
    c.addView(text("Notifications", 20, INK));
    Spinner modes = spinner(
        c, new String[] {"Sound + vibration", "Vibrate only", "Silent notifications", "Muted"});
    modes.setSelection(prefs().getInt("notifications", 0));
    modes.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      public void onItemSelected(AdapterView<?> a, View v, int i, long id) {
        prefs().edit().putInt("notifications", i).apply();
      }
      public void onNothingSelected(AdapterView<?> a) {}
    });
    CheckBox background = check(c, "Monitor nearby board while app is in background");
    background.setChecked(prefs().getBoolean("background", true));
    background.setOnCheckedChangeListener(
        (v, b) -> prefs().edit().putBoolean("background", b).apply());
    c.addView(text("Notifications use the nearby Bluetooth link, not the Internet. Android shows "
            + "an ongoing connection notification for background monitoring. System "
            + "notification settings can override sound and vibration.",
        13, MUTED));
    button(c, "Android notification settings",
        v
        -> startActivity(new Intent(android.provider.Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                .putExtra(android.provider.Settings.EXTRA_APP_PACKAGE, getPackageName())));
    c = card(p);
    c.addView(text("Community rules", 20, INK));
    c.addView(text("Visitors send RULES to read these. Write up to six short community rules. "
                   + "Only the owner app can change them.",
        14, MUTED));
    button(c, "Edit board rules", v -> editRules());
    c = card(p);
    c.addView(text("Ban list", 20, INK));
    c.addView(text("Banned node IDs receive your ban message instead of accessing the BBS. You can "
            + "also ban a commenter from a thread.",
        14, MUTED));
    banNode = input(c, "Node ID, for example !1234abcd", false);
    button(c, "Ban node", v -> ban(banNode.getText().toString(), true));
    banMessage = input(c, "Message shown to banned visitors", true);
    banMessage.setText("BBS: Access denied. This node is banned. Contact the board owner.");
    button(c, "Save ban response",
        v
        -> request(BoardService.object(
                       "op", "ban_message", "text", banMessage.getText().toString().trim()),
            r -> {
              if (ok(r))
                toast("Ban response saved");
            }));
    button(c, "Refresh ban list", v -> loadBans());
    banList = vertical();
    c.addView(banList);
    c = card(p);
    c.addView(text("Clock & expiry", 20, INK));
    c.addView(text(
        "New bulletins default to 24 hours. Choose no expiry, a custom duration, or a date and "
            + "time when creating one. Radio timestamps are UTC; the app displays your phone's "
            + "local "
            + "time. After power loss, reconnect the app to restore the board's clock.",
        14, MUTED));
    button(c, "Sync phone time",
        v
        -> request(
            BoardService.object("op", "clock", "time", System.currentTimeMillis() / 1000), r -> {
              if (ok(r))
                toast("Clock synchronized");
            }));
    c = card(p);
    c.addView(text("About MESHBBS", 20, INK));
    c.addView(text("Version 0.5.0 · GPL-3.0\nAn independent community bulletin board for Meshtastic and MeshCore. No accounts or analytics.", 14, MUTED));
    secondary(c, "Source, license & help", v -> openProject(""));
    secondary(c, "Privacy", v -> openProject("/blob/main/PRIVACY.md"));
  }
  private void openProject(String path) {
    try { startActivity(new Intent(Intent.ACTION_VIEW, android.net.Uri.parse("https://github.com/NikOh096/ESP32S3-MESH-BBS" + path))); }
    catch (ActivityNotFoundException e) { toast("Open github.com/NikOh096/ESP32S3-MESH-BBS in your browser."); }
  }
  private void console() {
    LinearLayout c = card(pages[3]);
    c.addView(text("Activity", 22, INK));
    c.addView(text("Connection events and board message-processing activity. PINs and owner keys "
            + "are excluded.",
        13, MUTED));
    button(c, "Copy activity", v -> {
      ((android.content.ClipboardManager) getSystemService(CLIPBOARD_SERVICE))
          .setPrimaryClip(ClipData.newPlainText("MESHBBS activity", console.getText()));
      toast("Copied");
    });
    console = text("", 12, INK);
    console.setTypeface(Typeface.MONOSPACE);
    c.addView(console);
  }
  private void guide() {
    new AlertDialog.Builder(this)
        .setTitle("Using the bulletin board")
        .setMessage(
            "Message the connected radio from another radio on the same mesh.\n\nUPDATE — oldest-to-newest "
            + "list\nA000001 — read this permanent ID\n!A000001 Your reply — add a comment\nCREATE "
            + "Title | Bulletin text — start a 24-hour thread\nQUEUE — your waiting "
            + "positions\nRULES — community rules\nHELP — short guide\nPING — connection "
              + "test\n\nPositions 1–9 move up "
            + "after deletion; IDs never change or repeat. Every thread ends with its expiry time. "
            + "When all nine places are occupied, new threads join the waiting queue (up to 32). "
            + "Their 24 hours starts when published.\n\nWait for numbered replies, five seconds "
            + "apart. Titles: 48 UTF-8 bytes; text: 160 bytes; 32 comments per thread. Comments "
            + "are public. Node names are labels, not verified identities. Only the owner app "
            + "moderates, changes expiry or bans nodes.")
        .setPositiveButton("Close", null)
        .show();
  }
  private void showQueue() {
    LinearLayout list = vertical();
    list.setPadding(dp(18), 0, dp(18), 0);
    ScrollView scroll = new ScrollView(this);
    scroll.addView(list);
    AlertDialog dialog = new AlertDialog.Builder(this)
                             .setTitle("Waiting bulletins")
                             .setView(scroll)
                             .setNegativeButton("Close", null)
                             .create();
    dialog.show();
    readQueue(list, dialog, 0);
  }
  private void readQueue(LinearLayout list, AlertDialog dialog, int index) {
    if (!dialog.isShowing())
      return;
    request(BoardService.object("op", "queue_list", "index", index), r -> {
      if (!ok(r) || !dialog.isShowing())
        return;
      if (r.optInt("count") == 0) {
        list.addView(text("No bulletins waiting.", 16, INK));
        return;
      }
      if (!r.has("title"))
        return;
      LinearLayout c = card(list);
      c.addView(text("Queue #" + r.optInt("position") + " · " + r.optString("title"), 17, INK));
      c.addView(text(r.optString("name") + " · " + date(r.optLong("at")), 13, MUTED));
      button(c, "Remove queued bulletin",
          v
          -> new AlertDialog.Builder(this)
              .setTitle("Remove from queue?")
              .setPositiveButton("Remove",
                  (d, w)
                      -> request(BoardService.object("op", "queue_delete", "node",
                                     r.optLong("node"), "packet", r.optLong("packet")),
                          x -> {
                            if (ok(x)) {
                              dialog.dismiss();
                              board.refresh();
                            }
                          }))
              .setNegativeButton("Cancel", null)
              .show());
      if (index + 1 < r.optInt("count"))
        readQueue(list, dialog, index + 1);
    });
  }
  private void request(JSONObject j, BoardService.Reply callback) {
    if (board == null) {
      toast("Waiting for Bluetooth service");
      return;
    }
    board.request(j, callback);
  }
  private boolean ok(JSONObject r) {
    if (!r.optBoolean("ok")) {
      toast(r.optString("error", "Request failed"));
      return false;
    }
    return true;
  }
  private static String date(long at) {
    return at == 0 ? "No expiry / unknown time"
                   : DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT)
                         .format(new Date(at * 1000));
  }
  @Override
  public void changed() {
    if (board == null)
      return;
    state.setText(board.ready ? (board.nodeStatus.optBoolean("api") ? "Board connected · Radio ready" : "Board connected · Radio not ready") : board.message);
    if(!board.ready) { nodeActionPending=false; scanningRadios=false; }
    boolean active = nodeActionPending || NodePairing.active(board.nodeStatus.optString("stage")) || ("ready".equals(board.nodeStatus.optString("stage")) && !board.nodeStatus.optBoolean("api"));
    boardConnect.setText(board.ready ? "Connected" : board.isConnecting() ? "Connecting…" : board.address.isEmpty() ? "Find my board" : "Connect");
    boardConnect.setEnabled(!board.ready && !board.isConnecting());
    ownerInstructions.setVisibility(board.ready ? View.GONE : View.VISIBLE);
    enroll.setVisibility(board.enrollment && !board.ready ? View.VISIBLE : View.GONE);
    nodeFind.setEnabled(board.ready && !active && !scanningRadios);
    nodeFind.setText(scanningRadios ? "Searching…" : "Find nearby radios");
    updateRadioConnect();
    nodeCancel.setVisibility(board.ready && active ? View.VISIBLE : View.GONE);
    for (Button choice : nodeButtons.values()) choice.setEnabled(board.ready && !active && Boolean.TRUE.equals(choice.getTag()));
    hopOptions.setVisibility(RadioProfile.meshCore(protocol()) ? View.GONE : View.VISIBLE);
    noPin.setVisibility(RadioProfile.meshCore(protocol()) ? View.GONE : View.VISIBLE);
    noPinHelp.setVisibility(noPin.getVisibility());
    radioHeading.setText("2 · Your " + protocol() + " radio");
    createButton.setText(board.ready ? "Create a bulletin" : "Connect your board");
    hardwareInfo.setText(hardwareName().isEmpty() ? "" : hardwareName() + "\n" + RadioProfile.guide(hardwareName(),protocol()));
    enroll.setEnabled(board.enrollment && !board.ready);
    JSONObject s = board.status;
    JSONObject ns = board.nodeStatus;
    nodeState.setText(board.ready
            ? NodePairing.describe(ns.optString("stage"), ns.optInt("code"), ns.optBoolean("api"))
                + "\n" + (selectedName.isEmpty() ? ns.optString("target", s.optString("target")) : selectedName)

            : "Connect to MESHBBS to configure the radio.");
    if (!filled && board.ready && !s.optString("target").isEmpty()) {
      target.setText(s.optString("target"));
      hops.setSelection(s.optInt("hops", 3));
      noPin.setChecked(s.optBoolean("no_pin"));
      filled = true;
    }
    totals.setText("Lifetime: " + s.optLong("total_threads") + " published threads · "
        + s.optLong("total_replies") + " replies · "
        + (s.optLong("total_threads") + s.optLong("total_replies"))
        + " messages\nWaiting queue: " + s.optInt("queued"));
    String[] activityLines=board.activity.split("\\n");
    StringBuilder recent=new StringBuilder();
    for(int i=Math.max(0,activityLines.length-5);i<activityLines.length;i++) recent.append(activityLines[i]).append('\n');
    homeLog.setText(recent.toString());
    console.setText(board.activity + "\n\n" + ActivityLog.snapshot());
    String fingerprint = board.ready + Arrays.toString(board.threads);
    if (!fingerprint.equals(lastHome)) {
      lastHome = fingerprint;
      renderHome();
    }
    long attempt = ns.optLong("attempt");
    boolean needsPin = board.ready && NodePairing.needsPin(ns.optString("stage"), attempt);
    if (pinDialog != null && pinDialog.isShowing() && (!needsPin || pinPromptedAttempt != attempt))
      pinDialog.dismiss();
    if (visible && needsPin && pinPromptedAttempt != attempt)
      pin();
    if (openFromNotification > 0 && board.ready) {
      for (int i = 0; i < 9; i++)
        if (board.threads[i] != null && board.threads[i].optBoolean("active")
            && board.threads[i].optInt("generation") == openFromNotification) {
          openFromNotification = 0;
          openThread(i + 1);
          break;
        }
    }
  }
  private void renderHome() {
    homeCards.removeAllViews();
    if(board==null || !board.ready) { homeCards.addView(text("Your bulletins will appear here once your board connects.",16,MUTED)); return; }
    int count=0;
    for(int i=0;i<9;i++) {
      JSONObject t=board.threads[i]; if(t==null || !t.optBoolean("active")) continue; count++;
      final int slot=i+1; LinearLayout c=card(homeCards);
      c.addView(text(t.optString("title"),20,INK));
      c.addView(text(t.optString("thread_id") + " · " + t.optInt("count") + " replies" + (t.optInt("unread")>0 ? " · " + t.optInt("unread") + " new" : ""),14,TEAL));
      c.addView(text(t.optLong("expires")==0 ? "Stays until you remove it" : "Expires " + date(t.optLong("expires")),14,MUTED));
      secondary(c,"Read conversation",v->openThread(slot));
    }
    if(count==0) homeCards.addView(text("A fresh board. Post a welcome message to start the conversation.",16,MUTED));
  }
  private void create(int slot) {
    LinearLayout form = vertical();
    form.setPadding(dp(20), 0, dp(20), 0);
    EditText title = input(form, "Title", false),
             body = input(form, "Bulletin text", true);
    final long[] expiry = {System.currentTimeMillis() / 1000 + 86400};
    Button expiryButton = button(form, "Expires in 24 hours", null);
    expiryButton.setOnClickListener(v -> expiry(expiryButton, expiry));
    AlertDialog dialog = new AlertDialog.Builder(this)
                             .setTitle("Create bulletin " + slot)
                             .setView(form)
                             .setNegativeButton("Cancel", null)
                             .setPositiveButton("Create", null)
                             .create();
    dialog.setOnShowListener(d -> dialog.getButton(-1).setOnClickListener(v -> {
      String a = title.getText().toString().trim(),
             b = body.getText().toString().trim().replaceAll("[\\r\\n\\t]+", " ");
      if (a.isEmpty() || b.isEmpty() || a.getBytes(StandardCharsets.UTF_8).length > 48
          || b.getBytes(StandardCharsets.UTF_8).length > 160) {
        toast("Use a title up to 48 bytes and text up to 160 bytes");
        return;
      }
      request(BoardService.object(
                  "op", "create", "slot", slot, "title", a, "text", b, "expires", expiry[0]),
          r -> {
            if (ok(r)) {
              dialog.dismiss();
              board.refresh();
            }
          });
    }));
    dialog.show();
  }
  private void expiry(Button label, long[] result) {
    new AlertDialog.Builder(this)
        .setTitle("Bulletin expiry")
        .setItems(new String[] {"24 hours from now", "No expiry", "Custom duration (minutes)",
                      "Choose date and time"},
            (d, which) -> {
              if (which == 0) {
                result[0] = System.currentTimeMillis() / 1000 + 86400;
                label.setText("Expires in 24 hours");
              } else if (which == 1) {
                result[0] = 0;
                label.setText("No expiry");
              } else if (which == 2) {
                EditText e = new EditText(this);
                e.setInputType(InputType.TYPE_CLASS_NUMBER);
                e.setHint("Minutes");
                new AlertDialog.Builder(this)
                    .setTitle("Duration in minutes")
                    .setView(e)
                    .setPositiveButton("Use",
                        (x, w) -> {
                          try {
                            long minutes = Long.parseLong(e.getText().toString());
                            if (minutes < 1 || minutes > 5256000)
                              throw new Exception();
                            result[0] = System.currentTimeMillis() / 1000 + minutes * 60;
                            label.setText("Expires " + date(result[0]));
                          } catch (Exception ex) {
                            toast("Enter 1 to 5,256,000 minutes");
                          }
                        })
                    .setNegativeButton("Cancel", null)
                    .show();
              } else {
                Calendar c = Calendar.getInstance();
                new DatePickerDialog(this,
                    (v, y, m, day)
                        -> {
                      c.set(y, m, day);
                      new TimePickerDialog(this, (view, h, min) -> {
                        c.set(Calendar.HOUR_OF_DAY, h);
                        c.set(Calendar.MINUTE, min);
                        c.set(Calendar.SECOND, 0);
                        long t = c.getTimeInMillis() / 1000;
                        if (t <= System.currentTimeMillis() / 1000) {
                          toast("Choose a future time");
                          return;
                        }
                        result[0] = t;
                        label.setText("Expires " + date(t));
                      }, c.get(Calendar.HOUR_OF_DAY), c.get(Calendar.MINUTE), true).show();
                    },
                    c.get(Calendar.YEAR), c.get(Calendar.MONTH), c.get(Calendar.DAY_OF_MONTH))
                    .show();
              }
            })
        .show();
  }
  private void openThread(int slot) {
    JSONObject t = board == null ? null : board.threads[slot - 1];
    if (t == null || !t.optBoolean("active")) {
      toast("Refresh the bulletin list first");
      return;
    }
    int generation = t.optInt("generation");
    LinearLayout content = vertical();
    content.setPadding(dp(18), 0, dp(18), 0);
    ScrollView scroll = new ScrollView(this);
    scroll.addView(content);
    AlertDialog dialog = new AlertDialog.Builder(this)
                             .setTitle(t.optString("thread_id"))
                             .setView(scroll)
                             .setNegativeButton("Close", null)
                             .create();
    dialog.show();
    request(BoardService.object("op", "read", "slot", slot, "generation", generation, "index", -1),
        r -> {
          if (!ok(r))
            return;
          content.addView(text(t.optString("title"), 20, INK));
          content.addView(text(date(r.optLong("at")) + " · Owner", 12, MUTED));
          content.addView(text(r.optString("text"), 17, INK));
          long[] until = {r.optLong("expires")};
          Button expiryButton =
              button(content, until[0] == 0 ? "No expiry" : "Expires " + date(until[0]), v -> {});
          expiryButton.setOnClickListener(v -> expiry(expiryButton, until));
          button(content, "Save expiry",
              v
              -> request(BoardService.object("op", "expiry", "slot", slot, "generation", generation,
                             "expires", until[0]),
                  x -> {
                    if (ok(x)) {
                      toast("Expiry saved");
                      board.refresh();
                    }
                  }));
          button(content, "Delete bulletin and its replies",
              v
              -> new AlertDialog.Builder(this)
                  .setTitle("Delete " + t.optString("thread_id") + "?")
                  .setMessage("This removes this bulletin and all its replies from the board.")
                  .setNegativeButton("Cancel", null)
                  .setPositiveButton("Delete",
                      (d, w)
                          -> request(BoardService.object(
                                         "op", "delete", "slot", slot, "generation", generation),
                              x -> {
                                if (ok(x)) {
                                  dialog.dismiss();
                                  board.refresh();
                                }
                              }))
                  .show());
          content.addView(text("Replies", 19, INK));
          loadReply(content, dialog, slot, generation, 0, r.optInt("count"));
        });
  }
  private void loadReply(
      LinearLayout content, AlertDialog dialog, int slot, int generation, int index, int count) {
    if (!dialog.isShowing())
      return;
    if (index >= count) {
      request(
          BoardService.object("op", "seen", "slot", slot, "generation", generation, "count", count),
          r -> {
            if (ok(r)) {
              board.clearNotification(generation);
              board.refresh();
            }
          });
      return;
    }
    request(
        BoardService.object("op", "read", "slot", slot, "generation", generation, "index", index),
        r -> {
          if (!ok(r) || !dialog.isShowing())
            return;
          LinearLayout c = card(content);
          String id = String.format(Locale.ROOT, "!%08x", r.optLong("node"));
          c.addView(text(r.optString("name") + " " + id + "\n" + date(r.optLong("at")), 13, MUTED));
          c.addView(text(r.optString("text"), 16, INK));
          button(c, "Ban this node", v -> ban(id, true));
          loadReply(content, dialog, slot, generation, index + 1, count);
        });
  }
  private void ban(String node, boolean add) {
    String id = node.trim();
    if (!id.matches("!?[0-9a-fA-F]{8}")) {
      toast("Use a full eight-digit hexadecimal node ID");
      return;
    }
    new AlertDialog.Builder(this)
        .setTitle((add ? "Ban " : "Unban ") + id + "?")
        .setPositiveButton(add ? "Ban" : "Unban",
            (d, w)
                -> request(BoardService.object("op", add ? "ban_add" : "ban_remove", "node", id),
                    r -> {
                      if (ok(r))
                        loadBans();
                    }))
        .setNegativeButton("Cancel", null)
        .show();
  }
  private void loadBans() {
    if (board == null || !board.ready)
      return;
    banList.removeAllViews();
    readBan(0);
  }
  private void readBan(int index) {
    request(BoardService.object("op", "ban_list", "index", index), r -> {
      if (!ok(r))
        return;
      if (index == 0 && r.has("message"))
        banMessage.setText(r.optString("message"));
      String id = r.optString("node", "");
      if (!id.isEmpty())
        button(banList, "Unban " + id, v -> ban(id, false));
      if (index + 1 < r.optInt("count"))
        readBan(index + 1);
    });
  }
  private void pin() {
    if (board == null || !board.ready
        || !NodePairing.needsPin(
            board.nodeStatus.optString("stage"), board.nodeStatus.optLong("attempt"))) {
      toast("Select the node first. The PIN prompt appears when the radio requests it.");
      return;
    }
    if (pinDialog != null && pinDialog.isShowing())
      return;
    final long attempt = board.nodeStatus.optLong("attempt");
    pinPromptedAttempt = attempt;
    EditText e = new EditText(this);
    e.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
    e.setSaveEnabled(false);
    e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);
    e.setHint("Six-digit radio PIN");
    pinDialog = new AlertDialog.Builder(this)
                    .setTitle("Node pairing PIN")
                    .setMessage(RadioProfile.pinHelp(protocol(), board.nodeStatus.optString("pin_mode"))
                                + " MESHBBS remembers the pairing for next time.")
                    .setView(e)
                    .setPositiveButton("Pair", null)
                    .setNegativeButton("Cancel pairing", (d, w) -> cancelPin(attempt))
                    .create();
    pinDialog.setCanceledOnTouchOutside(false);
    pinDialog.setOnCancelListener(d -> cancelPin(attempt));
    final AlertDialog dialog = pinDialog;
    dialog.setOnShowListener(d -> dialog.getButton(-1).setOnClickListener(v -> {
      String value = e.getText().toString();
      if (!value.matches("[0-9]{6}")) {
        toast("Enter exactly six digits");
        return;
      }
      dialog.getButton(-1).setEnabled(false);
      request(BoardService.object("op", "pin", "pin", value, "attempt", attempt), r -> {
        e.setText("");
        dialog.getButton(-1).setEnabled(true);
        if (ok(r))
          dialog.dismiss();
      });
    }));
    dialog.setOnDismissListener(d -> e.setText(""));
    dialog.show();
  }
  private void cancelPin(long attempt) {
    request(BoardService.object("op", "pair_cancel", "attempt", attempt), r -> ok(r));
  }
  private void editRules() {
    LinearLayout list = vertical();
    list.setPadding(dp(16), 0, dp(16), 0);
    ScrollView scroll = new ScrollView(this);
    scroll.addView(list);
    AlertDialog d = new AlertDialog.Builder(this)
                        .setTitle("Community rules")
                        .setView(scroll)
                        .setNegativeButton("Close", null)
                        .create();
    d.show();
    readRule(list, d, 0);
  }
  private void readRule(LinearLayout list, AlertDialog dialog, int index) {
    if (!dialog.isShowing())
      return;
    request(BoardService.object("op", "rules_get", "index", index), r -> {
      if (!ok(r) || !dialog.isShowing())
        return;
      LinearLayout c = card(list);
      c.addView(text("Rule " + (index + 1), 17, INK));
      EditText e = input(c, "Rule text (leave empty to remove)", true);
      e.setText(r.optString("text"));
      button(c, "Save rule " + (index + 1), v -> {
        String value = e.getText().toString().trim().replaceAll("[\\r\\n\\t]+", " ");
        if (value.getBytes(StandardCharsets.UTF_8).length > 160) {
          toast("Each rule can use up to 160 UTF-8 bytes");
          return;
        }
        request(BoardService.object("op", "rules_set", "index", index, "text", value), x -> {
          if (ok(x))
            toast("Rule saved on MESHBBS");
        });
      });
      if (index + 1 < r.optInt("count"))
        readRule(list, dialog, index + 1);
    });
  }
  private void selectNode(String address, boolean scanned) {
    if (address.isEmpty()) {
      toast("Choose a node first");
      return;
    }
    if (nodeActionPending || board == null || !board.ready) return;
    nodeActionPending = true;
    scanningRadios = false;
    ++nodeScanToken;
    target.setText(address);
    changed();
    filled = true;
    scanState.setText("Starting pairing with " + address + "…");
    request(BoardService.object("op", scanned ? "node_select" : "configure", "target", address,
                "hops", RadioProfile.meshCore(protocol()) ? 3 : hops.getSelectedItemPosition(), "no_pin", !RadioProfile.meshCore(protocol()) && noPin.isChecked()),
        r -> {
          nodeActionPending = false;
          if (ok(r)) {
            pinPromptedAttempt = 0;
            board.nodePairStarted();
            scanState.setText("Pairing requested. Enter the radio PIN when asked.");
          } else
            scanState.setText(r.optString("error"));
          changed();
        });
  }
  private void scanNodes() {
    if (board == null || !board.ready) {
      toast("Connect to MESHBBS first");
      return;
    }
    if(scanningRadios) return;
    scanningRadios=true;
    changed();
    final int ticket = ++nodeScanToken;
    nodeButtons.clear();
    nodeList.removeAllViews();
    scanState.setText("Looking for radios within range of MESHBBS…");
    request(BoardService.object("op", "node_scan"), r -> {
      if (ticket != nodeScanToken)
        return;
      if (!ok(r)) {
        scanningRadios=false; changed();
        scanState.setText(r.optString("error"));
        return;
      }
      ui.postDelayed(() -> readNodes(ticket, 0), 500);
    });
  }
  private void readNodes(int ticket, int index) {
    if (ticket != nodeScanToken || isDestroyed() || board == null || !board.ready)
      return;
    request(BoardService.object("op", "node_scan_results", "index", index), r -> {
      if (ticket != nodeScanToken) return;
      if (!ok(r)) { scanningRadios=false; changed(); scanState.setText("Search stopped. Tap Find nearby radios to search again."); return; }
      String address = r.optString("address");
      if (!address.isEmpty()) {
        Button b = nodeButtons.get(address);
        if (b == null) {
          final String name=r.optString("name");
          b = secondary(nodeList, "", v -> { selectedName=name; selectNode(address, true); });
          nodeButtons.put(address, b);
        }
        String name = r.optString("name");
        String detected=r.optString("protocol",protocol());
        boolean compatible=detected.equals(protocol()) && r.optBoolean("connectable");
        b.setText((name.isEmpty() ? detected + " radio" : name) + "\n" + detected + " · " + RadioProfile.signal(r.optInt("rssi"))
            + (!detected.equals(protocol()) ? "\nNeeds the " + detected + " BBS firmware" : "")
            + "\n" + address);
        b.setTag(compatible);
        b.setEnabled(compatible && !NodePairing.active(board.nodeStatus.optString("stage")));
      }
      if (index + 1 < r.optInt("count")) {
        readNodes(ticket, index + 1);
        return;
      }
      if (r.optBoolean("scanning")) {
        scanState.setText("Searching… Tap a radio to connect.");
        ui.postDelayed(() -> readNodes(ticket, 0), 750);
      } else if (r.optInt("code") != 0)
        scanState.setText("Node scan failed (" + r.optInt("code") + "). Try again.");
      else
        scanState.setText(nodeButtons.isEmpty()
                ? "No radios found. Enable Bluetooth on the radio and "
                  + "disconnect other clients, then scan again."
                : "Choose your radio to connect.");
      if(!r.optBoolean("scanning")) { scanningRadios=false; changed(); }
    });
  }
  private void ensurePermissions() {
    ArrayList<String> p = new ArrayList<>();
    String[] bt = Build.VERSION.SDK_INT >= 31
        ? new String[] {Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT}
        : new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
    for (String s : bt)
      if (checkSelfPermission(s) != PackageManager.PERMISSION_GRANTED)
        p.add(s);
    if (Build.VERSION.SDK_INT >= 33
        && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
            != PackageManager.PERMISSION_GRANTED)
      p.add(Manifest.permission.POST_NOTIFICATIONS);
    if (!p.isEmpty())
      requestPermissions(p.toArray(new String[0]), 7);
    else
      start();
  }
  @Override
  public void onRequestPermissionsResult(int r, String[] p, int[] g) {
    super.onRequestPermissionsResult(r, p, g);
    if (r == 7) {
      if (Build.VERSION.SDK_INT >= 31
          && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)
              != PackageManager.PERMISSION_GRANTED) {
        toast("Bluetooth access is needed to connect");
        return;
      }
      start();
    }
  }
  private void start() {
    if (bound)
      return;
    Intent intent = new Intent(this, BoardService.class);
    try {
      startForegroundService(intent);
      bindService(intent, connection, BIND_AUTO_CREATE);
      bound = true;
    } catch (Exception ex) {
      toast("Unable to start Bluetooth monitoring. Check permissions.");
    }
  }
  private final ServiceConnection connection = new ServiceConnection() {
    public void onServiceConnected(ComponentName n, IBinder b) {
      board = ((BoardService.LocalBinder) b).service();
      board.listener = MainActivity.this;
      board.setVisible(visible);
      changed();
    }
    public void onServiceDisconnected(ComponentName n) {
      board = null;
    }
  };
  @Override
  protected void onResume() {
    super.onResume();
    visible = true;
    if (board != null) {
      board.setVisible(true);
      changed();
    }
  }
  @Override
  protected void onStop() {
    visible = false;
    if (board != null)
      board.setVisible(false);
    super.onStop();
  }
  @Override
  protected void onNewIntent(Intent i) {
    super.onNewIntent(i);
    setIntent(i);
    openFromNotification = i.getIntExtra("thread_id", 0);
    changed();
  }
  @Override
  protected void onDestroy() {
    stopScan();
    ++nodeScanToken;
    if (pinDialog != null)
      pinDialog.dismiss();
    if (board != null) {
      board.listener = null;
      board.setVisible(false);
    }
    if (bound)
      unbindService(connection);
    super.onDestroy();
  }
  private void scan() {
    if (Build.VERSION.SDK_INT >= 31
        && checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)
            != PackageManager.PERMISSION_GRANTED) {
      ensurePermissions();
      return;
    }
    stopScan();
    BluetoothAdapter adapter =
        ((BluetoothManager) getSystemService(BLUETOOTH_SERVICE)).getAdapter();
    if (adapter == null || !adapter.isEnabled()) {
      toast("Turn Bluetooth on");
      return;
    }
    scanner = adapter.getBluetoothLeScanner();
    found.clear();
    boardList.removeAllViews();
    boardScanState.setText("Searching for boards in pairing mode…");
    final int token = ++scanToken;
    scan = new ScanCallback() {
      public void onScanResult(int type, ScanResult r) {
        ui.post(() -> {
          if (token != scanToken || r.getScanRecord() == null)
            return;
          String name = r.getScanRecord().getDeviceName();
          if (name == null)
            name = "";
          List<UUID> services = new ArrayList<>();
          if (r.getScanRecord().getServiceUuids() != null)
            for (ParcelUuid u : r.getScanRecord().getServiceUuids()) services.add(u.getUuid());
          if (!DeviceDiscovery.isBoard(name, services))
            return;
          String address = r.getDevice().getAddress();
          if (!found.add(address))
            return;
          final String label = name.isEmpty() ? address : name;
          boardScanState.setText("Tap your board to connect.");
          button(boardList, label + " · " + RadioProfile.signal(r.getRssi()), v -> {
            stopScan();
            boardScanState.setText("");
            if (board != null) {
              filled = false;
              board.select(address);
            }
          });
        });
      }
      public void onScanFailed(int code) {
        ui.post(() -> {
          if (token != scanToken) return;
          stopScan();
          ActivityLog.add("SCAN", "Board scan failed: " + code);
          boardScanState.setText("Search could not start. Check Bluetooth and nearby-device access, then choose Find my board again.");
        });
      }
    };
    try {
      scanner.startScan(Collections.emptyList(),
          new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scan);
    } catch (SecurityException ex) {
      stopScan();
      boardScanState.setText("Allow nearby-device access in Android settings, then search again.");
      toast("Allow nearby devices / location access in Android settings to scan");
      return;
    }
    ui.postDelayed(() -> {
      if (token == scanToken) {
        stopScan();
        boardScanState.setText(found.isEmpty() ? "No boards found. Open pairing with the BOOT sequence above, move closer and choose Find my board again." : "Search finished. Tap your board to connect.");
      }
    }, 12000);
  }
  private void stopScan() {
    if (scan != null && scanner != null)
      try {
        scanner.stopScan(scan);
      } catch (Exception ignored) {
      }
    scan = null;
    ++scanToken;
  }
  private void updateRadioConnect() {
    if (board == null || target == null || nodeConnect == null) return;
    boolean active = nodeActionPending || NodePairing.active(board.nodeStatus.optString("stage")) || ("ready".equals(board.nodeStatus.optString("stage")) && !board.nodeStatus.optBoolean("api"));
    String wanted = target.getText().toString().trim();
    String current = board.nodeStatus.optString("target", board.status.optString("target"));
    boolean sameReady = board.nodeStatus.optBoolean("api") && wanted.equalsIgnoreCase(current);
    nodeConnect.setEnabled(board.ready && !active && !scanningRadios && !sameReady && !wanted.isEmpty());
    nodeConnect.setText(sameReady ? "Connected" : active ? "Connecting…" : "Connect");
  }
}
