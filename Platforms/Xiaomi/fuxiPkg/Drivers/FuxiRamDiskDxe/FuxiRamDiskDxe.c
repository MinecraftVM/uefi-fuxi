/** @file
  Expose the UniXNU payload volume as a block device.

  `fastboot boot` loads the payload FAT volume into RAM, but nothing tells this
  firmware where. Scan the UEFI memory map for a FAT boot sector labelled
  FUXIESP and publish it through EFI_BLOCK_IO_PROTOCOL; the standard FAT driver
  then mounts it and the boot manager sees an ordinary file system.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#define FUXI_LABEL_OFFSET  0x47
#define FUXI_LABEL_LENGTH  7
#define FUXI_BLOCK_SIZE    512

STATIC EFI_BLOCK_IO_MEDIA     mMedia;
STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo;
STATIC UINT8                  *mVolumeBase;
STATIC BOOLEAN                mLastLabeled;
STATIC UINTN                  mVolumeBytes;

STATIC
BOOLEAN
Scannable (
  IN EFI_MEMORY_TYPE  Type
  )
{
  // The bootloader describes the ramdisk however it likes; only MMIO windows
  // are unsafe to read.
  return (Type != EfiMemoryMappedIO) && (Type != EfiMemoryMappedIOPortSpace) &&
         (Type != EfiPalCode) && (Type != EfiUnusableMemory);
}

STATIC
BOOLEAN
IsPayloadVolume (
  IN CONST UINT8  *Sector,
  OUT UINT64      *TotalSectors
  )
{
  UINT16  BytesPerSector;
  UINT32  SectorsPerFat;
  UINT32  Total32;
  UINT16  Total16;

  if ((Sector[510] != 0x55) || (Sector[511] != 0xAA)) {
    return FALSE;
  }

  // Prefer the expected label, but accept any plausible FAT volume so a
  // differently labelled payload still mounts.
  mLastLabeled = (CompareMem (Sector + FUXI_LABEL_OFFSET, "FUXIESP", FUXI_LABEL_LENGTH) == 0);

  BytesPerSector = (UINT16)(Sector[11] | (Sector[12] << 8));
  if (BytesPerSector != FUXI_BLOCK_SIZE) {
    return FALSE;
  }

  Total16 = (UINT16)(Sector[19] | (Sector[20] << 8));
  Total32 = (UINT32)(Sector[32] | (Sector[33] << 8) | (Sector[34] << 16) | ((UINT32)Sector[35] << 24));
  *TotalSectors = (Total16 != 0) ? Total16 : Total32;
  if (*TotalSectors == 0) {
    return FALSE;
  }

  SectorsPerFat = (UINT32)(Sector[36] | (Sector[37] << 8) | (Sector[38] << 16) | ((UINT32)Sector[39] << 24));
  if (SectorsPerFat == 0) {
    return FALSE;
  }

  //
  // Only a large volume can be the payload; this rejects stray boot sectors in
  // unrelated RAM without trusting the label alone.
  //
  if (*TotalSectors < (64ULL * 1024 * 1024 / FUXI_BLOCK_SIZE)) {
    return FALSE;
  }

  return TRUE;
}

STATIC
EFI_STATUS
EFIAPI
FuxiBlockReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
FuxiBlockRead (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  OUT VOID                  *Buffer
  )
{
  UINT64  Offset;
  UINT64  End;

  if ((MediaId != 0) && (MediaId != This->Media->MediaId)) {
    return EFI_MEDIA_CHANGED;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if ((BufferSize % FUXI_BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Offset = Lba * FUXI_BLOCK_SIZE;
  End    = Offset + BufferSize;
  if ((End < Offset) || (End > mVolumeBytes)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mVolumeBase + Offset, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
FuxiBlockWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
FuxiBlockFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
BOOLEAN
LocatePayload (
  VOID
  )
{
  EFI_STATUS             Status;
  EFI_MEMORY_DESCRIPTOR  *Map;
  UINTN                  MapSize;
  UINTN                  MapKey;
  UINTN                  DescSize;
  UINT32                 DescVersion;
  UINTN                  Index;
  UINTN                  Count;
  UINT64                 Address;
  UINT64                 TotalSectors;
  UINT8                  *Page;
  UINT64                 RegionEnd;

  MapSize = 0;
  gBS->GetMemoryMap (&MapSize, NULL, &MapKey, &DescSize, &DescVersion);
  MapSize += 16 * DescSize;
  Map = AllocatePool (MapSize);
  if (Map == NULL) {
    return FALSE;
  }

  Status = gBS->GetMemoryMap (&MapSize, Map, &MapKey, &DescSize, &DescVersion);
  if (EFI_ERROR (Status)) {
    FreePool (Map);
    return FALSE;
  }

  Count = MapSize / DescSize;
  Print (L"FUXIRAMDISK: scanning %lu memory regions\n", (UINT64)Count);
  for (Index = 0; Index < Count; Index++) {
    EFI_MEMORY_DESCRIPTOR  *Entry = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Index * DescSize);

    if (!Scannable (Entry->Type) || (Entry->PhysicalStart >= 0x100000000ULL)) {
      continue;
    }

    RegionEnd = Entry->PhysicalStart + (Entry->NumberOfPages << 12);

    // The bootloader may place the ramdisk on a 512-byte boundary.
    for (Address = Entry->PhysicalStart; Address + FUXI_BLOCK_SIZE <= RegionEnd; Address += FUXI_BLOCK_SIZE) {
      Page = (UINT8 *)(UINTN)Address;
      if (!IsPayloadVolume (Page, &TotalSectors)) {
        continue;
      }

      Print (L"FUXIRAMDISK: candidate at %lx, %lu sectors, labelled=%d\n",
             Address, TotalSectors, mLastLabeled ? 1 : 0);

      if (Address + TotalSectors * FUXI_BLOCK_SIZE > RegionEnd) {
        // The bootloader may have described the ramdisk as several adjacent
        // descriptors; trust the BPB size rather than dropping a real volume.
        Print (L"FUXIRAMDISK: volume at %lx crosses its descriptor, using the BPB size\n", Address);
      }

      mVolumeBase  = Page;
      mVolumeBytes = (UINTN)(TotalSectors * FUXI_BLOCK_SIZE);
      FreePool (Map);
      return TRUE;
    }
  }

  FreePool (Map);
  return FALSE;
}

EFI_STATUS
EFIAPI
FuxiRamDiskEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;

  Print (L"FUXIRAMDISK: driver entry\n");
  EFI_HANDLE                Handle;
  EFI_DEVICE_PATH_PROTOCOL  *Path;
  VENDOR_DEVICE_PATH        Vendor;

  if (!LocatePayload ()) {
    return EFI_SUCCESS;
  }

  ZeroMem (&Vendor, sizeof (Vendor));
  Vendor.Header.Type    = HARDWARE_DEVICE_PATH;
  Vendor.Header.SubType = HW_VENDOR_DP;
  SetDevicePathNodeLength (&Vendor.Header, sizeof (VENDOR_DEVICE_PATH));
  CopyGuid (&Vendor.Guid, &gEfiCallerIdGuid);

  Path = AppendDevicePathNode (NULL, (EFI_DEVICE_PATH_PROTOCOL *)&Vendor);
  if (Path == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (&mMedia, sizeof (mMedia));
  mMedia.MediaId          = 1;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = FUXI_BLOCK_SIZE;
  mMedia.IoAlign          = 0;
  mMedia.LastBlock        = (EFI_LBA)(mVolumeBytes / FUXI_BLOCK_SIZE) - 1;

  mBlockIo.Revision    = EFI_BLOCK_IO_PROTOCOL_REVISION;
  mBlockIo.Media       = &mMedia;
  mBlockIo.Reset       = FuxiBlockReset;
  mBlockIo.ReadBlocks  = FuxiBlockRead;
  mBlockIo.WriteBlocks = FuxiBlockWrite;
  mBlockIo.FlushBlocks = FuxiBlockFlush;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  &gEfiDevicePathProtocolGuid,
                  Path,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print (L"FUXIRAMDISK: payload at %lx, %lu KiB as a read-only block device\n",
         (UINT64)(UINTN)mVolumeBase, (UINT64)(mVolumeBytes / 1024));

  return EFI_SUCCESS;
}
