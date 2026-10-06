// bref.h -- block reference counts.
//
// The graded lab leaves CLONE_OFF defined.
// Removing it enables reference-count checks; cloning still needs to be implemented.
// While defined, balloc/bfree skip reference counts entirely.

#define CLONE_OFF

#ifndef CLONE_OFF

ushort brefget(uint dev, uint b);
void brefset(uint dev, uint b, ushort n);
ushort brefinc(uint dev, uint b);
ushort brefdec(uint dev, uint b);

#endif
