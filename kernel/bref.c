// Block reference-count infrastructure for CS3500 File System Lab.
//
// Each data block has a ushort reference count stored in the refcount
// region of the disk. The functions below read and modify those counts
// through the buffer cache.

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

// Return the current reference count of block b.
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

// Set the reference count of block b to n.
// Must be called inside a transaction.
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

// Increment the reference count of block b. Returns the new count.
// Panics on overflow. Must be called inside a transaction.
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

// Decrement the reference count of block b. Returns the new count.
// Panics if the count is already zero. Must be called inside a transaction.
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

#endif // CLONE_OFF
