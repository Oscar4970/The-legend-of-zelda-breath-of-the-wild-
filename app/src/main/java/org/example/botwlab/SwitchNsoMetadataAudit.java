package org.example.botwlab;

import java.util.Locale;

/**
 * Strict, allocation-bounded NSO0 header audit for a module inside a mounted,
 * user-selected NSP. Checks only the first 256 bytes, never reads instructions,
 * produces game-neutral metadata, and never executes or exports game code.
 */
public final class SwitchNsoMetadataAudit {
    public static final int HEADER_BYTES = 0x100;
    private static final long MAX_MODULE_BYTES = 512L * 1024 * 1024;
    private static final long MAX_SEGMENT_BYTES = 256L * 1024 * 1024;
    private static final long MAX_IMAGE_BYTES = 512L * 1024 * 1024;
    private static final String[] PARTS = {".text", ".rodata", ".data"};

    private SwitchNsoMetadataAudit() {}

    private static long u32(byte[] data, int offset) {
        return ((long)data[offset] & 255L) |
            (((long)data[offset+1] & 255L) << 8) |
            (((long)data[offset+2] & 255L) << 16) |
            (((long)data[offset+3] & 255L) << 24);
    }

    private static final class Segment {
        final long file, memory, length, stored;
        final boolean compressed, hashed;
        Segment(long file,long memory,long length,long stored,boolean compressed,boolean hashed) {
            this.file=file;this.memory=memory;this.length=length;
            this.stored=stored;this.compressed=compressed;this.hashed=hashed;
        }
        long diskLength() {return compressed?stored:length;}
    }

    public static final class Result {
        public final boolean valid;
        public final String report;
        private Result(boolean valid,String report) {this.valid=valid;this.report=report;}
    }

    private static Result bad(String reason) {
        return new Result(false,"cabecera NSO0 no coherente: " + reason);
    }

    public static Result inspect(byte[] header,long moduleSize) {
        if(header==null || header.length<HEADER_BYTES ||
           moduleSize<HEADER_BYTES || moduleSize>MAX_MODULE_BYTES)
            return bad("cabecera truncada o módulo demasiado grande.");
        if(header[0]!='N' || header[1]!='S' || header[2]!='O' || header[3]!='0')
            return bad("firma NSO0 no encontrada.");
        long flags=u32(header,0x0c);
        Segment[] segments=new Segment[3];
        long lastMemEnd=0;
        long imageStart=Long.MAX_VALUE,imageEnd=0;
        for(int index=0;index<3;index++) {
            int at=0x10+index*0x10;
            long file=u32(header,at),memory=u32(header,at+4),
                 length=u32(header,at+8),stored=u32(header,0x60+index*4);
            boolean compressed=(flags & (1L<<index))!=0;
            boolean hashed=(flags & (1L<<(index+3)))!=0;
            if(length>MAX_SEGMENT_BYTES)return bad(PARTS[index]+" excede límite.");
            if(length==0) {
                if(stored!=0)return bad(PARTS[index]+" vacía con bytes almacenados.");
                segments[index]=new Segment(file,memory,length,stored,compressed,hashed);
                continue;
            }
            long diskLength=compressed?stored:length;
            if(!compressed && stored!=0 && stored!=length)
                return bad(PARTS[index]+" declara tamaños incoherentes.");
            if(diskLength==0 || file<HEADER_BYTES ||
               file>moduleSize || diskLength>moduleSize-file)
                return bad(PARTS[index]+" apunta fuera del NSO.");
            if(memory+length>0x100000000L)
                return bad(PARTS[index]+" desborda direcciones.");
            segments[index]=new Segment(file,memory,length,stored,compressed,hashed);
            imageStart=Math.min(imageStart,memory);
            imageEnd=Math.max(imageEnd,memory+length);
            lastMemEnd=Math.max(lastMemEnd,memory+length);
        }
        for(int i=0;i<3;i++) {
            Segment a=segments[i];
            if(a.length==0)continue;
            for(int j=i+1;j<3;j++) {
                Segment b=segments[j];
                if(b.length==0)continue;
                if(a.file<b.file+b.diskLength() && b.file<a.file+a.diskLength())
                    return bad(PARTS[i]+" y "+PARTS[j]+" se superponen en disco.");
                if(a.memory<b.memory+b.length && b.memory<a.memory+a.length)
                    return bad(PARTS[i]+" y "+PARTS[j]+" se superponen en memoria.");
            }
        }
        long bss=u32(header,0x3c);
        if(bss>MAX_IMAGE_BYTES)return bad("BSS demasiado grande.");
        if(imageStart==Long.MAX_VALUE)return bad("no hay segmentos de código/datos.");
        if(lastMemEnd+bss-imageStart>MAX_IMAGE_BYTES ||
           lastMemEnd+bss>0x100000000L)
            return bad("imagen de memoria excede límites.");
        StringBuilder out=new StringBuilder("cabecera NSO0 estructuralmente coherente");
        out.append(" / versión ").append(u32(header,4))
           .append(" / BSS ").append(bss).append(" bytes");
        for(int i=0;i<3;i++) {
            Segment seg=segments[i];
            out.append("\n  ").append(PARTS[i]).append(": ")
               .append(seg.length).append(" B en memoria; ")
               .append(seg.diskLength()).append(" B almacenados; ")
               .append(seg.compressed?"comprimido":"sin comprimir")
               .append("; ").append(seg.hashed?"SHA-256 declarado (no verificado)":"SHA-256 no exigido");
        }
        if((flags&0x80)!=0)
            out.append("\n  AVISO: bandera ZBIC detectada; cargador actual incompatible.");
        out.append("\n  No se comprobaron hashes de segmentos ni se ejecutó código.");
        return new Result(true,out.toString());
    }
}
