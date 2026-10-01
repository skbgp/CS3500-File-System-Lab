#include "user/fs_test.h"
int
main(void)
{
  makefile("fst_max.tmp", 65803, 19);
  verify("fst_max.tmp", 65803, 19, 0, 0);
  int fd = open("fst_max.tmp", O_RDWR);
  check(fd >= 0, "open maximum file");
  uint left = 65803U * BSIZE;
  while (left) {
    uint n = left > sizeof(fs_buffer) ? sizeof(fs_buffer) : left;
    check(read(fd, fs_buffer, n) == n, "advance to maximum EOF");
    left -= n;
  }
  check(write(fd, fs_buffer, 1) < 1, "reject write past MAXFILE");
  check(close(fd) == 0, "close maximum file");
  verify("fst_max.tmp", 65803, 19, 0, 0);
  check(unlink("fst_max.tmp") == 0, "reclaim maximum file");
  printf("bigfile: passed\n");
  exit(0);
}
