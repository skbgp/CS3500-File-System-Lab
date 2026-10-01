#include "kernel/syscall.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#if defined(SYS_symlink) && defined(O_NOFOLLOW)

int
main(void)
{
  int fd;

  unlink("loop1");
  unlink("loop2");

  // Create a symlink cycle: loop1 -> loop2 -> loop1
  if (symlink("loop2", "loop1") < 0) {
    printf("symlinkloop: symlink loop1 failed\n");
    exit(1);
  }

  if (symlink("loop1", "loop2") < 0) {
    printf("symlinkloop: symlink loop2 failed\n");
    exit(1);
  }

  // Opening a cyclic symlink must fail (not hang or panic).
  fd = open("loop1", O_RDONLY);
  if (fd >= 0) {
    printf("symlinkloop: open loop should have failed\n");
    close(fd);
    exit(1);
  }

  printf("symlinkloop: passed\n");

  unlink("loop1");
  unlink("loop2");
  exit(0);
}

#else
int
main(void)
{
  fprintf(2, "symlink support not implemented\n");
  exit(1);
}
#endif
