@echo off
cd /d "%~dp0"

qemu-system-aarch64 ^
-M virt,gic-version=2,virtualization=on,secure=on  ^
-cpu cortex-a53 ^
-m 256M ^
-smp 4 ^
-kernel rtthread.bin ^
-nographic ^
-serial mon:stdio