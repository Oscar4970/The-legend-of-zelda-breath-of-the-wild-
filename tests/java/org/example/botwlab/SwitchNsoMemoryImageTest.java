package org.example.botwlab;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;

/** Synthetic-only AArch64 NSO0 memory-layout fixture; never executes a module. */
public final class SwitchNsoMemoryImageTest {
    private static void put32(byte[] data,int at,long n) {
        for(int i=0;i<4;i++)data[at+i]=(byte)(n>>>(8*i));
    }
    private static void check(boolean ok,String text) {
        if(!ok)throw new AssertionError(text);
    }

    private static byte[] module() throws Exception {
        byte[] file=new byte[0x200];
        System.arraycopy("NSO0".getBytes(StandardCharsets.US_ASCII),0,file,0,4);
        put32(file,0x0c,0x3d); // text/data LZ4; all sections SHA-256 hashed
        put32(file,0x10,0x100);put32(file,0x14,0);put32(file,0x18,16);
        put32(file,0x60,18);
        put32(file,0x20,0x120);put32(file,0x24,0x100);put32(file,0x28,12);
        put32(file,0x64,12);
        put32(file,0x30,0x140);put32(file,0x34,0x200);put32(file,0x38,16);
        put32(file,0x68,7);
        put32(file,0x3c,32);
        byte[] text=new byte[16];
        for(int i=0;i<text.length;i++)text[i]=(byte)(0x40+i);
        file[0x100]=(byte)0xf0;file[0x101]=1;
        System.arraycopy(text,0,file,0x102,16);
        byte[] rodata="HELLO-RODATA".getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(rodata,0,file,0x120,12);
        byte[] packed={(byte)0x48,'A','B','C','D',4,0};
        System.arraycopy(packed,0,file,0x140,7);
        byte[] expanded="ABCDABCDABCDABCD".getBytes(StandardCharsets.US_ASCII);
        MessageDigest sha=MessageDigest.getInstance("SHA-256");
        System.arraycopy(sha.digest(text),0,file,0xa0,32);
        System.arraycopy(sha.digest(rodata),0,file,0xc0,32);
        System.arraycopy(sha.digest(expanded),0,file,0xe0,32);
        return file;
    }

    private static SwitchNsoSectionHashes.Source source(byte[] file) {
        return (offset,count) -> {
            if(offset<0 || count<0 || offset>file.length ||
               count>file.length-offset)
                throw new IOException("Read outside synthetic module");
            return Arrays.copyOfRange(file,(int)offset,(int)offset+count);
        };
    }

    public static void main(String[] args) throws Exception {
        byte[] file=module();
        byte[] header=Arrays.copyOf(file,0x100);
        SwitchNsoMemoryImage mapped=SwitchNsoMemoryImage.prepare(
            header,file.length,source(file));
        check(mapped.base==0,"memory origin at NSO .text virtual offset");
        check(mapped.memory.length==0x230,"data plus BSS plus address gaps");
        check(mapped.verified==3,"all decompressed hashes verified");
        check(mapped.bssOffset==0x210 && mapped.bssSize==32,
              "BSS immediately after .data");
        for(int i=0;i<16;i++)
            check(mapped.memory[i]==(byte)(0x40+i),"decoded .text mapped");
        for(int i=0x10;i<0x100;i++)
            check(mapped.memory[i]==0,"memory gap must remain zero");
        check(new String(mapped.memory,0x100,12,StandardCharsets.US_ASCII)
                  .equals("HELLO-RODATA"),"rodata memory mapping");
        check(new String(mapped.memory,0x200,16,StandardCharsets.US_ASCII)
                  .equals("ABCDABCDABCDABCD"),"overlapping LZ4 data mapped");
        for(int i=0x210;i<0x230;i++)
            check(mapped.memory[i]==0,"BSS zero-filled");
        check(mapped.summary().contains("3/3"),"report verified segments");
        mapped.close();
        for(byte b:mapped.memory)check(b==0,"temporary NSO memory wiped on close");
        mapped.close(); // Idempotent.

        byte[] wrong=module();
        wrong[0x102]^=1;
        try {
            SwitchNsoMemoryImage.prepare(
                Arrays.copyOf(wrong,0x100),wrong.length,source(wrong)).close();
            throw new AssertionError("corrupt segment unexpectedly mapped");
        } catch(IOException expected) {
            check(expected.getMessage().contains("SHA-256"),
                  "altered module must fail SHA-256 verification");
        }

        byte[] badHash=module();
        put32(badHash,0x0c,0x35); // no hash on first section
        try {
            SwitchNsoMemoryImage.prepare(
                Arrays.copyOf(badHash,0x100),badHash.length,source(badHash)).close();
            throw new AssertionError("unverified section unexpectedly accepted");
        } catch(IOException expected) {
            check(expected.getMessage().contains("hash ausente"),
                  "missing section digest must reject mapping");
        }

        System.out.println("PASS: bounded verified NSO0 in-memory layout, LZ4 sections, zero BSS, wipe-on-close and corruption rejection");
    }
}
