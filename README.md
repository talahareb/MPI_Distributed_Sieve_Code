# MPI Pipelined Prime Sieve (C)

An educational **parallel-computing project** that uses the Message Passing Interface (MPI) to process prime-number candidates through a pipeline of communicating processes. The goal is to demonstrate **interprocess communication, filtering stages, synchronized result collection, and parallel-program correctness** in C.

## How it works

1. **Rank 0 — producer:** sends each integer from `2` through the requested limit to the first worker using `MPI_Send`.
2. **Intermediate ranks — filters:** each stage keeps the first prime number it receives and filters out multiples of that prime before sending surviving candidates downstream.
3. **Final rank — completes the sieve:** checks remaining candidates against the additional prime numbers it discovers locally. This ensures correct results even when there are more primes than MPI worker processes.
4. **Rank 0 — results:** collects all primes using `MPI_Gather` and `MPI_Gatherv`, sorts them, and displays the count and values.

A special end marker (`-1`) travels through the pipeline so each stage can finish cleanly.

```text
Rank 0            Rank 1                 Rank 2               Final rank
Producer  --->    filter multiples 2 --> filter multiples 3 --> verify survivors
   ^                (prime 2)              (prime 3)            (other primes)
   |                      |                    |                      |
   +----------------------+- MPI_Gatherv ------+----------------------+
```

The first primes assigned to intermediate stages depend on the number of processes. The last stage uses a growing local prime list to finish the calculation.

## Technology

- **C11** for the implementation
- **MPI** (`MPI_Send`, `MPI_Recv`, `MPI_Gather`, `MPI_Gatherv`, `MPI_Barrier`) for distributed communication and synchronization
- **Python 3**, optionally, for the integration-check script

## Repository contents

```text
Pipelined_Sieve_Code/
├── pipelined_sieve.c       # MPI prime-number pipeline
├── Makefile                # Build target using mpicc
├── tests/
│   └── test_sieve.py       # Checks results with multiple MPI process counts
├── .gitignore
└── README.md
```

## Build and run

Install an MPI implementation such as **Open MPI** or **MPICH** with its C compiler wrapper `mpicc` and launcher `mpirun` (or `mpiexec`).

```bash
make
mpirun -np 4 ./pipelined_sieve 100
```

Alternatively, compile directly:

```bash
mpicc -std=c11 -O2 -Wall -Wextra -Wpedantic pipelined_sieve.c -o pipelined_sieve
mpirun -np 4 ./pipelined_sieve 100
```

`-np 4` requests four MPI ranks: one producer and three filtering workers. The input limit must be an integer **at least 2**, and the program requires at least **two MPI ranks**.

For a limit of `100`, the correct count is **25 primes**. The program prints all primes for limits up to `1000`; for larger limits it prints the first 20 and the overall count. Output timing includes the pipeline and gathering results.

## Testing

Once `make` succeeds, the optional integration test compares the program output with a small reference sieve over several limits and MPI process counts:

```bash
python3 tests/test_sieve.py
```

The test needs a working MPI installation. A successful compile and benchmark should be verified on your own machine or cluster; no cluster-scale performance results are claimed here.

## Design decisions and limitations

- This is a **pipelined teaching example**, not a high-performance distributed sieve. It sends one integer per MPI message, which introduces communication overhead.
- The final stage handles the remaining prime checks locally, so the computational work is **not evenly balanced** across ranks.
- Adding MPI processes does **not** necessarily speed up computation. Run with multiple limits and ranks to evaluate actual performance.
- The algorithm uses standard blocking MPI sends/receives and reports elapsed time after all primes have been collected.
- This revised version replaces an earlier `w2.c` draft that had an extra final-rank receive loop and could hang after the end marker. The original draft remains in the older GitHub commit history.

## Learning outcomes

This project demonstrates message-passing fundamentals, a streaming filter pipeline, distributed termination handling, dynamic memory management, and the distinction between correct parallel algorithms and their real-world performance.

**Keywords:** C · MPI · Parallel Computing · Distributed Algorithms · Prime Sieve · Message Passing
