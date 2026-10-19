package org.example.botwlab;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/**
 * Local NSO0 segment integrity check. A Source is a bounded view into an
 * already-decrypted, user-selected ExeFS module. No file export or execution.
 */
final class SwitchNsoSectionHashes {
    private SwitchNsoSectionHashes() {}
    private static final int MAX_CHUNK = 32768;
    private static final int MAX_STORED = 48 * 1024 * 1024;
    private static final int MAX_MEMORY = 64 * 1024 * 1024;
    private static final String[] NAMES = {".text", ".rodata", ".data"};

    interface Source {
        byte[] read(long offset, int size) throws IOException;
    }

    static final class Result {
        final boolean ok;
        final int checked;
        final String report;
        Result(boolean ok, int checked, String report) {
            this.ok = ok; this.checked = checked; this.report = report;
        }
    }

    private static long u32(byte[] b, int offset) {
        return (b[offset] & 255L) |
            ((b[offset+1] & 255L) << 8) |
            ((b[offset+2] & 255L) << 16) |
            ((b[offset+3] & 255L) << 24);
    }

    private static int length(byte[] source, int[] position, int initial)
            throws IOException {
        int len = initial;
        if (len == 15) {
            int extra;
            do {
                if (position[0] >= source.length)
                    throw new IOException("LZ4: longitud incompleta");
                extra = source[position[0]++] & 255;
                if (len > Integer.MAX_VALUE-extra)
                    throw new IOException("LZ4: longitud fuera de rango");
                len += extra;
            } while (extra == 255);
        }
        return len;
    }

    static void decodeLz4(byte[] compressed, byte[] output) throws IOException {
        if (compressed == null || output == null)
            throw new IOException("LZ4: faltan buffers");
        int in = 0, out = 0;
        while (in < compressed.length) {
            int token = compressed[in++] & 255;
            int[] cursor = {in};
            int literal = length(compressed, cursor, token >>> 4);
            in = cursor[0];
            if (literal > compressed.length-in || literal > output.length-out)
                throw new IOException("LZ4: literales fuera de limite");
            System.arraycopy(compressed, in, output, out, literal);
            in += literal;
            out += literal;
            if (in == compressed.length) break;
            if (in + 2 > compressed.length)
                throw new IOException("LZ4: desplazamiento incompleto");
            int distance = (compressed[in] & 255) | ((compressed[in+1] & 255) << 8);
            in += 2;
            if (distance == 0 || distance > out)
                throw new IOException("LZ4: desplazamiento invalido");
            cursor[0] = in;
            int repeat = length(compressed, cursor, token & 15);
            in = cursor[0];
            if (repeat > output.length-out-4)
                throw new IOException("LZ4: repeticion fuera de limite");
            repeat += 4;
            for (int i=0; i<repeat; i++) output[out+i] = output[out+i-distance];
            out += repeat;
        }
        if (in != compressed.length || out != output.length)
            throw new IOException("LZ4: longitud resultante incorrecta");
    }

    private static void load(Source source, long start, byte[] output) throws IOException {
        for (int offset=0; offset<output.length;) {
            int count = Math.min(MAX_CHUNK, output.length-offset);
            byte[] part = source.read(start+offset, count);
            if (part == null || part.length != count)
                throw new IOException("Lectura de modulo incompleta");
            try {
                System.arraycopy(part,0,output,offset,count);
            } finally {
                Arrays.fill(part,(byte)0);
            }
            offset+=count;
        }
    }

    static Result verify(byte[] header, long moduleSize, Source source)
            throws IOException, NoSuchAlgorithmException {
        if (source == null)
            throw new IOException("Modulo virtual sin lector");
        SwitchNsoMetadataAudit.Result info =
            SwitchNsoMetadataAudit.inspect(header,moduleSize);
        if (!info.valid)
            return new Result(false,0,info.report);

        int checked = 0;
        StringBuilder report = new StringBuilder("Validacion de segmentos NSO0: \n");
        long flags = u32(header,0x0c);
        for (int section=0; section<3; section++) {
            long offset = u32(header,0x10+16*section);
            long memorySize = u32(header,0x18+16*section);
            long packedSize = u32(header,0x60+4*section);
            boolean compressed = (flags & (1L<<section)) != 0;
            boolean declaredHash = (flags & (1L<<(section+3))) != 0;
            String label = NAMES[section];
            if (memorySize == 0) {
                report.append(label).append(": segmento vacio.\n");
                continue;
            }
            if (!declaredHash) {
                report.append(label).append(": no declara SHA-256; sin verificar.\n");
                continue;
            }
            long stored = compressed ? packedSize : memorySize;
            if (memorySize > MAX_MEMORY || stored > MAX_STORED)
                return new Result(false,checked,
                    report.append(label).append(": supera limite de memoria seguro.\n").toString());

            MessageDigest sha = MessageDigest.getInstance("SHA-256");
            if (compressed) {
                byte[] input = new byte[(int)stored];
                byte[] expanded = new byte[(int)memorySize];
                try {
                    load(source, offset, input);
                    decodeLz4(input, expanded);
                    sha.update(expanded);
                } finally {
                    Arrays.fill(input,(byte)0);
                    Arrays.fill(expanded,(byte)0);
                }
            } else {
                for (long done=0; done<memorySize;) {
                    int count = (int)Math.min(MAX_CHUNK,memorySize-done);
                    byte[] bytes = source.read(offset+done,count);
                    if (bytes == null || bytes.length != count)
                        throw new IOException("Lectura incompleta de segmento");
                    try { sha.update(bytes); }
                    finally { Arrays.fill(bytes,(byte)0); }
                    done += count;
                }
            }
            byte[] actual = sha.digest();
            byte[] expected = Arrays.copyOfRange(header,
                0xA0+32*section, 0xA0+32*(section+1));
            boolean matches = MessageDigest.isEqual(actual,expected);
            Arrays.fill(actual,(byte)0);
            Arrays.fill(expected,(byte)0);
            if (!matches)
                return new Result(false,checked,
                    report.append(label).append(": SHA-256 NO COINCIDE.\n").toString());
            checked++;
            report.append(label).append(": SHA-256 coincide")
                .append(compressed?" tras LZ4.\n":".\n");
        }
        report.append("Segmentos verificados: ").append(checked)
            .append("/3. No se ejecuto ni guardo codigo.");
        return new Result(checked==3,checked,report.toString());
    }
}
