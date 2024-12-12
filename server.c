#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<dirent.h>
#include<fcntl.h>
#include<signal.h>

#define PORT 4455

void handle_client(int client_socket) {
    char buffer[1024];

    while (1) {
        memset(buffer, '\0', sizeof(buffer));

        // Receive command from client
        int recvSize = recv(client_socket, buffer, sizeof(buffer), 0);
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
                    // SKIP THE UNIX NONSENSE
                    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) 
                    {
                        continue;
                    }
                    strcat(fileList, dir->d_name);
                    strcat(fileList, "\n");
                }
                closedir(d);
            } else {
                strcpy(fileList, "Error: Could not open directory.\n");
            }

            send(client_socket, fileList, strlen(fileList), 0);
        } 
        else if (strncmp(buffer, "get ", 4) == 0) // GET
        {
            char filePath[1024] = "./server_files/";
            strcat(filePath, buffer + 4);

            int file = open(filePath, O_RDONLY);
            if (file < 0) {
                perror("Error opening file");
                char errorMsg[] = "Error: File not found.\n";
                send(client_socket, errorMsg, strlen(errorMsg), 0);
            } else {
                // Determine the file size
                off_t fileSize = lseek(file, 0, SEEK_END);
                lseek(file, 0, SEEK_SET); // Reset file pointer to the beginning

                // Send the file size to the client
                char sizeBuffer[64];
                snprintf(sizeBuffer, sizeof(sizeBuffer), "%ld", fileSize);
                send(client_socket, sizeBuffer, strlen(sizeBuffer), 0);
                recv(client_socket, buffer, sizeof(buffer), 0); // Wait for ACK

                // Send the file content
                char fileBuffer[1024];
                int bytesRead;
                while ((bytesRead = read(file, fileBuffer, sizeof(fileBuffer))) > 0) {
                    send(client_socket, fileBuffer, bytesRead, 0);
                }

                close(file);
                printf("File sent successfully.\n");
            }
        } 
        else if (strncmp(buffer, "exit", 4) == 0) 
        {
            printf("Exiting...\n");
            break;
        }
        else if (strncmp(buffer, "put ", 4) == 0) // PUT
        {
            char filePath[1024] = "./server_files/";
            strcat(filePath, buffer + 4);

            // Receive the file size
            memset(buffer, '\0', sizeof(buffer));
            recv(client_socket, buffer, sizeof(buffer), 0);
            long fileSize = atol(buffer);
            send(client_socket, "ACK", 3, 0); // Send ACK

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
                int recvSize = recv(client_socket, buffer, sizeof(buffer), 0);
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
        else
        {
            printf("Command not known.\n");
        }
    }

    close(client_socket);
    printf("Client disconnected.\n");
    exit(0);
}

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;

    int newSocket;
    struct sockaddr_in newAddr;

    socklen_t addr_size;

    // Handle SIGCHLD to prevent zombie processes
    signal(SIGCHLD, SIG_IGN);

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

    while (1) {
        // Accept incoming connection
        newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
        if (newSocket < 0) {
            perror("Error in accepting connection");
            continue;
        }
        printf("Client connected.\n");

        // Fork a new process to handle the client
        pid_t child_pid = fork();
        if (child_pid == 0) {
            // Child process
            close(sockfd); // Close the listening socket in the child
            handle_client(newSocket);
        } else if (child_pid > 0) {
            // Parent process
            close(newSocket); // Close the client socket in the parent
        } else {
            perror("Fork failed");
        }
    }

    close(sockfd);
    printf("Server shut down.\n");
    return 0;
}
