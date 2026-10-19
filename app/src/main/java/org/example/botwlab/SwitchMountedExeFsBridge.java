package org.example.botwlab;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * Proof-of-access bridge to ExeFS, reading only from the previously mounted
 * NSP virtual file system. It neither copies an NSP nor exports executable
 * bytes, firmware or keys; it does not execute Zelda.
 *
 * NCA header AES-XTS -> signed-header hash -> rights/key-area resolution ->
 * CTR partition -> hashed PartitionFS data region -> main/sdk NSO magic.
 * Content integrity beyond the FS header is not yet verified.
 */
public final class SwitchMountedExeFsBridge {
    private SwitchMountedExeFsBridge() {}

    private static final long GAME_ID = 0x01007EF00011E000L;
    private static final int HEADER_SIZE = 0xC00;
    private static final int MAX_READ = 65536;
    private static final int MAX_ENTRIES = 20000;
    private static final int MAX_KEY_TEXT = 128 * 1024;

    private static native String[] nativeEntryNames();
    private static native long nativeEntrySize(String name);
    private static native byte[] nativeReadEntry(String name, long offset, int length);

    interface Reader {
        String[] names();
        long size(String name);
        byte[] read(String name, long offset, int length);
    }

    public static String probe(InputStream localKeys) throws IOException, GeneralSecurityException {
        return inspect(new Reader() {
            public String[] names() { return nativeEntryNames(); }
            public long size(String name) { return nativeEntrySize(name); }
            public byte[] read(String name, long offset, int length) {
                return nativeReadEntry(name,offset,length);
            }
        }, localKeys);
    }

    /** Exposed to the synthetic test suite; not part of Android's UI. */
    static String inspect(Reader reader, InputStream localKeys)
            throws IOException, GeneralSecurityException {
        if (reader == null || localKeys == null) throw new IOException("Falta el NSP montado o el archivo de claves.");
        String[] names = reader.names();
        if (names == null || names.length == 0)
            return "RUNTIME ExeFS: primero monta un NSP mediante el boton RUNTIME VFS.";
        if (names.length > MAX_ENTRIES) throw new IOException("Demasiadas entradas virtuales.");
        List<String> tickets = new ArrayList<>();
        List<String> ncas = new ArrayList<>();
        for (String name : names) {
            if (name == null || name.length() > 255 || name.indexOf('/') >= 0 ||
                name.indexOf('\\') >= 0) throw new IOException("Nombre virtual inseguro.");
            if (name.toLowerCase(Locale.ROOT).endsWith(".tik")) tickets.add(name);
            if (name.toLowerCase(Locale.ROOT).endsWith(".nca")) ncas.add(name);
        }
        if (ncas.size() > 32 || tickets.size() > 32) throw new IOException("Demasiadas entradas NCA/ticket.");

        try (LocalKeys keys = new LocalKeys(localKeys)) {
            byte[] headerKey = keys.lookup("header_key",32);
            if (headerKey == null) return "RUNTIME ExeFS: falta header_key en el archivo local.";
            try {
                StringBuilder output = new StringBuilder();
                output.append("RUNTIME NSP → ExeFS (LECTURA DIRECTA Y LOCAL)\n");
                output.append("NCA virtuales: ").append(ncas.size()).append('\n');
                int candidates=0, resolved=0, pfs=0, nso=0;
                for (String name : ncas) {
                    long entrySize=reader.size(name);
                    if (entrySize < HEADER_SIZE) continue;
                    byte[] cipherHeader=read(reader,name,0,HEADER_SIZE);
                    byte[] header=SwitchNcaHeaderBridge.decryptHeader(cipherHeader,headerKey);
                    Arrays.fill(cipherHeader,(byte)0);
                    try {
                        if (header[0x200]!='N' || header[0x201]!='C' ||
                            header[0x202]!='A' || header[0x203]!='3' ||
                            (header[0x205]&255)!=0 ||
                            le64(header,0x210)!=GAME_ID) continue;
                        candidates++;
                        if (le64(header,0x208)>entrySize || le64(header,0x208)<HEADER_SIZE) {
                            output.append("PROGRAM: tamaño declarado incoherente.\n");
                            continue;
                        }
                        KeyResolution resolution = sectionKey(reader,keys,header,tickets);
                        if (resolution.key == null) {
                            output.append("PROGRAM: ").append(resolution.detail).append('\n');
                            continue;
                        }
                        byte[] sectionKey = resolution.key;
                        output.append("PROGRAM: ").append(resolution.detail).append('\n');
                        resolved++;
                        try {
                            for (int slot=0;slot<4;slot++) {
                                long startSector=le32(header,0x240+slot*16);
                                long endSector=le32(header,0x244+slot*16);
                                if (startSector==0 && endSector==0) continue;
                                long sectionStart=startSector*512L;
                                long sectionEnd=endSector*512L;
                                if (sectionStart<HEADER_SIZE || sectionEnd<=sectionStart ||
                                    sectionEnd>entrySize) continue;
                                int fs=0x400+slot*0x200;
                                byte[] expected=Arrays.copyOfRange(header,0x280+slot*32,0x280+slot*32+32);
                                byte[] actual=MessageDigest.getInstance("SHA-256").digest(
                                    Arrays.copyOfRange(header,fs,fs+0x200));
                                boolean valid=MessageDigest.isEqual(expected,actual);
                                Arrays.fill(expected,(byte)0);
                                Arrays.fill(actual,(byte)0);
                                if (!valid) continue;
                                int fsType=header[fs+2]&255;
                                int hashType=header[fs+3]&255;
                                int encryption=header[fs+4]&255;
                                if (fsType!=1) continue; // PFS0/ExeFS candidates only.
                                if (encryption!=3 && encryption!=1) {
                                    output.append("PartitionFS: cifrado no compatible todavía (tipo ")
                                        .append(encryption).append(").\n");
                                    continue;
                                }
                                long dataOffset=0;
                                long dataLength=sectionEnd-sectionStart;
                                if (hashType==2) {
                                    long layers=le32(header,fs+0x2c);
                                    if (layers<1 || layers>5) continue;
                                    int region=fs+0x30+(int)(layers-1)*16;
                                    dataOffset=le64(header,region);
                                    dataLength=le64(header,region+8);
                                    if (dataOffset<0 || dataLength<16 ||
                                        dataOffset>sectionEnd-sectionStart ||
                                        dataLength>sectionEnd-sectionStart-dataOffset) continue;
                                } else if(hashType!=1) continue;
                                long root=sectionStart+dataOffset;
                                long upperIv=le64(header,fs+0x140);
                                // Metadata only. A future runtime must validate all hash layers
                                // and support streaming partition blocks before executing code.
                                byte[] first=readPartition(reader,name,root,16,sectionKey,upperIv,encryption);
                                try {
                                    if (!startsWith(first,"PFS0")) {
                                        output.append("PartitionFS: inicio no coincide con PFS0.\n");
                                        continue;
                                    }
                                    long count=le32(first,4), stringBytes=le32(first,8);
                                    long metadataEnd=16+24*count+stringBytes;
                                    if (count<1 || count>64 || stringBytes<1 ||
                                        stringBytes>16384 || metadataEnd>MAX_READ-16 ||
                                        metadataEnd>dataLength) {
                                        output.append("PartitionFS: índice fuera de límites.\n");
                                        continue;
                                    }
                                    byte[] table=readPartition(reader,name,root,
                                        (int)metadataEnd,sectionKey,upperIv,encryption);
                                    try {
                                        List<Entry> entries=readPfs(table,count,stringBytes,dataLength);
                                        pfs++;
                                        output.append("ExeFS candidato: PFS0 leído desde NSP cifrado.")
                                            .append(" Archivos: ").append(entries.size()).append('\n');
                                        for (Entry file:entries) {
                                            if (!file.name.equals("main") && !file.name.equals("sdk")) continue;
                                            String label=file.name.equals("main")?"main":"sdk";
                                            long moduleOffset=root+metadataEnd+file.offset;
                                            if(file.size<4 || moduleOffset<root) continue;
                                            byte[] prefix=readPartition(reader,name,moduleOffset,4,
                                                sectionKey,upperIv,encryption);
                                            try {
                                                if (startsWith(prefix,"NSO0")) {
                                                    nso++;
                                                    output.append(label)
                                                      .append(": firma NSO0 válida en flujo descifrado (")
                                                      .append(file.size).append(" bytes).\n");
                                                    if (file.size>=SwitchNsoMetadataAudit.HEADER_BYTES) {
                                                        byte[] nsoHeader=readPartition(reader,name,
                                                            moduleOffset,SwitchNsoMetadataAudit.HEADER_BYTES,
                                                            sectionKey,upperIv,encryption);
                                                        try {
                                                            SwitchNsoMetadataAudit.Result info=
                                                                SwitchNsoMetadataAudit.inspect(
                                                                    nsoHeader,file.size);
                                                            output.append(label).append(": ")
                                                                .append(info.report).append('\n');
                                                            if (info.valid) {
                                                                try {
                                                                    SwitchNsoSectionHashes.Result hashes =
                                                                        SwitchNsoSectionHashes.verify(
                                                                            nsoHeader,file.size,
                                                                            new SwitchNsoSectionHashes.Source() {
                                                                                @Override public byte[] read(
                                                                                      long offset,int count)
                                                                                      throws IOException {
                                                                                    if(offset<0 || count<0 ||
                                                                                        offset>file.size ||
                                                                                        count>file.size-offset)
                                                                                        throw new IOException(
                                                                                            "Segmento fuera de ExeFS");
                                                                                    try {
                                                                                        return readPartition(
                                                                                            reader,name,
                                                                                            moduleOffset+offset,count,
                                                                                            sectionKey,upperIv,
                                                                                            encryption);
                                                                                    } catch (GeneralSecurityException e) {
                                                                                        throw new IOException(
                                                                                            "No se pudo descifrar una pagina");
                                                                                    }
                                                                                }
                                                                            });
                                                                    output.append(label).append(": ")
                                                                        .append(hashes.report).append('\n');
                                                                    if(hashes.ok) {
                                                                        try (SwitchNsoMemoryImage image =
                                                                             SwitchNsoMemoryImage.prepare(
                                                                                 nsoHeader,file.size,
                                                                                 (offset,count) -> {
                                                                                     if(offset<0 || count<0 ||
                                                                                        offset>file.size ||
                                                                                        count>file.size-offset)
                                                                                         throw new IOException(
                                                                                             "Modulo NSO fuera de limites");
                                                                                     try {
                                                                                         return readPartition(
                                                                                             reader,name,
                                                                                             moduleOffset+offset,
                                                                                             count,sectionKey,upperIv,
                                                                                             encryption);
                                                                                     } catch (GeneralSecurityException e) {
                                                                                         throw new IOException(
                                                                                             "Fallo de lectura cifrada");
                                                                                     }
                                                                                 })) {
                                                                            output.append(label).append(": ")
                                                                                .append(image.summary())
                                                                                .append('\n');
                                                                        }
                                                                    }
                                                                } catch (IOException |
                                                                         java.security.NoSuchAlgorithmException e) {
                                                                    output.append(label)
                                                                        .append(": verificación de segmentos ")
                                                                        .append("incompleta (")
                                                                        .append(e.getClass().getSimpleName())
                                                                        .append("); no se ejecutó código.\n");
                                                                }
                                                            }
                                                        } finally {
                                                            Arrays.fill(nsoHeader,(byte)0);
                                                        }
                                                    }
                                                } else {
                                                    output.append(label)
                                                      .append(": archivo localizado, sin firma NSO0.\n");
                                                }
                                            } finally {Arrays.fill(prefix,(byte)0);}
                                        }
                                    } finally {Arrays.fill(table,(byte)0);}
                                } finally {Arrays.fill(first,(byte)0);}
                            }
                        } finally {Arrays.fill(sectionKey,(byte)0);}
                    } finally {Arrays.fill(header,(byte)0);}
                }
                output.append("NCA PROGRAM coincidentes: ").append(candidates).append('\n')
                  .append("Claves de secciones resueltas: ").append(resolved).append('\n')
                  .append("PartitionFS leídos: ").append(pfs).append('\n')
                  .append("Módulos NSO0 detectados: ").append(nso).append('\n')
                  .append("NO se copió, guardó ni ejecutó código ni firmware.\n")
                  .append("Los hashes de contenido y servicios Horizon/NVN siguen pendientes.\n")
                  .append("NO ES ZELDA JUGABLE.");
                return output.toString();
            } finally {Arrays.fill(headerKey,(byte)0);}
        }
    }

    private static final class Entry {
        final String name;
        final long offset, size;
        Entry(String n,long o,long s) {name=n;offset=o;size=s;}
    }

    private static List<Entry> readPfs(byte[] data,long count,long nameBytes)
            throws IOException {
        return readPfs(data,count,nameBytes,Long.MAX_VALUE);
    }
    private static List<Entry> readPfs(byte[] data,long count,long nameBytes,long regionSize)
            throws IOException {
        int records=(int)count;
        int stringBase=16+records*24;
        int namesEnd=stringBase+(int)nameBytes;
        if (records<0 || namesEnd>data.length || regionSize<data.length)
            throw new IOException("PFS0 fuera de límites.");
        long payloadLimit=regionSize-data.length;
        List<Entry> items=new ArrayList<>();
        for (int i=0;i<records;i++) {
            int at=16+i*24;
            long off=le64(data,at);
            long length=le64(data,at+8);
            long nameOff=le32(data,at+16);
            if(off<0 || length<0 || off>payloadLimit || length>payloadLimit-off ||
               nameOff>=nameBytes) throw new IOException("Entrada PFS0 fuera de límites.");
            int first=stringBase+(int)nameOff,last=first;
            while(last<namesEnd && data[last]!=0 && last-first<=255) {
                if(data[last]<33 || data[last]>126 || data[last]=='/' || data[last]=='\\')
                    throw new IOException("Nombre PFS0 no válido.");
                last++;
            }
            if(last>=namesEnd || last==first || last-first>255)
                throw new IOException("Nombre PFS0 mal terminado.");
            String name=new String(data,first,last-first,StandardCharsets.US_ASCII);
            items.add(new Entry(name,off,length));
        }
        return items;
    }

    // Reports *only* a processing stage; never print tickets, rights IDs or key bytes.
    private static final class KeyResolution {
        final byte[] key;
        final String detail;
        KeyResolution(byte[] bytes, String stage) {key=bytes;detail=stage;}
    }

    private static int littleEndian32(byte[] data,int offset) {
        return (int)le32(data,offset);
    }

    private static int ticketDataBase(byte[] ticket) {
        if(ticket.length<4)return -1;
        int signatureType=littleEndian32(ticket,0);
        // TicketData starts after signed header: size depends on signature algorithm.
        // Values from Eden Core::Crypto::RSA4096Ticket/RSA2048Ticket/ECDSATicket.
        if(signatureType==0x10000 || signatureType==0x10003)return 0x240;
        if(signatureType==0x10001 || signatureType==0x10004)return 0x140;
        if(signatureType==0x10002 || signatureType==0x10005)return 0x80;
        return -1;
    }

    private static KeyResolution sectionKey(Reader reader, LocalKeys keys, byte[] header,
                                     List<String> tickets)
            throws IOException, GeneralSecurityException {
        int revision=Math.max(header[0x206]&255,header[0x220]&255);
        int master=Math.max(1,revision)-1;
        if(master>31) return new KeyResolution(null,
            "generación de cifrado fuera de rango soportado; no hay acceso ExeFS.");
        byte[] rights=Arrays.copyOfRange(header,0x230,0x240);
        try {
            boolean hasRights=false;
            for(byte b:rights) if(b!=0) {hasRights=true;break;}
            if(hasRights) {
                // The game uses a rights-protected title key; key-area keys are not
                // a substitute. The matching ticket is required.
                byte[] kek=keys.lookup(String.format(Locale.ROOT,"titlekek_%02X",master),16);
                if (kek==null) return new KeyResolution(null,
                    "NCA con Rights ID, pero falta la entrada titlekek para su generación.");
                try {
                    if(tickets.isEmpty())return new KeyResolution(null,
                        "NCA con Rights ID y titlekek disponible, pero el NSP no contiene .tik.");
                    int unknown=0, mismatched=0, malformed=0, personalized=0;
                    for (String path:tickets) {
                        long size=reader.size(path);
                        if(size<0xC0 || size>0x10000) {malformed++;continue;}
                        byte[] ticket=read(reader,path,0,(int)Math.min(size,0x500));
                        try {
                            int base=ticketDataBase(ticket);
                            if(base<0){unknown++;continue;}
                            // rights_id follows issuer, title_key_block and ticket fields
                            // at offset +0x160, regardless of signature scheme.
                            if(ticket.length<base+0x170){malformed++;continue;}
                            byte[] candidate=Arrays.copyOfRange(ticket,base+0x160,base+0x170);
                            boolean matches=MessageDigest.isEqual(rights,candidate);
                            Arrays.fill(candidate,(byte)0);
                            if(!matches){mismatched++;continue;}
                            int type=ticket[base+0x141]&255;
                            if(type!=0){personalized++;continue;}
                            byte[] enc=Arrays.copyOfRange(ticket,base+0x40,base+0x50);
                            try {
                                return new KeyResolution(decryptEcb(kek,enc),
                                    "clave de contenido resuelta con ticket común local; "
                                    + "ExeFS todavía sin validar.");
                            } finally {Arrays.fill(enc,(byte)0);}
                        } finally {Arrays.fill(ticket,(byte)0);}
                    }
                    if(personalized>0)return new KeyResolution(null,
                        "ticket coincidente, pero personalizado: este puente todavía no lo admite.");
                    if(mismatched>0)return new KeyResolution(null,
                        "titlekek disponible, pero el Rights ID del NCA no coincide con los "
                        +"tickets compatibles del NSP (sin revelar identificadores).");
                    if(unknown>0)return new KeyResolution(null,
                        "existe .tik, pero su tipo de firma no es reconocido por el lector.");
                    return new KeyResolution(null,
                        "hay tickets, pero ninguno tiene una estructura común utilizable.");
                } finally {Arrays.fill(kek,(byte)0);}
            }
            int index=header[0x207]&255;
            if(index>2)return new KeyResolution(null,
                "NCA sin Rights ID con índice de clave de área no soportado.");
            String[] area={"application","ocean","system"};
            byte[] kek=keys.lookup(String.format(Locale.ROOT,
                "key_area_key_%s_%02X",area[index],master),16);
            if(kek==null)return new KeyResolution(null,
                "NCA sin Rights ID, pero falta su key_area_key local.");
            byte[] enc=Arrays.copyOfRange(header,0x320,0x330);
            try{return new KeyResolution(decryptEcb(kek,enc),
                "clave de sección resuelta mediante key-area AES-ECB; ExeFS sin validar.");}
            finally {Arrays.fill(kek,(byte)0);Arrays.fill(enc,(byte)0);}
        } finally {Arrays.fill(rights,(byte)0);}
    }

    private static byte[] decryptEcb(byte[] key,byte[] encrypted)
            throws GeneralSecurityException {
        Cipher aes=Cipher.getInstance("AES/ECB/NoPadding");
        aes.init(Cipher.DECRYPT_MODE,new SecretKeySpec(key,"AES"));
        return aes.doFinal(encrypted);
    }

    private static byte[] readPartition(Reader reader,String name,long position,int amount,
                                         byte[] key,long upperIv,int encryption)
            throws IOException, GeneralSecurityException {
        if(amount<0 || amount>MAX_READ-16 || position<0 || position>Long.MAX_VALUE-16)
            throw new IOException("Acceso de partición fuera de límites.");
        long aligned=position & ~15L;
        int prefix=(int)(position-aligned);
        int n=(prefix+amount+15)&~15;
        if(n>MAX_READ)throw new IOException("Lectura de partición excesiva.");
        byte[] input=read(reader,name,aligned,n);
        if(encryption==1) {
            byte[] out=Arrays.copyOfRange(input,prefix,prefix+amount);
            Arrays.fill(input,(byte)0);
            return out;
        }
        byte[] iv=new byte[16];
        putBe64(iv,0,upperIv);
        putBe64(iv,8,aligned/16);
        try {
            Cipher aes=Cipher.getInstance("AES/CTR/NoPadding");
            aes.init(Cipher.DECRYPT_MODE,new SecretKeySpec(key,"AES"),new IvParameterSpec(iv));
            byte[] plain=aes.doFinal(input);
            try {
                return Arrays.copyOfRange(plain,prefix,prefix+amount);
            } finally { Arrays.fill(plain,(byte)0); }
        } finally {
            Arrays.fill(input,(byte)0);
            Arrays.fill(iv,(byte)0);
        }
    }

    private static byte[] read(Reader reader,String name,long offset,int length) throws IOException {
        if(length<0 || length>MAX_READ || offset<0 || offset>Long.MAX_VALUE-length ||
           reader.size(name)<0 || offset>reader.size(name)-length)
            throw new IOException("Lectura virtual fuera del archivo.");
        byte[] result=reader.read(name,offset,length);
        if(result==null || result.length!=length) throw new IOException("Lectura virtual fallida.");
        return result;
    }

    private static void putBe64(byte[] dest,int at,long value) {
        for(int i=7;i>=0;i--){dest[at+i]=(byte)(value&255);value>>>=8;}
    }
    private static boolean startsWith(byte[] bytes,String text) {
        if(bytes.length<text.length())return false;
        for(int j=0;j<text.length();j++)if(bytes[j]!=(byte)text.charAt(j))return false;
        return true;
    }
    private static long le32(byte[] bytes,int at) {
        return ((long)bytes[at]&255) | (((long)bytes[at+1]&255)<<8) |
            (((long)bytes[at+2]&255)<<16) | (((long)bytes[at+3]&255)<<24);
    }
    private static long le64(byte[] bytes,int at) {
        long value=0;
        for(int i=7;i>=0;i--)value=(value<<8)|((long)bytes[at+i]&255);
        return value;
    }

    private static final class LocalKeys implements AutoCloseable {
        private final byte[] data = new byte[MAX_KEY_TEXT+1];
        private int length;
        LocalKeys(InputStream in) throws IOException {
            try {
                while(length<data.length) {
                    int n=in.read(data,length,data.length-length);
                    if(n<0)break;
                    if(n==0) {int b=in.read();if(b<0)break;data[length++]=(byte)b;}
                    else length+=n;
                }
                if(length>MAX_KEY_TEXT)throw new IOException("Archivo de claves demasiado grande.");
            } catch(IOException e){close();throw e;}
        }
        byte[] lookup(String needle,int bytes) throws IOException {
            byte[] tag=needle.getBytes(StandardCharsets.US_ASCII);
            byte[] found=null;
            for(int start=0;start<length;){
                int end=start;
                while(end<length && data[end]!='\n' && data[end]!='\r')end++;
                int at=start;
                while(at<end && (data[at]==' ' || data[at]=='\t'))at++;
                boolean match=at+tag.length<end;
                if(match)for(int j=0;j<tag.length;j++)if(data[at+j]!=tag[j]){match=false;break;}
                if(match) {
                    int j=at+tag.length;
                    if(data[j]!='=' && data[j]!=' ' && data[j]!='\t')match=false;
                    else {
                        while(j<end && (data[j]==' ' || data[j]=='\t'))j++;
                        if(j>=end || data[j++]!='=')match=false;
                        else {
                            while(j<end && (data[j]==' ' || data[j]=='\t'))j++;
                            byte[] value=new byte[bytes];
                            if(j+bytes*2>end)throw new IOException("Formato de claves incorrecto.");
                            try {
                                for(int p=0;p<bytes;p++){
                                    int hi=hex(data[j+p*2]),lo=hex(data[j+p*2+1]);
                                    if(hi<0 || lo<0)throw new IOException("Clave hexadecimal inválida.");
                                    value[p]=(byte)((hi<<4)|lo);
                                }
                            }catch(IOException ex){Arrays.fill(value,(byte)0);throw ex;}
                            int next=j+bytes*2;
                            while(next<end && (data[next]==' ' || data[next]=='\t'))next++;
                            if(next!=end && data[next]!='#' && data[next]!=';') {
                                Arrays.fill(value,(byte)0);
                                throw new IOException("Longitud de clave incorrecta.");
                            }
                            if(found!=null) {
                                Arrays.fill(found,(byte)0);
                                Arrays.fill(value,(byte)0);
                                throw new IOException("Clave duplicada.");
                            }
                            found=value;
                        }
                    }
                }
                start=end;
                while(start<length && (data[start]=='\n' || data[start]=='\r'))start++;
            }
            return found;
        }
        private static int hex(byte b) {
            if(b>='0'&&b<='9')return b-'0';
            if(b>='a'&&b<='f')return b-'a'+10;
            if(b>='A'&&b<='F')return b-'A'+10;
            return -1;
        }
        public void close(){Arrays.fill(data,(byte)0);length=0;}
    }
}
