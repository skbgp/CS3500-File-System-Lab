#include "user/fs_test.h"

#if defined(SYS_symlink) && defined(O_NOFOLLOW) && defined(T_SYMLINK)
int
main(void)
{
  check(mkdir("fst_fail.tmp") == 0 && chdir("fst_fail.tmp") == 0,
        "create failure-test directory");
  const char *files[] = {"fill0", "fill1", "fill2", "fill3"};
  int count = 0;
  memset(fs_buffer, 'x', sizeof(fs_buffer));
  for (int i = 0; i < 4; i++) {
    int fd = open(files[i], O_CREATE | O_WRONLY);
    check(fd >= 0, "create disk-filling file");
    count++;
    while (write(fd, fs_buffer, sizeof(fs_buffer)) == sizeof(fs_buffer))
      ;
    check(close(fd) == 0, "close disk-filling file");
  }

  // A one-byte append consumes any space left at a chunk/size boundary.
  int fd = open("last", O_CREATE | O_RDWR);
  check(fd >= 0, "create final disk-filling file");
  while (write(fd, fs_buffer, BSIZE) == BSIZE)
    ;
  struct stat st;
  check(fstat(fd, &st) == 0 && st.size < MAXFILE * BSIZE,
        "failure comes from disk exhaustion rather than the file-size limit");
  check(write(fd, fs_buffer, 1) != 1 && close(fd) == 0,
        "disk has no data block available");

  for (int i = 0; i < 8; i++) {
    check(symlink("target", "failed") == -1,
          "creation on a full disk must fail");
    fd = open("failed", O_RDONLY | O_NOFOLLOW);
    if (fd >= 0)
      close(fd);
    check(fd == -1, "failed creation leaves no directory entry");
  }
  printf("FS_STAGE full_reject\n");
  for (int i = 0; i < count; i++)
    check(unlink(files[i]) == 0, "remove disk-filling file");
  check(unlink("last") == 0, "remove final filling file");
  check(symlink("target", "failed") == 0 && unlink("failed") == 0,
        "creation works again after freeing space");
  check(chdir("..") == 0 && unlink("fst_fail.tmp") == 0,
        "remove failure-test directory");
  printf("symlinkfail: passed\n");
  exit(0);
}
#else
int
main(void)
{
  fprintf(2, "symlinkfail: symlink support not implemented\n");
  exit(1);
}
#endif
