package com.meshbbs.setup;

/** Radio pairing state is owned by MESHBBS, which must retain the radio's bond. */
public final class NodePairing {
  private NodePairing() {}
  public static boolean needsPin(String stage, long attempt) {
    return "enter_pin".equals(stage) && attempt > 0 && attempt <= 0xffffffffL;
  }
  public static boolean active(String stage) {
    return "searching".equals(stage) || "connecting".equals(stage) || "pairing".equals(stage)
        || "enter_pin".equals(stage) || "securing".equals(stage) || "discovering".equals(stage);
  }
  public static String describe(String stage, int code, boolean api) {
    switch (stage) {
      case "searching":
        return "Finding the selected node from MESHBBS…";
      case "connecting":
        return "Connecting to the node over Bluetooth…";
      case "pairing":
        return "Requesting pairing. Have the radio's displayed or configured PIN ready.";
      case "enter_pin":
        return "Enter your radio's six-digit PIN to continue.";
      case "securing":
        return "Checking the PIN and saving the Bluetooth bond…";
      case "discovering":
        return "Pairing complete. Opening the radio connection…";
      case "ready":
        return api ? "Connected · BBS ready" : "Connected · synchronizing the node…";
      case "failed":
        return "Couldn't connect. Keep the radio nearby and disconnect its other app, then tap Connect.";
      default:
        return "Choose a node to start pairing.";
    }
  }
}
