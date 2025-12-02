#include <stdio.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "ft.h" 

#define BUFFER_SIZE 1024

void execute_file_transfer_rpc(CLIENT *clnt, const char *filename_to_send) {
    int file_fd_read = open(filename_to_send, O_RDONLY);
    if (file_fd_read < 0) {
        perror("ERROR: failed to open source file");
        return;
    }

    filename_request name_request;
    transfer_status *status_name;

    //Marshaling tên file thành tham số RPC
    name_request.filename = (char *)filename_to_send;
    
    printf("RPC: Calling send_filename_1...\n");
    status_name = send_filename_1(&name_request, clnt);

    if (status_name == NULL || status_name->success == 0) {
        fprintf(stderr, "RPC Error: Could not send filename or server failed setup.\n");
        close(file_fd_read);
        clnt_destroy(clnt); //Hủy Client Stub
        return;
    }
    printf("RPC: Server acknowledged filename. Starting data transfer...\n");

    char data_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    transfer_status *status_data;
    file_data data_chunk;

    while ((bytes_read = read(file_fd_read, data_buffer, BUFFER_SIZE)) > 0) {
        //Marshaling khối dữ liệu thành tham số RPC
        data_chunk.filename = (char *)filename_to_send;
        data_chunk.bytes_read = (int)bytes_read;

        memcpy(data_chunk.content, data_buffer, bytes_read); 
        data_chunk.bytes_read = (int)bytes_read;

        status_data = send_data_chunk_1(&data_chunk, clnt);

        if (status_data == NULL || status_data->success == 0) {
            fprintf(stderr, "RPC Error: Data transfer failed.\n");
            break;
        }
    }

    if (bytes_read == 0) {
        printf("RPC: Sending end-of-file signal...\n");
        data_chunk.bytes_read = 0;
        data_chunk.bytes_read = 0;

        status_data = send_data_chunk_1(&data_chunk, clnt);
        if (status_data != NULL && status_data->success == 1) {
             printf("RPC: File transfer complete and successful.\n");
        }
    } else if (bytes_read < 0) {
        perror("Error reading local file");
    }

    close(file_fd_read); 
    clnt_destroy(clnt); 
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_hostname_or_ip> <filename_to_send>\n", argv[0]);
        exit(1);
    }
    
    const char *server_hostname = argv[1];
    
    CLIENT *clnt;

    //Thay thế socket() và connect())
    clnt = clnt_create(argv[1], FILE_TRANSFER_PROG, FILE_TRANSFER_VERS, "tcp");

    if (clnt == NULL) {
        clnt_pcreateerror(argv[1]);
        exit(EXIT_FAILURE);
    }

    printf("Client Stub created. Initiating RPC...\n");
    
    execute_file_transfer_rpc(clnt, argv[2]);

    return 0;
}