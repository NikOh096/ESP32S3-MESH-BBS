package com.meshbbs.setup;

/** Names are user-editable; protocol metadata is the source for device details. */
public final class RadioProfile {
  private RadioProfile() {}
  public static String protocol(String reported, String version) {
    if ("MeshCore".equals(reported) || version.startsWith("MeshCore ")) return "MeshCore";
    return "Meshtastic"; // Older MESHBBS builds predate the protocol field.
  }
  public static boolean meshCore(String protocol) { return "MeshCore".equals(protocol); }
  public static String signal(int rssi) {
    return rssi >= -65 ? "Strong signal" : rssi >= -80 ? "Good signal" : "Weak signal · move closer";
  }
  public static String pinHelp(String protocol, String mode) {
    if ("fixed".equals(mode))
      return "Enter this radio's configured six-digit PIN. A fixed PIN does not change each time you connect.";
    if ("display".equals(mode))
      return "Enter the six-digit PIN shown on your radio. Wake its screen if needed.";
    return "Use the code shown on your radio. If it has no screen, use its configured PIN. "
        + "On an unchanged screenless setup this is often 123456. A custom PIN takes priority.";
  }
  public static String guide(String hardware, String protocol) {
    String base = meshCore(protocol)
        ? "Use Companion BLE firmware. Repeater, room-server and USB-only builds cannot provide this Bluetooth connection. "
          + "Add visitor contacts in MeshCore before leaving the BBS running."
        : "Enable Bluetooth on the radio and disconnect its other phone app. On ESP32 radios, enabled Wi-Fi can disable Bluetooth.";
    String h = hardware.toLowerCase(java.util.Locale.ROOT);
    if (h.contains("tower") || h.contains("solar") || h.contains("t1000") || h.contains("without display"))
      base += " Screenless radios usually use a fixed PIN. Use the owner's configured value.";
    if (h.contains("heltec") && (h.contains("v3") || h.contains("v 3")))
      base += " Keep MESHBBS close to this radio while pairing; Heltec V3 Bluetooth range can be limited.";
    if (h.contains("deck") || h.contains("pager"))
      base += " A standalone user interface may occupy the client connection. Enable the radio's companion or Bluetooth programming mode.";
    return base;
  }
}
