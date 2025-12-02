const MAX_BUF_SIZE = 1024;

struct filename_request {
    string filename<MAX_BUF_SIZE>;
};

struct file_data {
    string filename<MAX_BUF_SIZE>;
    opaque content[MAX_BUF_SIZE];
    int bytes_read;
};

struct transfer_status {
    int success;
};

program FILE_TRANSFER_PROG {
    version FILE_TRANSFER_VERS {
        transfer_status send_filename(filename_request) = 1;

        transfer_status send_data_chunk(file_data) = 2;
    } = 1;
} = 0x20000001;