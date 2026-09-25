import com.meshbbs.setup.OwnerProof;
public class OwnerProofTest {
  public static void main(String[] args) throws Exception {
    byte[] key =
        OwnerProof.unhex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    String nonce = "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f",
           id = "0A0B0C0D0E0F";
    String proof = OwnerProof.proof(key, nonce, id);
    if (!proof.equals("321ae2049849155d5b2da1831ff805072a04df4ad291fd0291a7bda00c159b7b"))
      throw new AssertionError("HMAC vector mismatch");
    if (proof.equals(OwnerProof.proof(key, nonce, "0A0B0C0D0E10")))
      throw new AssertionError("Board identity not bound");
    if (proof.equals(OwnerProof.proof(key, OwnerProof.hex(key), id)))
      throw new AssertionError("Nonce not bound");
    boolean rejected = false;
    try {
      OwnerProof.unhex("bad");
    } catch (IllegalArgumentException e) {
      rejected = true;
    }
    if (!rejected)
      throw new AssertionError("Invalid nonce accepted");
    key[0]++;
    if (proof.equals(OwnerProof.proof(key, nonce, id)))
      throw new AssertionError("Wrong key accepted");
    System.out.println(
        "PASS: owner HMAC golden vector, board/nonce/key binding, challenge validation");
  }
}
