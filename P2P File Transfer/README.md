PEER-TO-PEER FILE TRANSFER SYSTEM

**1. Introduction**

This project implements a simple Peer-to-Peer (P2P) File Transfer system using C socket programming.
The system consists of one centralized server and multiple clients, allowing users to share and download files across different devices through network communication.

**2. Deployment Guideline**

Compile the source files using gcc:

gcc server.c -o server
gcc client.c -o client

**Running the Server**

Run the server on one device only and only once:

./server

The server must be running continuously to handle client requests.

**Running the Client**

Open another terminal window (or use another device) and run:

./client

The client program can be executed on multiple terminals and multiple devices at the same time.

**3. Usage Guideline**

After running client.c, the user can choose one of the following options:

1. Share file
2. Download file


**Share file:**
Choose option 1 and enter the file name. The file must be located in the same directory as client.c.

**Download file:**
Choose option 2 and enter the file name to download. The file will be saved in the same directory as client.c.

**4. Contribution**

| **Member Name**        | **Contribution**                     |
| ---------------------- | ------------------------------------ |
| Nguyen Thi Minh Nguyet | Introduction, Architecture Design    |
| Nguyen Quynh Trang     | Protocol Design, Report Writing      |
| Vuong Ngoc Duy         | Implementation                       |
| Dao Phu Hong Dang      | Implementation, Deployment Guideline |
| Nguyen Luong Chinh     | Usage Guideline, Report Writing      |

