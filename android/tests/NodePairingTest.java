import com.meshbbs.setup.NodePairing;
public class NodePairingTest {
  private static void check(boolean condition) {
    if (!condition)
      throw new AssertionError();
  }
  public static void main(String[] args) {
    check(NodePairing.needsPin("enter_pin", 0xffffffffL));
    check(!NodePairing.needsPin("enter_pin", 0));
    check(!NodePairing.needsPin("enter_pin", -1));
    check(!NodePairing.needsPin("enter_pin", 0x100000000L));
    for (String s :
        new String[] {"searching", "connecting", "pairing", "securing", "discovering"}) {
      check(!NodePairing.needsPin(s, 123));
      check(NodePairing.active(s));
    }
    check(!NodePairing.active("failed"));
    check(!NodePairing.active("ready"));
    check(NodePairing.describe("failed", 574, false).contains("Connect"));
    check(NodePairing.describe("ready", 0, false).contains("synchronizing"));
    check(NodePairing.describe("ready", 0, true).contains("BBS ready"));
    System.out.println("PASS: radio PIN only on security request, unsigned attempt IDs, connection "
                       + "stages and API-ready state");
  }
}
