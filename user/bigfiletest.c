#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(void)
{
  int fd;
  char buf[BSIZE];
  int i;

  memset(buf, 'A', sizeof(buf));

  fd = open("bigfile", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("bigfiletest: open failed\n");
    exit(1);
  }

  // 270 blocks:
  // 11 direct + 256 single-indirect + 3 double-indirect
  for(i = 0; i < 270; i++){
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("bigfiletest: write failed at block %d\n", i);
      close(fd);
      exit(1);
    }
  }

  close(fd);

  printf("bigfiletest: created 270 blocks\n");
  exit(0);
}
