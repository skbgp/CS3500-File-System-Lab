import contextlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fs_grader as grader
from fs_image_check import Image, ImageError, prepare_full_disk

def fixture(path):
    """Create a valid shared 525-block tree independently of the kernel."""
    refs = {}
    blocks = {}
    nextblock = 673  # 672 holds root-directory data.
    def allocate(content=None):
        nonlocal nextblock
        b = nextblock
        nextblock += 1
        refs[b] = 1
        if content is not None:
            blocks[b] = content
        return b
    roots = [allocate() for _ in range(11)]
    single = allocate()
    blocks[single] = struct.pack("<256I", *[allocate() for _ in range(256)])
    double = allocate()
    children = []
    for n in (256, 2):
        child = allocate()
        values = [allocate() for _ in range(n)] + [0] * (256 - n)
        blocks[child] = struct.pack("<256I", *values)
        children.append(child)
    blocks[double] = struct.pack("<256I", *(children + [0] * 254))
    roots += [single, double]
    for b in roots:
        refs[b] = 2
    refs[672] = 1
    with open(path, "wb") as f:
        f.truncate(200000 * 1024)
        f.seek(1024)
        f.write(struct.pack("<9I", 0x10203040, 200000, 199328, 200, 241, 2, 243, 256, 281))
        def inode(n, kind, size, addrs):
            f.seek(243 * 1024 + n * 64)
            f.write(struct.pack("<4h14I", kind, 0, 0, 1, size, *addrs))
        inode(1, 1, 1024, [672] + [0] * 12)
        inode(2, 2, 525 * 1024, roots)
        inode(3, 2, 525 * 1024, roots)
        f.seek(672 * 1024)
        for ino, name in ((1, "."), (1, ".."), (2, "cl_src"), (3, "cl_dst"), (2, "fst_tree.tmp")):
            f.write(struct.pack("<H14s", ino, name.encode()))
        bitmap = bytearray(25 * 1024)
        for b in set(range(672)) | set(refs):
            bitmap[b // 8] |= 1 << (b % 8)
        f.seek(256 * 1024)
        f.write(bitmap)
        counts = bytearray(391 * 1024)
        for b, n in refs.items():
            struct.pack_into("<H", counts, b * 2, n)
        f.seek(281 * 1024)
        f.write(counts)
        for b, data in blocks.items():
            f.seek(b * 1024)
            f.write(data)
    return roots

class ImageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "fs.img"
        self.roots = fixture(self.path)

    def tearDown(self):
        self.tmp.cleanup()

    def test_full_disk_fixture_preserves_existing_files_and_leaves_space(self):
        with Image(self.path) as image:
            original = image.inode(2)
        prepare_full_disk(self.path)
        with Image(self.path) as image:
            self.assertEqual(image.size - len(image.allocated()), 16)
            image.audit(check_refcounts=False)
            self.assertEqual(image.inode(2), original)
            visited = set()
            for i in range(3):
                inode = image.lookup(f"fst_res{i}.tmp")
                mapping = image.mapping(inode)
                self.assertEqual(len(set(mapping)), len(mapping))
                self.assertFalse(visited.intersection(mapping))
                visited.update(mapping)
                self.assertTrue(set(mapping) <= image.allocated())

    def test_truncated_images_report_an_image_error(self):
        for size in (0, 1024, 200000 * 1024 - 1):
            with self.subTest(size=size):
                with open(self.path, "wb") as f:
                    f.truncate(size)
                with self.assertRaisesRegex(ImageError, "image size"):
                    Image(self.path)

    def test_recursive_counts_accept_valid_shared_tree(self):
        with Image(self.path) as image:
            image.check_clone()
            refs = image.audit()
            self.assertEqual(refs[self.roots[11]], 2)
            self.assertEqual(refs[image.pointers(self.roots[11])[0]], 1)

    def test_counts_per_file_instead_of_per_pointer_are_rejected(self):
        with Image(self.path) as image:
            child = image.pointers(self.roots[11])[0]
        with open(self.path, "r+b") as f:
            f.seek(281 * 1024 + child * 2)
            f.write(struct.pack("<H", 2))
        with Image(self.path) as image:
            with self.assertRaisesRegex(ImageError, "reference count"):
                image.audit()

    def test_missing_bitmap_allocation_is_rejected(self):
        b = self.roots[0]
        with open(self.path, "r+b") as f:
            offset = 256 * 1024 + b // 8
            f.seek(offset)
            byte = f.read(1)[0]
            f.seek(offset)
            f.write(bytes([byte & ~(1 << (b % 8))]))
        with Image(self.path) as image:
            with self.assertRaisesRegex(ImageError, "marked free"):
                image.audit()

    def test_eager_copy_is_rejected(self):
        with open(self.path, "r+b") as f:
            f.seek(243 * 1024 + 3 * 64 + 12)
            f.write(struct.pack("<I", 5000))
        with Image(self.path) as image:
            with self.assertRaisesRegex(ImageError, "rather than shared"):
                image.check_clone()

    def test_tree_requires_every_pointer_and_exact_indices(self):
        case = grader.Case("tree", 1, "", check="tree")
        with Image(self.path) as image:
            lines = image.tree_lines("fst_tree.tmp")
        output = "TREE_BEGIN\n" + "\n".join(lines) + "\nTREE_END\n"
        grader.validate(case, output, self.path, set(), set())
        bad = "TREE_BEGIN\n" + lines[0] + "\nTREE_END\n"
        with self.assertRaisesRegex(ImageError, "printed tree differs"):
            grader.validate(case, bad, self.path, set(), set())
        wrong = output.replace(".. ..255:", ".. ..254:", 1)
        with self.assertRaises(ImageError):
            grader.validate(case, wrong, self.path, set(), set())

    def test_cleanup_checks_allocation_leaks(self):
        with Image(self.path) as image:
            allocations = image.allocated()
            inodes = grader.active_inodes(image)
        case = grader.Case("cleanup", 1, "", check="reclaim")
        grader.validate(case, "", self.path, allocations, inodes)
        with self.assertRaisesRegex(ImageError, "allocated blocks"):
            grader.validate(case, "", self.path, allocations - {self.roots[0]}, inodes)

    def test_boot_console_is_the_only_allowed_new_inode(self):
        with Image(self.path) as image:
            before = grader.active_inodes(image)
            allocations = image.allocated()
        with open(self.path, "r+b") as f:
            f.seek(243 * 1024 + 4 * 64)
            f.write(struct.pack("<4h14I", 3, 1, 1, 1, 0, *([0] * 13)))
            f.seek(672 * 1024 + 5 * 16)
            f.write(struct.pack("<H14s", 4, b"console"))
        case = grader.Case("cleanup", 1, "", check="reclaim")
        grader.validate(case, "", self.path, allocations, before)
        with open(self.path, "r+b") as f:
            f.seek(243 * 1024 + 5 * 64)
            f.write(struct.pack("<4h14I", 2, 0, 0, 1, 0, *([0] * 13)))
        with self.assertRaisesRegex(ImageError, "inode leaked"):
            grader.validate(case, "", self.path, allocations, before)

class GraderTests(unittest.TestCase):
    def test_public_suite_scores_required_work_only(self):
        self.assertEqual(sum(case.points for case in grader.PUBLIC), 100)
        self.assertEqual(len({case.name for case in grader.PUBLIC}), len(grader.PUBLIC))
        self.assertFalse(any("clone" in case.name for case in grader.PUBLIC))

    def test_failed_cases_produce_nonzero_status_and_json(self):
        with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stdout(io.StringIO()):
            report = Path(tmp) / "result.json"
            with patch.object(grader, "run_case", side_effect=grader.TestError("known failure")):
                status = grader.main(["--only", "tree_direct", "--json", str(report)])
            data = json.loads(report.read_text())
        self.assertEqual(status, 1)
        self.assertEqual(data["score"], 0)
        self.assertFalse(data["passed"])
        self.assertTrue(data["partial"])

    def test_successful_cases_return_zero(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(grader, "ROOT", Path(tmp)), \
             contextlib.redirect_stdout(io.StringIO()), patch.object(grader, "run_case"):
            self.assertEqual(grader.main(["--only", "tree_direct"]), 0)
            data = json.loads((Path(tmp) / "grade-results.json").read_text())
            self.assertEqual(data["score"], 10)

    def test_build_failure_is_not_ignored(self):
        failed = subprocess.CompletedProcess([], 2, stdout="compiler failed")
        with patch.object(grader.subprocess, "run", return_value=failed):
            with self.assertRaisesRegex(grader.TestError, "build failed"):
                grader.build(Path("/tmp/test-image-unused.img"))

    def guest(self, body, timeout=2):
        real_popen = subprocess.Popen
        def launch(*args, **kwargs):
            return real_popen([sys.executable, "-u", "-c", body], **kwargs)
        with patch.object(grader.subprocess, "Popen", side_effect=launch):
            return grader.run_guest(Path("/tmp/unused.img"), "fake_test", timeout)

    def test_runner_waits_for_prompt_and_returns_child_status(self):
        body = (
            "import sys,time\n"
            "time.sleep(.1)\n"
            "print('$ ',end='',flush=True)\n"
            "args=sys.stdin.readline().split()\n"
            "print('fake_test: passed')\n"
            "print('\\nLAB_RESULT '+args[1]+' 7',flush=True)\n"
            "time.sleep(5)\n"
        )
        status, output = self.guest(body)
        self.assertEqual(status, 7)
        self.assertIn("fake_test: passed", output)

    def test_runner_detects_kernel_panic(self):
        with self.assertRaisesRegex(grader.TestError, "kernel panic"):
            self.guest("import time\nprint('panic: bad block',flush=True)\ntime.sleep(5)")

    def test_runner_fails_on_early_exit(self):
        with self.assertRaisesRegex(grader.TestError, "exited"):
            self.guest("print('boot failed',flush=True)")

    def test_runner_has_bounded_boot_timeout(self):
        with self.assertRaisesRegex(grader.TestError, "boot timed out"):
            self.guest("import time\ntime.sleep(5)", timeout=.2)

if __name__ == "__main__":
    unittest.main()
