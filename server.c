#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<dirent.h> // Required for listing directory contents
#include<fcntl.h>

#define PORT 4455

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    int newSocket;
    struct sockaddr_in newAddr;

    socklen_t addr_size;

    char buffer[1024];

    int file;

    // Create the server socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("\nServer socket created\n");

    // Fill the server address structure with 0
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind the socket to the specified address and port
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("\nBinded to port %d\n", PORT);

    // Listen for incoming connections
    listen(sockfd, 5);
    printf("Listening...\n");

    addr_size = sizeof(newAddr);

    // Accept incoming connection
    newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
    if (newSocket < 0) {
        perror("Error in accepting connection");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");

    // Server command handling loop
    while (1) {
        memset(buffer, '\0', sizeof(buffer));

        // Receive command from client
        int recvSize = recv(newSocket, buffer, sizeof(buffer), 0);
        if (recvSize <= 0) {
            perror("Error in receiving data or client disconnected");
            break;
        }
        printf("Command received: %s\n", buffer);

        // LIST
        if (strncmp(buffer, "list", 4) == 0) {
            DIR *d;
            struct dirent *dir;
            char fileList[1024] = "";

            d = opendir("./server_files");
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    strcat(fileList, dir->d_name);
                    strcat(fileList, "\n");
                }
                closedir(d);
            } else {
                strcpy(fileList, "Error: Could not open directory.\n");
            }

            send(newSocket, fileList, strlen(fileList), 0);
        // GET
        } 
        else if (strncmp(buffer, "get ", 4) == 0)
        {
            char filePath[1024] = "./server_files/";
            strcat(filePath, buffer + 4);

            file = open(filePath, O_RDONLY);
            if (file < 0) 
            {
                char errorMsg[] = "Error: File not found.\n";
                send(newSocket, errorMsg, strlen(errorMsg), 0);
            } 
            // certified C moment
            else 
            {
                char fileBuffer[1024];
                int bytesRead;
                while ((bytesRead = read(file, fileBuffer, sizeof(fileBuffer))) > 0) 
                {
                    send(newSocket, fileBuffer, bytesRead, 0);
                }
                close(file);

                // ITS JOEVER
                char endOfFile[] = "EOF";
                send(newSocket, endOfFile, strlen(endOfFile), 0);
                printf("File sent successfully.\n");
            }
        } else if (strncmp(buffer, "exit", 4) == 0) {
            printf("Exiting...\n");
            break;
        }
        else if (strncmp(buffer, "put ", 4) == 0) {
            char filePath[1024] = "./server_files/";
            strcat(filePath, buffer + 4);

            // Open a file to save the downloaded content
            int file = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file < 0) {
                perror("Error in creating file");
                continue;
            }

            // Receive file content
            while (1) {
                memset(buffer, '\0', sizeof(buffer));
                
                int recvSize = recv(newSocket, buffer, sizeof(buffer), 0);
                if (recvSize <= 0) {
                    perror("Error in receiving data");
                    break;
                }

                // Check for end-of-file marker
                if (recvSize == 3 && strncmp(buffer, "EOF", 3) == 0) {
                    printf("File download completed successfully.\n");
                    break;
                }

                // Write received data to the file
                write(file, buffer, recvSize);
            }

            close(file);
        }
        else {
            char unknownCommand[] = "Unknown command.\n";
            send(newSocket, unknownCommand, strlen(unknownCommand), 0);
        }
    }

    close(newSocket);
    close(sockfd);

    printf("Server shut down.\n");
    return 0;
}
