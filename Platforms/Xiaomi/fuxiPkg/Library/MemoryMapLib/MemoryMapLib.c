// SPDX-License-Identifier: BSD-3-Clause
//
// fuxiPkg/Library/MemoryMapLib — Xiaomi 13 (SM8550 / kalama)
//
// Source of truth: `uefiplatLA.cfg`, extracted from this device's
// `xbl_config_a` partition (HyperOS OS3.0.319.0.WPCCNXM). The file carries
// `MaxMemoryRegions = 74` and a `[MemoryMap]` section; the 28 regions below
// are that section verbatim. Unlike sheng's map, nothing here is reconstructed
// from Linux DTS reservations — it is the vendor's own firmware table.
//
// The UEFI allocation block is identical to sheng's at 0xA7000000, confirming
// it is a platform-wide SM8550 convention rather than a per-board choice:
//
//   0xA7000000  4 MiB  UEFI FD          <- FD_BASE / FD_SIZE for this board
//   0xA7400000  2 MiB  UEFI FD Reserved
//   0xA7600000  4 KiB  CPU Vectors
//   0xA7601000  ...    Info Blk / MMU PageTables / Log Buffer
//   0xA760D000  ...    UEFI Stack   (matches fuxi.dsc PcdCPUCoresStackBase)
//
// This file covers 0x80000000..0x100000000 only. The DRAM banks above 4 GiB
// (0x880000000 + 925 MiB, 0x8C0000000 + 3 GiB, 0x980000000 + 6 GiB, seen in
// the live FDT) are deliberately omitted: the first bring-up fits entirely in
// the 209 MiB DXE Heap at 0xBAB00000. They are added in stage 2, when the XNU
// kernel window needs real RAM.
//
// SPDX header above; original table from Qualcomm's uefiplatLA.cfg.
#include <Library/MemoryMapLib.h>

STATIC EFI_MEMORY_REGION_DESCRIPTOR gMemoryDescriptor[] = {
  // ---- firmware-owned reservations ----
  {"HobOnlyNoCacheSetting",             0x80000000, 0x01200000, HobOnlyNoCacheSetting,    MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"RSRV0",             0x81200000, 0x00800000, AddMem,   SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"XBL DT",            0x81A00000, 0x00040000, AddMem,   SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"XBL Ramdump",       0x81A40000, 0x001C0000, AddMem,   MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"AOP",               0x81C00000, 0x000A0000, AddMem,   MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"SMEM",              0x81D00000, 0x00200000, AddMem,   MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"RSRV1",             0x86E00000, 0x02A00000, AddMem,   MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"PIL Reserved",      0x89800000, 0x19280000, AddMem,   MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"Display Demura",    0xA2A80000, 0x02B00000, AddMem,   MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},
  {"RSRV2",             0xA5580000, 0x00B80000, AddMem,   MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"DBI Dump",          0xA6100000, 0x00F00000, NoHob,    MMAP_IO, INITIALIZED, Conv,  UNCACHED_UNBUFFERED_XN},
  {"TZApps Reserved",   0xD8800000, 0x08A00000, AddMem, MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},
  {"RSRV3",             0xEEB00000, 0x04B00000, AddMem,   MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},
  {"HobOnlyNoCacheSetting",             0xFF800000, 0x00800000, HobOnlyNoCacheSetting,    MEM_RES, UNCACHEABLE, Reserv, UNCACHED_UNBUFFERED_XN},

  // ---- UEFI allocation block ----
  {"UEFI FD",           0xA7000000, 0x00400000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"UEFI FD Reserved",  0xA7400000, 0x00200000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"CPU Vectors",       0xA7600000, 0x00001000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},
  {"Info Blk",          0xA7601000, 0x00001000, AddMem,   SYS_MEM, SYS_MEM_CAP, RtData, WRITE_BACK_XN},
  {"MMU PageTables",    0xA7602000, 0x00003000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"Log Buffer",        0xA7605000, 0x00008000, AddMem,   SYS_MEM, SYS_MEM_CAP, RtData, WRITE_BACK_XN},
  {"UEFI Stack",        0xA760D000, 0x00040000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"SEC Heap",          0xA764D000, 0x0008C000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"Sched Heap",        0xA76D9000, 0x00400000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"FV Region",         0xA7AD9000, 0x00400000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},
  {"UEFI RESV",         0xA7ED9000, 0x00127000, AddMem,   SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK_XN},

  // ---- handed to the next payload ----
  // 0xA8000000..0xB8000000 is where BootShim relocates the firmware volume and
  // where the XNU kernel window goes later.
  {"Kernel",            0xA8000000, 0x10000000, AddMem,   SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},

  // ---- the ABL frame buffer fuxi keeps scanning out ----
  // Identical to /reserved-memory/splash_region in the live device tree, and to
  // sheng's hand-patched dtb_node. SimpleFbDxe publishes a GOP on top of it.
  {"Display Reserved",  0xB8000000, 0x02B00000, AddMem,   MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},

  // ---- 209 MiB of general-purpose RAM for DXE ----
  {"DXE Heap",          0xBAB00000, 0x0D1C0000, AddMem,   SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
};

VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount)
{
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = ARRAY_SIZE (gMemoryDescriptor);
}
