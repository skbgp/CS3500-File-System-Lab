#include "user/fs_test.h"
int
main(void)
{
  for (int i = 0; i < 4; i++) {
    makefile("fst_trunc.tmp", 525, i + 1);
    verify("fst_trunc.tmp", 525, i + 1, 0, 0);
    int fd = open("fst_trunc.tmp", O_WRONLY | O_TRUNC);
    check(fd >= 0, "truncate existing file");
    check(close(fd) == 0, "close truncated file");
    verify("fst_trunc.tmp", 0, i + 1, 0, 0);
    makefile("fst_trunc.tmp", 525, i + 2);
    fd = open("fst_trunc.tmp", O_RDONLY);
    check(fd >= 0 && unlink("fst_trunc.tmp") == 0, "unlink open file");
    verifyfd(fd, 525, i + 2, 0, 0);
    check(close(fd) == 0, "last close reclaims unlinked file");
  }
  printf("trunctest: passed\n");
  exit(0);
}
