#include <stdio.h>
#include <rpc/rpc.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "ft.h" 

#define MAX_PENDING 10
#define BUFFER_SIZE 1024

int current_file_fd = -1;
char saved_filename[2 * BUFFER_SIZE]; 

//recv()
transfer_status *
send_filename_1_svc(filename_request *argp, struct svc_req *rqstp)
{
    static transfer_status result;

    if (current_file_fd != -1) {
        close(current_file_fd);
        current_file_fd = -1;
    }

    snprintf(saved_filename, sizeof(saved_filename), "received_%s", argp->filename);

    current_file_fd = open(saved_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (current_file_fd < 0) {
        perror("Error: Failed to create local file in RPC server");
        result.success = 0;
    } else {
        printf("\n--- File metadata received. Receiving content for: %s ---\n", saved_filename);
        result.success = 1;
    }
    
    return &result;
}

//Thay recv() và write()
transfer_status *
send_data_chunk_1_svc(file_data *argp, struct svc_req *rqstp)
{
    static transfer_status result;
    result.success = 0;

    //RPC không cần socket/bind/listen/accept, chỉ cần chạy
    if (current_file_fd < 0) {
        fprintf(stderr, "Error: File handle not initialized. Call send_filename first.\n");
        return &result;
    }

    if (argp->bytes_read > 0) {
        if (write(current_file_fd, argp->content, argp->bytes_read) < 0) {
            perror("Error: Failed to write to file in RPC server");
            result.success = 0;
            close(current_file_fd);
            current_file_fd = -1;
            return &result;
        }
    }

    if (argp->bytes_read == 0) {
        printf("File received successfully via RPC. Closing file handle.\n");
        close(current_file_fd);
        current_file_fd = -1;
    }
    
    result.success = 1;
    return &result;
}