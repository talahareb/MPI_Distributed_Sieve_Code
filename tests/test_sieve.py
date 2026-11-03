"""Small integration checks for the MPI pipelined sieve.

Requires Open MPI/MPICH, a compiled ./pipelined_sieve executable, and
an MPI launcher named mpirun or mpiexec on PATH.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = ROOT / ("pipelined_sieve.exe" if os.name == "nt" else "pipelined_sieve")


def expected_primes(limit: int) -> list[int]:
    sieve = [True] * (limit + 1)
    sieve[0] = sieve[1] = False
    for p in range(2, int(limit**0.5) + 1):
        if sieve[p]:
            for composite in range(p * p, limit + 1, p):
                sieve[composite] = False
    return [n for n in range(2, limit + 1) if sieve[n]]


def main() -> int:
    launcher = shutil.which("mpirun") or shutil.which("mpiexec")
    if not launcher or not EXECUTABLE.exists():
        print("Missing MPI launcher or compiled executable. Run 'make' first.")
        return 1

    # Open MPI permits this in a container running as root; no effect for MPICH.
    env = os.environ.copy()
    if hasattr(os, "geteuid") and os.geteuid() == 0:
        env.setdefault("OMPI_ALLOW_RUN_AS_ROOT", "1")
        env.setdefault("OMPI_ALLOW_RUN_AS_ROOT_CONFIRM", "1")

    cases = [(2, 2), (3, 3), (4, 10), (2, 100), (3, 100), (4, 100), (5, 30), (4, 1000)]
    for processes, limit in cases:
        proc = subprocess.run(
            [launcher, "-np", str(processes), str(EXECUTABLE), str(limit)],
            cwd=ROOT, env=env, capture_output=True, text=True, timeout=60,
        )
        if proc.returncode != 0:
            print(f"FAIL np={processes}, limit={limit}: {proc.stderr}")
            return 1
        result = re.search(r"^Primes:((?: \d+)*)$", proc.stdout, re.MULTILINE)
        if not result:
            print(f"FAIL np={processes}, limit={limit}: primes not found in output\n{proc.stdout}")
            return 1
        got = [int(n) for n in result.group(1).split()]
        expected = expected_primes(limit)
        if got != expected:
            print(f"FAIL np={processes}, limit={limit}\nExpected: {expected}\nGot: {got}")
            return 1
        print(f"PASS np={processes}, limit={limit}, primes={len(got)}")
    print("All MPI integration checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
