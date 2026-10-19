package org.example.botwlab;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/** Entirely synthetic firmware ZIP; never uses Nintendo firmware or user keys. */
public final class SwitchFirmwareBridgeTest {
    private static void check(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }
    private static byte[] encrypt(byte[] plaintext, byte[] key) throws Exception {
        byte[] result = new byte[plaintext.length];
        Cipher enc = Cipher.getInstance("AES/ECB/NoPadding");
        Cipher tweakCipher = Cipher.getInstance("AES/ECB/NoPadding");
        enc.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, 0, 16, "AES"));
        tweakCipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, 16, 16, "AES"));
        for (int sector=0; sector<plaintext.length/512; sector++) {
            byte[] iv = new byte[16];
            iv[15] = (byte)sector;
            byte[] tweak = tweakCipher.doFinal(iv);
            for (int offset=0; offset<512; offset+=16) {
                int idx = sector*512+offset;
                byte[] tmp = new byte[16];
                for (int i=0;i<16;i++) tmp[i]=(byte)(plaintext[idx+i]^tweak[i]);
                byte[] cipher = enc.doFinal(tmp);
                for (int i=0;i<16;i++) result[idx+i]=(byte)(cipher[i]^tweak[i]);
                int carry=0;
                for (int i=0;i<16;i++) {
                    int v=tweak[i]&255;
                    tweak[i]=(byte)((v<<1)|carry);
                    carry=v>>>7;
                }
                if (carry!=0) tweak[0]^=(byte)0x87;
                Arrays.fill(tmp, (byte)0);
                Arrays.fill(cipher, (byte)0);
            }
            Arrays.fill(tweak, (byte)0);
        }
        return result;
    }
    private static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b:bytes) sb.append(String.format("%02x", b&0xff));
        return sb.toString();
    }
    private static byte[] zipWith(byte[] sample) throws Exception {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        try (ZipOutputStream z = new ZipOutputStream(out)) {
            ZipEntry a = new ZipEntry("0123456789abcdef0123456789abcdef.nca");
            z.putNextEntry(a);
            z.write(sample);
            z.closeEntry();
            ZipEntry b = new ZipEntry("not-an-nca.txt");
            z.putNextEntry(b);
            z.write("NO FIRMWARE".getBytes(StandardCharsets.US_ASCII));
            z.closeEntry();
        }
        return out.toByteArray();
    }
    public static void main(String[] args) throws Exception {
        byte[] key = new byte[32];
        for (int i=0;i<key.length;i++) key[i]=(byte)i;
        byte[] plain = new byte[0xC00];
        System.arraycopy("NCA3".getBytes(StandardCharsets.US_ASCII),0,plain,0x200,4);
        plain[0x205]=0; // Program, synthetic.
        plain[0x208]=0; plain[0x209]=0x0C; // 3072 bytes
        byte[] tid={(byte)0x00,(byte)0xe0,(byte)0x11,(byte)0x00,
                    (byte)0xf0,(byte)0x7e,(byte)0x00,(byte)0x01};
        System.arraycopy(tid,0,plain,0x210,8);
        byte[] encrypted = encrypt(plain,key);
        byte[] zip = zipWith(encrypted);
        String keys="header_key = "+hex(key)+"\n";
        String report = SwitchFirmwareBridge.inspect(new ByteArrayInputStream(zip),
                    new ByteArrayInputStream(keys.getBytes(StandardCharsets.US_ASCII)));
        check(report.contains("Entradas ZIP examinadas: 2"), "counts ZIP entries");
        check(report.contains("NCA reconocidos: 1"), "recognized sample NCA");
        check(report.contains("PROGRAM: 1"), "reports NCA Program");
        check(report.contains("Title ID 01007EF00011E000"), "reports metadata title ID");
        check(report.contains("Otros archivos ignorados: 1"), "skipped unknown archive file");
        check(!report.contains(hex(key)), "never reports keys");
        byte[] changed = encrypted.clone();
        changed[0x20] ^= 0x55;
        // First signature block modification does not invalidate NCA magic.
        String still = SwitchFirmwareBridge.inspect(new ByteArrayInputStream(zipWith(changed)),
                    new ByteArrayInputStream(keys.getBytes(StandardCharsets.US_ASCII)));
        check(still.contains("NCA reconocidos: 1"), "does not pretend to verify signatures");
        changed = encrypted.clone();
        changed[0x210] ^= 0x55;
        String broken = SwitchFirmwareBridge.inspect(new ByteArrayInputStream(zipWith(changed)),
                    new ByteArrayInputStream(keys.getBytes(StandardCharsets.US_ASCII)));
        check(broken.contains("NCA reconocidos: 0"), "rejects tampered NCA magic");
        Arrays.fill(plain,(byte)0);
        Arrays.fill(encrypted,(byte)0);
        Arrays.fill(key,(byte)0);
        System.out.println("PASS: synthetic firmware ZIP -> transient NCA header inspection, no keys or files exported");
    }
}
