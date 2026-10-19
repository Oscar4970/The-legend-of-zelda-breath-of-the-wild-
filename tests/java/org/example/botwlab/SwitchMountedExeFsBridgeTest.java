package org.example.botwlab;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** Synthetic-only game-shaped fixture: original homebrew NSO headers, no BOTW files. */
public final class SwitchMountedExeFsBridgeTest {
    static final long GAME_ID=0x01007EF00011E000L;
    private static void assertThat(boolean value,String description) {
        if(!value)throw new AssertionError(description);
    }
    private static void put32(byte[] target,int at,long n) {
        for(int i=0;i<4;i++)target[at+i]=(byte)(n >>> (8*i));
    }
    private static void put64(byte[] target,int at,long n) {
        for(int i=0;i<8;i++)target[at+i]=(byte)(n >>> (8*i));
    }
    private static void be64(byte[] target,int at,long n) {
        for(int i=7;i>=0;i--){target[at+i]=(byte)n;n>>>=8;}
    }
    private static byte[] encryptXts(byte[] plain,byte[] key) throws Exception {
        byte[] result=new byte[plain.length];
        Cipher data=Cipher.getInstance("AES/ECB/NoPadding");
        Cipher tweakCipher=Cipher.getInstance("AES/ECB/NoPadding");
        data.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(key,0,16,"AES"));
        tweakCipher.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(key,16,16,"AES"));
        for(int sector=0;sector<plain.length/512;sector++){
            byte[] iv=new byte[16];be64(iv,8,sector);
            byte[] tweak=tweakCipher.doFinal(iv);
            for(int start=0;start<512;start+=16){
                byte[] input=new byte[16];
                int at=sector*512+start;
                for(int i=0;i<16;i++)input[i]=(byte)(plain[at+i]^tweak[i]);
                byte[] output=data.doFinal(input);
                for(int i=0;i<16;i++)result[at+i]=(byte)(output[i]^tweak[i]);
                int carry=0;
                for(int i=0;i<16;i++){
                    int v=tweak[i]&255;
                    tweak[i]=(byte)((v<<1)|carry);
                    carry=v>>>7;
                }
                if(carry!=0)tweak[0]^=(byte)0x87;
                Arrays.fill(input,(byte)0);Arrays.fill(output,(byte)0);
            }
            Arrays.fill(tweak,(byte)0);
        }
        return result;
    }
    private static String hex(byte[] bytes){
        StringBuilder result=new StringBuilder();
        for(byte b:bytes)result.append(String.format("%02X",b&255));
        return result.toString();
    }
    private static byte[] ctrCrypt(byte[] input,byte[] key,long upper,long offset)
            throws Exception {
        byte[] iv=new byte[16];be64(iv,0,upper);be64(iv,8,offset/16);
        Cipher aes=Cipher.getInstance("AES/CTR/NoPadding");
        aes.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(key,"AES"),new IvParameterSpec(iv));
        return aes.doFinal(input);
    }
    private static final class MemoryReader implements SwitchMountedExeFsBridge.Reader {
        final Map<String,byte[]> files=new HashMap<>();
        @Override public String[] names(){return files.keySet().toArray(new String[0]);}
        @Override public long size(String name){byte[] value=files.get(name);return value==null?-1:value.length;}
        @Override public byte[] read(String name,long offset,int length){
            byte[] value=files.get(name);
            if(value==null || offset<0 || offset>value.length ||
               length<0 || length>value.length-offset)return null;
            return Arrays.copyOfRange(value,(int)offset,(int)offset+length);
        }
    }

    private static final class Fixture {
        final MemoryReader reader;
        final String keyText;
        Fixture(MemoryReader r,String k){reader=r;keyText=k;}
        String probe() throws Exception {
            return SwitchMountedExeFsBridge.inspect(reader,
                new ByteArrayInputStream(keyText.getBytes(StandardCharsets.US_ASCII)));
        }
    }
    private static Fixture make(boolean ticketRights) throws Exception {
        byte[] headerKey=new byte[32],contentKey=new byte[16],kek=new byte[16];
        for(int i=0;i<32;i++)headerKey[i]=(byte)i;
        for(int i=0;i<16;i++){contentKey[i]=(byte)(0x40+i);kek[i]=(byte)(0x70+i);}
        byte[] rights=new byte[16];
        for(int i=0;i<16;i++)rights[i]=(byte)(i+1);
        byte[] nca=new byte[0x6000];
        byte[] head=new byte[0xC00];
        System.arraycopy("NCA3".getBytes(StandardCharsets.US_ASCII),0,head,0x200,4);
        head[0x205]=0;head[0x206]=1;head[0x220]=1;
        put64(head,0x208,nca.length);
        put64(head,0x210,GAME_ID);
        if(ticketRights) {
            System.arraycopy(rights,0,head,0x230,16);
        } else {
            head[0x207]=0; // application keyarea index
            Cipher area=Cipher.getInstance("AES/ECB/NoPadding");
            area.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(kek,"AES"));
            System.arraycopy(area.doFinal(contentKey),0,head,0x320,16);
        }
        put32(head,0x240,6); // Section starts at 0xC00
        put32(head,0x244,16); // Section ends at 0x2000
        final int fs=0x400;
        head[fs+2]=1; // PartitionFS
        head[fs+3]=2; // Hierarchical SHA-256
        head[fs+4]=3; // AES-CTR
        put32(head,fs+0x2c,2); // Two hash layers
        put64(head,fs+0x40,0x200); // Last layer offset relative to FS section
        put64(head,fs+0x48,0x1000);
        long upper=0x0123456789ABCDEFL;
        put64(head,fs+0x140,upper);
        byte[] digest=MessageDigest.getInstance("SHA-256").digest(
            Arrays.copyOfRange(head,0x400,0x600));
        System.arraycopy(digest,0,head,0x280,32);
        byte[] encryptedHeader=encryptXts(head,headerKey);
        System.arraycopy(encryptedHeader,0,nca,0,encryptedHeader.length);

        byte[] partition=new byte[0x1000];
        System.arraycopy("PFS0".getBytes(StandardCharsets.US_ASCII),0,partition,0,4);
        put32(partition,4,3);
        put32(partition,8,14); // "main\0sdk\0rtld\0"
        put64(partition,16,0);
        put64(partition,24,0x200);
        put32(partition,32,0);
        put64(partition,40,0x200);
        put64(partition,48,0x200);
        put32(partition,56,5);
        put64(partition,64,0x400);
        put64(partition,72,0x200);
        put32(partition,80,9);
        System.arraycopy("main\0sdk\0rtld\0".getBytes(StandardCharsets.US_ASCII),
            0,partition,88,14);
        int dataStart=16+3*24+14;
        for (int moduleBase : new int[]{dataStart,dataStart+0x200,dataStart+0x400}) {
            System.arraycopy("NSO0".getBytes(StandardCharsets.US_ASCII),
                0,partition,moduleBase,4);
            // Three synthetic sections: LZ4 literal-only .text, raw .rodata,
            // and overlapping-copy LZ4 .data. All hashes are SHA-256 of
            // the DECOMPRESSED bytes (never ciphertext or compressed blocks).
            put32(partition,moduleBase+0x0c,0x3D); // compression text/data, hash all
            put32(partition,moduleBase+0x10,0x100);
            put32(partition,moduleBase+0x14,0);
            put32(partition,moduleBase+0x18,16);
            put32(partition,moduleBase+0x60,18);
            put32(partition,moduleBase+0x20,0x120);
            put32(partition,moduleBase+0x24,0x100);
            put32(partition,moduleBase+0x28,12);
            put32(partition,moduleBase+0x64,12);
            put32(partition,moduleBase+0x30,0x140);
            put32(partition,moduleBase+0x34,0x200);
            put32(partition,moduleBase+0x38,16);
            put32(partition,moduleBase+0x68,7);
            put32(partition,moduleBase+0x3c,32);
            byte[] textSection=new byte[16];
            for(int i=0;i<16;i++)textSection[i]=(byte)(0x50+i);
            partition[moduleBase+0x100]=(byte)0xF0;
            partition[moduleBase+0x101]=1; // LZ4 literal length 16
            System.arraycopy(textSection,0,partition,moduleBase+0x102,16);
            byte[] rodata="HELLO-RODATA".getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(rodata,0,partition,moduleBase+0x120,12);
            byte[] packedData={0x48,'A','B','C','D',4,0};
            System.arraycopy(packedData,0,partition,moduleBase+0x140,7);
            byte[] fullData="ABCDABCDABCDABCD".getBytes(StandardCharsets.US_ASCII);
            MessageDigest hasher=MessageDigest.getInstance("SHA-256");
            System.arraycopy(hasher.digest(textSection),0,partition,moduleBase+0xA0,32);
            hasher.reset();
            System.arraycopy(hasher.digest(rodata),0,partition,moduleBase+0xC0,32);
            hasher.reset();
            System.arraycopy(hasher.digest(fullData),0,partition,moduleBase+0xE0,32);
        }
        byte[] encryptedPart=ctrCrypt(partition,contentKey,upper,0xE00);
        System.arraycopy(encryptedPart,0,nca,0xE00,encryptedPart.length);

        MemoryReader reader=new MemoryReader();
        reader.files.put("0123456789abcdef0123456789abcdef.nca",nca);
        if(ticketRights) {
            byte[] ticket=new byte[0x400];
            // Standard RSA-2048 SHA-256 ticket signature format (TicketData at 0x140).
            put32(ticket,0,0x10004);
            System.arraycopy("Root-CA".getBytes(StandardCharsets.US_ASCII),
                0,ticket,0x140,7);
            System.arraycopy(rights,0,ticket,0x2A0,16);
            ticket[0x281]=0; // Common title key
            Cipher titleCipher=Cipher.getInstance("AES/ECB/NoPadding");
            titleCipher.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(kek,"AES"));
            System.arraycopy(titleCipher.doFinal(contentKey),0,ticket,0x180,16);
            reader.files.put("0123456789abcdef0123456789abcdef.tik",ticket);
        }
        String keys="header_key = "+hex(headerKey)+"\n"+
            (ticketRights?"titlekek_00 = ":"key_area_key_application_00 = ")+hex(kek)+"\n";
        Arrays.fill(headerKey,(byte)0);
        Arrays.fill(contentKey,(byte)0);
        Arrays.fill(kek,(byte)0);
        return new Fixture(reader,keys);
    }

    public static void main(String[] args) throws Exception {
        Fixture common=make(true);
        String report=common.probe();
        assertThat(report.contains("NCA PROGRAM coincidentes: 1"),"finds synthetic program");
        assertThat(report.contains("Claves de secciones resueltas: 1"),"titlekey from synthetic ticket");
        assertThat(report.contains("PartitionFS leídos: 1"),"decrypts synthetic PFS0");
        assertThat(report.contains("main: firma NSO0 válida"),"main header via CTR");
        assertThat(report.contains("sdk: firma NSO0 válida"),"sdk header via CTR");
        assertThat(report.contains("Módulos NSO0 detectados: 3"),"two synthetic NSO headers");
        assertThat(report.contains("main: Validacion de segmentos NSO0:") &&
                   report.contains("sdk: Validacion de segmentos NSO0:"),
            "checks both modules' decoded SHA-256 values");
        assertThat(report.split("Segmentos verificados: 3/3", -1).length==4,
            "main sdk and rtld each have three verified LZ4/raw sections");
        assertThat(report.contains("rtld: firma NSO0 válida") &&
                   report.contains("rtld: Imagen NSO temporal construida y verificada"),
            "executable runtime linker candidate is discovered and mapped transiently");
        assertThat(report.contains("main: Imagen NSO temporal construida y verificada") &&
                   report.contains("sdk: Imagen NSO temporal construida y verificada"),
            "both modules mapped into temporary nonexecutable memory");
        assertThat(report.contains("BSS 32 bytes") &&
                   report.contains("no se aplicaron relocaciones ni se ejecuto codigo"),
            "BSS and explicit non-execution noted in read-only report");

        assertThat(report.contains("main: cabecera NSO0 estructuralmente coherente"),
            "validates main segment layout from mounted encrypted NSP");
        assertThat(report.contains("sdk: cabecera NSO0 estructuralmente coherente"),
            "validates sdk segment layout from mounted encrypted NSP");
        assertThat(!report.contains(hex(new byte[]{0x70,0x71,0x72})),"no key values in report");
        assertThat(report.contains("NO ES ZELDA JUGABLE"),"does not pretend to run BOTW");

        Fixture keyArea=make(false);
        String alternate=keyArea.probe();
        assertThat(alternate.contains("Módulos NSO0 detectados: 3"),
            "key area AES-ECB resolution alternative works");

        Fixture wrong=make(true);
        wrong.reader.files.remove("0123456789abcdef0123456789abcdef.tik");
        String withoutTicket=wrong.probe();
        assertThat(withoutTicket.contains("no contiene .tik") &&
            withoutTicket.contains("PartitionFS leídos: 0"),
            "missing ticket must not fake content decryption");

        Fixture wrongId=make(true);
        byte[] wrongTicket=wrongId.reader.files.get("0123456789abcdef0123456789abcdef.tik");
        wrongTicket[0x2A0] ^= 1;
        String mismatch=wrongId.probe();
        assertThat(mismatch.contains("no coincide con los tickets compatibles") &&
            mismatch.contains("Claves de secciones resueltas: 0"),
            "safe diagnosis for mismatched ticket rights ID");

        Fixture invalidSignature=make(true);
        byte[] unsigned=invalidSignature.reader.files.get("0123456789abcdef0123456789abcdef.tik");
        put32(unsigned,0,0xFFFFFFFFL);
        String unsupported=invalidSignature.probe();
        assertThat(unsupported.contains("tipo de firma no es reconocido"),
            "unsupported ticket signature should be reported, no keys shown");

        Fixture personal=make(true);
        byte[] personalTicket=personal.reader.files.get("0123456789abcdef0123456789abcdef.tik");
        personalTicket[0x281]=1;
        String personalReport=personal.probe();
        assertThat(personalReport.contains("ticket coincidente, pero personalizado"),
            "personalized ticket must not be treated as common");

        Fixture invalidSegment=make(true);
        byte[] tampered=invalidSegment.reader.files.get(
            "0123456789abcdef0123456789abcdef.nca");
        int plainTextOffset=0xE00+(16+3*24+14)+0x100+2;
        tampered[plainTextOffset] ^= 1; // CTR plaintext byte, only text hash fails
        String brokenHash=invalidSegment.probe();
        assertThat(brokenHash.contains(".text: SHA-256 NO COINCIDE") &&
                   brokenHash.contains("Módulos NSO0 detectados: 3"),
            "tampered NSO content must not pass segment digest");

        Fixture corrupted=make(true);
        byte[] bytes=corrupted.reader.files.get("0123456789abcdef0123456789abcdef.nca");
        bytes[0xE00] ^= 1;
        String badReport=corrupted.probe();
        assertThat(badReport.contains("PartitionFS leídos: 0"),
            "tampered partition header cannot count as PFS0");
        System.out.println("PASS: mounted NSP -> ticket / key area -> AES-CTR -> PFS0 -> synthetic main/sdk NSO0");
    }
}
