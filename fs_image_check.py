"""Independent reader for the lab's 1 KiB xv6 disk format.

This code never calls the student's inspection syscall to decide correctness.
"""
from collections import Counter
import mmap
import struct

BSIZE = 1024
NDIRECT = 11
MAXFILE = 65803

class ImageError(ValueError):
    pass

def require(ok, message):
    if not ok:
        raise ImageError(message)

class Image:
    def __init__(self, path):
        self.file = open(path, "rb")
        try:
            require(self.file.seek(0, 2) == 200000 * BSIZE, "wrong filesystem/image size")
            self.data = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
            fields = struct.unpack_from("<9I", self.data, BSIZE)
            (self.magic, self.size, self.nblocks, self.ninodes, self.nlog,
             self.logstart, self.inodestart, self.bmapstart, self.refstart) = fields
            require(self.magic == 0x10203040, "wrong filesystem magic")
            require(self.size == 200000 and len(self.data) == self.size * BSIZE,
                    "wrong filesystem/image size")
            self.first = self.size - self.nblocks
            require((self.nlog, self.logstart, self.inodestart,
                     self.bmapstart, self.refstart, self.first) ==
                    (241, 2, 243, 256, 281, 672), "unexpected disk layout")
        except Exception:
            self.close()
            raise

    def close(self):
        if hasattr(self, "data"):
            self.data.close()
        self.file.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def block(self, number):
        require(0 <= number < self.size, "out-of-range block")
        return self.data[number * BSIZE:(number + 1) * BSIZE]

    def pointers(self, number):
        require(self.first <= number < self.size, "pointer outside allocatable region")
        return struct.unpack("<256I", self.block(number))

    def inode(self, number):
        require(0 < number < self.ninodes, "invalid inode number")
        offset = self.inodestart * BSIZE + number * 64
        values = struct.unpack_from("<4h14I", self.data, offset)
        return dict(inum=number, type=values[0], nlink=values[3],
                    size=values[4], addrs=values[5:])

    def mapping(self, inode):
        """Return logical data slots, including holes, up to file size."""
        require(inode["size"] <= MAXFILE * BSIZE, "file exceeds maximum size")
        slots = list(inode["addrs"][:NDIRECT])
        single, double = inode["addrs"][11:]
        slots.extend(self.pointers(single) if single else [0] * 256)
        if double:
            for child in self.pointers(double):
                slots.extend(self.pointers(child) if child else [0] * 256)
        needed = (inode["size"] + BSIZE - 1) // BSIZE
        require(len(slots) >= needed, "missing doubly-indirect region")
        slots = slots[:needed]
        require(all(self.first <= b < self.size for b in slots), "hole or invalid data pointer")
        return slots

    def contents(self, inode):
        return b"".join(self.block(b) for b in self.mapping(inode))[:inode["size"]]

    def lookup(self, name):
        # Test fixtures live directly in the root.
        entries = self.contents(self.inode(1))
        for off in range(0, len(entries), 16):
            ino, raw = struct.unpack_from("<H14s", entries, off)
            if ino and raw.split(b"\0", 1)[0].decode("ascii", errors="replace") == name:
                return self.inode(ino)
        raise ImageError("missing fixture: " + name)

    def allocated(self):
        start = self.bmapstart * BSIZE
        return {b for b in range(self.size)
                if self.data[start + b // 8] & (1 << (b % 8))}

    def audit(self, check_refcounts=True):
        """Check immediate-pointer reference counts and bitmap, including metadata sharing."""
        refs = Counter()
        levels = {}
        def edge(block, level):
            if not block:
                return
            require(self.first <= block < self.size, "reserved or invalid block referenced")
            refs[block] += 1
            if block in levels:
                require(levels[block] == level, "block has incompatible data/pointer roles")
                return
            levels[block] = level
            if level:
                for child in self.pointers(block):
                    edge(child, level - 1)
        for n in range(1, self.ninodes):
            inode = self.inode(n)
            if inode["type"] == 0:
                continue
            require(inode["type"] in (1, 2, 3, 4), "unknown inode type")
            require(inode["nlink"] >= 0, "negative link count")
            if inode["type"] != 3:
                self.mapping(inode)
            for b in inode["addrs"][:11]:
                edge(b, 0)
            edge(inode["addrs"][11], 1)
            edge(inode["addrs"][12], 2)
        allocated = self.allocated()
        require(set(range(self.first)) <= allocated, "reserved block marked free")
        require(allocated - set(range(self.first)) == set(refs),
                "leaked allocation or referenced block marked free")
        if not check_refcounts:
            return refs
        for b in range(self.first, self.size):
            actual = struct.unpack_from("<H", self.data, self.refstart * BSIZE + b * 2)[0]
            require(actual == refs[b], f"block {b}: reference count {actual}, expected {refs[b]}")
        return refs

    def tree_lines(self, name):
        ip = self.lookup(name)
        lines = [f'inode {ip["inum"]} type {ip["type"]} size {ip["size"]}']
        for i, b in enumerate(ip["addrs"][:11]):
            if b:
                lines.append(f"..{i}: {b}")
        single, double = ip["addrs"][11:]
        if single:
            lines.append(f"..indirect: {single}")
            for i, b in enumerate(self.pointers(single)):
                if b:
                    lines.append(f".. ..{i}: {b}")
        if double:
            lines.append(f"..double-indirect: {double}")
            for i, child in enumerate(self.pointers(double)):
                if child:
                    lines.append(f".. ..{i}: {child}")
                    for j, b in enumerate(self.pointers(child)):
                        if b:
                            lines.append(f".. .. ..{j}: {b}")
        return lines

    def check_clone(self, cow=False):
        a, b = self.lookup("cl_src"), self.lookup("cl_dst")
        require(a["inum"] != b["inum"] and a["nlink"] == b["nlink"] == 1,
                "clone must have a distinct inode and one link")
        require(a["type"] == b["type"] == 2 and a["size"] == b["size"] == 525 * BSIZE,
                "clone type or size differs")
        if not cow:
            require(a["addrs"] == b["addrs"], "clone copied rather than shared its roots")
        else:
            left, right = self.mapping(a), self.mapping(b)
            changed = {0, 1, 11, 267, 523}
            for n, (x, y) in enumerate(zip(left, right)):
                require((x != y) == (n in changed), f"unexpected sharing for logical block {n}")
            require(a["addrs"][11] != b["addrs"][11] and
                    a["addrs"][12] != b["addrs"][12], "shared pointer block was modified")
        self.audit()

def prepare_full_disk(path, free_blocks=16):
    """Reserve space with ordinary files before booting the failure test."""
    with Image(path) as disk:
        allocated = disk.allocated()
        available = [b for b in range(disk.first, disk.size) if b not in allocated]
        require(0 < free_blocks < len(available), "invalid free-space fixture")
        empty_inodes = [n for n in range(1, disk.ninodes)
                        if disk.inode(n)["type"] == 0]
        root = disk.inode(1)
        require(root["size"] == BSIZE, "fixture requires a one-block root")
        directory = bytearray(disk.contents(root))
        empty_entries = [off for off in range(0, BSIZE, 16)
                         if struct.unpack_from("<H", directory, off)[0] == 0]
        inode_start, bitmap_start = disk.inodestart, disk.bmapstart
        root_block = root["addrs"][0]
        size = disk.size
    pointers, inodes = {}, []
    cursor = 0
    def allocate(pointer=False):
        nonlocal cursor
        block = available[cursor]
        cursor += 1
        allocated.add(block)
        if pointer:
            pointers[block] = [0] * 256
        return block
    while len(available) - cursor > free_blocks:
        require(len(inodes) < min(len(empty_inodes), len(empty_entries)),
                "not enough names or inodes for the full-disk fixture")
        roots = [0] * 13
        count = 0
        while count < MAXFILE and len(available) - cursor > free_blocks:
            if count < 11:
                needed = 1
            elif count < 267:
                needed = 1 + (roots[11] == 0)
            else:
                outer = (count - 267) // 256
                needed = 1 + (roots[12] == 0)
                needed += roots[12] == 0 or pointers[roots[12]][outer] == 0
            if len(available) - cursor - free_blocks < needed:
                break
            if count < 11:
                roots[count] = allocate()
            elif count < 267:
                if roots[11] == 0:
                    roots[11] = allocate(pointer=True)
                pointers[roots[11]][count - 11] = allocate()
            else:
                outer, inner = divmod(count - 267, 256)
                if roots[12] == 0:
                    roots[12] = allocate(pointer=True)
                if pointers[roots[12]][outer] == 0:
                    pointers[roots[12]][outer] = allocate(pointer=True)
                child = pointers[roots[12]][outer]
                pointers[child][inner] = allocate()
            count += 1
        require(count > 0, "full-disk fixture made no progress")
        number = empty_inodes[len(inodes)]
        offset = empty_entries[len(inodes)]
        name = f"fst_res{len(inodes)}.tmp".encode()
        struct.pack_into("<H14s", directory, offset, number, name)
        inodes.append((number, count, roots))
    bitmap = bytearray(((size + BSIZE * 8 - 1) // (BSIZE * 8)) * BSIZE)
    for b in allocated:
        bitmap[b // 8] |= 1 << (b % 8)
    with open(path, "r+b") as file:
        for b, entries in pointers.items():
            file.seek(b * BSIZE)
            file.write(struct.pack("<256I", *entries))
        for number, count, roots in inodes:
            file.seek(inode_start * BSIZE + number * 64)
            file.write(struct.pack("<4h14I", 2, 0, 0, 1, count * BSIZE, *roots))
        file.seek(root_block * BSIZE)
        file.write(directory)
        file.seek(bitmap_start * BSIZE)
        file.write(bitmap)
