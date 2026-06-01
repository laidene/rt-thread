qemu-system-aarch64 ^
  -M virt,gic-version=2,virtualization=on,secure=on,dumpdtb=all.dtb ^
  -cpu cortex-a53 ^
  -m 256M ^
  -smp 4


dtc -I dtb -O dts -o all.dts all.dtb