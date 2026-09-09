// bref.h -- block reference counts.
//
// Remove CLONE_OFF to enable clone support.
// While defined, balloc/bfree skip reference counts entirely.

#define CLONE_OFF

#ifndef CLONE_OFF

ushort brefget(uint dev, uint b);
void   brefset(uint dev, uint b, ushort n);
ushort brefinc(uint dev, uint b);
ushort brefdec(uint dev, uint b);

#endif
