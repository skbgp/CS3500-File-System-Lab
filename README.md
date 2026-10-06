# CS3500 Lab 7: File System

Release: 12 October 2026  
Due: 1 November 2026, 11:59 PM IST

Follow the handout on Moodle for the tasks and submission requirements.

## Setup

Start Docker Desktop, then run:

```sh
git clone --branch cs3500-filesystem-starter https://github.com/skbgp/CS3500-File-System-Lab.git
cd CS3500-File-System-Lab
docker run --rm -it --tmpfs /tmp -v "$PWD":/home/xv6-labs \
    -w /home/xv6-labs nandhagk/xv6-tools:latest sh
make qemu
```

`$PWD` is your cloned directory; `/home/xv6-labs` is where Docker mounts it.
Leave `CLONE_OFF` defined for the required tasks.

## Testing

Exit QEMU before running these commands in the container shell:

```sh
python3 grade-lab-fs --list
python3 grade-lab-fs --only large_boundaries
make grade
```

Use selected cases while working and the full grader before submission.
Feature tests fail until implemented. A full run may take 15–25 minutes.
The grader shows marks for each case and saves subtest results and failure
details in `grade-results.json`.

## Submission

In your host terminal, commit your source changes, `answers-fs.txt`, `time.txt`,
`reference.txt`, and any required prompt logs. Run `make zipball`, then rename
`lab.zip` to your roll number, for example `CS24B046.zip`, and upload it to Moodle.
Only committed files are included. Do not submit binaries or `fs.img`.
