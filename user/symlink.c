#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("usage: symlink target linkpath\n");
    exit(1);
  }

  if(symlink(argv[1], argv[2]) < 0){
    printf("symlink: failed\n");
    exit(1);
  }

  exit(0);
}
