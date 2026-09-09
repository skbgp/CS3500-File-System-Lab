#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
  int fd;
  char buf[32];
  int n;

  unlink("target");
  unlink("s1");

  fd = open("target", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("target create failed\n");
    exit(1);
  }

  write(fd, "hello", 5);
  close(fd);

  if(symlink("target", "s1") < 0){
    printf("symlink failed\n");
    exit(1);
  }

  fd = open("s1", O_RDONLY | O_NOFOLLOW);
  if(fd < 0){
    printf("O_NOFOLLOW open failed\n");
    exit(1);
  }

  n = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if(n != 7){
    printf("wrong symlink size: %d\n", n);
    exit(1);
  }

  buf[n] = 0;

  if(strcmp(buf, "target") != 0){
    printf("wrong symlink contents: %s\n", buf);
    exit(1);
  }

  printf("nofollowtest: passed\n");

  unlink("s1");
  unlink("target");
  exit(0);
}
