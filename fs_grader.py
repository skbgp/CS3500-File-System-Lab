#!/usr/bin/env python3
"""CS3500 filesystem grader: fresh images, bounded QEMU, independent disk checks."""
import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import secrets
import selectors
import signal
import subprocess
import tempfile
import time

from fs_image_check import Image, ImageError, require, prepare_full_disk

ROOT = Path(__file__).resolve().parent
MAX_OUTPUT = 32 * 1024 * 1024

@dataclass(frozen=True)
class Case:
    name: str
    points: int
    command: str
    marker: str = ""
    check: str = ""
    timeout: int = 300

PUBLIC = (
    Case("tree_direct", 10, "itreetest 3", check="tree"),
    Case("tree_indirect", 10, "itreetest 525", check="tree"),
    Case("large_boundaries", 15, "bigfiletest", "bigfiletest: passed", "reclaim"),
    Case("large_maximum", 20, "bigfile", "bigfile: passed", "reclaim", 1200),
    Case("truncation", 15, "trunctest", "trunctest: passed", "reclaim"),
    Case("symlinks", 20, "symlinktest", "symlinktest: passed", "reclaim"),
    Case("symlink_failure", 5, "symlinkfail", "symlinkfail: passed", "reclaim", 300),
    Case("regression", 5, "usertests -q", "ALL TESTS PASSED", timeout=1200),
)
PRIVATE = (
    Case("private_maximum", 15, "bigfile", "bigfile: passed", "reclaim", 1200),
    Case("private_truncation", 15, "trunctest", "trunctest: passed", "reclaim"),
    Case("private_nofollow", 10, "nofollowtest", "nofollowtest: passed", "reclaim"),
    Case("private_cycle", 10, "symlinkloop", "symlinkloop: passed", "reclaim"),
)

class TestError(RuntimeError):
    pass

def stop_process(proc):
    """Kill the make/QEMU process group, including children after make exits."""
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        pass
    finally:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.wait(timeout=3)
        if proc.stdin:
            proc.stdin.close()
        if proc.stdout:
            proc.stdout.close()

def run_guest(image, command, timeout):
    token = secrets.token_hex(8)
    args = ["make", "--no-print-directory", "CPUS=3", f"FS_IMAGE={image}",
            "grading-qemu" if os.environ.get("CS3500_GRADING_TARGETS") else "qemu"]
    proc = subprocess.Popen(args, cwd=ROOT, stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            start_new_session=True, bufsize=0)
    output = bytearray()
    sent = False
    started = time.monotonic()
    deadline = started + min(timeout, 120)
    with selectors.DefaultSelector() as selector:
        selector.register(proc.stdout, selectors.EVENT_READ)
        try:
            while time.monotonic() < deadline:
                for key, _ in selector.select(timeout=0.2):
                    chunk = os.read(key.fileobj.fileno(), 65536)
                    if chunk:
                        output.extend(chunk)
                        if len(output) > MAX_OUTPUT:
                            raise TestError("guest output limit exceeded")
                    elif proc.poll() is not None:
                        raise TestError("QEMU exited before reporting a test result")
                text = output.decode("utf-8", errors="replace").replace("\r", "")
                if re.search(r"(?:^|\n)panic:", text):
                    raise TestError("kernel panic\n" + text[-2000:])
                if not sent and re.search(r"(?:^|\n)\$ $", text):
                    payload = f"labrun {token} {command}\n".encode()
                    proc.stdin.write(payload)
                    sent = True
                    deadline = time.monotonic() + timeout
                result = re.search(rf"(?:^|\n)LAB_RESULT {token} (-?\d+)\n", text)
                if result:
                    return int(result.group(1)), text
                if proc.poll() is not None:
                    raise TestError("QEMU exited before reporting a test result\n" + text[-2000:])
            stage = "test" if sent else "boot"
            raise TestError(f"{stage} timed out\n" + output.decode(errors="replace")[-2000:])
        finally:
            stop_process(proc)

def build(image):
    result = subprocess.run(
        ["make", "--no-print-directory", "-j2", f"FS_IMAGE={image}",
         "kernel/kernel",
         "grading-image" if os.environ.get("CS3500_GRADING_TARGETS") else str(image)], cwd=ROOT, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, timeout=180)
    if result.returncode:
        raise TestError("build failed\n" + result.stdout[-6000:])

def active_inodes(image):
    return {n for n in range(1, image.ninodes) if image.inode(n)["type"]}

def expected_inodes_after_boot(image, before):
    """xv6 init creates /console on first boot if mkfs did not include it."""
    try:
        console = image.lookup("console")
    except ImageError:
        return before
    if console["inum"] not in before:
        require(console["type"] == 3 and console["nlink"] == 1 and
                console["size"] == 0 and not any(console["addrs"]),
                "unexpected inode created at boot")
    return before | {console["inum"]}

def validate(case, output, image, allocated_before, inodes_before):
    if case.marker:
        require(re.search(r"^" + re.escape(case.marker) + r"$", output, re.MULTILINE),
                "missing success marker")
    if not case.check:
        return
    with Image(image) as disk:
        if case.check == "tree":
            match = re.search(r"(?:^|\n)TREE_BEGIN\n(.*?)\nTREE_END(?:\n|$)", output, re.S)
            require(match is not None, "missing complete tree output")
            actual = match.group(1).splitlines()
            expected = disk.tree_lines("fst_tree.tmp")
            require(actual == expected, "printed tree differs from independently read on-disk tree")
        elif case.check in ("share", "cow"):
            disk.check_clone(cow=case.check == "cow")
        elif case.check in ("reclaim", "clone_reclaim"):
            require(disk.allocated() == allocated_before, "allocated blocks not restored after cleanup")
            require(active_inodes(disk) == expected_inodes_after_boot(disk, inodes_before),
                    "inode leaked after cleanup")
            if case.check == "clone_reclaim":
                disk.audit()

def run_case(case):
    # Every test gets its own image; the user's fs.img is never touched.
    with tempfile.TemporaryDirectory(prefix="cs3500-grade-") as tmp:
        image = Path(tmp) / "fs.img"
        build(image)
        if case.name == "symlink_failure":
            prepare_full_disk(image)
        with Image(image) as disk:
            before, inodes = disk.allocated(), active_inodes(disk)
        status, output = run_guest(image, case.command, case.timeout)
        if status != 0:
            raise TestError(f"guest exit status {status}\n" + output[-2500:])
        validate(case, output, image, before, inodes)

def main(argv=None, private=False):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="list available cases")
    parser.add_argument("--only", help="comma-separated case names (partial run)")
    parser.add_argument("--baseline", action="store_true", help="run only stock regression checks")
    parser.add_argument("--json", type=Path, default=ROOT / "grade-results.json",
                        help="results file (default: grade-results.json)")
    options = parser.parse_args(argv)
    cases = PRIVATE if private else PUBLIC
    if options.baseline and (private or options.only):
        parser.error("--baseline cannot be combined with private tests or --only")
    partial = options.baseline or bool(options.only)
    if options.baseline:
        cases = tuple(c for c in PUBLIC if c.name == "regression")
    if options.only:
        wanted = options.only.split(",")
        unknown = set(wanted) - {c.name for c in cases}
        if unknown:
            parser.error("unknown cases: " + ", ".join(sorted(unknown)))
        cases = tuple(c for c in cases if c.name in wanted)
    if options.list:
        for case in cases:
            print(f"{case.name}: {case.points} points; {case.command}")
        return 0
    results = []
    for case in cases:
        start = time.monotonic()
        print(f"{case.name}: ", end="", flush=True)
        try:
            run_case(case)
            ok, detail = True, ""
        except (TestError, ImageError, OSError, subprocess.SubprocessError) as exc:
            ok, detail = False, str(exc)
        elapsed = round(time.monotonic() - start, 2)
        results.append(dict(name=case.name, passed=ok, points=case.points,
                            seconds=elapsed, detail=detail))
        reason = " (" + detail.splitlines()[0][:160] + ")" if detail else ""
        print(f"{'PASS' if ok else 'FAIL'} ({case.points if ok else 0}/{case.points})" + reason, flush=True)
    score = sum(r["points"] for r in results if r["passed"])
    total = sum(r["points"] for r in results)
    passed = bool(results) and all(r["passed"] for r in results)
    report = dict(score=score, total=total, passed=passed, partial=partial, results=results)
    options.json.parent.mkdir(parents=True, exist_ok=True)
    options.json.write_text(json.dumps(report, indent=2) + "\n")
    label = "Private" if private else "Public"
    if partial:
        label += " (selected)"
    print(f"{label}: {score}/{total}", flush=True)
    if not passed:
        print(f"Details: {options.json}", flush=True)
    return 0 if passed else 1

if __name__ == "__main__":
    raise SystemExit(main())
