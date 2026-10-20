## fuxiPkg/Include/RAW.inc
## Xiaomi 13 (SM8550)
##
## sheng's RAW.inc carries the stock vendor FFS files and the N81A panel XMLs
## under their original GUIDs. fuxi has none of that yet, and the
## FUXI_VENDOR_HW = 0 build does not need it: the panel is already scanning out
## when the ABL hands over, and SimpleFbDxe publishes a GOP on top of it.
##
## When the vendor stack is imported (阶段 3+), the equivalents go here:
##   - stock DisplayDxe + the m3_38_0c_0a panel XMLs
##   - PmicDxe / ClockDxe / TLMMDxe / I2CDxe / SPIDxe blobs
##   - the FocalTech firmware `st_fts_m3.ftb` for the SPI touch driver
##
## Intentionally empty for the first bring-up.

# (no raw FFS sections yet)
