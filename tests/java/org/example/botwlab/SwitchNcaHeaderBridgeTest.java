package org.example.botwlab;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Independent synthetic AES-XTS and parsing tests; no real console key/game. */
public final class SwitchNcaHeaderBridgeTest {
    private static void check(boolean pass, String what) {
        if (!pass) throw new AssertionError(what);
    }
    private static String hex(byte[] bytes) {
        StringBuilder out = new StringBuilder();
        for (byte b : bytes) out.append(String.format("%02x", b & 255));
        return out.toString();
    }
    // Synthetic encryption counterpart, verified against Python/OpenSSL XTS test output.
    private static byte[] encrypt(byte[] plain, byte[] key)
            throws GeneralSecurityException {
        byte[] result = new byte[plain.length];
        Cipher aes = Cipher.getInstance("AES/ECB/NoPadding");
        Cipher tweakAes = Cipher.getInstance("AES/ECB/NoPadding");
        aes.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, 0, 16, "AES"));
        tweakAes.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, 16, 16, "AES"));
        for (int sector = 0; sector < plain.length / 512; sector++) {
            byte[] iv = new byte[16];
            // Independent implementation of Nintendo's big-endian XTS data-unit IV.
            iv[15] = (byte)sector;
            byte[] tweak = tweakAes.doFinal(iv);
            for (int offset = 0; offset < 512; offset += 16) {
                int at = sector * 512 + offset;
                byte[] x = new byte[16];
                for (int i = 0; i < 16; i++) x[i] = (byte)(plain[at+i] ^ tweak[i]);
                byte[] y = aes.doFinal(x);
                for (int i = 0; i < 16; i++) result[at+i] = (byte)(y[i] ^ tweak[i]);
                int carry = 0;
                for (int i = 0; i < 16; i++) {
                    int b = tweak[i] & 255;
                    tweak[i] = (byte)((b << 1) | carry);
                    carry = b >>> 7;
                }
                if (carry != 0) tweak[0] ^= (byte)0x87;
                Arrays.fill(x,(byte)0);
                Arrays.fill(y,(byte)0);
            }
            Arrays.fill(tweak,(byte)0);
        }
        return result;
    }
    public static void main(String[] args) throws Exception {
        byte[] key = new byte[32];
        for (int i = 0; i < key.length; i++) key[i] = (byte)i;
        String named = "unrelated_key = aabb\nheader_key_source = aabb\n# comment\nheader_key = " + hex(key) + "\n";
        byte[] read = SwitchNcaHeaderBridge.loadHeaderKey(
            new ByteArrayInputStream(named.getBytes(StandardCharsets.US_ASCII)));
        check(Arrays.equals(key, read), "read a fake header_key from local bytes");
        Arrays.fill(read, (byte)0);
        byte[] plain = new byte[0xC00];
        System.arraycopy("NCA3".getBytes(StandardCharsets.US_ASCII),0,plain,0x200,4);
        plain[0x205] = 0; // program
        plain[0x208] = 0x00; plain[0x209] = 0x40; // 16384 bytes
        // Two synthetic program sections: one candidate ExeFS (PartitionFS)
        // and one candidate RomFS. Sector windows: [6, 14), [14, 22).
        plain[0x240] = 6;
        plain[0x244] = 14;
        plain[0x250] = 14;
        plain[0x254] = 22;
        plain[0x400+2] = 1; // PartitionFS
        plain[0x400+3] = 2; // SHA256 type
        plain[0x400+4] = 3; // AES-CTR
        plain[0x600+2] = 0; // RomFS
        plain[0x600+3] = 3; // Integrity hash type
        plain[0x600+4] = 3; // AES-CTR
        java.security.MessageDigest digester = java.security.MessageDigest.getInstance("SHA-256");
        System.arraycopy(digester.digest(Arrays.copyOfRange(plain,0x400,0x600)),
                         0,plain,0x280,32);
        System.arraycopy(digester.digest(Arrays.copyOfRange(plain,0x600,0x800)),
                         0,plain,0x2A0,32);
        byte[] title = {(byte)0x00,(byte)0xe0,(byte)0x11,(byte)0x00,
                        (byte)0xf0,(byte)0x7e,(byte)0x00,(byte)0x01};
        System.arraycopy(title,0,plain,0x210,8);
        byte[] cipher = encrypt(plain, key);
        // Fixed independent AES-XTS ciphertext (Python cryptography/OpenSSL).
        check(hex(Arrays.copyOfRange(cipher, 0, 32)).equals(
            "f071a2b402c105ea37024133e24d6ef6212e8cc0175e1b6b32657d54f159daf6"),
            "XTS sector 0 differs from independent implementation");
        check(hex(Arrays.copyOfRange(cipher, 512, 544)).equals(
            "b7d83a5910831ba82bed19878404c2cfb099d69f7ad608fbfc2cef09964f9850"),
            "XTS sector 1 differs from independent implementation");
        byte[] decrypted = SwitchNcaHeaderBridge.decryptHeader(cipher,key);
        check(Arrays.equals(plain, decrypted), "local decryption of synthetic header");
        Arrays.fill(decrypted,(byte)0);
        String report = SwitchNcaHeaderBridge.inspect(
            new ByteArrayInputStream(named.getBytes(StandardCharsets.US_ASCII)),
            new byte[][]{cipher});
        check(report.contains("Cabeceras NCA reconocidas: 1/1"), "NCA header counted");
        check(report.contains("NCA PROGRAM detectados: 1"), "program header counted");
        check(report.contains("FS 0: PartitionFS"), "ExeFS-like section identified");
        check(report.contains("FS 1: RomFS"), "RomFS-like section identified");
        check(report.contains("CANDIDATO a ExeFS"), "conservative code-section label");
        check(report.contains("CANDIDATO a RomFS"), "conservative data-section label");
        check(report.contains("SHA-256 coincide"), "section header SHA-256 verified");
        check(report.contains("Secciones de programa con cabecera SHA-256 consistente: 2"),
              "both section headers verified");
        byte[] corrupted = cipher.clone();
        corrupted[0x400] ^= 1; // break FS header integrity without changing the NCA header
        String damaged = SwitchNcaHeaderBridge.inspect(
            new ByteArrayInputStream(named.getBytes(StandardCharsets.US_ASCII)),
            new byte[][]{corrupted});
        check(damaged.contains("hash SHA-256 no coincide") &&
              !damaged.contains("CANDIDATO a ExeFS"), "reject tampered section metadata");
        Arrays.fill(corrupted,(byte)0);
        check(report.contains("01007EF00011E000"), "title ID recognized");
        check(!report.contains(hex(key)), "must not display key material");
        check(!report.contains("ExeFS obtenido"), "must not misrepresent extraction");
        String bad = "header_key = 0000\n";
        boolean rejected = false;
        try {
            SwitchNcaHeaderBridge.loadHeaderKey(
                new ByteArrayInputStream(bad.getBytes(StandardCharsets.US_ASCII)));
        } catch (IOException expected) { rejected = true; }
        check(rejected, "reject short key");
        rejected = false;
        try {
            SwitchNcaHeaderBridge.loadHeaderKey(
                new ByteArrayInputStream((named+"header_key = "+hex(key)+"\n")
                    .getBytes(StandardCharsets.US_ASCII)));
        } catch (IOException expected) { rejected = true; }
        check(rejected, "reject duplicated key");
        rejected = false;
        try {
            SwitchNcaHeaderBridge.loadHeaderKey(
                new ByteArrayInputStream(new byte[128 * 1024 + 2]));
        } catch (IOException expected) { rejected = true; }
        check(rejected, "reject excessive key file");
        System.out.println("PASS: synthetic NSP NCA AES-XTS bridge; local-only key and metadata");
        Arrays.fill(key,(byte)0);
        Arrays.fill(cipher,(byte)0);
        Arrays.fill(plain,(byte)0);
    }
}
