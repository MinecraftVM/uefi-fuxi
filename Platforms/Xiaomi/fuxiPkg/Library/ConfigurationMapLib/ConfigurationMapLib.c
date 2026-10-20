// SPDX-License-Identifier: BSD-3-Clause
//
// fuxiPkg/Library/ConfigurationMapLib — Xiaomi 13 (SM8550 / kalama)
//
// 39 of the 40 values below are read from this device's own `uefiplatLA.cfg`
// (extracted from xbl_config_a, HyperOS OS3.0.319.0.WPCCNXM); only
// LoadAutoImageInPILFlag is absent there and falls back to sheng's value.
//
// Four keys are then deliberately overridden for first bring-up, following the
// documented sheng experience: display initialisation, image-FV loading,
// parallel decompression and multithreading were all disabled, and only one
// core was kept active while the panel and the memory map were unproven.
// NumCpus still reports the real eight-core topology.
//
//   key                        stock   here   why
//   EnableDisplayThread        0x1     0x0    BDS display thread is a QcomBds
//                                              feature this FV does not ship
//   EnableMultiThreading       0x1     0x0    single core during bring-up
//   NumActiveCores             0x8     0x1    idem
//   (NumCpus                   0x8     0x8    real topology, informational)
//
// Restore the stock values only after the menu has been observed on hardware.
#include <Library/ConfigurationMapLib.h>

STATIC
EFI_CONFIGURATION_ENTRY_DESCRIPTOR
gConfigurationDescriptor[] = {
  {"AllowNonPersistentVarsInRetail", 0x1},
  {"DDRInfoNotifyFlag", 0x0},
  {"DRAM_CLK_PERIOD_ADDR", 0x240BA050},
  {"DetectRetailUserAttentionHotkey", 0x0},
  {"DetectRetailUserAttentionHotkeyCode", 0x17},
  {"DloadCookieAddr", 0x1FD9000},
  {"DloadCookieValue", 0x10},
  {"EarlyInitCoreCnt", 0x1},
  {"EnableACPIFallback", 0x0},
  {"EnableDisplayImageFv", 0x0},
  {"EnableDisplayThread", 0x0},          // override: stock 0x1
  {"EnableLogFsSyncInRetail", 0x0},
  {"EnableMultiCoreFvDecompression", 0x0},
  {"EnableMultiThreading", 0x0},         // override: stock 0x1
  {"EnableOEMSetupAppInRetail", 0x0},
  {"EnableSDHCSwitch", 0x1},
  {"EnableShell", 0x1},
  {"EnableUefiSecAppDebugLogDump", 0x0},
  {"EnableUfsIOC", 0x1},
  {"EnableVariablePolicyEngine", 0x0},
  {"LoadAutoImageInPILFlag", 0x1},       // fallback: not present in fuxi cfg
  {"MaxCoreCnt", 0x8},
  {"MaxLogFileSize", 0x400000},
  {"NumActiveCores", 0x1},               // override: stock 0x8
  {"NumCpus", 0x8},
  {"PilSubsysDbgCookieAddr", 0x146AA6DC},
  {"PilSubsysDbgCookieVal", 0x53444247},
  {"PwrBtnShutdownFlag", 0x0},
  {"Sdc1GpioConfigOff", 0xA00},
  {"Sdc1GpioConfigOn", 0x1E92},
  {"Sdc2GpioConfigOff", 0xA00},
  {"Sdc2GpioConfigOn", 0x1E92},
  {"SecPagePoolCount", 0x800},
  {"SecurityFlag", 0xC4},
  {"SharedIMEMBaseAddr", 0x146AA000},
  {"ShmBridgememSize", 0xA00000},
  {"USBHS1_Config", 0x0},
  {"UefiMemUseThreshold", 0xE1},
  {"UfsSmmuConfigForOtherBootDev", 0x1},
  {"UsbFnIoRevNum", 0x10001}
};

VOID
GetConfigurationMap (
  OUT EFI_CONFIGURATION_ENTRY_DESCRIPTOR **ConfigurationDescriptor,
  OUT UINT8                               *ConfigurationDescriptorCount)
{
  *ConfigurationDescriptor      = gConfigurationDescriptor;
  *ConfigurationDescriptorCount = ARRAY_SIZE (gConfigurationDescriptor);
}
