package org.example.botwlab;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Module structure tests with entirely synthetic NSO0 headers; no Zelda data. */
public final class SwitchNsoMetadataAuditTest {
    private static void write32(byte[] b,int at,long value) {
        for(int i=0;i<4;i++) b[at+i]=(byte)(value >>> (i*8));
    }
    private static void required(boolean condition,String message) {
        if(!condition) throw new AssertionError(message);
    }
    private static byte[] fixture() {
        byte[] header=new byte[0x100];
        System.arraycopy("NSO0".getBytes(StandardCharsets.US_ASCII),0,header,0,4);
        write32(header,4,0);
        write32(header,0xc,1|8|16); // text compressed, text/rodata hashes declared
        write32(header,0x10,0x100);write32(header,0x14,0);write32(header,0x18,0x20);
        write32(header,0x20,0x120);write32(header,0x24,0x100);write32(header,0x28,0x10);
        write32(header,0x30,0x130);write32(header,0x34,0x200);write32(header,0x38,0x10);
        write32(header,0x3c,0x20);
        write32(header,0x60,0x10);write32(header,0x64,0x10);write32(header,0x68,0x10);
        return header;
    }
    public static void main(String[] args) {
        byte[] header=fixture();
        SwitchNsoMetadataAudit.Result result=SwitchNsoMetadataAudit.inspect(header,0x200);
        required(result.valid,"valid synthetic NSO0 metadata");
        required(result.report.contains(".text: 32 B") &&
                 result.report.contains(".rodata: 16 B") &&
                 result.report.contains(".data: 16 B"),
                 "all segment lengths recognized");
        required(result.report.contains("SHA-256 declarado (no verificado)"),
                 "hash requirement is not a hash verification");
        required(!result.report.contains("NSO0 módulo ejecutado"),
                 "never claim that game instructions executed");
        required(!SwitchNsoMetadataAudit.inspect(header,0xff).valid,
                 "reject module smaller than header");
        byte[] bad=header.clone();
        bad[0]='X';
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,"reject invalid signature");
        bad=header.clone();
        write32(bad,0x10,0x1F8);
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,
                 "reject segment beyond module end");
        bad=header.clone();
        write32(bad,0x20,0x108);
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,
                 "reject overlapping file segments");
        bad=header.clone();
        write32(bad,0x24,0x10);
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,
                 "reject overlapping in-memory sections");
        bad=header.clone();
        write32(bad,0x18,0x11000000L);
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,
                 "reject excessive uncompressed segment size");
        bad=header.clone();
        write32(bad,0x3c,0x30000000L);
        required(!SwitchNsoMetadataAudit.inspect(bad,0x200).valid,
                 "reject excessive BSS memory");
        bad=header.clone();
        write32(bad,0x0c,1|8|16|0x80);
        required(SwitchNsoMetadataAudit.inspect(bad,0x200).report.contains(
                  "bandera ZBIC detectada"),"flag unsupported loader feature");
        Arrays.fill(header,(byte)0);
        System.out.println("PASS: NSO0 streaming metadata audit with synthetic segments, bounds and no game execution");
    }
}
