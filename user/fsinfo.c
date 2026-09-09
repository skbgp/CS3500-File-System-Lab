#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct istat_info info;

  if (argc != 2) {
    fprintf(2, "usage: fsinfo path\n");
    exit(1);
  }

  if (istat(argv[1], &info) < 0) {
    fprintf(2, "fsinfo: cannot inspect %s\n", argv[1]);
    exit(1);
  }

  printf("filesystem:\n");
  printf("  size        %d\n", info.sb.size);
  printf("  nblocks     %d\n", info.sb.nblocks);
  printf("  ninodes     %d\n", info.sb.ninodes);
  printf("  nlog        %d\n", info.sb.nlog);
  printf("  logstart    %d\n", info.sb.logstart);
  printf("  inodestart  %d\n", info.sb.inodestart);
  printf("  bmapstart   %d\n", info.sb.bmapstart);
  printf("  refstart    %d\n", info.sb.refstart);

  printf("inode:\n");
  printf("  inum        %d\n", info.inum);
  printf("  type        %d\n", info.type);
  printf("  nlink       %d\n", info.nlink);
  printf("  size        %d\n", info.size);

  for (int i = 0; i < 13; i++) {
    if (info.addrs[i] != 0)
      printf("  addr[%d]     %d\n", i, info.addrs[i]);
  }

  exit(0);
}
