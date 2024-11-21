
// extracted from https://www.geeksforgeeks.org/socket-programming-cc/

// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "localhost"

void receive_file(int client_socket);
char *getInput();
void getPaths(const char *dir, const char *regex);
void getFiles(const char *dir, const char *regex);
void create_socket(const char *method, const char *uri);

// rfind . -name SEARCH_NAME
// rfind ./source -get -name "*.cpp"
int main(int argc, char const *argv[])
{
    // extract values from  argv
    if (argc != 4 && argc != 5)
    {
        printf("Usage: %s <dir> -name <regex>\n", argv[0]);
        printf("Usage: %s <dir> -get -name <regex>\n", argv[0]);
        return 1;
    }

    if (argc == 4)
    {
        // validate that the second argument is -name
        if (strcmp(argv[2], "-name") != 0)
        {
            printf("Usage: %s <dir> -name <regex>\n", argv[0]);
            return 1;
        }
        getPaths(argv[1], argv[3]);
    }
    else
    {
        // validate that the second argument is -get
        if (strcmp(argv[2], "-get") != 0)
        {
            printf("Usage: %s <dir> -get -name <regex>\n", argv[0]);
            return 1;
        }

        // validate that the fourth argument is -name
        if (strcmp(argv[3], "-name") != 0)
        {
            printf("Usage: %s <dir> -get -name <regex>\n", argv[0]);
            return 1;
        }

        getFiles(argv[1], argv[4]);
    }

    return 0;
}

/*
uri: /paths?dir=./source&regex=.*
    /files?dir=./source&regex=.*
 */
void create_socket(const char *method, const char *uri)
{
    int status, client_fd;
    struct sockaddr_in serv_addr;
    char request[BUFFER_SIZE * 3];

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                          sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return;
    }

    // Format the HTTP GET/POST/PUT/DELETE request with URI and query parameters
    // ONLY SUPPORTS GET
    snprintf(request, sizeof(request), "%s %s HTTP/1.0\r\nHost: %s\r\n\r\n", method, uri, SERVER_IP);

    printf("Requesting file: %s\n", request);
    send(client_fd, request, strlen(request), 0);

    receive_file(client_fd);

    // closing the connected socket
    close(client_fd);
}

// TODO : this handle the responses from the server
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

char *getInput()
{
    char *input = malloc(BUFFER_SIZE); // TODO : use correct size
    fgets(input, BUFFER_SIZE, stdin);

    // Remove the newline character if it exists
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
    {
        input[len - 1] = '\0'; // Replace newline with null terminator
    }

    return input;
}

void getPaths(const char *dir, const char *regex)
{
    printf("dir: '%s'\n", dir);
    printf("regex: '%s'\n", regex);

    // TODO : use correct size
    char *uri = malloc(BUFFER_SIZE);
    snprintf(uri, BUFFER_SIZE, "/paths?dir=%s&regex=%s", dir, regex);

    printf("uri: '%s'\n", uri);
    create_socket("GET", uri);
    // this will print the file to stdout
}

void getFiles(const char *dir, const char *regex)
{
    printf("dir: '%s'\n", dir);
    printf("regex: '%s'\n", regex);

    // TODO : use correct size
    char *uri = malloc(BUFFER_SIZE);
    snprintf(uri, BUFFER_SIZE, "/files?dir=%s&regex=%s", dir, regex);

    printf("uri: '%s'\n", uri);
    create_socket("GET", uri);
    // this will create a file with the contents of the file
}

// gcc client.c -o rfind