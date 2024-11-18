/*
Implementar un server que se encarga de buscar los archivos que matchean un regex
Devolver los archivos solicitados

Comando para compilar -> gcc server.c -o server -pthread

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <regex.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8899
#define BUFFER_SIZE 1024

void handleClient(int clientSocket);
void searchFiles(const char *dir, const char *regex, int clientSocket);

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    // Crear socket
    if((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configurar serverAddr
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind el socket
    if(bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1){
        perror("Error binding socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    //Escuchar conexiones
    if(listen(serverSocket, 5) == -1){
        perror("Error listening");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d...\n", PORT);

    while(1){
        //Aceptar conexiones
        if((clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &addrLen)) == -1){
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }

        printf("Cliente conectado: %s\n", inet_ntoa(clientAddr.sin_addr));
        handleClient(clientSocket);
        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}

void handleClient(int clientSocket){
    char buffer[BUFFER_SIZE];
    char dir[BUFFER_SIZE];
    char regex[BUFFER_SIZE];

    // Recibir el directorio y el regex
    memset(buffer, 0, BUFFER_SIZE);
    recv(clientSocket, buffer, BUFFER_SIZE, 0);

    sscanf(buffer, "SEARCH_NAME:%[^|]|%s", dir, regex);

    printf("Comando recibido: %s\n", buffer);
    printf("Buscando archivos en el directorio %s que matcheen %s\n", dir, regex);

    searchFiles(dir, regex, clientSocket);
}

void searchFiles(const char *dir, const char *regex, int clientSocket){
    DIR *dirp;
    struct dirent *entry;
    regex_t reg;
    
    char path[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    int fd, bytesRead;

    if(regcomp(&reg, regex, REG_EXTENDED) != 0){
        perror("Error compilando el regex");
        return;
    }

    if((dirp = opendir(dir)) == NULL){
        perror("Error abriendo el directorio");
        return;
    }

    while((entry = readdir(dirp)) != NULL){
        if(regexec(&reg, entry->d_name, 0, NULL, 0) == 0){
            sprintf(path, "%s/%s", dir, entry->d_name);

            // Enviar el nombre del archivo encontrado
            send(clientSocket, path, strlen(path), 0);
            send(clientSocket, "\n", 1, 0);  // Salto de línea

            if((fd = open(path, O_RDONLY)) == -1){
                fprintf(stderr, "Error al abrir el archivo %s: %s\n", path, strerror(errno));
                continue;
            }

            // Leer y enviar el archivo
            while((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0){
                if(send(clientSocket, buffer, bytesRead, 0) == -1){
                    perror("Error enviando el archivo");
                    close(fd);
                    return;
                }
            }

            if(bytesRead == -1){
                fprintf(stderr, "Error leyendo el archivo %s: %s\n", path, strerror(errno));
            }

            close(fd);
            printf("Archivo enviado: %s\n", path);
        }
    }

    regfree(&reg);
    closedir(dirp);
}
