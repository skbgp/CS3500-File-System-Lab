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

  unlink("slink");
  unlink("target");

  fd = open("target", O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("symlinktest: target create failed\n");
    exit(1);
  }

  if(write(fd, "hello\n", 6) != 6){
    printf("symlinktest: write failed\n");
    close(fd);
    exit(1);
  }
  close(fd);

  if(symlink("target", "slink") < 0){
    printf("symlinktest: symlink failed\n");
    exit(1);
  }

  fd = open("slink", O_RDONLY);
  if(fd < 0){
    printf("symlinktest: open through symlink failed\n");
    exit(1);
  }

  n = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if(n != 6){
    printf("symlinktest: wrong read size\n");
    exit(1);
  }

  buf[n] = 0;
  printf("through symlink: %s", buf);

  fd = open("slink", O_RDONLY | O_NOFOLLOW);
  if(fd < 0){
    printf("symlinktest: O_NOFOLLOW failed\n");
    exit(1);
  }
  close(fd);

  unlink("slink");
  unlink("target");

  printf("symlinktest: passed\n");
  exit(0);
}
