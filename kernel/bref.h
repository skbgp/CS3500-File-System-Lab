// Block reference-count interface for CS3500 File System Lab.
//
// Remove the CLONE_OFF define below to enable clone support.
// With CLONE_OFF defined, balloc/bfree do not touch reference counts
// and the clone system call is not compiled.

#define CLONE_OFF

#ifndef CLONE_OFF

ushort brefget(uint dev, uint b);
void   brefset(uint dev, uint b, ushort n);
ushort brefinc(uint dev, uint b);
ushort brefdec(uint dev, uint b);

#endif // CLONE_OFF
