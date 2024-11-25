
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
#define HTTP_REQUEST_SIZE 1024
#define HTTP_RESPONSE_SIZE 1024
#define HTTP_METHOD_SIZE 8
#define HTTP_URI_SIZE 256
#define HTTP_VERSION_SIZE 16
#define PATH_SIZE 512
#define PATH_PARAMETER_SIZE 128
#define MAX_STRINGS 100
#define MAX_CLIENTS 3

int clients[MAX_CLIENTS];
int numClients = 0;

typedef struct
{
    int numFiles;
    char list[MAX_STRINGS][PATH_SIZE];
    regex_t reg;
} FindPaths;

FindPaths *newFindPaths(const char *regex)
{
    FindPaths *findPaths = malloc(sizeof(FindPaths));
    findPaths->numFiles = 0;
    // findPaths->reg = {0};
    //  Compilar el regex
    if (regcomp(&findPaths->reg, regex, REG_EXTENDED) != 0)
    {
        perror("Error compilando el regex");
        free(findPaths);
        return NULL;
    }

    for (int i = 0; i < MAX_STRINGS; i++)
    {
        for (int j = 0; j < PATH_SIZE; j++)
        {
            findPaths->list[i][j] = '\0';
        }
    }
    return findPaths;
}

void freeFindPaths(FindPaths *findPaths)
{
    regfree(&findPaths->reg);
    free(findPaths);
}

void searchFiles(const char *dir, FindPaths *findPaths);
void send_file(int client_socket, FindPaths *findPaths);
char *extract_path_parameter(const char *uri, char *parameterName);
void paths(int client_socket, const char *uri);
void files(int client_socket, const char *uri);
void *handle_client(void *arg);
void send_files_list(int client_socket, FindPaths *findPaths);

int main()
{

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

    char buffer[HTTP_REQUEST_SIZE] = {0};

    // TODO : check difference between read and recv

    // Read from the socket
    ssize_t valread = recv(newsockfd, buffer, HTTP_REQUEST_SIZE, 0);
    if (valread < 0)
    {
        perror("webserver (read)");
        close(newsockfd);
        return NULL;
    }

    printf("buffer: %s\n", buffer);

    // TODO : create router func

    // Read the request
    char method[HTTP_METHOD_SIZE], uri[HTTP_URI_SIZE], version[HTTP_VERSION_SIZE];
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
    else if (strncmp(uri, "/files", 5) == 0)
    {
        files(newsockfd, uri);
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

    // Llamar a searchFiles para obtener la lista de archivos
    FindPaths *findPaths = newFindPaths(regex);

    searchFiles(dir, findPaths);

    if (findPaths != NULL && findPaths->numFiles > 0)
    {
        // Enviar la lista de archivos a la funcion send_files_list
        send_files_list(client_socket, findPaths);
    }
    else
    {
        // TODO : send a message to the client that there are no files
        printf("No se encontraron archivos en el directorio %s con el patrón %s\n", dir, regex);
    }

    freeFindPaths(findPaths);

    free(dir);
    free(regex);
}

void files(int client_socket, const char *uri)
{
    // Extraer los parámetros 'dir' y 'regex' de la URI
    char *dir = extract_path_parameter(uri, "dir");
    char *regex = extract_path_parameter(uri, "regex");

    // Obtener la lista de archivos que coinciden con el patrón
    FindPaths *findPaths = newFindPaths(regex);

    searchFiles(dir, findPaths);

    // Iterar sobre el findPaths y enviar cada archivo individualmente, llamar a send_file
    if (findPaths->numFiles > 0)
    {
        send_file(client_socket, findPaths);

        freeFindPaths(findPaths);
    }
    else
    {
        // TODO : send a message to the client that there are no files
        printf("No se encontraron archivos en el directorio %s con el patrón %s\n", dir, regex);
    }
}

void searchFiles(const char *dir, FindPaths *findPaths)
{
    DIR *dirp;
    struct dirent *entry;
    regex_t reg = {0}; // Initialize the reg variable
    struct stat fileStat;

    char path[PATH_SIZE];

    if (findPaths == NULL)
    {
        printf("Error: findPaths is NULL\n");
        return;
    }

    // Abrir el directorio
    if ((dirp = opendir(dir)) == NULL)
    {
        perror("Error abriendo el directorio");

        return;
    }

    // Leer entradas en el directorio
    while ((entry = readdir(dirp)) != NULL)
    {
        // Ignorar "." y ".." para evitar ciclos infinitos
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Crear la ruta completa del archivo o directorio
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        // Obtener la información del archivo o directorio
        if (stat(path, &fileStat) == 0)
        {
            // Verificar si es un directorio
            if (S_ISDIR(fileStat.st_mode))
            {
                // Si es un directorio, llamar recursivamente a searchFiles
                searchFiles(path, findPaths);
            }
            // Verificar si es un archivo regular
            else if (S_ISREG(fileStat.st_mode))
            {
                // Si es un archivo regular, verificar con el regex
                if (regexec(&findPaths->reg, entry->d_name, 0, NULL, 0) == 0)
                {
                    strncpy(findPaths->list[findPaths->numFiles], path, PATH_SIZE - 1);
                    // findPaths->list[findPaths->numFiles][PATH_SIZE - 1] = '\0'; // Ensure null-termination
                    findPaths->numFiles++;
                }
            }
        }
    }

    closedir(dirp);
}

typedef struct
{
    char name[PATH_SIZE];
    char body[HTTP_RESPONSE_SIZE]; // size is 1024 - name - header - 1 = 1024 - 512 - 128 - 1 = 383
} ResponseFiles;

ResponseFiles *newResponseFiles()
{
    ResponseFiles *responseFiles = malloc(sizeof(ResponseFiles));
    return responseFiles;
}

/* char *getResponseFilesJson(ResponseFiles *responseFiles, int bufferSize)
{
    char *json = malloc(HTTP_RESPONSE_SIZE);
    snprintf(json, HTTP_RESPONSE_SIZE, "{\"name\":\"%s\",\"body\":\"%s\"}", responseFiles->name, responseFiles->body);
    return json;
} */

// TODO : send fileName
void send_file(int client_socket, FindPaths *findPaths)
{

    // Crear encabezado de respuesta HTTP
    char response[HTTP_RESPONSE_SIZE];
    const char *header = "HTTP/1.0 200 OK\r\n"
                         "Server: webserver-c\r\n"
                         "Content-Type: application/octet-stream\r\n"
                         "Content-Length: %ld\r\n\r\n";

    snprintf(response, sizeof(response), header, HTTP_RESPONSE_SIZE); // file_size is not the correct conten length
                                                                      // Enviar encabezado
    ssize_t bytes_send_header = send(client_socket, response, strlen(response), 0);
    printf("bytes sent header: %ld\n", bytes_send_header);

    for (int i = 0; i < findPaths->numFiles; i++)
    {

        FILE *file = fopen(findPaths->list[i], "r");
        if (file == NULL)
        {
            perror("error opening file");
            return;
        }

        // Obtener el tamaño del archivo
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET); // Volver al inicio del archivo

        // get size of filename
        int filename_size = strlen(findPaths->list[i]);

        // Enviar el archivo en partes
        size_t bytes_read;

        const char *responseFile = "{\"name\":\"%s\",\"body\":\"%s\"}";
        char responseBody[HTTP_RESPONSE_SIZE];
        memset(responseBody, 0, sizeof(responseBody));

        int responseFile_size = strlen(responseFile);
        int remainingSize = HTTP_RESPONSE_SIZE - responseFile_size - filename_size /* - 1*/;

        char body_content[remainingSize];
        memset(body_content, 0, sizeof(body_content));

        int counter = 0;

        while ((bytes_read = fread(body_content, 1, sizeof(body_content), file)) > 0)
        {
            snprintf(responseBody, sizeof(responseBody), responseFile, findPaths->list[i], body_content);

            printf("responseBody: %s, %ld\n", responseBody, sizeof(responseBody));

            // Enviar el contenido del archivo en partes
            ssize_t bytes_send_body = send(client_socket, responseBody, strlen(responseBody), 0);

            printf("bytes sent body: %ld\n", bytes_send_body);

            memset(responseBody, 0, sizeof(responseBody));
            memset(body_content, 0, sizeof(body_content));

            printf("counter : %d\n", counter++);
        }

        fclose(file);
    }
}

void send_files_list(int client_socket, FindPaths *findPaths)
{
    // Respuesta HTTP con el contenido del cuerpo
    char response[HTTP_RESPONSE_SIZE];
    // clean response
    memset(response, 0, sizeof(response));
    char *files = malloc(HTTP_RESPONSE_SIZE);
    files[0] = '\0';

    // Crear el encabezado HTTP, asumiendo que la respuesta será JSON
    const char *header = "HTTP/1.0 200 OK\r\n"
                         "Server: webserver-c\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %ld\r\n\r\n";

    int total_size = 0;

    // Iterar sobre el findPaths y construir el array JSON
    for (int i = 0; i < findPaths->numFiles; i++)
    {
        // Agregar el nombre de archivo al array JSON
        strcat(files, findPaths->list[i]);
        strcat(files, "\n");

        // Calcular la longitud de la cadena actual files
        total_size = strlen(files);

        // Validacion del Buffer
        int body_size = HTTP_RESPONSE_SIZE - strlen(header) - 1;

        // Si agregar otro archivo excede el tamaño del buffer, enviar el contenido actual
        if (total_size > body_size)
        {
            // Formatear el encabezado HTTP
            snprintf(response, sizeof(response), header, total_size);

            // Enviar el encabezado
            send(client_socket, response, strlen(response), 0);
            printf("response header: %s\n", response);

            // Enviar el contenido de los archivos
            send(client_socket, files, strlen(files), 0);
            printf("response body: %s\n", files);

            // Limpiar el buffer de archivos para el siguiente fragmento
            files[0] = '\0';
        }
    }

    // Asegurarse de que los archivos restantes sean enviados después del ciclo
    if (strlen(files) > 1)
    {
        // Formatear el encabezado HTTP
        snprintf(response, sizeof(response),
                 header,
                 strlen(files));

        // Enviar el encabezado
        send(client_socket, response, strlen(response), 0);
        printf("response header: %s\n", response);

        // Enviar el contenido del cuerpo
        send(client_socket, files, strlen(files), 0);
        printf("response body: %s\n", files);
    }

    // Liberar la memoria dinámica después de usarla
    free(files);
}

char *extract_path_parameter(const char *uri, char *parameterName)
{

    // add = to the end of the parameter name
    char pName[PATH_PARAMETER_SIZE];
    sprintf(pName, "%s=", parameterName);
    char *parameter = malloc(PATH_PARAMETER_SIZE);

    // Look for "{parameter}=" in the URI

    char *start = strstr(uri, pName);
    if (start)
    {
        // Move pointer to the beginning of the parameterName value
        start += strlen(pName);

        // Copy the filename value until the end of the string or until '&' (if there are multiple parameters)
        int i = 0;
        while (start[i] != '\0' && start[i] != '&' && i < PATH_PARAMETER_SIZE - 1)
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