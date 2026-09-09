#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  int fd;
  char buf[BSIZE];
  struct stat st;

  memset(buf, 'T', sizeof(buf));

  fd = open("truncfile", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("trunctest: open failed\n");
    exit(1);
  }

  for(int i = 0; i < 300; i++){
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("trunctest: write failed at block %d\n", i);
      close(fd);
      exit(1);
    }
  }

  close(fd);

  if(stat("truncfile", &st) < 0){
    printf("trunctest: stat failed\n");
    exit(1);
  }

  printf("before unlink: %ld bytes\n", st.size);

  if(unlink("truncfile") < 0){
    printf("trunctest: unlink failed\n");
    exit(1);
  }

  printf("trunctest: file removed successfully\n");

  if(stat("truncfile", &st) == 0){
    printf("trunctest: file still exists\n");
    exit(1);
  }

  printf("trunctest: truncation passed\n");
  exit(0);
}
