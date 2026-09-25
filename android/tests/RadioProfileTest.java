import com.meshbbs.setup.RadioProfile;
public class RadioProfileTest {
  static void check(boolean ok) { if(!ok) throw new AssertionError(); }
  public static void main(String[] args) {
    check(RadioProfile.protocol("", "2.7.26").equals("Meshtastic"));
    check(RadioProfile.protocol("", "MeshCore v1.17.1").equals("MeshCore"));
    check(RadioProfile.protocol("MeshCore", "unknown").equals("MeshCore"));
    check(RadioProfile.pinHelp("MeshCore","unknown").contains("configured PIN"));
    check(RadioProfile.pinHelp("Meshtastic","fixed").contains("does not change"));
    check(!RadioProfile.pinHelp("MeshCore","fixed").contains("123456"));
    check(RadioProfile.guide("Heltec Tower V2","MeshCore").contains("fixed PIN"));
    check(RadioProfile.guide("Heltec V3","Meshtastic").contains("range can be limited"));
    check(RadioProfile.guide("T-Deck","Meshtastic").contains("programming mode"));
    check(RadioProfile.signal(-90).contains("move closer"));
    System.out.println("PASS: legacy compatibility, firmware identification and fixed/display PIN guidance");
  }
}
