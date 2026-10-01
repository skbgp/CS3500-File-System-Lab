#include "user/fs_test.h"
int main(void) {
  makefile("fst_small.tmp", 525, 7);
  verify("fst_small.tmp", 525, 7, 0, 0);
  check(unlink("fst_small.tmp") == 0, "remove test file");
  printf("bigfiletest: passed\n");
  exit(0);
}
