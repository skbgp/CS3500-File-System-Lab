#include "user/fs_test.h"

static void
data_test(int blocks)
{
  makefile("fst_part.tmp", blocks, 7);
  verify("fst_part.tmp", blocks, 7, 0, 0);
  check(unlink("fst_part.tmp") == 0, "remove boundary fixture");
}

static void
trunc_test(int mode)
{
  for (int i = 0; i < (mode == 2 ? 4 : 1); i++) {
    makefile("fst_part.tmp", 525, i + 1);
    verify("fst_part.tmp", 525, i + 1, 0, 0);
    if (mode != 1) {
      int fd = open("fst_part.tmp", O_WRONLY | O_TRUNC);
      check(fd >= 0 && close(fd) == 0, "truncate file");
      verify("fst_part.tmp", 0, i + 1, 0, 0);
    }
    if (mode != 0) {
      makefile("fst_part.tmp", 525, i + 2);
      int fd = open("fst_part.tmp", O_RDONLY);
      check(fd >= 0 && unlink("fst_part.tmp") == 0, "unlink open file");
      verifyfd(fd, 525, i + 2, 0, 0);
      check(close(fd) == 0, "last close");
    } else {
      check(unlink("fst_part.tmp") == 0, "remove truncated fixture");
    }
  }
}

#if defined(SYS_symlink) && defined(O_NOFOLLOW) && defined(T_SYMLINK)
static void
unavailable(const char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
    close(fd);
  check(fd == -1, "unresolvable link must fail");
}
static void
put(const char *path, char byte)
{
  int fd = open(path, O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0 && write(fd, &byte, 1) == 1 && close(fd) == 0, "create target");
}
static void
get(const char *path, char byte)
{
  char actual;
  int fd = open(path, O_RDONLY);
  check(fd >= 0 && read(fd, &actual, 1) == 1 && actual == byte &&
          read(fd, &actual, 1) == 0 && close(fd) == 0,
        "follow correct target");
}
static void
basic(void)
{
  put("target", 'a');
  check(symlink("target", "link") == 0, "create symlink");
  get("link", 'a');
  check(symlink("/fst_parts.tmp/target", "absolute") == 0, "absolute symlink");
  get("absolute", 'a');
  check(link("link", "hard") == 0, "hard-link symlink inode");
  get("hard", 'a');
  check(unlink("link") == 0, "unlink link");
  get("target", 'a');
  check(symlink("future", "dangling") == 0, "dangling target allowed");
  unavailable("dangling");
  put("future", 'b');
  get("dangling", 'b');
  const char *names[] = {"target", "absolute", "hard", "dangling", "future"};
  for (int i = 0; i < 5; i++)
    check(unlink(names[i]) == 0, "basic cleanup");
}
static void
relative(void)
{
  check(mkdir("a") == 0 && mkdir("b") == 0, "relative directories");
  put("a/file", 'a');
  put("b/file", 'b');
  check(symlink("file", "a/link") == 0, "relative target");
  get("a/link", 'a');
  check(link("a/link", "b/alias") == 0, "symlink entry alias");
  get("b/alias", 'b');
  check(symlink("file", "b/next") == 0 && symlink("../b/next", "a/cross") == 0,
        "cross-directory chain");
  get("a/cross", 'b');
  check(chdir("b") == 0, "different caller directory");
  get("../a/link", 'a');
  check(symlink("../a/file", "up") == 0, "dot-dot target");
  get("up", 'a');
  check(unlink("up") == 0 && chdir("..") == 0, "return to fixture directory");
  const char *names[] = {"a/link", "b/alias", "a/cross", "b/next",
                         "a/file", "b/file",  "a",       "b"};
  for (int i = 0; i < 8; i++)
    check(unlink(names[i]) == 0, "relative cleanup");
}
static void
chains(void)
{
  put("target", 'a');
  char previous[14] = "target", name[] = "chain00";
  for (int i = 0; i < 11; i++) {
    name[5] = '0' + i / 10;
    name[6] = '0' + i % 10;
    check(symlink(previous, name) == 0, "create chain");
    strcpy(previous, name);
  }
  get("chain09", 'a');
  unavailable("chain10");
  check(symlink("self", "self") == 0, "self cycle");
  unavailable("self");
  check(symlink("loopb", "loopa") == 0 && symlink("loopa", "loopb") == 0,
        "two-link cycle");
  unavailable("loopa");
  for (int i = 0; i < 11; i++) {
    name[5] = '0' + i / 10;
    name[6] = '0' + i % 10;
    check(unlink(name) == 0, "chain cleanup");
  }
  check(unlink("target") == 0 && unlink("self") == 0 && unlink("loopa") == 0 &&
          unlink("loopb") == 0,
        "cycle cleanup");
}
static void
flags(void)
{
  int fd = open("edge_target", O_CREATE | O_RDWR | O_TRUNC);
  check(fd >= 0 && write(fd, "hello", 5) == 5 && close(fd) == 0,
        "create edge-case target");
  check(symlink("edge_target", "edge_link") == 0, "create edge-case link");
  struct stat linkstat, hardstat, targetstat;
  check(stat("edge_target", &targetstat) == 0 &&
          link("edge_link", "edge_hard") == 0,
        "hard-link symlink inode");
  fd = open("edge_link", O_RDONLY | O_NOFOLLOW);
  int hardfd = open("edge_hard", O_RDONLY | O_NOFOLLOW);
  check(fd >= 0 && hardfd >= 0 && fstat(fd, &linkstat) == 0 &&
          fstat(hardfd, &hardstat) == 0 && linkstat.type == T_SYMLINK &&
          linkstat.ino != targetstat.ino && linkstat.ino == hardstat.ino,
        "NOFOLLOW returns link inode and preserves hard-link identity");
  check(close(fd) == 0 && close(hardfd) == 0 && unlink("edge_hard") == 0,
        "remove hard link");
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
}
static void
validation(void)
{
  int fd;
  put("target", 'a');
  check(symlink("missing", "existing") == 0, "existing dangling destination");
  check(symlink("other", "existing") == -1 && symlink("other", "target") == -1,
        "reject existing destinations");
  check(symlink("", "empty") == -1 && symlink("target", "") == -1,
        "empty arguments");
  char longpath[MAXPATH + 1];
  memset(longpath, 'x', sizeof(longpath));
  longpath[MAXPATH - 1] = 0;
  check(symlink(longpath, "long") == 0 && unlink("long") == 0,
        "maximum target length");
  longpath[MAXPATH - 1] = 'x';
  longpath[MAXPATH] = 0;
  check(symlink(longpath, "long") == -1, "oversized target");
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
  check(unlink("existing") == 0 && unlink("target") == 0, "validation cleanup");
}
#endif

int
main(int argc, char **argv)
{
  check(argc >= 2, "subtest mode required");
  if (!strcmp(argv[1], "data")) {
    check(argc == 3, "block count required");
    data_test(atoi(argv[2]));
  } else if (!strcmp(argv[1], "trunc")) {
    check(argc == 3, "truncation mode required");
    trunc_test(atoi(argv[2]));
  }
#if defined(SYS_symlink) && defined(O_NOFOLLOW) && defined(T_SYMLINK)
  else {
    check(mkdir("fst_parts.tmp") == 0 && chdir("fst_parts.tmp") == 0,
          "isolate symlink subtest");
    if (!strcmp(argv[1], "basic"))
      basic();
    else if (!strcmp(argv[1], "relative"))
      relative();
    else if (!strcmp(argv[1], "chains"))
      chains();
    else if (!strcmp(argv[1], "flags"))
      flags();
    else if (!strcmp(argv[1], "validation"))
      validation();
    else
      check(0, "unknown symlink subtest");
    check(chdir("..") == 0 && unlink("fst_parts.tmp") == 0,
          "remove subtest directory");
  }
#else
  else
    check(0, "symlink support missing");
#endif
  printf("fssubtest: passed\n");
  exit(0);
}
