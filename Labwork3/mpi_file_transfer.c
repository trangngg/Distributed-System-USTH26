#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define FILENAME_TAG 100
#define DATA_TAG 200
#define END_TAG 300

void rank_0_sender(const char *filename, int dest_rank) {
    printf("Rank 0: Starting file transmission of '%s' to Rank %d.\n", filename, dest_rank);

    int file_fd_read = open(filename, O_RDONLY);
    if (file_fd_read < 0) {
        perror("Rank 0 ERROR: failed to open source file");
        MPI_Send("", 1, MPI_CHAR, dest_rank, FILENAME_TAG, MPI_COMM_WORLD); 
        return;
    }

    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int data_sent_total = 0;

    int filename_len = strlen(filename);
    MPI_Send((void*)filename, filename_len + 1, MPI_CHAR, dest_rank, FILENAME_TAG, MPI_COMM_WORLD); 
    
    while ((bytes_read = read(file_fd_read, data_buffer, BUFFER_SIZE)) > 0) {
        MPI_Send(data_buffer, (int)bytes_read, MPI_CHAR, dest_rank, DATA_TAG, MPI_COMM_WORLD);
        data_sent_total += (int)bytes_read;
    }

    MPI_Send(NULL, 0, MPI_CHAR, dest_rank, END_TAG, MPI_COMM_WORLD);
    
    close(file_fd_read); 
    printf("Rank 0: File transfer complete. Total bytes sent: %d.\n", data_sent_total);
}

void rank_1_receiver(int source_rank) {
    char client_filename[BUFFER_SIZE];
    MPI_Status status;
    char saved_filename[2 * BUFFER_SIZE];
    
    MPI_Recv(client_filename, BUFFER_SIZE, MPI_CHAR, source_rank, FILENAME_TAG, MPI_COMM_WORLD, &status);
    
    if (client_filename[0] == '\0') {
        printf("Rank 1: ERROR - Received empty filename, transmission aborted by sender.\n");
        return;
    }

    snprintf(saved_filename, sizeof(saved_filename), "received_mpi_%s", client_filename);

    int file_fd_write = open(saved_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd_write < 0) {
        perror("Rank 1 ERROR: Failed to create local file");
        return;
    }
    printf("Rank 1: Preparing to receive and save file as '%s' from Rank %d.\n", saved_filename, source_rank);

    char data_buffer[BUFFER_SIZE];
    int data_received_total = 0;
    int count;

    while (1) {
        MPI_Recv(data_buffer, BUFFER_SIZE, MPI_CHAR, source_rank, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if (status.MPI_TAG == END_TAG) {
            break;
        }
        
        MPI_Get_count(&status, MPI_CHAR, &count);

        if (write(file_fd_write, data_buffer, count) < 0) {
            perror("Rank 1 ERROR: Failed to write to file");
            break;
        }
        data_received_total += count;
    }
    
    close(file_fd_write); 
    printf("Rank 1: File received successfully. Total bytes received: %d.\n", data_received_total);
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "Error: Must run with at least 2 processes (e.g., mpiexec -n 2 ...)\n");
        }
    } else {
        if (rank == 0) {
            if (argc != 2) {
                fprintf(stderr, "Usage: mpiexec -n <num_procs> %s <filename_to_send>\n", argv[0]);
            } else {
                rank_0_sender(argv[1], 1); 
            }
        } else if (rank == 1) {
            rank_1_receiver(0); 
        } else {
            printf("Rank %d: Idle.\n", rank);
        }
    }

    MPI_Finalize();
    
    return 0;
}