#include "user/fs_test.h"
int main(int argc, char **argv) {
  check(argc == 2, "itreetest requires block count");
  int n = atoi(argv[1]);
  check(n == 3 || n == 525, "supported tree fixture size");
  makefile("fst_tree.tmp", n, 11);
  verify("fst_tree.tmp", n, 11, 0, 0);
  printf("\nTREE_BEGIN\n");
  check(iprint("fst_tree.tmp") == 0, "iprint succeeds");
  printf("TREE_END\n");
  // Retain fixture for independent host inspection.
  exit(0);
}
