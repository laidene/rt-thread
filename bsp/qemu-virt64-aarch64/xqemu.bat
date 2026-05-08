@echo off
cd /d "%~dp0"

qemu-system-aarch64 ^
-M virt,gic-version=2,virtualization=on,secure=on ^
-cpu cortex-a53 ^
-m 256M ^
-smp 4 ^
-kernel rtthread.bin ^
-dtb rtt.dtb ^
-device loader,file=Image,addr=0x48200000,force-raw=on ^
-device loader,file=linux.dtb,addr=0x4f000000,force-raw=on ^
-drive if=none,file=ubuntu-rootfs.img,format=raw,id=vdisk0 ^
-device virtio-blk-device,drive=vdisk0,bus=virtio-mmio-bus.1 ^
-nographic ^
-serial mon:stdio ^
-chardev socket,host=127.0.0.1,port=4322,server=on,wait=off,telnet=on,id=linux_console ^
-device virtio-serial-device,bus=virtio-mmio-bus.0 ^
-device virtconsole,chardev=linux_console
