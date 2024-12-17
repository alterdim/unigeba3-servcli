#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>  // For file operations

#define PORT 4455

int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[1024];
    char fileName[1024];
    int file;

    //NET SETUP
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("Client socket created successfully\n");
    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    char ipAddr[30];

    printf("IP : ");
    if (fgets(ipAddr, sizeof(ipAddr), stdin) == NULL) {
        // Handle EOF or read error
        fprintf(stderr, "Error reading input.\n");
        return 1;
    }

    // Remove the trailing newline if it exists
    char *newline = strchr(ipAddr, '\n');
    if (newline != NULL) {
        *newline = '\0';
    }

    // Convert IP address from text to binary form
    serverAddr.sin_addr.s_addr = inet_addr(ipAddr);
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        fprintf(stderr, "Invalid IP address.\n");
        return 1;
    }

    // CONNECT
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error in connection");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server\n");
    sleep(0.1);
    while (1) {
        // SEND COMMAND, REMOVE FUNNY NEWLINE
        memset(buffer, '\0', sizeof(buffer));
        printf("\nEnter a command (e.g., 'get [FILE]', 'exit'): ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        send(clientSocket, buffer, strlen(buffer), 0);

        if (strcmp(buffer, "exit") == 0) // EXIT
        {
            printf("Exiting...\n");
            break;
        }
        else if (strcmp(buffer, "list") == 0) // LIST
        {
            // Receive the file list from the server
            memset(buffer, '\0', sizeof(buffer));
            int recvSize = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (recvSize > 0) {
                printf("Files on server:\n%s\n", buffer);
            } else {
                perror("Error receiving file list");
            }
        }
        else if (strncmp(buffer, "get ", 4) == 0) // GET
        {
            char filePath[1024] = "./client_files/";
            strcat(filePath, buffer + 4);

            // Receive the file size
            memset(buffer, '\0', sizeof(buffer));
            recv(clientSocket, buffer, sizeof(buffer), 0);
            long fileSize = atol(buffer);
            send(clientSocket, "ACK", 3, 0); // Send ACK

            if (fileSize <= 0) {
                printf("Error: Invalid file size received.\n");
                continue;
            }

            // Open a file to write the received data
            int file = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file < 0) {
                perror("Error creating file");
                continue;
            }

            // Receive the file content
            long bytesReceived = 0;
            while (bytesReceived < fileSize) {
                memset(buffer, '\0', sizeof(buffer));
                int recvSize = recv(clientSocket, buffer, sizeof(buffer), 0);
                if (recvSize <= 0) {
                    perror("Error receiving data");
                    break;
                }

                write(file, buffer, recvSize);
                bytesReceived += recvSize;
            }

            close(file);

            if (bytesReceived == fileSize) {
                printf("File download completed successfully.\n");
            } else {
                printf("Error: Incomplete file transfer.\n");
            }
        } 
        else if (strncmp(buffer, "put ", 4) == 0) // PUT
        {
            char filePath[1024] = "./client_files/";
            strcat(filePath, buffer + 4);

            int file = open(filePath, O_RDONLY);
            if (file < 0) {
                perror("Error opening file");
                char errorMsg[] = "Error: File not found.\n";
                send(clientSocket, errorMsg, strlen(errorMsg), 0);
            } else {
                // Determine the file size
                off_t fileSize = lseek(file, 0, SEEK_END);
                lseek(file, 0, SEEK_SET); // Reset file pointer to the beginning

                // Send the file size to the client
                char sizeBuffer[64];
                snprintf(sizeBuffer, sizeof(sizeBuffer), "%ld", fileSize);
                send(clientSocket, sizeBuffer, strlen(sizeBuffer), 0);
                recv(clientSocket, buffer, sizeof(buffer), 0); // Wait for ACK

                // Send the file content
                char fileBuffer[1024];
                int bytesRead;
                while ((bytesRead = read(file, fileBuffer, sizeof(fileBuffer))) > 0) {
                    send(clientSocket, fileBuffer, bytesRead, 0);
                }

                close(file);
                printf("File sent successfully.\n");
            }
        } else 
        {
            printf("Command not known");
        }
    }

    close(clientSocket);
    printf("Connection closed.\n");

    return 0;
}
