#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define MAX_BLOCKS 65803
#define WRITE_BLOCKS 32

char buf[WRITE_BLOCKS * BSIZE];

int
main(void)
{
  int fd;
  int blocks = 0;

  memset(buf, 'B', sizeof(buf));

  fd = open("maxfile", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("bigfile: open failed\n");
    exit(1);
  }

  while(blocks + WRITE_BLOCKS <= MAX_BLOCKS){
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
      printf("bigfile: write failed at block %d\n", blocks);
      close(fd);
      exit(1);
    }
    blocks += WRITE_BLOCKS;

  }

  while(blocks < MAX_BLOCKS){
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("bigfile: final write failed at block %d\n", blocks);
      close(fd);
      exit(1);
    }
    blocks++;
  }

  close(fd);

  printf("bigfile: created %d blocks\n", blocks);
  printf("expected size: %d bytes\n", MAX_BLOCKS * BSIZE);

  exit(0);
}
