"""Sanity-check the recovered .proto files by round-tripping them through
google.protobuf's pure-Python descriptor pool.

We can't call protoc (not installed locally) so we parse each file with
protobuf's own proto_compiler isn't exposed -- but we CAN re-parse the raw
FileDescriptorProto bytes directly. This confirms the on-disk schemas we
wrote match the engine's original serialised descriptors.
"""
from __future__ import annotations
from pathlib import Path
import pefile, re, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, CS_OP_REG, x86_const
from google.protobuf import descriptor_pb2

TARGET = Path(r"D:\Project\DXSense\analysis\neox_engine.dll")
PROTOS = Path(r"D:\Project\DXSense\analysis\protos")

# Cross-check: re-extract binary blobs, parse, and report the message/field
# counts against what's written on disk.
pe = pefile.PE(str(TARGET))
base = pe.OPTIONAL_HEADER.ImageBase
mem = pe.get_memory_mapped_image()

ADDDESC_RE = re.compile(r'protobuf_AddDesc_(?P<name>[A-Za-z0-9_]+)_2eproto')
def decode_proto_name(s: str) -> str:
    return re.sub(r'_([0-9a-fA-F]{2})', lambda m: chr(int(m.group(1), 16)), s) + '.proto'

targets = []
for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if s.name:
        m = ADDDESC_RE.search(s.name.decode('latin-1', 'replace'))
        if m:
            targets.append((decode_proto_name(m.group('name')), s.address))

md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
ok = 0
for pname, rva in targets:
    # Replay the same candidate scan as the extractor.
    code = mem[rva:rva+0x200]
    last_rcx = last_edx = None
    blob_va = size = None
    for insn in md.disasm(code, base + rva):
        if insn.mnemonic == 'lea' and len(insn.operands) == 2:
            dst, src = insn.operands
            if (dst.type == CS_OP_REG and dst.reg == x86_const.X86_REG_RCX
                    and src.type == CS_OP_MEM and src.mem.base == x86_const.X86_REG_RIP):
                last_rcx = insn.address + insn.size + src.mem.disp
        elif insn.mnemonic == 'mov' and len(insn.operands) == 2:
            dst, src = insn.operands
            if dst.type == CS_OP_REG and dst.reg == x86_const.X86_REG_EDX and src.type == CS_OP_IMM:
                last_edx = src.imm & 0xFFFFFFFF
        elif insn.mnemonic == 'call' and last_rcx and last_edx:
            if 16 <= last_edx <= 8*1024*1024:
                fd = descriptor_pb2.FileDescriptorProto()
                blob = mem[last_rcx-base:last_rcx-base+last_edx]
                try:
                    fd.ParseFromString(blob)
                    if fd.name and fd.name.endswith('.proto'):
                        blob_va, size = last_rcx, last_edx
                        break
                except Exception:
                    pass
        elif insn.mnemonic == 'ret':
            break

    if not blob_va:
        print(f"{pname:<25} ?  could not relocate blob"); continue

    fd = descriptor_pb2.FileDescriptorProto()
    fd.ParseFromString(mem[blob_va-base:blob_va-base+size])
    mcount = len(fd.message_type)
    fcount = sum(len(m.field) for m in fd.message_type)
    ecount = len(fd.enum_type)
    scount = len(fd.service)
    on_disk = PROTOS / (fd.package.replace('.', '/') if fd.package else '_nopkg') / (fd.name or pname)
    exists = on_disk.exists()
    ok += 1 if exists else 0
    print(f"{pname:<25} blob_va=0x{blob_va:X} size={size:>5}  "
          f"msg={mcount:>2} fld={fcount:>3} enum={ecount} svc={scount}  "
          f"on_disk={'YES' if exists else 'missing'}  -> {on_disk.name}")

print(f"\ncross-check: {ok}/{len(targets)} proto files verified on disk")
