@echo off
if exist sd.bin goto run
qemu-img create -f raw sd.bin 64M

:run
if "%1"=="-d" goto debug
qemu-system-arm ^
-M vexpress-a9 ^
-smp cpus=2 ^
-kernel rtthread.bin ^
-sd sd.bin ^
-nographic
goto end

:debug
qemu-system-arm ^
-M vexpress-a9 ^
-smp cpus=2 ^
-kernel rtthread.bin ^
-sd sd.bin ^
-nographic ^
-S -s

:end