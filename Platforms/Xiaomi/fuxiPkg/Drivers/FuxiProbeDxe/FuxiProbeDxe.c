/** @file
  Bring-up probe: locate the UniXNU payload volume in physical memory.

  `fastboot boot` loads the ramdisk somewhere, but nothing tells this firmware
  where. The payload is a FAT32 volume labelled FUXIESP, so scan the low
  physical window and report every candidate.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>

#define FUXI_SCAN_START  0x08000000ULL
#define FUXI_SCAN_END    0x40000000ULL
#define FUXI_SCAN_STEP   0x00001000ULL

STATIC
BOOLEAN
LooksLikePayload (
  IN CONST UINT8  *Page
  )
{
  if ((Page[510] != 0x55) || (Page[511] != 0xAA)) {
    return FALSE;
  }

  return (CompareMem (Page + 0x47, "FUXIESP", 7) == 0);
}

EFI_STATUS
EFIAPI
FuxiProbeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT64  Address;
  UINT8   *Page;
  UINTN   Hits;
  UINT64  FirstHit;
  UINT8   Label[12];

  Print (L"FUXIPROBE: scanning for FUXIESP\n");

  Hits     = 0;
  FirstHit = 0;

  for (Address = FUXI_SCAN_START; Address < FUXI_SCAN_END; Address += FUXI_SCAN_STEP) {
    Page = (UINT8 *)(UINTN)Address;
    if (!LooksLikePayload (Page)) {
      continue;
    }

    CopyMem (Label, Page + 0x47, 11);
    Label[11] = 0;
    Print (L"FUXIPROBE: hit at %lx label=%a\n", Address, Label);

    if (Hits == 0) {
      FirstHit = Address;
    }

    Hits++;
    if (Hits >= 8) {
      break;
    }
  }

  if (Hits == 0) {
    Print (L"FUXIPROBE: no FUXIESP volume in the scanned window\n");
  } else {
    Print (L"FUXIPROBE: %lu hits, first at %lx\n", (UINT64)Hits, FirstHit);
  }

  return EFI_SUCCESS;
}
