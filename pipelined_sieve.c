/*
 * Pipelined prime sieve using MPI.
 *
 * Rank 0 generates candidate integers from 2 through limit.
 * Each intermediate worker holds one prime and discards its multiples.
 * The final worker checks the surviving numbers against all further primes
 * found locally. All ranks contribute their discovered primes to rank 0.
 *
 * Educational implementation: one MPI message per candidate; optimized for
 * demonstrating a pipeline, not for maximum throughput.
 */

#include <errno.h>
#include <limits.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define END_MARKER (-1)
#define MESSAGE_TAG 0

typedef struct {
    int *values;
    int count;
    int capacity;
} PrimeList;

static void append_prime(PrimeList *primes, int value) {
    if (primes->count == primes->capacity) {
        int new_capacity;
        if (primes->capacity == 0) {
            new_capacity = 16;
        } else if (primes->capacity > INT_MAX / 2) {
            fprintf(stderr, "Too many primes to store in one MPI rank.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return;
        } else {
            new_capacity = primes->capacity * 2;
        }
        int *expanded = realloc(primes->values, (size_t)new_capacity * sizeof(*expanded));
        if (expanded == NULL) {
            fprintf(stderr, "Could not allocate memory for the prime list.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            return;
        }
        primes->values = expanded;
        primes->capacity = new_capacity;
    }
    primes->values[primes->count++] = value;
}

static bool is_prime_after_upstream_filters(int candidate, const PrimeList *local_primes) {
    /* Smaller primes handled by upstream ranks have already filtered out
       their multiples. Check only the additional primes found at this rank. */
    for (int i = 0; i < local_primes->count; ++i) {
        int p = local_primes->values[i];
        if (p > candidate / p) {
            break;
        }
        if (candidate % p == 0) {
            return false;
        }
    }
    return true;
}

static int compare_ints(const void *lhs, const void *rhs) {
    int a = *(const int *)lhs;
    int b = *(const int *)rhs;
    return (a > b) - (a < b);
}

static bool parse_limit(const char *text, int *limit) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 2 || value > INT_MAX) {
        return false;
    }
    *limit = (int)value;
    return true;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank = -1;
    int world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int limit = 0;
    if (argc != 2 || !parse_limit(argv[1], &limit) || world_size < 2) {
        if (rank == 0) {
            fprintf(stderr,
                    "Usage: mpirun -np <processes>=2 ./pipelined_sieve <limit>=2\n"
                    "Example: mpirun -np 4 ./pipelined_sieve 100\n");
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    PrimeList local = {NULL, 0, 0};
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    if (rank == 0) {
        /* Source of the pipeline. Only workers collect prime values. */
        for (int candidate = 2; ; ++candidate) {
            MPI_Send(&candidate, 1, MPI_INT, 1, MESSAGE_TAG, MPI_COMM_WORLD);
            if (candidate == limit) {
                break;  /* Avoid overflowing int when limit == INT_MAX. */
            }
        }
        int marker = END_MARKER;
        MPI_Send(&marker, 1, MPI_INT, 1, MESSAGE_TAG, MPI_COMM_WORLD);
    } else {
        const bool is_last_worker = (rank == world_size - 1);
        int stage_prime = 0;

        while (true) {
            int candidate = END_MARKER;
            MPI_Recv(&candidate, 1, MPI_INT, rank - 1, MESSAGE_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (candidate == END_MARKER) {
                if (!is_last_worker) {
                    MPI_Send(&candidate, 1, MPI_INT, rank + 1,
                             MESSAGE_TAG, MPI_COMM_WORLD);
                }
                break;
            }

            if (is_last_worker) {
                /* After all earlier filter stages, the last stage identifies
                   and stores every additional prime from the survivors. */
                if (is_prime_after_upstream_filters(candidate, &local)) {
                    append_prime(&local, candidate);
                }
            } else if (stage_prime == 0) {
                /* The first surviving candidate is the next prime. */
                stage_prime = candidate;
                append_prime(&local, candidate);
            } else if (candidate % stage_prime != 0) {
                MPI_Send(&candidate, 1, MPI_INT, rank + 1,
                         MESSAGE_TAG, MPI_COMM_WORLD);
            }
        }
    }

    /* Gather primes only after every stage has processed the end marker.
       This avoids bidirectional sends that could deadlock the pipeline. */
    int *counts = NULL;
    int *displacements = NULL;
    int *all_primes = NULL;
    if (rank == 0) {
        counts = calloc((size_t)world_size, sizeof(*counts));
        displacements = calloc((size_t)world_size, sizeof(*displacements));
        if (!counts || !displacements) {
            fprintf(stderr, "Could not allocate MPI gather metadata.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    MPI_Gather(&local.count, 1, MPI_INT, counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int total_primes = 0;
    if (rank == 0) {
        for (int i = 0; i < world_size; ++i) {
            displacements[i] = total_primes;
            if (counts[i] < 0 || counts[i] > INT_MAX - total_primes) {
                fprintf(stderr, "Prime list exceeds MPI count limits.\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }
            total_primes += counts[i];
        }
        all_primes = malloc((size_t)(total_primes > 0 ? total_primes : 1) * sizeof(*all_primes));
        if (!all_primes) {
            fprintf(stderr, "Could not allocate final prime list.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    int unused = 0;
    MPI_Gatherv(local.values ? local.values : &unused, local.count, MPI_INT,
                all_primes, counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double elapsed = MPI_Wtime() - start;
        qsort(all_primes, (size_t)total_primes, sizeof(*all_primes), compare_ints);
        printf("Prime numbers up to %d: %d\n", limit, total_primes);
        printf("MPI processes: %d\n", world_size);
        if (limit <= 1000) {
            printf("Primes:");
            for (int i = 0; i < total_primes; ++i) {
                printf(" %d", all_primes[i]);
            }
            putchar('\n');
        } else {
            int displayed = total_primes < 20 ? total_primes : 20;
            printf("First %d primes:", displayed);
            for (int i = 0; i < displayed; ++i) {
                printf(" %d", all_primes[i]);
            }
            putchar('\n');
        }
        printf("Elapsed time (including transfer and collection): %.6f s\n", elapsed);
    }

    free(local.values);
    free(counts);
    free(displacements);
    free(all_primes);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
