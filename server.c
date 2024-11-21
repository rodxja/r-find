
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <sys/stat.h>

// thread
#include <pthread.h>
#include <dirent.h>

// regex
#include <regex.h>

#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

int clients[MAX_CLIENTS];
int numClients = 0;


char **searchFiles(const char *dir, const char *regex, int *matchedFiles);
void send_file(int client_socket, char *filename);
char *extract_path_parameter(const char *uri, char *parameterName);
void paths(int client_socket, const char *uri);
void files(int client_socket, const char *uri);
void *handle_client(void *arg);

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

// close in each return
void *handle_client(void *arg)
{
    int newsockfd = *((int *)arg);

    // Create client address
    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    // Get client address
    int sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
    if (sockn < 0)
    {
        perror("webserver (getsockname)");
        close(newsockfd);
        return NULL;
    }

    char buffer[1024] = {0};

    // TODO : check difference between read and recv

    // Read from the socket
    ssize_t valread = recv(newsockfd, buffer, BUFFER_SIZE, 0);
    if (valread < 0)
    {
        perror("webserver (read)");
        close(newsockfd);
        return NULL;
    }

    printf("buffer: %s\n", buffer);

    // TODO : create router func

    // Read the request
    char method[BUFFER_SIZE], uri[BUFFER_SIZE], version[BUFFER_SIZE];
    sscanf(buffer, "%s %s %s", method, uri, version);

    printf("[%s:%u] method : '%s' version : '%s' uri : '%s'\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), method, version, uri);

    // compare method with GET
    if (strcmp(method, "GET") != 0)
    {
        printf("method not supported\n");
        close(newsockfd);
        return NULL;
    }

    // compare uri, there are two routes
    // - /paths
    // - /files

    // check that beggining starts with /path or /file
    if (strncmp(uri, "/paths", 5) == 0)
    {
        paths(newsockfd, uri);
        close(newsockfd);
        return NULL;
    }
    else if (strncmp(uri, "/files", 5) != 0)
    {
        close(newsockfd);
        return NULL;
    }
    else
    {
        printf("uri not supported\n");
        close(newsockfd);
        return NULL;
    }
}

// localhost:8080/paths?path=./dir&regex=.*
void paths(int client_socket, const char *uri)
{

    // extract from uri as /paths?path=dir
    // the var dir in order to search

    char *dir = extract_path_parameter(uri, "dir");
    char *regex = extract_path_parameter(uri, "regex");
    
    // TODO: Hay que quitar el client_socket por que lo que me va a devolver es una lista de archivos 
    searchFiles(dir, regex, client_socket);
    // llamar a sendFiles
}

// TODO: Esa va a llamar searchFile para obtener la lista de archivos y luego esta lo llama a sendFiles
void files(int client_socket, const char *uri)
{
    // Extraer los parámetros 'dir' y 'regex' de la URI
    char *dir = extract_path_parameter(uri, "dir");
    char *regex = extract_path_parameter(uri, "regex");

    // Contador de archivos encontrados
    int matchedFiles = 0;

    // Obtener la lista de archivos que coinciden con el patrón
    char **fileList = searchFiles(dir, regex, &matchedFiles);

    // Verificar si se encontraron archivos
    if (fileList != NULL && matchedFiles > 0)
    {
        // Llamar a send_file para enviar la lista de archivos como JSON
        send_file(client_socket, fileList, matchedFiles);

        // Liberar la memoria después de enviar los archivos
        for (int i = 0; i < matchedFiles; i++)
        {
            free(fileList[i]);  
        }
        free(fileList);  
    }
    else
    {
        printf("No se encontraron archivos que coincidan con el patrón.\n");
    }
}


// TODO: Retorne solo la lista con los archivos que hagan match y que no envie por sockets 
// TODO: Hacer validación en el while que verifica si es archivo o directorio
char **searchFiles(const char *dir, const char *regex, int *matchedFiles)
{
    DIR *dirp;
    struct dirent *entry;
    regex_t reg;
    struct stat fileStat;

    char path[BUFFER_SIZE];

    // Inicializar la lista de archivos encontrados
    char **fileList = NULL;  
    *matchedFiles = 0;  // Contador de archivos encontrados

    // Compilar el regex
    if (regcomp(&reg, regex, REG_EXTENDED) != 0)
    {
        perror("Error compilando el regex");
        return NULL;
    }

    // Abrir el directorio
    if ((dirp = opendir(dir)) == NULL)
    {
        perror("Error abriendo el directorio");
        return NULL;
    }

    // Leer entradas en el directorio
    while ((entry = readdir(dirp)) != NULL)
    {
        // Verificar si el nombre del archivo coincide con el regex
        if (regexec(&reg, entry->d_name, 0, NULL, 0) == 0)
        {
            sprintf(path, "%s/%s", dir, entry->d_name);

            // Verificar si es un archivo regular
            if (stat(path, &fileStat) == 0 && S_ISREG(fileStat.st_mode))
            {
                // Agregar el archivo a la lista de archivos encontrados
                fileList = realloc(fileList, (*matchedFiles + 1) * sizeof(char *));  
                fileList[*matchedFiles] = strdup(path);  
                (*matchedFiles)++;  
            }
        }
    }

    regfree(&reg);
    closedir(dirp);

    return fileList;
}

// TODO: Crear funcion que se encarga de mandar los nombres de los archivos 
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
                 "{\"name\": \"%s\", \"body\": \"%s\"}", // Body content
                 strlen(body_content), filename, body_content);

        printf("response: \n----------------\n%s\n----------------\n", response);

        send(client_socket, response, BUFFER_SIZE, 0);

        memset(body_content, 0, sizeof(body_content));
        memset(response, 0, sizeof(response));

        printf("\nsent iteration number '%d'\n", i++);
    }

    fclose(file);
}

// TODO: Crear otra funcion que se encarga de mandar la lista archivos que recibe

char *extract_path_parameter(const char *uri, char *parameterName)
{

    // add = to the end of the parameter name
    char pName[BUFFER_SIZE];
    sprintf(pName, "%s=", parameterName);
    char *parameter = malloc(BUFFER_SIZE);

    // Look for "{parameter}=" in the URI

    char *start = strstr(uri, pName);
    if (start)
    {
        // Move pointer to the beginning of the parameterName value
        start += strlen(pName);

        // Copy the filename value until the end of the string or until '&' (if there are multiple parameters)
        int i = 0;
        while (start[i] != '\0' && start[i] != '&' && i < BUFFER_SIZE - 1)
        {
            parameter[i] = start[i];
            i++;
        }
        parameter[i] = '\0'; // Null-terminate the parameterName string
    }
    else
    {
        // If "parameterName=" is not found, return an empty string
        parameter[0] = '\0';
    }
    return parameter;
}

// gcc server.c -o server -pthread

// free port
// sudo lsof -i :8080