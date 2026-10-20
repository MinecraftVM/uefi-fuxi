#!/usr/bin/env python3
"""Android boot image v3 packer (Mu-Silicium / UniXNU layout on SM8550).

Layout, verified byte-for-byte against Mu-Silicium v4.0 `Mu-qrd8550.img`
(2026-10-21):

    0x0000  header, 1580 bytes, zero-padded to PAGE (4096)
    0x1000  kernel field:
              gzip stream  = BootShim + UEFI firmware volume
              device tree  = raw FDT, appended right after the gzip stream
            kernel_size covers BOTH parts
            ramdisk
            zero padding to PAGE

Measured on the reference image:
    gzip consumed      1,074,187 bytes
    decompressed       3,145,840 bytes   (~3 MiB firmware volume)
    appended FDT         776,668 bytes   (starts with 0xd00dfeed)

This is why UniXNU's documentation says "the device tree follows the gzip
stream": BootShim locates it there, not in a separate boot-image field.

The fuxi ABL accepts this container (`fastboot boot` -> "Booting OKAY") and
rejects the legacy v0 container, so `unixnu bootimg` cannot be used as-is.

Usage:
    python mkbootimg_v3.py pack   --kernel K [--dtb D] [--ramdisk R] --out boot.img
    python mkbootimg_v3.py unpack --image boot.img --outdir DIR
    python mkbootimg_v3.py info   --image boot.img
"""
import argparse
import os
import struct
import zlib

MAGIC = b"ANDROID!"
FDT_MAGIC = b"\xd0\x0d\xfe\xed"
PAGE = 4096
HEADER_SIZE = 1580
VERSION = 3


def _header(kernel_size, ramdisk_size, os_version, cmdline):
    cmd = cmdline.encode("utf-8")[:1535]
    cmd += b"\x00" * (1536 - len(cmd))
    h = bytearray()
    h += MAGIC
    h += struct.pack("<I", kernel_size)
    h += struct.pack("<I", ramdisk_size)
    h += struct.pack("<I", os_version)
    h += struct.pack("<I", HEADER_SIZE)
    h += struct.pack("<IIII", 0, 0, 0, 0)
    h += struct.pack("<I", VERSION)
    h += cmd
    assert len(h) == HEADER_SIZE, len(h)
    return bytes(h)


def pack(kernel, dtb, ramdisk, out, cmdline="", os_version=0):
    k = open(kernel, "rb").read()
    t = open(dtb, "rb").read() if dtb else b""
    r = open(ramdisk, "rb").read() if ramdisk else b""

    if k[:2] != b"\x1f\x8b":
        print("warning: kernel is not gzip; BootShim expects a gzip stream", flush=True)
    if t and t[:4] != FDT_MAGIC:
        print("warning: dtb does not start with the FDT magic", flush=True)
    if not t:
        print("warning: no dtb given; BootShim will not find a device tree", flush=True)

    field = k + t
    hdr = _header(len(field), len(r), os_version, cmdline)
    pad = (-(len(hdr) + len(field) + len(r))) % PAGE

    with open(out, "wb") as f:
        f.write(hdr)
        f.write(b"\x00" * ((-len(hdr)) % PAGE))
        f.write(field)
        f.write(r)
        f.write(b"\x00" * pad)

    print(f"packed {out}")
    print(f"  header        {len(hdr)} (kernel at {(len(hdr) + PAGE - 1) // PAGE * PAGE:#x})")
    print(f"  gzip          {len(k)}")
    print(f"  dtb           {len(t)}")
    print(f"  kernel_size   {len(field)}")
    print(f"  ramdisk       {len(r)}")
    print(f"  padding       {pad}")
    print(f"  total         {len(hdr) + ((-len(hdr)) % PAGE) + len(field) + len(r) + pad}")


def parse(path):
    d = open(path, "rb").read()
    if d[:8] != MAGIC:
        raise SystemExit("not an ANDROID! image")
    ks, = struct.unpack_from("<I", d, 8)
    rs, = struct.unpack_from("<I", d, 12)
    osv, = struct.unpack_from("<I", d, 16)
    hs, = struct.unpack_from("<I", d, 20)
    hv, = struct.unpack_from("<I", d, 40)
    cmdline = d[44:44 + 1536].split(b"\x00")[0].decode("utf-8", "replace")
    return dict(file_size=len(d), kernel_size=ks, ramdisk_size=rs, os_version=osv,
                header_size=hs, header_version=hv, cmdline=cmdline,
                kernel_at=((hs + PAGE - 1) // PAGE) * PAGE, data=d)


def split_field(buf):
    """Split a kernel field into (gzip_bytes, dtb_bytes)."""
    if buf[:2] != b"\x1f\x8b":
        return buf, b""
    dec = zlib.decompressobj(16 + zlib.MAX_WBITS)
    dec.decompress(buf)
    used = len(buf) - len(dec.unused_data)
    return buf[:used], dec.unused_data


def unpack(image, outdir):
    i = parse(image)
    os.makedirs(outdir, exist_ok=True)
    d, off = i["data"], i["kernel_at"]
    field = d[off:off + i["kernel_size"]]
    off += i["kernel_size"]
    gz, dtb = split_field(field)
    open(os.path.join(outdir, "kernel"), "wb").write(gz)
    open(os.path.join(outdir, "dtb"), "wb").write(dtb)
    open(os.path.join(outdir, "ramdisk"), "wb").write(d[off:off + i["ramdisk_size"]])
    print(f"unpacked to {outdir}: gzip={len(gz)} dtb={len(dtb)} ramdisk={i['ramdisk_size']}")


def info(image):
    i = parse(image)
    d, off = i["data"], i["kernel_at"]
    field = d[off:off + i["kernel_size"]]
    gz, dtb = split_field(field)
    print(f"  {'header_version':16}: {i['header_version']}")
    print(f"  {'header_size':16}: {i['header_size']}  (kernel at {i['kernel_at']:#x})")
    print(f"  {'kernel_size':16}: {i['kernel_size']}  = gzip {len(gz)} + dtb {len(dtb)}")
    print(f"  {'ramdisk_size':16}: {i['ramdisk_size']}")
    print(f"  {'file_size':16}: {i['file_size']}")
    if gz:
        dec = zlib.decompressobj(16 + zlib.MAX_WBITS)
        out = dec.decompress(gz) + dec.flush()
        print(f"  {'gzip decompresses':16}: {len(out)} bytes")
    if dtb:
        m, tot = struct.unpack_from(">II", dtb, 0)[0], struct.unpack_from(">I", dtb, 4)[0]
        print(f"  {'dtb':16}: magic {m:#x} totalsize {tot} "
              f"({'valid' if m == 0xd00dfeed and tot == len(dtb) else 'MISMATCH'})")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("pack")
    p.add_argument("--kernel", required=True)
    p.add_argument("--dtb", default="")
    p.add_argument("--ramdisk", default="")
    p.add_argument("--out", required=True)
    p.add_argument("--cmdline", default="")
    p.add_argument("--os-version", type=int, default=0)

    u = sub.add_parser("unpack")
    u.add_argument("--image", required=True)
    u.add_argument("--outdir", required=True)

    n = sub.add_parser("info")
    n.add_argument("--image", required=True)

    a = ap.parse_args()
    if a.cmd == "pack":
        pack(a.kernel, a.dtb, a.ramdisk, a.out, a.cmdline, a.os_version)
    elif a.cmd == "unpack":
        unpack(a.image, a.outdir)
    else:
        info(a.image)


if __name__ == "__main__":
    main()
