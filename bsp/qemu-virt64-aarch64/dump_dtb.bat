@REM -kernel rtthread.bin 让 dump 出来的 all.dtb 包含 QEMU 自动生成的 psci 节点

qemu-system-aarch64 ^
  -M virt,gic-version=2,virtualization=on,secure=on,dumpdtb=all.dtb ^
  -cpu cortex-a53 ^
  -m 256M ^
  -smp 4 ^
  -kernel rtthread.bin

dtc -I dtb -O dts -o all.dts all.dtb
