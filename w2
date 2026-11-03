#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

void pipelined_sieve(int limit, int process_rank, int total_procs) {
    int candidate;
    int end_marker = -1;  // Marker for termination
    double begin_time = MPI_Wtime();

    int *prime_list = malloc((limit / 2) * sizeof(int)); // Allocate space for primes
    int prime_counter = 0;

    if (process_rank == 0) {
        for (candidate = 2; candidate <= limit; candidate++) {
            MPI_Send(&candidate, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        }
        MPI_Send(&end_marker, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } else {
        while (1) {
            MPI_Recv(&candidate, 1, MPI_INT, process_rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (candidate == end_marker) {
                if (process_rank < total_procs - 1) {
                    MPI_Send(&end_marker, 1, MPI_INT, process_rank + 1, 0, MPI_COMM_WORLD);
                }
                break;
            }

            bool is_prime_flag = true;
            for (int i = 0; i < prime_counter; i++) {
                if (candidate % prime_list[i] == 0) {
                    is_prime_flag = false;
                    break;
                }
                if (prime_list[i] * prime_list[i] > candidate) {
                    break;
                }
            }

            // Store the prime number and send to the next process
            if (is_prime_flag) {
                printf("Process %d identified prime: %d\n", process_rank, candidate);
                prime_list[prime_counter++] = candidate;
                if (process_rank < total_procs - 1) {
                    MPI_Send(&candidate, 1, MPI_INT, process_rank + 1, 0, MPI_COMM_WORLD);
                }
            }
        }
    }

    // Process for collecting the primes (final process in the sequence)
    if (process_rank == total_procs - 1) {
        while (1) {
            MPI_Recv(&candidate, 1, MPI_INT, total_procs - 2, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (candidate == end_marker) break;
            
            // can print the primes 
        }
       
    }

    // Release allocated memory
    free(prime_list);

    // End of timing
    double finish_time = MPI_Wtime();

    if (process_rank == 0) {
        printf("Total execution time: %.6f seconds\n", finish_time - begin_time);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int process_rank, total_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &total_procs);

    if (argc != 2) {
        if (process_rank == 0) printf("Usage: %s <limit>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    int limit = atoi(argv[1]);

    pipelined_sieve(limit, process_rank, total_procs);

    MPI_Finalize();
    return 0;
}
