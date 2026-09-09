#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(2, "usage: print_itree path\n");
    exit(1);
  }

  if (iprint(argv[1]) < 0) {
    fprintf(2, "print_itree: cannot inspect %s\n", argv[1]);
    exit(1);
  }

  exit(0);
}
