import com.meshbbs.setup.SetupCodec;
import java.util.Arrays;
public class SetupCodecTest {
    public static void main(String[] args) {
        byte[] golden={1,3,4,0x40,(byte)0xe2,1,0,'T','e','s','t'};
        if(!Arrays.equals(golden,SetupCodec.encode("Test",123456,3)))throw new AssertionError("C/Java golden vector differs");
        byte[] noPin=SetupCodec.encode("Test",-1,3); for(int i=3;i<7;i++)if(noPin[i]!=(byte)0xff)throw new AssertionError();
        if(SetupCodec.encode("Test",-2,3)[3]!=(byte)0xfe)throw new AssertionError();
        SetupCodec.Status status=SetupCodec.decode(new byte[]{1,3,3,4,'T','e','s','t'});
        if(!status.hasPin||!status.saved||status.hops!=3||!status.target.equals("Test"))throw new AssertionError();
        for(String bad:new String[]{"","X\nY",new String(new char[64]).replace('\0','a'),"\ud800"}) {
            try {SetupCodec.encode(bad,123456,3);throw new AssertionError("Accepted invalid target");}catch(IllegalArgumentException expected){}
        }
        for(byte[] bad:new byte[][]{ {},{1,0,0},{2,0,0,0},{1,0,8,0},{1,0,0,2,65} }) {
            try{SetupCodec.decode(bad);throw new AssertionError("Accepted bad status");}catch(IllegalArgumentException expected){}
        }
        System.out.println("PASS: Android/C golden vectors, PIN modes, status and validation.");
    }
}
