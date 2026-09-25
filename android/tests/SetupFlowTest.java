import com.meshbbs.setup.*;
import java.util.*;
public class SetupFlowTest {
    private static void check(boolean ok,String why){if(!ok)throw new AssertionError(why);}
    public static void main(String[] args) {
        SetupDraft draft=new SetupDraft();
        for(String phase:new String[]{"","connect","pair","discover","mtu","read"}) {
            check(SetupDraft.editingAllowed(phase),"Connection progress must not lock the node form: "+phase);
        }
        draft.edit("My RAK Node","123456",4,false);
        draft.disconnected();
        check(draft.encode().length>7,"Editing and validation must work before a board connection.");
        SetupCodec.Status remote=SetupCodec.decode(new byte[]{1,1,3,3,'O','l','d'});
        check(!draft.acceptRemote(remote,false),"Connecting must not overwrite a prepared node.");
        check(draft.target.equals("My RAK Node")&&draft.hops==4,"Draft was lost after read.");
        draft.edit("New node","",3,false);
        check(!draft.canKeepPin(),"A different node must not silently inherit the old node PIN.");
        try{draft.encode();throw new AssertionError("Missing new node PIN accepted");}catch(IllegalArgumentException expected){}
        check(draft.acceptRemote(remote,true),"Explicit reload must work.");
        check(draft.canKeepPin()&&draft.pinValue()==-2,"Same node should allow retaining its saved PIN.");
        draft.disconnected();
        check(!draft.canKeepPin(),"A disconnected board must not authorize retaining a PIN.");
        check(DeviceDiscovery.isBoard("MESHBBS-0D0E0F",Collections.emptyList()),"Name fallback failed.");
        check(DeviceDiscovery.isNode("Custom RAK",Arrays.asList(SetupCodec.MESH_SERVICE),false),"Custom Meshtastic name rejected.");
        check(DeviceDiscovery.isNode("Custom Heltec",Arrays.asList(SetupCodec.MESH_SERVICE),false),"Manufacturer restriction.");
        check(DeviceDiscovery.isNode("Custom device",Collections.emptyList(),true),"Show-all fallback failed.");
        check(!DeviceDiscovery.isNode("MESHBBS-0D0E0F",Collections.emptyList(),true),"BBS host is not its own node.");
        ActivityLog.clear();
        for(int i=0;i<305;i++)ActivityLog.add("TEST","event "+i);
        String log=ActivityLog.snapshot();
        check(log.split("\n").length==300&&!log.contains("event 0\n")&&log.endsWith("event 304"),"Console must retain the newest 300 events.");
        ActivityLog.clear();check(ActivityLog.snapshot().isEmpty(),"Console Clear did not clear.");
        System.out.println("PASS: disconnected editing, failed connection recovery, draft retention, per-node PIN and board-independent discovery.");
    }
}
