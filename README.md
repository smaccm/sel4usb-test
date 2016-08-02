sel4usb-test
============

The seL4 USB test is configured to run on the GVR-Bot.
This is a short description of how to build and run this configuration.

Building
--------

To build do
```
    make clean
    make gvrbot_storage_defconfig
    make silentoldconfig
    make
```

Then boot `images/kernel-ia32-pc99` and `images/storage-image-ia32-pc99` with 
the multiboot boot loader of your choice

Running
-------
When the application is running, it enumerates all the devices that have already
connected to the host at first. Then the application will wait for an USB
storage device to be plugged in. Once the storage device is plugged in, the
application will start initializing the device, report vid:did, manufacturer,
device serial number etc, reads the first sector on the disk into memory.
Finally, the application will print the USB device tree and halt.

Most of the devices, like USB thumb disk and USB SD card reader should work.
However there are known devices which would fail to respond certain commands or
have very long respond time.

Note that the USB driver doesn't support unplugging a device at the moment, 
unplugging a device won't harm the application, but you won't be able to plug 
in any new devices again.
