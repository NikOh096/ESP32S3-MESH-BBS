package com.meshbbs.setup;
import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.security.SecureRandom;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
final class OwnerKey {
  private static final String ALIAS = "meshbbs.owner.v1";
  private static SecretKey wrapping() throws Exception {
    KeyStore store = KeyStore.getInstance("AndroidKeyStore");
    store.load(null);
    if (!store.containsAlias(ALIAS)) {
      KeyGenerator g = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
      g.init(new KeyGenParameterSpec
              .Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
              .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
              .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
              .build());
      g.generateKey();
    }
    return (SecretKey) store.getKey(ALIAS, null);
  }
  static byte[] load(Context c, String id) throws Exception {
    String saved = c.getSharedPreferences("owner", 0).getString(id, null);
    if (saved == null)
      return null;
    byte[] value = Base64.decode(saved, Base64.NO_WRAP);
    if (value.length < 29)
      throw new Exception("Owner key invalid");
    Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
    cipher.init(
        Cipher.DECRYPT_MODE, wrapping(), new GCMParameterSpec(128, Arrays.copyOf(value, 12)));
    cipher.updateAAD(id.getBytes(StandardCharsets.US_ASCII));
    return cipher.doFinal(Arrays.copyOfRange(value, 12, value.length));
  }
  static byte[] create(Context c, String id) throws Exception {
    byte[] key = new byte[32];
    new SecureRandom().nextBytes(key);
    Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
    cipher.init(Cipher.ENCRYPT_MODE, wrapping());
    cipher.updateAAD(id.getBytes(StandardCharsets.US_ASCII));
    byte[] encrypted = cipher.doFinal(key), iv = cipher.getIV(),
           all = new byte[iv.length + encrypted.length];
    System.arraycopy(iv, 0, all, 0, iv.length);
    System.arraycopy(encrypted, 0, all, iv.length, encrypted.length);
    if (!c.getSharedPreferences("owner", 0)
            .edit()
            .putString(id, Base64.encodeToString(all, Base64.NO_WRAP))
            .commit())
      throw new Exception("Cannot save owner key");
    return key;
  }
  static String proof(byte[] key, String nonce, String id) throws Exception {
    return OwnerProof.proof(key, nonce, id);
  }
  static String hex(byte[] bytes) {
    return OwnerProof.hex(bytes);
  }
}
