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

  printf("superblock: size %d nblocks %d ninodes %d nlog %d\n",
         info.sb.size, info.sb.nblocks, info.sb.ninodes, info.sb.nlog);
  printf("            logstart %d inodestart %d bmapstart %d refstart %d\n",
         info.sb.logstart, info.sb.inodestart, info.sb.bmapstart, info.sb.refstart);
  printf("\n");

  printf("inode %d: type %d nlink %d size %d\n",
         info.inum, info.type, info.nlink, info.size);

  // Print direct block addresses in a 3-column table.
  int rows = (NDIRECT + 2) / 3;
  for (int row = 0; row < rows; row++) {
    printf("  ");
    for (int col = 0; col < 3; col++) {
      int i = row + col * rows;
      if (i < NDIRECT)
        printf("addrs[%-2d] = %-5d ", i, info.addrs[i]);
    }
    printf("\n");
  }
  printf("  addrs[NDIRECT] = %d\n", info.addrs[NDIRECT]);

  exit(0);
}
