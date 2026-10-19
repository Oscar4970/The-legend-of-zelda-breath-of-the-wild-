package org.example.botwlab;

import java.io.IOException;
import java.io.InputStream;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import java.security.MessageDigest;
import java.util.Locale;
import javax.crypto.Cipher;
import javax.crypto.spec.SecretKeySpec;

/**
 * First local-only native-port bridge milestone.
 *
 * Accepts only 0xC00-byte NCA encrypted headers and section headers from a local NSP;
 * uses the user's selected local key stream transiently; returns metadata only.
 * It does NOT extract ExeFS/RomFS, export decrypted bytes, implement Horizon,
 * execute a commercial NSO, or turn BOTW into a native Android application.
 */
public final class SwitchNcaHeaderBridge {
    private static final int SAMPLE_SIZE = 0xC00;
    private static final int MAIN_HEADER_SIZE = 0x400;
    private static final int FS_HEADER_SIZE = 0x200;
    private static final int FS_COUNT = 4;
    private static final int FS_INFO_OFFSET = 0x240;
    private static final int FS_HASH_OFFSET = 0x280;
    private static final int SECTOR_SIZE = 0x200;
    private static final int MAX_KEY_FILE = 128 * 1024;
    private static final int MAX_SAMPLES = 32;
    private static final byte[] NAME = "header_key".getBytes(java.nio.charset.StandardCharsets.US_ASCII);

    private SwitchNcaHeaderBridge() {}

    /** Keys are ephemeral and never copied into a report, preferences, logs or APK. */
    static byte[] loadHeaderKey(InputStream in) throws IOException {
        if (in == null) throw new IOException("Selecciona el archivo local de claves.");
        byte[] raw = new byte[MAX_KEY_FILE + 1];
        int size = 0;
        try {
            while (size < raw.length) {
                int n = in.read(raw, size, raw.length - size);
                if (n < 0) break;
                if (n == 0) {
                    int single = in.read();
                    if (single < 0) break;
                    raw[size++] = (byte) single;
                } else {
                    size += n;
                }
            }
            if (size > MAX_KEY_FILE)
                throw new IOException("Archivo de claves demasiado grande; máximo 128 KiB.");
            byte[] result = null;
            for (int start = 0; start < size;) {
                int end = start;
                while (end < size && raw[end] != '\n' && raw[end] != '\r') end++;
                int i = start;
                while (i < end && space(raw[i])) i++;
                if (i + NAME.length < end && matches(raw, i, NAME) &&
                    (space(raw[i + NAME.length]) || raw[i + NAME.length] == '=')) {
                    int k = i + NAME.length;
                    while (k < end && space(raw[k])) k++;
                    if (k >= end || raw[k] != '=')
                        throw new IOException("Entrada header_key sin formato válido.");
                    k++;
                    while (k < end && space(raw[k])) k++;
                    if (result != null) throw new IOException("Entrada header_key duplicada.");
                    if (end - k < 64) throw new IOException("Longitud de header_key incorrecta.");
                    byte[] key = new byte[32];
                    for (int j = 0; j < 32; j++) {
                        int hi = hex(raw[k + j * 2]);
                        int lo = hex(raw[k + j * 2 + 1]);
                        if (hi < 0 || lo < 0) {
                            Arrays.fill(key, (byte)0);
                            throw new IOException("Formato hexadecimal de header_key incorrecto.");
                        }
                        key[j] = (byte)((hi << 4) | lo);
                    }
                    k += 64;
                    while (k < end && space(raw[k])) k++;
                    if (k < end && raw[k] != '#' && raw[k] != ';') {
                        Arrays.fill(key, (byte)0);
                        throw new IOException("Longitud o formato de header_key incorrecto.");
                    }
                    result = key;
                }
                start = end;
                while (start < size && (raw[start] == '\n' || raw[start] == '\r')) start++;
            }
            if (result == null)
                throw new IOException("El archivo no contiene header_key. No se modificó nada.");
            return result;
        } finally {
            Arrays.fill(raw, (byte)0);
        }
    }

    private static boolean matches(byte[] data, int offset, byte[] pattern) {
        for (int i = 0; i < pattern.length; i++) {
            if (data[offset + i] != pattern[i]) return false;
        }
        return true;
    }
    private static boolean space(byte c) { return c == ' ' || c == '\t'; }
    private static int hex(byte c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // Standard XTS-AES-128 with a 512-byte NCA header data unit and
    // Nintendo XTS IV uses a BIG-ENDIAN sector index (sector 1 is IV[15] = 1).
    // The internal GF(2^128) tweak multiplication is LITTLE-endian: these differ.
    // No CBC/CTR or fake "decrypt" shortcut.
    private static void multiplyTweak(byte[] tweak) {
        int carry = 0;
        for (int i = 0; i < 16; i++) {
            int b = tweak[i] & 0xff;
            tweak[i] = (byte)((b << 1) | carry);
            carry = b >>> 7;
        }
        if (carry != 0) tweak[0] ^= (byte)0x87;
    }

    static byte[] decryptHeader(byte[] ciphertext, byte[] key)
            throws GeneralSecurityException {
        if (ciphertext == null || ciphertext.length != SAMPLE_SIZE ||
            key == null || key.length != 32)
            throw new IllegalArgumentException("Muestra NCA o header_key inválida.");
        byte[] plaintext = new byte[SAMPLE_SIZE];
        byte[] tweak = new byte[16];
        byte[] block = new byte[16];
        Cipher dataCipher = Cipher.getInstance("AES/ECB/NoPadding");
        Cipher tweakCipher = Cipher.getInstance("AES/ECB/NoPadding");
        dataCipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, 0, 16, "AES"));
        tweakCipher.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, 16, 16, "AES"));
        try {
            for (int sector = 0; sector < SAMPLE_SIZE / SECTOR_SIZE; sector++) {
                Arrays.fill(tweak, (byte)0);
                // Verified against Eden's AesXtsStorage::MakeAesXtsIv.
                tweak[15] = (byte)sector;
                byte[] encryptedTweak = tweakCipher.doFinal(tweak);
                System.arraycopy(encryptedTweak, 0, tweak, 0, 16);
                Arrays.fill(encryptedTweak, (byte)0);
                for (int i = 0; i < SECTOR_SIZE; i += 16) {
                    int pos = sector * SECTOR_SIZE + i;
                    for (int j = 0; j < 16; j++) block[j] = (byte)(ciphertext[pos+j] ^ tweak[j]);
                    byte[] plainBlock = dataCipher.doFinal(block);
                    for (int j = 0; j < 16; j++)
                        plaintext[pos+j] = (byte)(plainBlock[j] ^ tweak[j]);
                    Arrays.fill(plainBlock, (byte)0);
                    multiplyTweak(tweak);
                }
            }
            return plaintext;
        } finally {
            Arrays.fill(tweak, (byte)0);
            Arrays.fill(block, (byte)0);
        }
    }

    private static long le32(byte[] data, int offset) {
        return ((long)data[offset] & 255L) |
               (((long)data[offset+1] & 255L) << 8) |
               (((long)data[offset+2] & 255L) << 16) |
               (((long)data[offset+3] & 255L) << 24);
    }

    private static String fsTypeName(int fsType) {
        return fsType == 0 ? "RomFS" : fsType == 1 ? "PartitionFS" : "desconocido";
    }

    private static String encryptionName(int value) {
        switch (value) {
            case 1: return "NONE";
            case 2: return "AES-XTS";
            case 3: return "AES-CTR";
            case 4: return "AES-CTR-EX";
            case 5: return "AES-CTR-SKIP-HASH";
            case 6: return "AES-CTR-EX-SKIP-HASH";
            default: return "no reconocido";
        }
    }

    /**
     * Section header is SHA-256 authenticated against its digest from the
     * decrypted NCA header, not against a hardcoded pattern. The hash check
     * proves internal metadata consistency; NOT Nintendo signature validity.
     */
    private static String inspectFsHeaders(byte[] decrypted, int ncaIndex, int contentType,
                                           long declaredSize) throws GeneralSecurityException {
        StringBuilder out = new StringBuilder();
        MessageDigest sha = MessageDigest.getInstance("SHA-256");
        int verifiedCount = 0;
        for (int slot = 0; slot < FS_COUNT; slot++) {
            int info = FS_INFO_OFFSET + slot * 16;
            long firstSector = le32(decrypted, info);
            long lastSector = le32(decrypted, info + 4);
            if (firstSector == 0 && lastSector == 0) continue;
            // Limits are strictly bounded by the file size declared in NCA.
            long startByte = firstSector * 512L;
            long endByte = lastSector * 512L;
            if (firstSector == 0 || lastSector <= firstSector ||
                declaredSize < SAMPLE_SIZE || endByte > declaredSize) {
                out.append("  FS ").append(slot).append(": sectores fuera de límites; ignorados.\n");
                continue;
            }
            int offset = MAIN_HEADER_SIZE + slot * FS_HEADER_SIZE;
            int hashOffset = FS_HASH_OFFSET + slot * 32;
            sha.reset();
            sha.update(decrypted, offset, FS_HEADER_SIZE);
            byte[] actualHash = sha.digest();
            byte[] expectedHash = Arrays.copyOfRange(decrypted, hashOffset, hashOffset + 32);
            boolean matches = MessageDigest.isEqual(actualHash, expectedHash);
            Arrays.fill(actualHash, (byte)0);
            Arrays.fill(expectedHash, (byte)0);
            if (!matches) {
                out.append("  FS ").append(slot)
                    .append(": hash SHA-256 no coincide; metadatos no fiables.\n");
                continue;
            }
            verifiedCount++;
            int fsType = decrypted[offset + 2] & 255;
            int hashType = decrypted[offset + 3] & 255;
            int encryptionType = decrypted[offset + 4] & 255;
            out.append("  FS ").append(slot)
               .append(": ").append(fsTypeName(fsType))
               .append(" / cifrado ").append(encryptionName(encryptionType))
               .append(" / hash tipo ").append(hashType)
               .append(" / ").append(endByte - startByte).append(" bytes")
               .append(" / SHA-256 coincide\n");
            if (contentType == 0 && fsType == 1)
                out.append("    CANDIDATO a ExeFS (aun sin leer archivos main/sdk).\n");
            if (contentType == 0 && fsType == 0)
                out.append("    CANDIDATO a RomFS (aun sin leer recursos).\n");
        }
        if (contentType == 0)
            out.append("  Secciones de programa con cabecera SHA-256 consistente: ")
               .append(verifiedCount).append('\n');
        return out.toString();
    }

    private static long le64(byte[] data, int offset) {
        long result = 0;
        for (int i = 7; i >= 0; i--) result = (result << 8) | (data[offset + i] & 0xffL);
        return result;
    }

    public static String inspect(InputStream selectedKeyStream, byte[][] ncaCipherHeaders)
            throws IOException, GeneralSecurityException {
        if (ncaCipherHeaders == null || ncaCipherHeaders.length == 0 ||
            ncaCipherHeaders.length > MAX_SAMPLES)
            throw new IOException("El NSP no contiene muestras NCA válidas (máximo 32).");
        byte[] key = loadHeaderKey(selectedKeyStream);
        int recognized = 0;
        int programs = 0;
        StringBuilder output = new StringBuilder();
        output.append("PUENTE EDEN → PORT ANDROID | PRUEBA LOCAL\n");
        output.append("Muestras NCA: ").append(ncaCipherHeaders.length).append('\n');
        try {
            for (int index = 0; index < ncaCipherHeaders.length; index++) {
                byte[] decrypted = decryptHeader(ncaCipherHeaders[index], key);
                try {
                    boolean validMagic = decrypted[0x200] == 'N' &&
                        decrypted[0x201] == 'C' && decrypted[0x202] == 'A' &&
                        (decrypted[0x203] == '3' || decrypted[0x203] == '2');
                    int type = decrypted[0x205] & 0xff;
                    long declaredSize = le64(decrypted, 0x208);
                    if (validMagic && type <= 5 && declaredSize >= SAMPLE_SIZE) {
                        recognized++;
                        if (type == 0) programs++;
                        String category = type == 0 ? "PROGRAM" : type == 1 ? "META"
                            : type == 2 ? "CONTROL" : type == 3 ? "MANUAL"
                            : type == 4 ? "DATA" : "PUBLIC_DATA";
                        output.append("NCA ").append(index + 1).append(": NCA")
                            .append((char)decrypted[0x203]).append(" / ")
                            .append(category).append(" / Title ID ")
                            .append(String.format(Locale.ROOT, "%016X", le64(decrypted, 0x210)))
                            .append('\n');
                        if (type == 0)
                            output.append(inspectFsHeaders(decrypted, index, type, declaredSize));
                    } else {
                        output.append("NCA ").append(index + 1)
                            .append(": cabecera no reconocida; llave/variante desconocida.\n");
                    }
                } finally {
                    Arrays.fill(decrypted, (byte)0);
                }
            }
        } finally {
            Arrays.fill(key, (byte)0);
        }
        output.append("Cabeceras NCA reconocidas: ").append(recognized)
            .append('/').append(ncaCipherHeaders.length).append('\n')
            .append("NCA PROGRAM detectados: ").append(programs).append('\n');
        if (programs > 0)
            output.append("AVANCE: se reconoció metadata de un NCA de programa.\n");
        else
            output.append("No se identificó programa; esto NO prueba que el NSP esté dañado.\n");
        output.append("NO se extrae ExeFS/RomFS; no ejecuta Zelda ni genera port nativo.\n");
        output.append("Claves transitorias, no incluidas en APK, reporte ni almacenamiento.\n");
        output.append("No se cargó el NSP completo en RAM; solo muestras NCA de 3 KiB.");
        return output.toString();
    }
}
