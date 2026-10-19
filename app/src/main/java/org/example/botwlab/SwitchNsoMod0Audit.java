package org.example.botwlab;

/**
 * Read-only, bounded MOD0 + ELF64 dynamic relocation table audit.
 * Runs on a caller-owned temporary verified NSO0 memory image. This class
 * NEVER changes the memory, patches relocations or executes instructions.
 */
final class SwitchNsoMod0Audit {
    private static final int MAX_DYNAMIC=8192;
    private static final long MAX_RELOCATIONS=250000L;

    private SwitchNsoMod0Audit() {}

    static final class Result {
        final boolean found;
        final boolean valid;
        final String report;
        Result(boolean found, boolean valid, String report) {
            this.found=found;this.valid=valid;this.report=report;
        }
    }

    private static long u32(byte[] mem,int index) {
        return (mem[index]&255L) |
            ((mem[index+1]&255L)<<8) |
            ((mem[index+2]&255L)<<16) |
            ((mem[index+3]&255L)<<24);
    }

    private static long u64(byte[] mem,int index) {
        return u32(mem,index) | (u32(mem,index+4)<<32);
    }

    private static long rel32(byte[] mem,int index) {
        return (int)u32(mem,index);
    }

    private static boolean span(byte[] mem,long base,long va,long len) {
        return mem!=null && len>=0 && va>=base && va-base<=mem.length &&
            len<=mem.length-(va-base);
    }

    private static int at(long va,long base) {
        return (int)(va-base);
    }

    private static Result issue(String detail) {
        return new Result(true,false,"MOD0 detectado, pero "+detail+
            " No se aplicaron relocaciones ni se ejecuto codigo.");
    }

    private static final class Table {
        long address,size,entrySize;
        boolean used;
    }

    private static final class Tally {
        long records,relative,external,unknown,invalid;
        void add(int type,boolean inRange) {
            records++;
            if(!inRange)invalid++;
            if(type==1027)relative++;
            else if(type==257||type==258||type==1025||type==1026)external++;
            else unknown++;
        }
    }

    private static boolean walk(byte[] mem,long base,Table table,boolean rela,
                                Tally out) {
        if(!table.used || table.size==0)return true;
        long standardSize=rela?24:16;
        if(table.entrySize==0)table.entrySize=standardSize;
        if(table.entrySize!=standardSize || table.size%standardSize!=0 ||
           table.size/standardSize>MAX_RELOCATIONS ||
           !span(mem,base,table.address,table.size))return false;
        int count=(int)(table.size/standardSize);
        for(int i=0;i<count;i++) {
            long at=table.address+((long)i*standardSize);
            int p=at(at,base);
            long target=u64(mem,p);
            long info=u64(mem,p+8);
            int type=(int)(info&0xffffffffL);
            int width=(type==258)?4:8;
            out.add(type,span(mem,base,target,width));
        }
        return true;
    }

    static Result inspect(SwitchNsoMemoryImage image) {
        return inspect(image.memory,image.base,image.bssOffset,image.bssSize);
    }

    /** Test seam; arguments must describe the existing transient image. */
    static Result inspect(byte[] mem,long base,long bssStart,long bssSize) {
        if(!span(mem,base,4,4))
            return new Result(false,false,"MOD0: no hay ubicación de cabecera +0x04.");
        long header=u32(mem,at(4,base));
        if(!span(mem,base,header,0x1c))
            return new Result(false,false,"MOD0: cabecera no encontrada en imagen.");
        int hdr=at(header,base);
        if(mem[hdr]!='M'||mem[hdr+1]!='O'||mem[hdr+2]!='D'||mem[hdr+3]!='0')
            return new Result(false,false,"MOD0: firma no encontrada en offset indicado.");
        long dynamic=header+rel32(mem,hdr+4);
        long modBssStart=header+rel32(mem,hdr+8);
        long modBssEnd=header+rel32(mem,hdr+0xc);
        if(modBssEnd<modBssStart)
            return issue("BSS de MOD0 tiene limites invertidos.");
        if(!span(mem,base,dynamic,16))
            return issue("puntero a tabla .dynamic fuera de memoria.");
        boolean consistentBss=(modBssStart==bssStart &&
                               modBssEnd==bssStart+bssSize);

        Table rela=new Table(),rel=new Table(),plt=new Table();
        long pltRelType=0,needed=0,relativeHint=0;
        boolean end=false;
        int dynamicRecords=0;
        for(int i=0;i<MAX_DYNAMIC;i++) {
            long address=dynamic+16L*i;
            if(!span(mem,base,address,16))
                return issue("tabla .dynamic truncada.");
            int ptr=at(address,base);
            long tag=u64(mem,ptr);
            long val=u64(mem,ptr+8);
            if(tag==0){end=true;break;}
            dynamicRecords++;
            switch((int)tag) {
                case 1: needed++;break;
                case 7: rela.address=val;rela.used=true;break;
                case 8: rela.size=val;break;
                case 9: rela.entrySize=val;break;
                case 17: rel.address=val;rel.used=true;break;
                case 18: rel.size=val;break;
                case 19: rel.entrySize=val;break;
                case 23: plt.address=val;plt.used=true;break;
                case 2: plt.size=val;break;
                case 20: pltRelType=val;break;
                case 0x6ffffff9: relativeHint=val;break;
                default:break;
            }
        }
        if(!end)return issue(".dynamic no termina con DT_NULL dentro del limite.");
        if((rela.size!=0&&!rela.used)||(rel.size!=0&&!rel.used)||
           (plt.size!=0&&!plt.used))
            return issue("tamaño de relocaciones sin tabla correspondiente.");
        Tally relas=new Tally(),rels=new Tally(),plts=new Tally();
        if(!walk(mem,base,rela,true,relas))
            return issue("DT_RELA fuera de limites o entrada distinta de ELF64_Rela.");
        if(!walk(mem,base,rel,false,rels))
            return issue("DT_REL fuera de limites o entrada distinta de ELF64_Rel.");
        if(plt.used&&plt.size!=0) {
            if(pltRelType!=7&&pltRelType!=17)
                return issue("tipo DT_PLTREL no admitido para diagnostico.");
            plt.entrySize=(pltRelType==7?24:16);
            if(!walk(mem,base,plt,pltRelType==7,plts))
                return issue("DT_JMPREL fuera de limites.");
        }
        long total=relas.records+rels.records+plts.records;
        long relative=relas.relative+rels.relative+plts.relative;
        long external=relas.external+rels.external+plts.external;
        long unknown=relas.unknown+rels.unknown+plts.unknown;
        long invalid=relas.invalid+rels.invalid+plts.invalid;
        StringBuilder out=new StringBuilder("MOD0 localizado y tabla .dynamic validada: ")
            .append(dynamicRecords).append(" entradas; BSS ")
            .append(consistentBss?"coincide con NSO.":"difiere del NSO (requiere investigacion).")
            .append("\n  ELF64 RELA: ").append(relas.records)
            .append("; REL: ").append(rels.records)
            .append("; PLT: ").append(plts.records)
            .append("\n  Relocaciones catalogadas: ").append(total)
            .append("; R_AARCH64_RELATIVE: ").append(relative)
            .append("; dependientes de simbolos: ").append(external)
            .append("; otras: ").append(unknown)
            .append("; destinos fuera de imagen: ").append(invalid)
            .append("\n  Bibliotecas DT_NEEDED: ").append(needed)
            .append("; pista DT_RELACOUNT: ").append(relativeHint)
            .append("\n  Solo lectura: NO se aplicaron relocaciones ni se ejecuto codigo.");
        return new Result(true,invalid==0&&consistentBss,out.toString());
    }
}
