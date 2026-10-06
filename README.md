# CS3500 Operating Systems - Lab 7: File System

**Release:** 12 October 2026 · **Due:** 1 November 2026, 11:59 PM IST

Required: inode inspection, block-tree printing, large files, and symbolic links.
Relative link targets resolve from the link's containing directory. File COW
cloning is optional and ungraded. See the handout on Moodle for full requirements.

## Start

```sh
git clone --branch cs3500-filesystem-starter https://github.com/skbgp/CS3500-File-System-Lab.git
cd CS3500-File-System-Lab
docker run --rm -it --tmpfs /tmp -v "$PWD":/home/xv6-labs \
    -w /home/xv6-labs nandhagk/xv6-tools:latest sh
make qemu
```

Run these commands in order. `"$PWD"` is the cloned directory on your computer;
`/home/xv6-labs` is its location inside the container.

The `--tmpfs /tmp` option supplies the temporary directory needed by the
build tools and grader in this image.

Docker Desktop must be running. A local RISC-V toolchain and QEMU can also run
`make qemu` from the repository root.

Run `make grade` after implementing the required features. The starter passes
`usertests -q`; feature tests fail until those features are implemented.
Commit your work before `make zipball`, which archives the current commit.

## Tests

Before making changes, check the starter with:

```sh
python3 grade-lab-fs --baseline
```

After completing the lab, run `make grade`. The feature tests are expected to
fail in the starter. The public suite includes symlink validation, concurrent
operations, and failed creation on a full disk. Each case uses a fresh image.
The full-disk case starts with a nearly full image to avoid a long setup. Run
`python3 grade-lab-fs --only symlink_failure` to check it separately.

Inside xv6, run `bigfiletest`, `bigfile`, `trunctest`, `symlinktest`,
`nofollowtest`, and `usertests -q`. Use `itreetest 3` and
`itreetest 525` to inspect block trees. They leave `fst_tree.tmp` for inspection;
remove it when finished. The `symlink target path` command becomes available
once the syscall is implemented.

## Submission

Commit the required source files, written answers, references, time spent, and
any required prompt logs before running `make zipball`. It archives `HEAD`;
uncommitted changes are not included. Submit the archive on Moodle as directed
in the handout.
