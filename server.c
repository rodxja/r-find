
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

    int matchedFiles = 0;

    // extract from uri as /paths?path=dir
    // the var dir in order to search

    char *dir = extract_path_parameter(uri, "dir");
    char *regex = extract_path_parameter(uri, "regex");
    
    // Llamar a searchFiles para obtener la lista de archivos
    char **fileList = searchFiles(dir, regex, &matchedFiles);

    if (fileList != NULL && matchedFiles > 0) {
        // Enviar la lista de archivos a la funcion send_files_list
        send_files_list(client_socket, fileList, matchedFiles);

        // Liberar la memoria de la lista de archivos
        for (int i = 0; i < matchedFiles; i++)
        {
            free(fileList[i]);
        }
        free(fileList);
    } else {
        printf("No se encontraron archivos en el directorio %s con el patrón %s\n", dir, regex);
    }

    free(dir);
    free(regex);

}

void files(int client_socket, const char *uri)
{
    // Extraer los parámetros 'dir' y 'regex' de la URI
    char *dir = extract_path_parameter(uri, "dir");
    char *regex = extract_path_parameter(uri, "regex");

    // Contador de archivos encontrados
    int matchedFiles = 0;

    // Obtener la lista de archivos que coinciden con el patrón
    char **fileList = searchFiles(dir, regex, &matchedFiles);

    // Iterar sobre el fileList y enviar cada archivo individualmente, llamar a send_file
    if (matchedFiles > 0 ) {
        //Iterar sobre la lista
        for (int i = 0; i < matchedFiles; i++)
        {
            send_file(client_socket, fileList[i]);
        }

        // Liberar la memoria de la lista de archivos
        for (int i = 0; i < matchedFiles; i++)
        {
            free(fileList[i]);
        }
        free(fileList);
    } else {
        printf("No se encontraron archivos en el directorio %s con el patrón %s\n", dir, regex);
    }
}

char **searchFiles(const char *dir, const char *regex, int *matchedFiles)
{
    DIR *dirp;
    struct dirent *entry;
    regex_t reg;
    struct stat fileStat;

    char path[BUFFER_SIZE];

    // Inicializar la lista de archivos encontrados
    char **fileList = NULL;

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
        regfree(&reg);
        return NULL;
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
                char **subDirFiles = searchFiles(path, regex, matchedFiles);
                if (subDirFiles != NULL)
                {
                    // Concatenar la lista de archivos encontrados en el subdirectorio
                    int subDirCount = *matchedFiles; 
                    fileList = realloc(fileList, subDirCount * sizeof(char *));
                    for (int i = 0; i < subDirCount; i++)
                    {
                        fileList[*matchedFiles - subDirCount + i] = subDirFiles[i];
                    }
                    free(subDirFiles);
                }
            }
            // Verificar si es un archivo regular
            else if (S_ISREG(fileStat.st_mode))
            {
                // Si es un archivo regular, verificar con el regex
                if (regexec(&reg, entry->d_name, 0, NULL, 0) == 0)
                {
                    // Agregar el archivo a la lista
                    fileList = realloc(fileList, (*matchedFiles + 1) * sizeof(char *));
                    fileList[*matchedFiles] = strdup(path);
                    (*matchedFiles)++;
                }
            }
        }
    }

    regfree(&reg);
    closedir(dirp);

    return fileList;
}

void send_file(int client_socket, char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("error opening file");
        return;
    }

    // Obtener el tamaño del archivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET); // Volver al inicio del archivo

    // Crear encabezado de respuesta HTTP
    char response[BUFFER_SIZE];
    const char *header = "HTTP/1.0 200 OK\r\n"
                         "Server: webserver-c\r\n"
                         "Content-Type: application/octet-stream\r\n"
                         "Content-Length: %ld\r\n\r\n";
    
    snprintf(response, sizeof(response), header, file_size);

    // Enviar encabezado
    send(client_socket, response, strlen(response), 0);

    // Enviar el archivo en partes
    size_t bytes_read;
    char body_content[BUFFER_SIZE];
    
    while ((bytes_read = fread(body_content, 1, sizeof(body_content), file)) > 0)
    {
        // Enviar el contenido del archivo en partes
        send(client_socket, body_content, bytes_read, 0);
    }

    fclose(file);
}


void send_files_list(int client_socket, char **fileList, int matchedFiles)
{
    // Respuesta HTTP con el contenido del cuerpo
    char response[BUFFER_SIZE];
    char *files = malloc(BUFFER_SIZE);  
    files[0] = '\0'; 

    // Crear el encabezado HTTP, asumiendo que la respuesta será JSON
    const char *header = "HTTP/1.0 200 OK\r\n"
                         "Server: webserver-c\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %ld\r\n\r\n";

    int total_size = 0;

    // Iterar sobre el fileList y construir el array JSON
    for (int i = 0; i < matchedFiles; i++)
    {
        // Agregar el nombre de archivo al array JSON
        strcat(files, "\"");
        strcat(files, fileList[i]);
        strcat(files, "\"");

        
        if (i < matchedFiles - 1)
        {
            strcat(files, ",");
        }

        // Calcular la longitud de la cadena actual files
        total_size = strlen(files);

        // Validacion del Buffer
        int body_size = BUFFER_SIZE - strlen(header) - 1;

        // Si agregar otro archivo excede el tamaño del buffer, enviar el contenido actual
        if (total_size > body_size)
        {
            // Formatear el encabezado HTTP
            snprintf(response, sizeof(response),
                     header,
                     total_size);

            // Enviar el encabezado
            send(client_socket, response, strlen(response), 0);

            // Enviar el contenido de los archivos
            send(client_socket, files, strlen(files), 0);

            // Limpiar el buffer de archivos para el siguiente fragmento
            files[0] = '\0'; 
            strcat(files, "["); 
        }
    }

    // Asegurarse de que los archivos restantes sean enviados después del ciclo
    if (strlen(files) > 1) 
    {
        // Agregar el corchete de cierre del array JSON
        strcat(files, "]");

        // Formatear el encabezado HTTP
        snprintf(response, sizeof(response),
                 header,
                 strlen(files));

        // Enviar el encabezado
        send(client_socket, response, strlen(response), 0);

        // Enviar el contenido del cuerpo
        send(client_socket, files, strlen(files), 0);
    }

    // Liberar la memoria dinámica después de usarla
    free(files);
}


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