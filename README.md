# CS3500 File System Lab

This repository contains the starter xv6 source for the CS3500 Operating
Systems File System Lab.

The lab is based on the xv6 RISC-V teaching operating system developed by
MIT PDOS. The course lab adds filesystem inspection tools, symbolic links,
and a copy-on-write clone extension.

Refer to the lab handout distributed on Moodle for the complete assignment,
requirements, questions, and submission instructions.

## Building

A RISC-V cross compiler and QEMU are required.

On macOS, the following tools are expected:

riscv64-elf-gcc
riscv64-elf-objdump
qemu-system-riscv64

Build and run xv6 with:

make qemu

## Repository branches

The riscv branch tracks the upstream xv6 baseline.

The cs3500-filesystem-starter branch contains the CS3500 lab starter code.

## Source

The base xv6 source is derived from:

https://github.com/mit-pdos/xv6-riscv

See the Moodle handout for the complete list of references and
acknowledgements.
