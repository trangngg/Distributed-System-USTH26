#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>
#include <sys/wait.h>

#define SERVER_PORT 9000
#define NODE_PORT   10000

#define BUF_SIZE 1024
#define MAX_NAME 256
#define MAX_FILES 100

#define MSG_REGISTER  1
#define MSG_LOOKUP    2
#define MSG_REPLY     3
#define MSG_LIST      4
#define MSG_DISCOVERY 5

typedef struct {
    char filename[MAX_NAME];
    char node_ip[INET_ADDRSTRLEN];
    int  node_port;
} FileEntry;

typedef struct {
    int type;
    FileEntry entry;
} Message;

/* Global to track background process */
pid_t sharing_pid = 0;

/* Global Variable to store Server IP after discovery */
char SERVER_IP[INET_ADDRSTRLEN]; 

/* ---------- Detect local LAN IP ---------- */
void get_local_ip(char *ip) {
    struct ifaddrs *ifaddr, *ifa;
    getifaddrs(&ifaddr);

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            const char *addr = inet_ntoa(sa->sin_addr);

            if (strcmp(addr, "127.0.0.1") != 0) {
                strcpy(ip, addr);
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
}

/* ---------- Auto Discover Server IP ---------- */
void auto_discover_server() {
    printf("[Init] Looking for server on LAN...\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(SERVER_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    Message msg;
    msg.type = MSG_DISCOVERY;

    sendto(sock, &msg, sizeof(msg), 0, 
           (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));

    struct sockaddr_in server_response_addr;
    socklen_t len = sizeof(server_response_addr);
    Message reply;
    
    struct timeval tv = {2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (recvfrom(sock, &reply, sizeof(reply), 0, 
                (struct sockaddr*)&server_response_addr, &len) > 0) {
        strcpy(SERVER_IP, inet_ntoa(server_response_addr.sin_addr));
        printf("[Init] Server found at: %s\n", SERVER_IP);
    } else {
        printf("[Error] Server not found. Make sure server is running.\n");
        exit(1);
    }
    close(sock);
}

/* ---------- Serve file ---------- */
void serve_file(const char *filename) {
    int server = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(NODE_PORT), /* Using Hardcoded Port */
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[Error] Bind failed");
        return;
    }
    listen(server, 5);

    printf("Sharing '%s' on port %d...\n", filename, NODE_PORT);

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;

        FILE *fp = fopen(filename, "rb");
        if (fp) {
            char buffer[BUF_SIZE];
            int n;
            while ((n = fread(buffer, 1, BUF_SIZE, fp)) > 0)
                send(client, buffer, n, 0);
            fclose(fp);
            printf("\n[Background] File '%s' sent to peer.\n", filename);
        } else {
            printf("\n[Error] Could not open file '%s'\n", filename);
        }
        close(client);
    }
}

/* ---------- Register file ---------- */
void register_file(const char *filename) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)
    };
    inet_aton(SERVER_IP, &server.sin_addr);

    char local_ip[INET_ADDRSTRLEN];
    get_local_ip(local_ip);

    Message msg;
    msg.type = MSG_REGISTER;
    strcpy(msg.entry.filename, filename);
    strcpy(msg.entry.node_ip, local_ip);
    msg.entry.node_port = NODE_PORT; /* Using Hardcoded Port */

    sendto(sock, &msg, sizeof(msg), 0,
           (struct sockaddr*)&server, sizeof(server));

    close(sock);
    printf("Registered '%s' (%s:%d)\n", filename, local_ip, NODE_PORT);
}

/* ---------- Get list ---------- */
int get_file_list(FileEntry *list) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)
    };
    inet_aton(SERVER_IP, &server.sin_addr);

    struct timeval tv = {2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    Message msg;
    msg.type = MSG_LIST;

    sendto(sock, &msg, sizeof(msg), 0,
           (struct sockaddr*)&server, sizeof(server));

    int count = 0;
    printf("\n--- Available Files ---\n");

    while (1) {
        int n = recvfrom(sock, &msg, sizeof(msg), 0, NULL, NULL);
        if (n <= 0) break;

        if (msg.type == MSG_REPLY && count < MAX_FILES) {
            list[count] = msg.entry;
            printf("%d. %s (%s:%d)\n",
                   count + 1,
                   msg.entry.filename,
                   msg.entry.node_ip,
                   msg.entry.node_port);
            count++;
        }
    }
    close(sock);
    if (count == 0) printf("No files available.\n");
    return count;
}

/* ---------- Download ---------- */
void download_selected_file(FileEntry entry) {
    int peer = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in peer_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(entry.node_port)
    };
    inet_aton(entry.node_ip, &peer_addr.sin_addr);

    if (connect(peer, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        printf("[Error] Could not connect to peer %s:%d\n", entry.node_ip, entry.node_port);
        close(peer);
        return;
    }

    char outname[300];
    sprintf(outname, "downloaded_%s", entry.filename);

    FILE *fp = fopen(outname, "wb");
    char buffer[BUF_SIZE];
    int n;

    while ((n = recv(peer, buffer, BUF_SIZE, 0)) > 0)
        fwrite(buffer, 1, n, fp);

    fclose(fp);
    close(peer);

    printf("Downloaded: %s\n", outname);
}

/* ---------- Main ---------- */
int main() {
    /* Auto-Discover Server immediately */
    auto_discover_server();

    int choice;
    char filename[MAX_NAME];
    FileEntry available[MAX_FILES];

    while (1) {
        printf("\n=== P2P File Transfer ===\n");
        printf("1. Share file\n");
        printf("2. Download file(s)\n");
        printf("3. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        if (choice == 1) {
            if (sharing_pid > 0) {
                printf("[!] Stopping previous share (PID %d)...\n", sharing_pid);
                kill(sharing_pid, SIGKILL);
                waitpid(sharing_pid, NULL, 0);
                sharing_pid = 0;
            }

            printf("File name: ");
            scanf("%s", filename);
            
            register_file(filename); /* No port arg */

            pid_t pid = fork();

            if (pid == 0) {
                serve_file(filename); /* No port arg */
                exit(0); 
            }
            else if (pid > 0) {
                sharing_pid = pid;
                printf("[Info] Background server started (PID: %d)\n", pid);
                usleep(100000); 
            }
        }
        else if (choice == 2) {
            printf("Show all shared files:\n");
            int count = get_file_list(available);
            if (count == 0) continue;

            int c;
            while ((c = getchar()) != '\n' && c != EOF); 

            char line[256];
            printf("Select file numbers: ");
            fgets(line, sizeof(line), stdin);

            char *token = strtok(line, ", ");
            while (token) {
                int idx = atoi(token) - 1;
                if (idx >= 0 && idx < count)
                    download_selected_file(available[idx]);
                token = strtok(NULL, ", ");
            }
        }
        else {
            if (sharing_pid > 0) kill(sharing_pid, SIGKILL);
            break;
        }
    }
    return 0;
}