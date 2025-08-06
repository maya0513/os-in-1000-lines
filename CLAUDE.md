# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a minimal RISC-V operating system implementation written in C, following the tutorial at https://operating-system-in-1000-lines.vercel.app/ja/. The OS runs on QEMU and demonstrates fundamental OS concepts including process management, virtual memory, system calls, and VirtIO disk I/O.

## Build and Run Commands

### Build and Run the OS
```bash
./run.sh
```
This script builds both the user shell and kernel, creates a disk image from files in the `disk/` directory, and launches QEMU with the OS.

### Manual Build Steps
```bash
# Build user shell
clang -std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fno-stack-protector -ffreestanding -nostdlib -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf shell.c user.c common.c
llvm-objcopy --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
llvm-objcopy -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o

# Build kernel
clang -std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fno-stack-protector -ffreestanding -nostdlib -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf kernel.c common.c shell.bin.o

# Create disk image
cd disk && tar cf ../disk.tar --format=ustar *.txt
```

### QEMU Launch Command
```bash
qemu-system-riscv32 -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
  -d unimp,guest_errors,int,cpu_reset -D qemu.log \
  -drive id=drive0,file=disk.tar,format=raw,if=none \
  -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
  -kernel kernel.elf
```

## Architecture Overview

### Memory Layout
- **Kernel**: Loaded at 0x80200000 with 128KB stack and 64MB heap
- **User processes**: Loaded at 0x1000000 with 64KB stack per process
- **Page size**: 4KB with SV32 virtual memory (2-level page tables)

### Core Components

**kernel.c**: Main kernel implementation containing:
- SBI (Supervisor Binary Interface) calls for basic I/O
- Memory management with simple page allocator
- Process management supporting up to 8 processes
- VirtIO block device driver for disk I/O
- TAR filesystem for reading files from disk
- System call handling (putchar, getchar, exit)
- Context switching and cooperative scheduling

**common.c/h**: Shared utilities including printf, string operations, and memory functions

**user.c/h**: User-space runtime providing system call wrappers and process entry point

**shell.c**: Simple interactive shell supporting "hello" and "exit" commands

### Key Design Patterns

1. **Embedded System Design**: No standard library dependencies, bare-metal RISC-V implementation
2. **Cooperative Multitasking**: Processes must explicitly call `yield()` to switch context
3. **Simple Memory Model**: Linear page allocator without deallocation or virtual memory mapping beyond basic kernel/user separation
4. **VirtIO Integration**: Uses VirtIO for standardized device communication with QEMU

### System Interfaces

**System Calls**: Implemented via RISC-V `ecall` instruction
- SYS_PUTCHAR (1): Output single character
- SYS_GETCHAR (2): Input single character (blocking)
- SYS_EXIT (3): Terminate current process

**Device I/O**: VirtIO block device at physical address 0x10001000 for disk operations

### Memory Management Notes

The kernel uses a simple bump allocator (`alloc_pages`) that never frees memory. All allocations are page-aligned (4KB). Virtual memory is set up with identity mapping for kernel space and separate mappings for each user process.

Process creation involves:
1. Allocating page table and stack
2. Setting up virtual memory mappings
3. Loading user binary into allocated pages
4. Initializing process control block

The system supports a maximum of 8 concurrent processes with basic round-robin scheduling.