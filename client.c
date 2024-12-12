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
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // CONNECT
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error in connection");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the server\n");

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
        if (strncmp(buffer, "get ", 4) == 0)  // GET
        {
            char filePath[1024] = "./client_files/";
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

                int recvSize = recv(clientSocket, buffer, sizeof(buffer), 0);
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
        } else if (strncmp(buffer, "put ", 4) == 0) // PUT
        {
            char filePath[1024] = "./client_files/";
            strcat(filePath, buffer + 4);

            file = open(filePath, O_RDONLY);
            if (file < 0) 
            {
                char errorMsg[] = "Error: File not found.\n";
                send(clientSocket, errorMsg, strlen(errorMsg), 0);
            } 
            // certified C moment
            else 
            {
                char fileBuffer[1024];
                int bytesRead;
                while ((bytesRead = read(file, fileBuffer, sizeof(fileBuffer))) > 0) 
                {
                    send(clientSocket, fileBuffer, bytesRead, 0);
                }
                close(file);

                // ITS JOEVER
                char endOfFile[] = "EOF";
                send(clientSocket, endOfFile, strlen(endOfFile), 0);
                printf("File sent successfully.\n");
            }
        } else 
        {
            // Handle other responses from the server
            memset(buffer, '\0', sizeof(buffer));
            int recvSize = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (recvSize > 0) {
                printf("Server Response:\n%s", buffer);
            }
        }
    }

    close(clientSocket);
    printf("Connection closed.\n");

    return 0;
}
