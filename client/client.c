
// extracted from https://www.geeksforgeeks.org/socket-programming-cc/

// programming
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stdlib.h>

#define PORT 8080
#define HTTP_REQUEST_SIZE 1024
#define HTTP_RESPONSE_SIZE 1024
#define URI_SIZE 256
#define INPUT_SIZE 128
#define SERVER_IP "localhost"

void receive_file(int client_socket);
void receive_paths(int client_socket);
void getPaths(const char *dir, const char *regex);
void getFiles(const char *dir, const char *regex);
int *create_socket_request(const char *method, const char *uri);
void receive_header(int client_socket);

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
int *create_socket_request(const char *method, const char *uri)
{
    static int client_fd;
    int status;
    struct sockaddr_in serv_addr;
    char request[HTTP_REQUEST_SIZE];

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return NULL;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return NULL;
    }

    // Format the HTTP GET/POST/PUT/DELETE request with URI and query parameters
    // ONLY SUPPORTS GET
    snprintf(request, sizeof(request), "%s %s HTTP/1.0\r\nHost: %s\r\n\r\n", method, uri, SERVER_IP);

    printf("Requesting file: %s\n", request);
    send(client_fd, request, strlen(request), 0);

    return &client_fd;
}
void createDirectories(const char *filePath)
{
    char tempPath[256];
    size_t len = strlen(filePath);

    // Copy the file path into a temporary buffer
    strncpy(tempPath, filePath, sizeof(tempPath) - 1);
    tempPath[sizeof(tempPath) - 1] = '\0'; // Ensure null termination

    // Remove the last segment (the file name)
    char *lastSlash = strrchr(tempPath, '/');
    if (lastSlash != NULL)
    {
        *lastSlash = '\0'; // Null-terminate to get the directory path
    }
    else
    {
        // No directory separators found, nothing to create
        return;
    }

    // Create directories recursively
    for (size_t i = 0; i < strlen(tempPath); i++)
    {
        if (tempPath[i] == '/')
        {
            tempPath[i] = '\0'; // Temporarily null-terminate
            if (mkdir(tempPath, 0755) != 0 && errno != EEXIST)
            {
                perror("mkdir failed");
                exit(EXIT_FAILURE);
            }
            tempPath[i] = '/'; // Restore separator
        }
    }

    // Create the final directory (if not already created)
    if (mkdir(tempPath, 0755) != 0 && errno != EEXIST)
    {
        perror("mkdir failed");
        exit(EXIT_FAILURE);
    }
}

// TODO : this handle the responses from the server
void receive_file(int client_socket)
{
    char response[HTTP_RESPONSE_SIZE];
    ssize_t bytes_read;

    char header[HTTP_RESPONSE_SIZE];
    memset(header, 0, sizeof(header));
    bytes_read = read(client_socket, header, HTTP_RESPONSE_SIZE);

    printf("Header: %s\n----------------\n\n", header);

    // TODO : read from response the size of the request and use it to know when to stop reading
    int counter = 0;
    while ((bytes_read = read(client_socket, response, HTTP_RESPONSE_SIZE)) > 0)
    {

        printf("%s\n", response);
        char name[HTTP_RESPONSE_SIZE] = {0};
        char body[HTTP_RESPONSE_SIZE] = {0};

        // Find "name" value
        const char *nameStart = strstr(response, "\"name\":\"");
        if (nameStart)
        {
            nameStart += strlen("\"name\":\"");
            const char *nameEnd = strchr(nameStart, '"');
            if (nameEnd)
            {
                strncpy(name, nameStart, nameEnd - nameStart);
            }
        }

        // Find "body" value
        const char *bodyStart = strstr(response, "\"body\":\"");
        if (bodyStart)
        {
            bodyStart += strlen("\"body\":\"");
            const char *bodyEnd = strchr(bodyStart, '"');
            if (bodyEnd)
            {
                strncpy(body, bodyStart, bodyEnd - bodyStart);
            }
        }

        printf("name: %s\n", name);

        // create and write to file

        createDirectories(name);

        FILE *file = fopen(name, "a");
        if (file == NULL)
        {
            printf("%s\n", response);
            perror("error opening file");
            return;
        }

        fprintf(file, "%s", body);
        fclose(file);

        // CLEAR BUFFER
        memset(response, 0, sizeof(response));
        printf("counter : %d\n", counter++);
    }
    printf("File received\n");
}

// TODO : print the paths in a legible way to the console
void receive_paths(int client_socket)
{
    char response[HTTP_RESPONSE_SIZE];
    memset(response, 0, sizeof(response));
    ssize_t bytes_read;
    printf("Paths: \n");
    while ((bytes_read = read(client_socket, response, HTTP_RESPONSE_SIZE)) > 0)
    {
        printf("%s\n", response);

        // CLEAR BUFFER
        memset(response, 0, sizeof(response));
    }
}

void receive_header(int client_socket)
{
    char response[HTTP_RESPONSE_SIZE];
    int i = 0;
    ssize_t bytes_read = read(client_socket, response, HTTP_RESPONSE_SIZE);

    // PRINT TO STDOUT

    printf("Response header: \n----------------\n%s\n----------------\n", response);
}

void getPaths(const char *dir, const char *regex)
{
    printf("dir: '%s'\n", dir);
    printf("regex: '%s'\n", regex);

    char *uri = malloc(URI_SIZE);
    snprintf(uri, URI_SIZE, "/paths?dir=%s&regex=%s", dir, regex);

    printf("uri: '%s'\n", uri);
    int *client_fd = create_socket_request("GET", uri);
    if (client_fd)
    {
        // receive_header(*client_fd);
        receive_paths(*client_fd);
        // closing the connected socket
        close(*client_fd);
    }
}

void getFiles(const char *dir, const char *regex)
{
    printf("dir: '%s'\n", dir);
    printf("regex: '%s'\n", regex);

    char *uri = malloc(URI_SIZE);
    snprintf(uri, URI_SIZE, "/files?dir=%s&regex=%s", dir, regex);

    printf("uri: '%s'\n", uri);
    int *client_fd = create_socket_request("GET", uri);
    if (client_fd)
    {
        // receive_header(*client_fd);
        receive_file(*client_fd);
        // closing the connected socket
        close(*client_fd);
    }
}

// gcc client.c -o rfind