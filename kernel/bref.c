// bref.c -- block reference-count helpers.
//
// Each data block has a ushort refcount in the refcount region on disk.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "buf.h"
#include "bref.h"

#ifndef CLONE_OFF

extern struct superblock sb;

ushort
brefget(uint dev, uint b)
{
  struct buf *bp;
  ushort *ref;
  ushort count;

  bp = bread(dev, RBBLOCK(b, sb));
  ref = (ushort *)bp->data;
  count = ref[b % REFPB];
  brelse(bp);
  return count;
}

void
brefset(uint dev, uint b, ushort n)
{
  struct buf *bp;
  ushort *ref;

  bp = bread(dev, RBBLOCK(b, sb));
  ref = (ushort *)bp->data;
  ref[b % REFPB] = n;
  log_write(bp);
  brelse(bp);
}

ushort
brefinc(uint dev, uint b)
{
  struct buf *bp;
  ushort *ref;
  ushort count;

  bp = bread(dev, RBBLOCK(b, sb));
  ref = (ushort *)bp->data;
  count = ref[b % REFPB];
  if (count == 0xFFFF)
    panic("brefinc: overflow");
  ref[b % REFPB] = count + 1;
  log_write(bp);
  brelse(bp);
  return count + 1;
}

ushort
brefdec(uint dev, uint b)
{
  struct buf *bp;
  ushort *ref;
  ushort count;

  bp = bread(dev, RBBLOCK(b, sb));
  ref = (ushort *)bp->data;
  count = ref[b % REFPB];
  if (count == 0)
    panic("brefdec: already zero");
  ref[b % REFPB] = count - 1;
  log_write(bp);
  brelse(bp);
  return count - 1;
}

#endif
