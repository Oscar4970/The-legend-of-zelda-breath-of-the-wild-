#!/usr/bin/env python3
"""Pack independently authored freestanding AArch64 ELF into NSO-format recompiler fixture.

NOT a Nintendo Switch title or fully fledged NSO packer, loader, decryptor or game.
The generated .text, .rodata and .data come exclusively from our own C source.
"""
import argparse
from pathlib import Path
import struct

def parse_elf(elf):
    if len(elf)<64 or elf[:4]!=b'\x7fELF' or elf[4:6]!=b'\x02\x01':
        raise ValueError('ELF64 little-endian required')
    if struct.unpack_from('<H',elf,18)[0]!=183:
        raise ValueError('AArch64 ELF required')
    shoff=struct.unpack_from('<Q',elf,40)[0]
    entsize,n,shstr=(struct.unpack_from('<H',elf,p)[0] for p in (58,60,62))
    if n<4 or entsize<64 or shoff+n*entsize>len(elf) or shstr>=n:
        raise ValueError('Invalid ELF section table')
    sections=[]
    for i in range(n):
        name,typ,flags,addr,off,size,link,info,align,ent=struct.unpack_from(
            '<IIQQQQIIQQ',elf,shoff+i*entsize)
        if typ!=8 and (off>len(elf) or size>len(elf)-off):
            raise ValueError('Invalid ELF section span')
        sections.append(dict(nameidx=name,typ=typ,addr=addr,size=size,link=link,
                             entsize=ent,blob=elf[off:off+size] if typ!=8 else b''))
    names=sections[shstr]['blob']
    for s in sections:
        start=s['nameidx']
        if start>=len(names):raise ValueError('Invalid ELF section name')
        end=names.find(b'\0',start)
        if end<0:raise ValueError('Unterminated ELF section name')
        s['name']=names[start:end].decode('ascii')
    return sections

def symbols(sections):
    table=next(s for s in sections if s['name']=='.symtab')
    strings=sections[table['link']]['blob']
    if table['entsize']!=24 or len(table['blob'])%24:raise ValueError('Invalid ELF symbols')
    out={}
    for i in range(0,len(table['blob']),24):
        name,info,other,sec,val,size=struct.unpack_from('<IBBHQQ',table['blob'],i)
        if name>=len(strings):raise ValueError('Invalid symbol string offset')
        end=strings.find(b'\0',name)
        if end<0:raise ValueError('Invalid symbol string')
        label=strings[name:end].decode('ascii')
        if label in ('demo_entry','mix','guest_scale','guest_bias','guest_counter'):
            out[label]=(info,sec,val,size)
    return out

def build(elf):
    sections=parse_elf(elf); parts={x['name']:x for x in sections}
    code,data,bss=(parts[x] for x in ('.text','.data','.bss'))
    rodata=parts.get('.rodata')
    if code['addr']!=0x40 or not 0<code['size']<=0x3c0:
        raise ValueError('Unexpected text address/size')
    if data['addr']!=0x2000 or data['size']>0x100:
        raise ValueError('Unexpected data address/size')
    if bss['addr']!=0x2100 or bss['size']>0x100:
        raise ValueError('Unexpected bss address/size')
    if rodata and (rodata['addr']!=0x1000 or rodata['size']>0x100):
        raise ValueError('Unexpected rodata address/size')
    names=symbols(sections)
    if set(names)!={'demo_entry','mix','guest_scale','guest_bias','guest_counter'}:
        raise ValueError('Missing C functions or global variables')
    values={name:record[2] for name,record in names.items()}
    if values['demo_entry']!=0x40 or not 0x40<values['mix']<0x400:
        raise ValueError('Unexpected function locations')
    if not 0x2000<=values['guest_scale']<0x2100 or not 0x2100<=values['guest_counter']<0x2200:
        raise ValueError('Unexpected global variables')
    image=bytearray(0x2200)
    image[0x40:0x40+code['size']]=code['blob']
    if rodata:image[0x1000:0x1000+rodata['size']]=rodata['blob']
    image[0x2000:0x2000+data['size']]=data['blob']
    struct.pack_into('<I',image,4,0x20)
    image[0x20:0x24]=b'MOD0'
    struct.pack_into('<iiiii',image,0x24,0x1100-0x20,0x2100-0x20,0x2200-0x20,-0x20,-0x20)
    symbols_sorted=sorted(((n,record) for n,record in names.items()
                           if n in ('demo_entry','mix')),key=lambda x:x[1][2])
    sym_list=[b'\0'*24]
    strings=bytearray(b'\0')
    for name,(info,sec,addr,size) in symbols_sorted:
        off=len(strings);strings+=name.encode('ascii')+b'\0'
        sym_list.append(struct.pack('<IBBHQQ',off,0x12,0,1,addr,size))
    symtab=0x1200
    strtab=symtab+24*len(sym_list)
    if strtab+len(strings)>=0x1600:raise ValueError('Symbol table overflow')
    for i,row in enumerate(sym_list):image[symtab+i*24:symtab+(i+1)*24]=row
    image[strtab:strtab+len(strings)]=strings
    for i,(tag,val) in enumerate(((5,strtab),(6,symtab),(11,24),(10,len(strings)),(0,0))):
        struct.pack_into('<qQ',image,0x1100+i*16,tag,val)
    txt,ro,dat=bytes(image[:code['addr']+code['size']]),bytes(image[0x1000:0x1600]),bytes(image[0x2000:0x2100])
    payloads=(txt,ro,dat);header=bytearray(0x100);header[:4]=b'NSO0'
    cursor=0x100
    for i,(addr,payload) in enumerate(zip((0,0x1000,0x2000),payloads)):
        struct.pack_into('<III',header,0x10+16*i,cursor,addr,len(payload))
        struct.pack_into('<I',header,0x60+4*i,len(payload))
        cursor+=len(payload)
    struct.pack_into('<I',header,0x3c,0x100)
    return bytes(header)+b''.join(payloads),payloads,values

def header_file(path,segments,values):
    def array(name,blob):
        lines=[f'inline constexpr unsigned char {name}[] = {{']
        lines+=['    '+', '.join(f'0x{n:02x}' for n in blob[k:k+16])+','
                for k in range(0,len(blob),16)]
        return '\n'.join(lines+['};'])
    output=['#pragma once','#include <cstdint>','#include <cstddef>',
            '// Generated from C program written for this test; not Nintendo code.',
            'namespace homebrew_fixture {',
            'inline constexpr uintptr_t kTextOffset = 0;',
            'inline constexpr uintptr_t kRoOffset = 0x1000;',
            'inline constexpr uintptr_t kDataOffset = 0x2000;',
            'inline constexpr uintptr_t kBssOffset = 0x2100;',
            'inline constexpr size_t kMapBytes = 0x4000;',
            f'inline constexpr uintptr_t kEntryOffset = {values["demo_entry"]:#x};',
            f'inline constexpr uintptr_t kMixOffset = {values["mix"]:#x};']
    output += [array(name,blob) for name,blob in zip(('kText','kRo','kData'),segments)]
    output.append('} // namespace homebrew_fixture')
    path.parent.mkdir(exist_ok=True,parents=True)
    path.write_text('\n'.join(output)+'\n',encoding='utf-8')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('elf',type=Path);p.add_argument('nso',type=Path)
    p.add_argument('header',type=Path)
    args=p.parse_args()
    blob,segments,values=build(args.elf.read_bytes())
    args.nso.parent.mkdir(parents=True,exist_ok=True)
    args.nso.write_bytes(blob)
    header_file(args.header,segments,values)
    print(f'PASS: cross-compiled C ELF -> NSO-format fixture, {len(blob)} bytes')
    print(f'ELF ARM64 symbols: {values}')

if __name__=='__main__':
    main()
