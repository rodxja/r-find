
// extracted from https://www.geeksforgeeks.org/socket-programming-cc/

// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "localhost"

void receive_file(int client_socket)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int i = 0;
    while ((bytes_read = read(client_socket, buffer, BUFFER_SIZE)) > 0)
    {
        // PRINT TO STDOUT
        printf("buffer: \n----------------\n%s\n----------------\n", buffer);

        // CLEAR BUFFER
        memset(buffer, 0, sizeof(buffer));

        printf("\nread iteration number '%d'\n", i);

        i++;
    }
}

int main(int argc, char const *argv[])
{
    int status, client_fd;
    struct sockaddr_in serv_addr;
    char request[BUFFER_SIZE * 3];

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                          sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Enter filename: \n");
    char filename[BUFFER_SIZE];
    fgets(filename, BUFFER_SIZE, stdin);

    // Remove the newline character if it exists
    size_t len = strlen(filename);
    if (len > 0 && filename[len - 1] == '\n')
    {
        filename[len - 1] = '\0'; // Replace newline with null terminator
    }

    const char *uri = "/file";   // Change to desired endpoint
    char query[BUFFER_SIZE * 2]; // Your query parameters here

    snprintf(query, sizeof(query), "filename=%s", filename);

    // Format the HTTP GET request with URI and query parameters
    snprintf(request, sizeof(request), "GET %s?%s HTTP/1.0\r\nHost: %s\r\n\r\n", uri, query, SERVER_IP);

    printf("Requesting file: %s\n", request);
    send(client_fd, request, strlen(request), 0);

    receive_file(client_fd);

    // closing the connected socket
    close(client_fd);
    return 0;
}

// gcc tarea5_4-2019243821_client.c -o tarea5_4-2019243821_client