# CS3500 Operating Systems — Lab 7: File System

**Release:** 12 October 2026 · **Due:** 1 November 2026, 11:59 PM IST

Required: inode inspection, block-tree printing, large files, and symbolic links.
Relative link targets resolve from the link's containing directory. File COW
cloning is optional and ungraded. See the handout on Moodle for full requirements.

## Start

```sh
git clone --branch cs3500-filesystem-starter https://github.com/skbgp/CS3500-File-System-Lab.git
cd CS3500-File-System-Lab
docker run --rm -it -v "$PWD":/work -w /work nandhagk/xv6-tools:latest sh
make qemu
```

Docker Desktop must be running. A local RISC-V toolchain and QEMU can also run
`make qemu` from the repository root.

Run `make grade` after implementing the required features. The starter passes
`usertests -q`; feature tests fail until those features are implemented.
Commit your work before `make zipball`, which archives the current commit.
