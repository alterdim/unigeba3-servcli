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
            perror("Client left.");
            break;
        }
        printf("Client sent command %s\n", buffer);

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
                strcpy(fileList, "IO ERROR\n");
            }

            send(client_socket, fileList, strlen(fileList), 0);
        } 
        else if (strncmp(buffer, "get ", 4) == 0) // GET
        {
            char filePath[1024] = "./server_files/";
            strcat(filePath, buffer + 4);

            int file = open(filePath, O_RDONLY);
            if (file < 0) {
                perror("FILE NOT FOUND");
                char errorMsg[] = "FLIE NOT FOUND.\n";
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
                printf("FILE OK.\n");
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
                printf("FILE TRANSFER ERROR.\n");
                continue;
            }

            // Open a file to write the received data
            int file = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file < 0) {
                perror("IO ERROR");
                continue;
            }

            // Receive the file content
            long bytesReceived = 0;
            while (bytesReceived < fileSize) {
                memset(buffer, '\0', sizeof(buffer));
                int recvSize = recv(client_socket, buffer, sizeof(buffer), 0);
                if (recvSize <= 0) {
                    perror("I HOPE YOU DONT GET THIS ERROR BECAUSE ITS GOING TO BE PAINFUL TO FIX");
                    break;
                }

                write(file, buffer, recvSize);
                bytesReceived += recvSize;
            }

            close(file);

            if (bytesReceived == fileSize) {
                printf("DOWNLOAD SUCCESSFUL\n");
            } else {
                printf("INCOMPLETE FILETRANSFER\n");
            }
        }
        else
        {
            printf("Command not known.\n");
        }
    }

    close(client_socket);
    printf("Client left.\n");
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
        perror("Socket error.");
        exit(EXIT_FAILURE);
    }
    printf("\nSocket created ! Waiting for connections.\n");

    // Fill the server address structure with 0
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Bind the socket to the specified address and port
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind did not go well");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("\nBinded to port %d\n successfully", PORT);

    // Listen for incoming connections
    listen(sockfd, 6);
    printf("Waiting...\n");

    addr_size = sizeof(newAddr);

    while (1) {
        // Accept incoming connection
        newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
        if (newSocket < 0) {
            perror("Error in accepting connection");
            continue;
        }
        printf("Client joined.\n");

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
