#ifndef FS_TEST_H
#define FS_TEST_H
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/syscall.h"
#include "user/user.h"

struct patch {
  uint offset;
  uchar value;
};
static char fs_buffer[8 * BSIZE];

static inline void
check(int ok, const char *why)
{
  if (!ok) {
    fprintf(2, "TEST FAILURE: %s\n", why);
    exit(1);
  }
}

static inline uchar
pattern(uint block, uint byte, uint seed)
{
  if (byte < 4)
    return (block >> (8 * byte)) & 255;
  if (byte == 4)
    return seed;
  return ((block * 131) ^ (byte * 17) ^ (seed * 29) ^ (block >> 8)) & 255;
}

static inline void
makefile(const char *name, uint blocks, uint seed)
{
  unlink(name);
  int fd = open(name, O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0, "create test file");
  for (uint first = 0; first < blocks;) {
    uint n = blocks - first;
    if (n > 8)
      n = 8;
    for (uint b = 0; b < n; b++)
      for (uint j = 0; j < BSIZE; j++)
        fs_buffer[b * BSIZE + j] = pattern(first + b, j, seed);
    check(write(fd, fs_buffer, n * BSIZE) == n * BSIZE, "write full test data");
    first += n;
  }
  check(close(fd) == 0, "close written file");
}

static inline void
verifyfd(int fd, uint blocks, uint seed, const struct patch *patches, int np)
{
  struct stat st;
  check(fstat(fd, &st) == 0 && st.size == (uint64)blocks * BSIZE,
        "exact file size");
  for (uint first = 0; first < blocks;) {
    uint n = blocks - first;
    if (n > 8)
      n = 8;
    check(read(fd, fs_buffer, n * BSIZE) == n * BSIZE, "read full test data");
    for (uint b = 0; b < n; b++) {
      for (uint j = 0; j < BSIZE; j++) {
        uint off = (first + b) * BSIZE + j;
        uchar want = pattern(first + b, j, seed);
        for (int k = 0; k < np; k++)
          if (patches[k].offset == off)
            want = patches[k].value;
        check((uchar)fs_buffer[b * BSIZE + j] == want, "file contents differ");
      }
    }
    first += n;
  }
  check(read(fd, fs_buffer, 1) == 0, "EOF after exact file size");
}

static inline void
verify(const char *name, uint blocks, uint seed, const struct patch *patches,
       int np)
{
  int fd = open(name, O_RDONLY);
  check(fd >= 0, "open for verification");
  verifyfd(fd, blocks, seed, patches, np);
  check(close(fd) == 0, "close verified file");
}

static inline void
patchbyte(const char *name, uint offset, uchar value)
{
  int fd = open(name, O_RDWR);
  check(fd >= 0, "open for overwrite");
  while (offset > 0) {
    uint n = offset > sizeof(fs_buffer) ? sizeof(fs_buffer) : offset;
    check(read(fd, fs_buffer, n) == n, "advance overwrite offset");
    offset -= n;
  }
  check(write(fd, &value, 1) == 1, "overwrite one byte");
  check(close(fd) == 0, "close overwrite");
}
#endif
