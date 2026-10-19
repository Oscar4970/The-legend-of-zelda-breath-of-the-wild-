package org.example.botwlab;

import java.util.Arrays;

/** Test-only synthetic Switch MOD0, ELF64 dynamic metadata and RELA records. */
public final class SwitchNsoMod0AuditTest {
    private static void p32(byte[] b,int offset,long x) {
        for(int i=0;i<4;i++)b[offset+i]=(byte)(x>>>(8*i));
    }
    private static void p64(byte[] b,int offset,long x) {
        for(int i=0;i<8;i++)b[offset+i]=(byte)(x>>>(8*i));
    }
    private static void needed(boolean value,String why) {
        if(!value)throw new AssertionError(why);
    }
    private static byte[] fixture() {
        byte[] memory=new byte[0x400];
        p32(memory,4,0x20);
        memory[0x20]='M';memory[0x21]='O';memory[0x22]='D';memory[0x23]='0';
        p32(memory,0x24,0x40); // dynamic at MOD0 + 0x40 = 0x60.
        p32(memory,0x28,0x2e0); // BSS VA 0x300.
        p32(memory,0x2c,0x320); // BSS end VA 0x340.
        int d=0x60;
        p64(memory,d,7);p64(memory,d+8,0x160);d+=16; // DT_RELA
        p64(memory,d,8);p64(memory,d+8,48);d+=16;    // DT_RELASZ
        p64(memory,d,9);p64(memory,d+8,24);d+=16;    // DT_RELAENT
        p64(memory,d,1);p64(memory,d+8,0x20);d+=16;  // DT_NEEDED
        p64(memory,d,0x6ffffff9L);p64(memory,d+8,1);d+=16;
        p64(memory,d,0);p64(memory,d+8,0); // DT_NULL
        p64(memory,0x160,0x280);p64(memory,0x168,1027);p64(memory,0x170,0x250);
        p64(memory,0x178,0x288);p64(memory,0x180,(3L<<32)|1025);p64(memory,0x188,0);
        return memory;
    }
    public static void main(String[] args) {
        byte[] good=fixture(),copy=good.clone();
        SwitchNsoMod0Audit.Result result=SwitchNsoMod0Audit.inspect(good,0,0x300,0x40);
        needed(result.found && result.valid,"recognize synthetically valid MOD0");
        needed(result.report.contains("ELF64 RELA: 2") &&
               result.report.contains("R_AARCH64_RELATIVE: 1") &&
               result.report.contains("dependientes de simbolos: 1"),
               "separate relative and unresolved relocations");
        needed(result.report.contains("Bibliotecas DT_NEEDED: 1"),
               "count declared library dependencies only");
        needed(Arrays.equals(good,copy),"read-only inspection must not patch code");
        byte[] absent=fixture();absent[0x20]='X';
        needed(!SwitchNsoMod0Audit.inspect(absent,0,0x300,0x40).found,
               "reject absent MOD0 without false relocation counts");
        byte[] unsafeTarget=fixture();p64(unsafeTarget,0x160,0xFF000000L);
        SwitchNsoMod0Audit.Result bad=SwitchNsoMod0Audit.inspect(unsafeTarget,0,0x300,0x40);
        needed(!bad.valid && bad.report.contains("destinos fuera de imagen: 1"),
               "detect unsafe relocation destination");
        byte[] brokenTable=fixture();p64(brokenTable,0x68,0x3f0);
        needed(!SwitchNsoMod0Audit.inspect(brokenTable,0,0x300,0x40).valid,
               "reject out-of-range ELF64 relocation table");
        byte[] badWidth=fixture();p64(badWidth,0x88,16);
        needed(!SwitchNsoMod0Audit.inspect(badWidth,0,0x300,0x40).valid,
               "reject nonstandard ELF64_RELAENT");
        byte[] negativeDynamic=fixture();p32(negativeDynamic,0x24,0xfffffff0L);
        needed(!SwitchNsoMod0Audit.inspect(negativeDynamic,0,0x300,0x40).valid,
               "reject invalid signed dynamic offset");
        needed(!SwitchNsoMod0Audit.inspect(good,0,0x300,0x41).valid,
               "mark MOD0 versus image BSS mismatch");
        System.out.println("PASS: synthetic MOD0, dynamic tables, bounded relocations, BSS metadata, read-only audit");
    }
}
