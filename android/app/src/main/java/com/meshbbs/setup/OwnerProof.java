package com.meshbbs.setup;
import java.nio.charset.StandardCharsets;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
/** Wire-level owner proof; Android Keystore storage lives in OwnerKey. */
public final class OwnerProof {
  private OwnerProof() {}
  public static String proof(byte[] key, String nonce, String id) throws Exception {
    if (key.length != 32 || !id.matches("[0-9A-F]{12}"))
      throw new IllegalArgumentException("Invalid owner identity");
    Mac mac = Mac.getInstance("HmacSHA256");
    mac.init(new SecretKeySpec(key, "HmacSHA256"));
    mac.update(unhex(nonce));
    return hex(mac.doFinal(id.getBytes(StandardCharsets.US_ASCII)));
  }
  public static String hex(byte[] bytes) {
    StringBuilder s = new StringBuilder();
    for (byte b : bytes) s.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
    return s.toString();
  }
  public static byte[] unhex(String text) {
    if (!text.matches("[0-9a-fA-F]{64}"))
      throw new IllegalArgumentException("Invalid challenge");
    byte[] b = new byte[32];
    for (int i = 0; i < 32; i++)
      b[i] = (byte) Integer.parseInt(text.substring(i * 2, i * 2 + 2), 16);
    return b;
  }
}
