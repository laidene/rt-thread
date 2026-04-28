@echo off

setlocal

REM Use binary file with device loader to specify physical load address
if not exist rtthread.bin (
    echo [Warning] rtthread.bin not found, trying to use rtthread.elf
    if not exist rtthread.elf (
        echo [Error] Neither rtthread.bin nor rtthread.elf found
        exit /b 1
    )
    set USE_ELF=1
) else (
    set USE_ELF=0
)

if "%1"=="-d" (
    echo [c run qemu debug mode...] 
    if %USE_ELF%==0 (
        REM Use device loader with binary file - loads at physical address 0x80001000
        qemu-system-arm -M mcimx6ul-evk -m 512M -device loader,file=rtthread.bin,addr=0x80001000,cpu-num=0 -nographic -s -S
    ) else (
        REM Fallback: use kernel option with ELF (may have issues with virtual addresses)
        qemu-system-arm -M mcimx6ul-evk -m 512M -kernel rtthread.elf -nographic -s -S
    )
) else (
    echo [c run qemu ...]
    if %USE_ELF%==0 (
        REM Use device loader with binary file - loads at physical address 0x80001000
        qemu-system-arm -M mcimx6ul-evk -m 512M -device loader,file=rtthread.bin,addr=0x80001000,cpu-num=0 -nographic
    ) else (
        REM Fallback: use kernel option with ELF (may have issues with virtual addresses)
        qemu-system-arm -M mcimx6ul-evk -m 512M -kernel rtthread.elf -nographic
    )
)


endlocal

