#!/bin/bash
set -xue

QEMU=qemu-system-riscv32

# clangのパス
CC=clang

OBJCOPY=llvm-objcopy

CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fno-stack-protector -ffreestanding -nostdlib"

# buildディレクトリを作成
mkdir -p build

# シェルをビルド
$CC $CFLAGS -Wl,-Tsrc/user.ld -Wl,-Map=build/shell.map -o build/shell.elf src/shell.c src/user.c src/common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary build/shell.elf build/shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv build/shell.bin build/shell.bin.o

# カーネルをビルド
$CC $CFLAGS -Wl,-Tsrc/kernel.ld -Wl,-Map=build/kernel.map -o build/kernel.elf \
  src/kernel.c src/common.c build/shell.bin.o

(cd disk && tar cf ../build/disk.tar --format=ustar *.txt)

# QEMUを起動
$QEMU -machine virt -bios firmware/opensbi-riscv32-generic-fw_dynamic.bin -nographic -serial mon:stdio --no-reboot \
  -d unimp,guest_errors,int,cpu_reset -D build/qemu.log \
  -drive id=drive0,file=build/disk.tar,format=raw,if=none \
  -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
  -kernel build/kernel.elf