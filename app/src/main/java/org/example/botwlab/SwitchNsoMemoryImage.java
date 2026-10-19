package org.example.botwlab;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/**
 * Non-executable NSO0 memory image: one transient Java heap array per module.
 * It does NOT perform relocations, grant executable memory or run game code.
 * The caller closes the image to wipe loaded bytes immediately after the test.
 */
final class SwitchNsoMemoryImage implements AutoCloseable {
    private static final long MAX_IMAGE=96L*1024*1024;
    private static final long MAX_STORED=48L*1024*1024;
    private static final int CHUNK=32768;
    private static final String[] NAMES={".text",".rodata",".data"};

    final long base;
    final long bssOffset;
    final long bssSize;
    final int verified;
    final byte[] memory;

    private boolean closed=false;
    private SwitchNsoMemoryImage(long base,long bssOffset,long bssSize,
                                 int verified,byte[] memory) {
        this.base=base;this.bssOffset=bssOffset;this.bssSize=bssSize;
        this.verified=verified;this.memory=memory;
    }

    private static long u32(byte[] data,int at) {
        return ((long)data[at]&255) |
            (((long)data[at+1]&255)<<8) |
            (((long)data[at+2]&255)<<16) |
            (((long)data[at+3]&255)<<24);
    }

    private static void read(SwitchNsoSectionHashes.Source reader,
                             long from,byte[] dest,int destOffset,int amount)
            throws IOException {
        if(from<0 || amount<0 || destOffset<0 || destOffset>dest.length ||
           amount>dest.length-destOffset)
            throw new IOException("Imagen NSO: limites de lectura invalidos");
        for(int copied=0;copied<amount;) {
            int count=Math.min(CHUNK,amount-copied);
            byte[] part=reader.read(from+copied,count);
            if(part==null||part.length!=count)
                throw new IOException("Imagen NSO: lectura virtual incompleta");
            try {
                System.arraycopy(part,0,dest,destOffset+copied,count);
            } finally {
                Arrays.fill(part,(byte)0);
            }
            copied+=count;
        }
    }

    static SwitchNsoMemoryImage prepare(byte[] header,long fileSize,
                                        SwitchNsoSectionHashes.Source reader)
            throws IOException, NoSuchAlgorithmException {
        if(reader==null)throw new IOException("Imagen NSO: no hay lectura virtual");
        SwitchNsoMetadataAudit.Result checked=
            SwitchNsoMetadataAudit.inspect(header,fileSize);
        if(!checked.valid)throw new IOException("Imagen NSO: "+checked.report);

        long flags=u32(header,0xc);
        long[] memStart=new long[3],memSize=new long[3];
        long lo=Long.MAX_VALUE,hi=0,total=0;
        for(int i=0;i<3;i++) {
            memStart[i]=u32(header,0x14+16*i);
            memSize[i]=u32(header,0x18+16*i);
            if(memSize[i]==0)continue;
            lo=Math.min(lo,memStart[i]);
            hi=Math.max(hi,memStart[i]+memSize[i]);
            total+=memSize[i];
        }
        if(lo==Long.MAX_VALUE)throw new IOException("Imagen NSO: no hay segmentos");
        long bssSize=u32(header,0x3c);
        long bssStart=memStart[2]+memSize[2];
        long bssEnd=bssStart+bssSize;
        if(bssSize!=0) {
            if(memSize[2]==0)throw new IOException("Imagen NSO: BSS requiere .data");
            if(bssEnd>0x100000000L)
                throw new IOException("Imagen NSO: BSS excede direccion virtual");
            for(int i=0;i<2;i++) {
                if(memSize[i]!=0 && bssStart<memStart[i]+memSize[i] &&
                   memStart[i]<bssEnd)
                    throw new IOException("Imagen NSO: BSS se superpone");
            }
            hi=Math.max(hi,bssEnd);
        }
        if(hi<lo || hi-lo>MAX_IMAGE || total+bssSize>MAX_IMAGE)
            throw new IOException("Imagen NSO: memoria excede limite de 96 MiB");

        byte[] image=new byte[(int)(hi-lo)]; // Zero-filled gaps and BSS.
        try {
            int verified=0;
            for(int i=0;i<3;i++) {
                if(memSize[i]==0)continue;
                long packed=u32(header,0x60+4*i);
                long diskOffset=u32(header,0x10+16*i);
                boolean compressed=(flags&(1L<<i))!=0;
                boolean hashRequired=(flags&(1L<<(i+3)))!=0;
                if(!hashRequired)
                    throw new IOException("Imagen NSO: hash ausente para "+NAMES[i]);
                long stored=compressed?packed:memSize[i];
                if(stored>MAX_STORED)
                    throw new IOException("Imagen NSO: segmento comprimido demasiado grande");
                int at=(int)(memStart[i]-lo);
                int length=(int)memSize[i];
                if(compressed) {
                    byte[] encoded=new byte[(int)stored];
                    try {
                        read(reader,diskOffset,encoded,0,encoded.length);
                        SwitchNsoSectionHashes.decodeLz4(encoded,image,at,length);
                    } finally {
                        Arrays.fill(encoded,(byte)0);
                    }
                } else {
                    read(reader,diskOffset,image,at,length);
                }
                MessageDigest sha=MessageDigest.getInstance("SHA-256");
                sha.update(image,at,length);
                byte[] actual=sha.digest();
                byte[] expected=Arrays.copyOfRange(header,0xA0+32*i,0xC0+32*i);
                boolean match=MessageDigest.isEqual(actual,expected);
                Arrays.fill(actual,(byte)0);
                Arrays.fill(expected,(byte)0);
                if(!match)
                    throw new IOException("Imagen NSO: SHA-256 no coincide en "+NAMES[i]);
                verified++;
            }
            // Explicitly verify the BSS stayed zero. No memory is executable.
            int bssAt=(int)(bssStart-lo);
            for(int i=0;i<bssSize;i++) {
                if(image[bssAt+i]!=0)
                    throw new IOException("Imagen NSO: BSS no inicializada en cero");
            }
            return new SwitchNsoMemoryImage(lo,bssStart,bssSize,verified,image);
        } catch(IOException|NoSuchAlgorithmException|RuntimeException error) {
            Arrays.fill(image,(byte)0);
            throw error;
        }
    }

    String summary() {
        if(closed)return "Imagen NSO liberada";
        return "Imagen NSO temporal construida y verificada: "+memory.length+
            " bytes en heap de Java; base virtual "+base+
            "; 3 segmentos cotejados SHA-256 ("+verified+"/3); BSS "+
            bssSize+" bytes a partir de "+bssOffset+
            "; no se aplicaron relocaciones ni se ejecuto codigo.";
    }

    @Override public void close() {
        if(!closed) {
            Arrays.fill(memory,(byte)0);
            closed=true;
        }
    }
}
