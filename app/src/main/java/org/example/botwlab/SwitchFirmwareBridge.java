package org.example.botwlab;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Local read-only firmware NCA inventory. No firmware files, game content,
 * decrypted NCA bytes, or user keys are packaged, extracted, logged or saved.
 * NOT a Horizon firmware executor nor an HLE runtime.
 */
public final class SwitchFirmwareBridge {
    private static final int SAMPLE_BYTES = 0xC00;
    private static final int MAX_FILES = 512;
    private static final long MAX_TOTAL_UNCOMPRESSED = 1024L * 1024L * 1024L;
    private static final long MAX_SINGLE_FILE = 256L * 1024L * 1024L;
    private static final int REPORT_EXAMPLES = 12;

    private SwitchFirmwareBridge() {}

    private static long readLittleEndian64(byte[] bytes, int offset) {
        long out = 0;
        for (int j = 7; j >= 0; j--) out = (out << 8) | (bytes[offset + j] & 0xffL);
        return out;
    }

    private static String nameForType(int type) {
        switch (type) {
            case 0: return "PROGRAM";
            case 1: return "META";
            case 2: return "CONTROL";
            case 3: return "MANUAL";
            case 4: return "DATA";
            case 5: return "PUBLIC_DATA";
            default: return "DESCONOCIDO";
        }
    }

    private static boolean safeNcaName(String name) {
        // A firmware dump typically has flat hashed .nca/.cnmt.nca filenames.
        return name != null && name.toLowerCase(Locale.ROOT)
            .matches("[0-9a-f]{32}(?:\\.cnmt)?\\.nca");
    }

    public static String inspect(InputStream zipStream, InputStream keyStream)
            throws IOException, GeneralSecurityException {
        if (zipStream == null || keyStream == null)
            throw new IOException("Selecciona el firmware ZIP y claves locales.");
        byte[] key = SwitchNcaHeaderBridge.loadHeaderKey(keyStream);
        byte[] chunk = new byte[64 * 1024];
        byte[] ciphertext = new byte[SAMPLE_BYTES];
        int total = 0, valid = 0, unreadable = 0, skipped = 0, cnmtNames = 0;
        int[] types = new int[6];
        long totalUncompressed = 0;
        StringBuilder examples = new StringBuilder();
        try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(zipStream))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (entry.isDirectory()) {
                    zip.closeEntry();
                    continue;
                }
                if (++total > MAX_FILES) throw new IOException("Demasiados archivos en el ZIP.");
                long declared = entry.getSize();
                if (declared > MAX_SINGLE_FILE)
                    throw new IOException("Archivo de firmware demasiado grande.");
                final boolean nca = safeNcaName(entry.getName());
                if (nca && entry.getName().toLowerCase(Locale.ROOT).endsWith(".cnmt.nca"))
                    cnmtNames++;
                Arrays.fill(ciphertext, (byte)0);
                int sampleCount = 0;
                long fileBytes = 0;
                while (true) {
                    int n = zip.read(chunk);
                    if (n == -1) break;
                    if (n == 0) continue;
                    if (fileBytes > MAX_SINGLE_FILE - n ||
                        totalUncompressed > MAX_TOTAL_UNCOMPRESSED - n)
                        throw new IOException("Firmware descomprimido excede límites de seguridad.");
                    if (nca && sampleCount < SAMPLE_BYTES) {
                        int amount = Math.min(n, SAMPLE_BYTES - sampleCount);
                        System.arraycopy(chunk, 0, ciphertext, sampleCount, amount);
                        sampleCount += amount;
                    }
                    fileBytes += n;
                    totalUncompressed += n;
                }
                zip.closeEntry();
                if (!nca) {
                    skipped++;
                    continue;
                }
                if (sampleCount != SAMPLE_BYTES || (declared >= 0 && fileBytes != declared)) {
                    unreadable++;
                    continue;
                }
                byte[] plain = SwitchNcaHeaderBridge.decryptHeader(ciphertext, key);
                try {
                    boolean magic = plain[0x200] == 'N' && plain[0x201] == 'C' &&
                                    plain[0x202] == 'A' && plain[0x203] == '3';
                    int type = plain[0x205] & 0xff;
                    long contentSize = readLittleEndian64(plain, 0x208);
                    if (!magic || type >= types.length || contentSize < SAMPLE_BYTES ||
                        contentSize > MAX_SINGLE_FILE || contentSize != fileBytes) {
                        unreadable++;
                        continue;
                    }
                    valid++;
                    types[type]++;
                    if (valid <= REPORT_EXAMPLES) {
                        examples.append("  ").append(nameForType(type))
                            .append(" / Title ID ")
                            .append(String.format(Locale.ROOT, "%016X", readLittleEndian64(plain, 0x210)))
                            .append('\n');
                    }
                } finally {
                    Arrays.fill(plain, (byte)0);
                }
            }
        } finally {
            Arrays.fill(key, (byte)0);
            Arrays.fill(chunk, (byte)0);
            Arrays.fill(ciphertext, (byte)0);
        }
        StringBuilder out = new StringBuilder();
        out.append("PUENTE FIRMEWARE → ANDROID | INVENTARIO LOCAL\n")
           .append("Entradas ZIP examinadas: ").append(total).append('\n')
           .append("NCA reconocidos: ").append(valid).append('\n')
           .append("NCA sin identificar: ").append(unreadable).append('\n')
           .append("Otros archivos ignorados: ").append(skipped).append('\n')
           .append("Nombres .cnmt.nca detectados: ").append(cnmtNames).append('\n');
        for (int type=0;type<types.length;type++)
            if (types[type] != 0)
                out.append(nameForType(type)).append(": ").append(types[type]).append('\n');
        if (examples.length() != 0)
            out.append("Ejemplos de tipos e identificadores (máximo ")
                .append(REPORT_EXAMPLES).append("):\n").append(examples);
        if (valid == 0)
            out.append("No se pudo verificar ningún NCA: comprobar formato o claves localmente.\n");
        out.append("Este inventario solo identifica metadatos; NO instala ni ejecuta firmware.\n")
           .append("No analiza servicios del sistema ni extrae ExeFS/RomFS.\n")
           .append("El runtime Horizon/HLE y la ejecución nativa de Zelda siguen pendientes.\n")
           .append("No se guardaron claves, NCA ni firmware; solo se genera este texto.");
        return out.toString();
    }
}
