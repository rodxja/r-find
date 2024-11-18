
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>

// thread
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

int clients[MAX_CLIENTS];
int numClients = 0;

void send_file(int client_socket, char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("error opening file");
        return;
    }

    // HTTP response with body content

    char response[BUFFER_SIZE];

    size_t bytes_read;
    int i = 0;

    const char *header = "HTTP/1.0 200 OK\r\n"
                         "Server: webserver-c\r\n"
                         "Content-Type: text/plain\r\n"
                         "Content-Length: %ld\r\n\r\n";

    int body_size = BUFFER_SIZE - strlen(header) - 1;
    char body_content[body_size];

    while ((bytes_read = fread(body_content, 1, body_size, file)) > 0)
    {

       
        snprintf(response, sizeof(response),
                 "HTTP/1.0 200 OK\r\n"
                 "Server: webserver-c\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %ld\r\n\r\n" // Include Content-Length
                 "%s",                         // Body content
                 strlen(body_content), body_content);

        printf("response: \n----------------\n%s\n----------------\n", response);

        send(client_socket, response, BUFFER_SIZE, 0);

        memset(body_content, 0, sizeof(body_content));
        memset(response, 0, sizeof(response));

        printf("\nsent iteration number '%d'\n", i++);
    }

    fclose(file);
}

void extract_filename(const char *uri, char *filename)
{
    // Look for "filename=" in the URI
    char *start = strstr(uri, "filename=");
    if (start)
    {
        // Move pointer to the beginning of the filename value
        start += strlen("filename=");

        // Copy the filename value until the end of the string or until '&' (if there are multiple parameters)
        int i = 0;
        while (start[i] != '\0' && start[i] != '&' && i < BUFFER_SIZE - 1)
        {
            filename[i] = start[i];
            i++;
        }
        filename[i] = '\0'; // Null-terminate the filename string
    }
    else
    {
        // If "filename=" is not found, return an empty string
        filename[0] = '\0';
    }
}


void *handle_client(void *arg)
{
    int newsockfd = *((int *)arg);

    // Get client address
    int sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
    if (sockn < 0)
    {
        perror("webserver (getsockname)");
        return NULL;
    }

    char buffer[1024] = {0};
    ssize_t valread;

    // TODO : check difference between read and recv

    // Read from the socket
    int valread = recv(newsockfd, buffer, BUFFER_SIZE, 0);
    if (valread < 0)
    {
        perror("webserver (read)");
        continue;
    }

    printf("buffer: %s\n", buffer);

    // TODO : create router func

    // Read the request
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);
    printf("[%s:%u] method : '%s' version : '%s' uri : '%s'\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), method, version, uri);

    char *filename = malloc(BUFFER_SIZE);
    extract_filename(uri, filename);

    send_file(newsockfd, filename);

    close(newsockfd);
    // TO THIS  MOVE TO handle_client/////////

    /* while ((valread = read(client_socket, buffer, 1024 - 1)) > 0)
    {
        printf("got from client : '%s'\n", buffer);

        broadcast(buffer, client_socket);

        char *msg = "your message was broadcasted";

        send(client_socket, msg, strlen(msg), 0);
        printf("send to client : '%s'\n", msg);
    }

    // closing the connected socket
    close(client_socket);
    // remove from client list
    for (int i = 0; i < numClients; i++)
    {
        if (clients[i] == client_socket)
        {
            for (int j = i; j < numClients - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            numClients--;
            break;
        }
    } */

    return NULL;
}

int main()
{
    char buffer[BUFFER_SIZE];

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("webserver (socket)");
        return 1;
    }
    printf("socket created successfully\n");

    // Create the address to bind the socket to
    struct sockaddr_in host_addr;
    int host_addrlen = sizeof(host_addr);
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(PORT);

    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Create client address
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    // Bind the socket to the address
    if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0)
    {
        perror("webserver (bind)");
        return 1;
    }
    printf("socket successfully bound to address\n");

    // Listen for incoming connections
    if (listen(sockfd, SOMAXCONN) != 0)
    {
        perror("webserver (listen)");
        return 1;
    }
    printf("server listening for connections\n");
    while (1)
    {

        // Accept incoming connections
        int newsockfd = accept(sockfd, (struct sockaddr *)&host_addr, (socklen_t *)&host_addrlen);
        if (newsockfd < 0)
        {
            perror("webserver (accept)");
            continue;
        }
        printf("connection accepted\n");

        if (numClients == MAX_CLIENTS)
        {
           printf("server cannot accept more clients\n"); 
           close(newsockfd);
        }

        clients[numClients++] = newsockfd;

        // create a new thread for each client
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, &newsockfd);
        pthread_detach(thread);
    }

    close(sockfd);

    return 0;
}

// gcc http_server.c -o http_server

// free port
// sudo lsof -i :8080