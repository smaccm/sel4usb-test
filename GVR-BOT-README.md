# Running sel4 on GVR-BOT

The GVR-BOT is set up to boot via EFI.  This document explains the changes
that need to be made to the kernel to support EFI boot, and how to get seL4
booting on the GVR-BOT.

## 1. Modify the seL4 kernel
  
Find the function `BOOT_CODE static acpi_rsdp_t* acpi_get_rsdp(void)`
in `src/plat/pc99/machine/acpi.c` (Should be around line 173, depending on the
kernel version).

Change the code:
```
    -  for (addr = (char*)BIOS_PADDR_START; addr < (char*)BIOS_PADDR_END; addr += 16) {
    +  for (addr = (char*)0; addr < (char*)PPTR_BASE; addr += 16) {
```

## 2. Build your image as usual

## 3. Copy the seL4 images to the GVR-BOT

 1. Start the pre-installed Ubuntu
 2. Copy the seL4 images to the GVRBot using USB storage or SSH.
    There are two images (in directory `images`, `kernel-ia32-pc99` and
    `storage-image-ia32-pc99`. Copy them to, say
    `/home/gvrbot/seL4/`
 3. Open file `/boot/grub/grub.cfg`, find `set timeout=0`, change it to `set
    timeout=5`

## 4. Connect GVR-BOT to host

Connect serial cable to GVR-BOT and host, start minicom on the host

## 5. Boot seL4
 
 1. Restart GVR-BOT
 2. Press "C" at the Grub menu window to enter the grub command line.
 3. On the grub command line:
```
    grub> set root="hd0,gpt2"
    grub> multiboot /home/gvrbot/seL4/kernel-ia32-pc99
    grub> module /home/gvrbot/seL4/storage-image-ia32-pc99
    grub> boot
```
