#include "user/fs_test.h"
#if defined(SYS_symlink) && defined(O_NOFOLLOW) && defined(T_SYMLINK)
static void
unavailable(const char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    close(fd);
  check(fd < 0, "dangling link, cycle, or excessive chain must fail");
}
static void
edge_tests(void)
{
  int fd = open("edge_target", O_CREATE | O_RDWR | O_TRUNC);
  check(fd >= 0 && write(fd, "hello", 5) == 5 && close(fd) == 0,
        "create edge-case target");
  check(symlink("edge_target", "edge_link") == 0, "create edge-case link");
  fd = open("edge_link", O_RDONLY | O_NOFOLLOW | O_TRUNC);
  check(fd >= 0 && read(fd, fs_buffer, 32) == sizeof("edge_target") &&
          memcmp(fs_buffer, "edge_target", sizeof("edge_target")) == 0 &&
          close(fd) == 0,
        "NOFOLLOW with TRUNC preserves link contents");
  fd = open("edge_link", O_WRONLY | O_NOFOLLOW);
  check(fd >= 0 && read(fd, fs_buffer, 1) == -1 && close(fd) == 0,
        "write-only link descriptor rejects reads");
  fd = open("edge_link", O_RDONLY | O_NOFOLLOW);
  check(fd >= 0 && write(fd, "x", 1) == -1 && close(fd) == 0,
        "read-only link descriptor rejects writes");
  fd = open("edge_link", O_WRONLY | O_TRUNC);
  check(fd >= 0 && close(fd) == 0, "truncate followed regular file");
  struct stat st;
  check(stat("edge_target", &st) == 0 && st.size == 0,
        "TRUNC applies to the resolved target");
  fd = open("edge_link", O_RDONLY | O_NOFOLLOW);
  check(fd >= 0 && read(fd, fs_buffer, 32) == sizeof("edge_target") &&
          close(fd) == 0,
        "following with TRUNC preserves the link");
  fd = open("edge_link", O_RDWR | O_NOFOLLOW);
  check(fd >= 0 && write(fd, "X", 1) == 1 && close(fd) == 0,
        "NOFOLLOW permits writes with the requested mode");
  unavailable("edge_link");
  check(unlink("edge_link") == 0 && unlink("edge_target") == 0,
        "remove access-mode fixtures");

  const char *valid_names[] = {"abcdefg", "a"};
  for (int i = 0; i < 2; i++) {
    fd = open(valid_names[i], O_CREATE | O_WRONLY);
    check(fd >= 0 && close(fd) == 0, "create malformed-target control");
  }
  // Change valid stored targets into malformed ones through NOFOLLOW.
  const char *bad[] = {"abcdefg", "a\0bcdef", "\0abcdef"};
  for (int i = 0; i < 3; i++) {
    check(symlink("target", "edge_bad") == 0, "create malformed fixture");
    fd = open("edge_bad", O_WRONLY | O_NOFOLLOW);
    check(fd >= 0 && write(fd, bad[i], 7) == 7 && close(fd) == 0,
          "replace stored target bytes");
    unavailable("edge_bad");
    check(unlink("edge_bad") == 0, "remove malformed fixture");
  }
  check(symlink("target", "edge_bad") == 0, "create oversized fixture");
  memset(fs_buffer, 'x', MAXPATH + 1);
  fs_buffer[0] = 'a';
  fs_buffer[1] = 0;
  fs_buffer[MAXPATH] = 0;
  fd = open("edge_bad", O_WRONLY | O_NOFOLLOW);
  check(fd >= 0 && write(fd, fs_buffer, MAXPATH + 1) == MAXPATH + 1 &&
          close(fd) == 0,
        "extend stored target beyond MAXPATH");
  unavailable("edge_bad");
  check(unlink("edge_bad") == 0, "remove oversized fixture");
  for (int i = 0; i < 2; i++)
    check(unlink(valid_names[i]) == 0, "remove malformed-target control");

  check(mkdir("edge_dir") == 0, "create expansion directory");
  fd = open("edge_dir/t", O_CREATE | O_WRONLY);
  check(fd >= 0 && close(fd) == 0, "create expansion target");
  char target[MAXPATH];
  memset(target, '/', sizeof(target));
  target[0] = '.';
  target[MAXPATH - 2] = 't';
  target[MAXPATH - 1] = 0;
  check(symlink(target, "edge_dir/long") == 0,
        "create link with a maximum-length relative target");
  unavailable("edge_dir/long");
  check(unlink("edge_dir/long") == 0 && unlink("edge_dir/t") == 0 &&
          unlink("edge_dir") == 0,
        "remove expansion fixtures");

  int gate[2];
  check(pipe(gate) == 0, "create concurrent-start pipe");
  for (int i = 0; i < 4; i++) {
    int pid = fork();
    check(pid >= 0, "fork concurrent creator");
    if (pid == 0) {
      char ready;
      close(gate[1]);
      if (read(gate[0], &ready, 1) != 1)
        exit(2);
      close(gate[0]);
      int result = symlink("target", "edge_race");
      exit(result == 0 ? 0 : result == -1 ? 1 : 2);
    }
  }
  close(gate[0]);
  check(write(gate[1], "go!!", 4) == 4 && close(gate[1]) == 0,
        "release concurrent creators");
  int winners = 0;
  for (int i = 0; i < 4; i++) {
    int status;
    check(wait(&status) > 0 && (status == 0 || status == 1),
          "concurrent creation returns success or failure");
    winners += status == 0;
  }
  check(winners == 1 && unlink("edge_race") == 0,
        "exactly one creator owns the destination");
}

int
main(void)
{
  check(mkdir("fst_syms.tmp") == 0 && chdir("fst_syms.tmp") == 0,
        "create symlink test directory");
  struct stat st, target;
  const char text[] = "hello\n";
  int fd = open("sl_target", O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0 && write(fd, text, 6) == 6, "create symlink target");
  check(close(fd) == 0, "close target");
  check(stat("sl_target", &target) == 0, "stat target");
  check(symlink("sl_target", "sl_link") == 0, "create symlink");
  fd = open("sl_link", O_RDONLY);
  check(fd >= 0, "follow symlink");
  check(read(fd, fs_buffer, 8) == 6 && memcmp(fs_buffer, text, 6) == 0,
        "exact target contents");
  check(close(fd) == 0, "close followed link");
  fd = open("sl_link", O_RDONLY | O_NOFOLLOW);
  check(fd >= 0 && fstat(fd, &st) == 0 && st.type == T_SYMLINK &&
          st.ino != target.ino,
        "NOFOLLOW returns link inode");
  check(read(fd, fs_buffer, 32) == sizeof("sl_target") &&
          memcmp(fs_buffer, "sl_target", sizeof("sl_target")) == 0,
        "NOFOLLOW returns terminated target pathname");
  check(close(fd) == 0, "close link inode");
  check(symlink("missing", "sl_dang") == 0, "create dangling link");
  unavailable("sl_dang");
  check(symlink("other", "sl_dang") == -1,
        "reject existing dangling destination");
  check(symlink("other", "sl_target") == -1,
        "reject existing regular destination");
  check(symlink("", "sl_empty") == -1 && symlink("sl_target", "") == -1,
        "reject empty paths");
  char longpath[MAXPATH + 1];
  memset(longpath, 'x', sizeof(longpath));
  longpath[MAXPATH - 1] = 0;
  check(symlink(longpath, "sl_long") == 0, "accept MAXPATH-1 characters");
  check(unlink("sl_long") == 0, "remove maximum target");
  longpath[MAXPATH - 1] = 'x';
  longpath[MAXPATH] = 0;
  check(symlink(longpath, "sl_long") == -1, "reject MAXPATH characters");
  char prev[14] = "sl_target", name[14] = "sl_c00";
  for (int i = 0; i < 11; i++) {
    name[4] = '0' + i / 10;
    name[5] = '0' + i % 10;
    check(symlink(prev, name) == 0, "create link chain");
    strcpy(prev, name);
  }
  fd = open("sl_c09", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6 &&
          memcmp(fs_buffer, text, 6) == 0,
        "follow exactly ten links");
  check(close(fd) == 0, "close chain");
  unavailable("sl_c10");
  check(symlink("sl_loopb", "sl_loopa") == 0 &&
          symlink("sl_loopa", "sl_loopb") == 0,
        "create cycle");
  unavailable("sl_loopa");
  check(link("sl_link", "sl_hard") == 0, "hard-link the symlink");
  fd = open("sl_hard", O_RDONLY | O_NOFOLLOW);
  struct stat hard;
  check(fd >= 0 && fstat(fd, &hard) == 0 && hard.type == T_SYMLINK &&
          hard.ino == st.ino,
        "link must not follow symlink");
  check(close(fd) == 0 && unlink("sl_link") == 0, "unlink only the link");
  fd = open("sl_target", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6 &&
          memcmp(fs_buffer, text, 6) == 0,
        "target survives link removal");
  check(close(fd) == 0, "close preserved target");
  const char inner[] = "inside\n";
  check(mkdir("sl_dir") == 0 && mkdir("sl_else") == 0,
        "relative-target directories");
  fd = open("sl_dir/sl_target", O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0 && write(fd, inner, 7) == 7 && close(fd) == 0,
        "create target beside the link");
  check(symlink("sl_target", "sl_dir/rel") == 0, "create relative link");
  fd = open("sl_dir/rel", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 7 &&
          memcmp(fs_buffer, inner, 7) == 0,
        "relative target uses link directory");
  check(close(fd) == 0, "close relative target");
  int child = fork();
  check(child >= 0, "fork for cwd-independent lookup");
  if (child == 0) {
    if (chdir("sl_else") < 0)
      exit(1);
    int otherfd = open("../sl_dir/rel", O_RDONLY);
    if (otherfd < 0 || read(otherfd, fs_buffer, 8) != 7 ||
        memcmp(fs_buffer, inner, 7) != 0 || close(otherfd) < 0)
      exit(1);
    exit(0);
  }
  int status;
  check(wait(&status) == child && status == 0,
        "relative target is independent of caller cwd");
  check(symlink("../sl_target", "sl_dir/up") == 0,
        "create target with parent component");
  fd = open("sl_dir/up", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6 &&
          memcmp(fs_buffer, text, 6) == 0 && close(fd) == 0,
        "relative target with dot-dot");
  check(symlink("rel", "sl_dir/chain") == 0,
        "create link chain in subdirectory");
  fd = open("sl_dir/chain", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 7 &&
          memcmp(fs_buffer, inner, 7) == 0 && close(fd) == 0,
        "each link resolves from its own directory");
  const char elsewhere[] = "other\n";
  fd = open("sl_else/sl_target", O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0 && write(fd, elsewhere, 6) == 6 && close(fd) == 0,
        "create target in second directory");
  check(symlink("sl_target", "sl_else/next") == 0 &&
          symlink("../sl_else/next", "sl_dir/cross") == 0,
        "create chain across directories");
  fd = open("sl_dir/cross", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6 &&
          memcmp(fs_buffer, elsewhere, 6) == 0 && close(fd) == 0,
        "second link uses its own directory");
  check(link("sl_dir/rel", "sl_else/alias") == 0,
        "hard-link a symlink into another directory");
  fd = open("sl_else/alias", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6 &&
          memcmp(fs_buffer, elsewhere, 6) == 0 && close(fd) == 0,
        "hard-linked entry uses the directory traversed");
  check(symlink("/fst_syms.tmp/sl_target", "sl_abs") == 0, "absolute target");
  fd = open("sl_abs", O_RDONLY);
  check(fd >= 0 && read(fd, fs_buffer, 8) == 6, "follow absolute target");
  check(close(fd) == 0, "close absolute target");
  for (int i = 0; i < 11; i++) {
    name[4] = '0' + i / 10;
    name[5] = '0' + i % 10;
    check(unlink(name) == 0, "remove chain");
  }
  const char *cleanup[] = {
    "sl_dang",       "sl_loopa",     "sl_loopb",         "sl_hard",
    "sl_else/alias", "sl_dir/cross", "sl_else/next",     "sl_dir/chain",
    "sl_dir/up",     "sl_dir/rel",   "sl_dir/sl_target", "sl_else/sl_target",
    "sl_dir",        "sl_else",      "sl_abs",           "sl_target"};
  for (uint i = 0; i < sizeof(cleanup) / sizeof(cleanup[0]); i++)
    check(unlink(cleanup[i]) == 0, "symlink cleanup");
  edge_tests();
  check(chdir("..") == 0 && unlink("fst_syms.tmp") == 0,
        "remove symlink test directory");
  printf("symlinktest: passed\n");
  exit(0);
}
#else
int
main(void)
{
  fprintf(
    2,
    "symlinktest: symlink syscall, T_SYMLINK and O_NOFOLLOW are not implemented\n");
  exit(1);
}
#endif
