// Host grader wrapper: observe the actual child exit status and flush its writes.
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int status = -1;
  if (argc < 3) {
    fprintf(2, "usage: labrun token program [arguments]\n");
    exit(1);
  }
  int pid = fork();
  if (pid == 0) {
    exec(argv[2], &argv[2]);
    fprintf(2, "labrun: exec failed\n");
    exit(127);
  }
  if (pid > 0 && wait(&status) != pid)
    status = -1;
  sync();
  printf("\nLAB_RESULT %s %d\n", argv[1], status);
  exit(status == 0 ? 0 : 1);
}
