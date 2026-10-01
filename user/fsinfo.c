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

  for (int i = 0; i < NDIRECT; i++)
    printf("  addrs[%d] = %d\n", i, info.addrs[i]);
  printf("  indirect = %d\n", info.addrs[NDIRECT]);
  if (NDIRECT == 11)
    printf("  double-indirect = %d\n", info.addrs[NDIRECT + 1]);

  exit(0);
}
