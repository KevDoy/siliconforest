# Darwin Apple Silicon: pre-development starter

Prepared for Chris Kearney on November 7, 2026. This is a local starter bundle, not a published GitHub repository or a bootable OS. The directory name is a working label, not a product-name decision.

## What exists

A candidate source registry imported from the earlier audit; proposed architecture and milestones; a target inventory template; porting contracts; a task backlog; and a small, standard-library-only Python utility with offline tests. The original audit and manifest are preserved in `archive/`.

The utility validates metadata, prints a blocked hardware-test plan, hashes regular files, and summarizes synthetic or explicitly unverified log observations. It has no network, SSH, USB, flashing, boot, source-checkout, or subprocess execution implementation.

**No XNU, m1n1, Mesa, PureDarwin, or Chonkstep build has been run for this bundle. No real target has been booted. Source pins were imported, not independently revalidated.** A passing helper test is not an operating-system milestone.

## Machine roles

The repaired Linux machine will build and run development tests. The M5 Mac remains the USB/proxy/debug controller. A separate Apple Silicon target remains unspecified. Nothing here requires writing to or accessing the Btrfs filesystem under repair.

## Offline quick start

Use an existing Python 3.10+ interpreter on a healthy machine. These commands require no package installation:

```sh
python3 -m unittest discover -s tests -v
python3 tools/lab.py validate
python3 tools/lab.py plan
python3 tools/lab.py inspect-log fixtures/entry-console.jsonl --run-id fixture-run --kind fixture
python3 tools/lab.py hash README.md
```

`validate` checks syntax and local metadata consistency, not remote repository existence. `plan` returns `BLOCKED`, intentionally. `inspect-log` reports observations, never a hardware PASS. All commands print JSON or test output and do not modify an existing machine configuration.

## Start reading

- `docs/PREPARATION_PLAN.md`: work possible now and work that must wait.
- `docs/ARCHITECTURE.md`: working decisions and unresolved choices.
- `docs/FIRST_LINUX_SESSION.md`: future setup sequence, after recovery.
- `specs/BOOT_CONTRACT.md`: boot handoff and test-controller boundaries.
- `specs/GRAPHICS_CONTRACT.md`: memory and ABI acceptance tests.
- `docs/EXPERIENCE_BRIEF.md`: product goals and application milestones.
- `planning/tasks.json`: ordered tasks and acceptance criteria.
- `AGENTS.md`: contributor and coding-agent constraints.

No third-party driver implementation, SDK, firmware, or API credentials are bundled. Project licensing and redistribution decisions remain open; do not infer license clearance from this scaffold.
