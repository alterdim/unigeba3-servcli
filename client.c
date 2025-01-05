#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 4455

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[1024];

    // client socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("Client socket created successfully.\n");

    // server setup
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    // ip input
    char ipAddr[64];
    printf("Server IP (e.g. 127.0.0.1): ");
    if (fgets(ipAddr, sizeof(ipAddr), stdin) == NULL) {
        fprintf(stderr, "Error reading IP.\n");
        close(clientSocket);
        return 1;
    }
    ipAddr[strcspn(ipAddr, "\n")] = '\0'; 

    serverAddr.sin_addr.s_addr = inet_addr(ipAddr);
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "Invalid IP address.\n");
        close(clientSocket);
        return 1;
    }

    // connect
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error in connection");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server.\n");

    // MAINLOOP
    while (1) {
        // INPUT
        memset(buffer, 0, sizeof(buffer));
        printf("\nEnter command (list, get <f>, put <f>, rename <old> <new>, exit): ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            // erm so that happened ? EOF error ig
            break;
        }
    
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) {
            continue;  // empty line
        }

        // EXIT
        if (strcmp(buffer, "exit") == 0) {
            
            send(clientSocket, buffer, strlen(buffer), 0);
            printf("Exiting...\n");
            break;
        }

        // LIST
        else if (strcmp(buffer, "list") == 0) {
            // Send "list" command
            send(clientSocket, buffer, strlen(buffer), 0);

            // Receive server's file list
            memset(buffer, 0, sizeof(buffer));
            int recvSize = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (recvSize > 0) {
                buffer[recvSize] = '\0';
                printf("Files on server:\n%s\n", buffer);
            } else {
                perror("Error receiving file list");
            }
        }

        // GET
        else if (strncmp(buffer, "get ", 4) == 0) {
            char userCmd[256];
            strncpy(userCmd, buffer, sizeof(userCmd) - 1);
            userCmd[sizeof(userCmd) - 1] = '\0';

            char dummy[10], requestedFile[256];
            if (sscanf(userCmd, "%s %255s", dummy, requestedFile) != 2) {
                printf("Usage: get <filename>\n");
                continue;
            }


            send(clientSocket, buffer, strlen(buffer), 0);

            memset(buffer, 0, sizeof(buffer));
            int recvSize = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (recvSize <= 0) {
                printf("Error: No file size received. Possibly file not found.\n");
                continue;
            }
            buffer[recvSize] = '\0';
            long fileSize = atol(buffer);

            // isn't it tcp's job to do this for me
            send(clientSocket, "ACK", 3, 0);

            if (fileSize <= 0) {
                printf("Server says: File not found or zero size.\n");
                continue;
            }

            char localPath[512] = "./client_files/";
            strcat(localPath, requestedFile);

            int fd = open(localPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror("Error creating local file for GET");
                long bytesToDiscard = fileSize;
                while (bytesToDiscard > 0) {
                    char tmp[512];
                    int n = recv(clientSocket, tmp, (bytesToDiscard < (long)sizeof(tmp) ? bytesToDiscard : (long)sizeof(tmp)), 0); // wow that's ugly
                    if (n <= 0) break;
                    bytesToDiscard -= n;
                }
                continue;
            }

            // 6) Receive the file data
            long bytesReceived = 0;
            while (bytesReceived < fileSize) {
                char fileBuf[1024];
                int n = recv(clientSocket, fileBuf, sizeof(fileBuf), 0);
                if (n <= 0) {
                    perror("Error receiving data (GET)");
                    break;
                }
                write(fd, fileBuf, n);
                bytesReceived += n;
            }
            close(fd);

            if (bytesReceived == fileSize) {
                printf("File downloaded successfully -> %s\n", localPath);
            } else {
                printf("File incomplete. Received %ld of %ld.\n", bytesReceived, fileSize);
            }
        }

        // put
        else if (strncmp(buffer, "put ", 4) == 0) {
            send(clientSocket, buffer, strlen(buffer), 0);
            char localFile[256];
            sscanf(buffer + 4, "%255s", localFile);
            char filePath[512] = "./client_files/";
            strcat(filePath, localFile);
            int file = open(filePath, O_RDONLY);
            if (file < 0) {
                perror("Error opening file for PUT");
                char zeroSize[] = "0";
                send(clientSocket, zeroSize, strlen(zeroSize), 0);
                continue;
            }

            off_t fileSize = lseek(file, 0, SEEK_END);
            lseek(file, 0, SEEK_SET);

            char sizeBuffer[64];
            snprintf(sizeBuffer, sizeof(sizeBuffer), "%ld", (long)fileSize);
            send(clientSocket, sizeBuffer, strlen(sizeBuffer), 0);
            memset(buffer, 0, sizeof(buffer));
            recv(clientSocket, buffer, sizeof(buffer), 0);
            if (strncmp(buffer, "ACK", 3) != 0) {
                printf("Server did not ACK. Received: %s\n", buffer);
                close(file);
                continue;
            }

            off_t bytesSent = 0;
            while (bytesSent < fileSize) {
                char fileBuf[1024];
                int r = read(file, fileBuf, sizeof(fileBuf));
                if (r <= 0) {
                    break;
                }
                send(clientSocket, fileBuf, r, 0);
                bytesSent += r;
            }
            close(file);

            if (bytesSent == fileSize) {
                printf("File upload completed.\n");
            } else {
                printf("File upload incomplete.\n");
            }
        }
        else if (strncmp(buffer, "rename ", 7) == 0) {

            char oldFile[256], newFile[256];
            if (sscanf(buffer + 7, "%255s %255s", oldFile, newFile) != 2) {
                printf("Usage: rename <old_filename> <new_filename>\n");
                continue;
            }

            char oldPath[512] = "./client_files/";
            strcat(oldPath, oldFile);
            char newPath[512] = "./client_files/";
            strcat(newPath, newFile);

            if (rename(oldPath, newPath) != 0) {
                perror("Error renaming file locally");
                continue;
            }

            printf("Uploading '%s' as PUT.\n", newFile);

            char putCommand[512];
            // Masquerade as put command
            snprintf(putCommand, sizeof(putCommand), "put %s", newFile);

            send(clientSocket, putCommand, strlen(putCommand), 0);

            int file = open(newPath, O_RDONLY);
            if (file < 0) {
                perror("Error opening file after rename");
                // let server know: send "0" for size
                char zeroSize[] = "0";
                send(clientSocket, zeroSize, strlen(zeroSize), 0);
                continue;
            }


            off_t fileSize = lseek(file, 0, SEEK_END);
            lseek(file, 0, SEEK_SET);

            char sizeBuffer[64];
            snprintf(sizeBuffer, sizeof(sizeBuffer), "%ld", (long)fileSize);
            send(clientSocket, sizeBuffer, strlen(sizeBuffer), 0);

            // 4) Wait for ACK
            memset(buffer, 0, sizeof(buffer));
            recv(clientSocket, buffer, sizeof(buffer), 0);
            if (strncmp(buffer, "ACK", 3) != 0) {
                printf("Server did not ACK. Received: %s\n", buffer);
                close(file);
                continue;
            }

            // 5) Send file data
            off_t bytesSent = 0;
            while (bytesSent < fileSize) {
                char fileBuf[1024];
                int r = read(file, fileBuf, sizeof(fileBuf));
                if (r <= 0) break;
                send(clientSocket, fileBuf, r, 0);
                bytesSent += r;
            }
            close(file);
            rename(newPath, oldPath);

            if (bytesSent == fileSize) {
                printf("File rename+upload completed successfully.\n");
            } else {
                printf("File rename+upload incomplete.\n");
            }
        }

        // ================== UNKNOWN CMD =================
        else {
            printf("Command not known: %s\n", buffer);
        }
    }

    // 5) Close socket
    close(clientSocket);
    printf("Connection closed.\n");
    return 0;
}
