#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define PORT 8889
#define BUFFER_SIZE 1024

void list_remote_files(int client_socket) {
    DIR *dir;
    struct dirent *entry;

    // Abrir el directorio actual
    dir = opendir(".");
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return;
    }

    // Enviar la lista de archivos al cliente
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            send(client_socket, entry->d_name, strlen(entry->d_name), 0);
            send(client_socket, "\n", 1, 0); // Enviamos un salto de línea después de cada nombre de archivo
        }
    }

    closedir(dir);
}

void get_file(int socket, char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("No se encontro en este directorio archivo");
        return;
    }

    //enviar el archivo al cliente
    char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(socket, buffer, bytesRead, 0) < 0) {
            perror("Error al enviar archivo");
            break;
        }
    }
    fclose(file);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while (1) {
        // Recibir comando o nombre de archivo del cliente
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // Si se recibe 0 o un valor negativo, se cierra la conexión
            break;
        }
        buffer[bytes_received] = '\0';

        // Procesar comando o nombre de archivo
        if (strcmp(buffer, "ls") == 0) {
            // Si es el comando "ls", llamar a la función correspondiente
            list_remote_files(client_socket);
            
        }else if (strncmp(buffer, "cd ", 3) == 0) {
            // Obtener el directorio especificado
            char *directory = buffer + 3;
            directory[strcspn(directory, "\n")] = '\0'; // Eliminar el salto de línea
            if (chdir(directory) == -1) {
                perror("Error al cambiar de directorio");
            }

        }else if(strncmp(buffer, "get ", 4) == 0){
            //saltarse los 4 caracteres y obtener
            //el nombre del archivo
            char *filename = buffer + 4;
            printf(filename);
            get_file(client_socket, filename);

        }else {
            // Si no es un comando conocido, tratarlo como nombre de archivo
            printf("Recibiendo archivo: %s\n", buffer);
            // Abrir el archivo para escribir
            FILE *file = fopen(buffer, "wb");
            if (file == NULL) {
                perror("Error al abrir el archivo");
                break;
            }

            // Recibir el archivo
            while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
                fwrite(buffer, 1, bytes_received, file);
                if (bytes_received < BUFFER_SIZE) {
                    break;
                }
            }

            fclose(file);
            if (bytes_received <= 0) {
                break;
            }
            printf("Archivo recibido con éxito\n");
        }
    }
    
    close(client_socket);
    printf("Conexión cerrada por el cliente\n");
    return NULL;
}


void *start_server(void *arg) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Crear el socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Vincular el socket al puerto
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al hacer bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Escuchar por conexiones entrantes
    if (listen(server_socket, 10) == -1) {
        perror("Error al escuchar");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d\n", PORT);

    // Aceptar conexiones entrantes
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error al aceptar la conexión");
            continue;
        }
        printf("Conexión aceptada\n");

        // Crear un hilo para manejar al cliente
        pthread_t thread_id;
        int *pclient = malloc(sizeof(int));
        if (pclient == NULL) {
            perror("Error al asignar memoria");
            close(client_socket);
            continue;
        }
        *pclient = client_socket;
        if (pthread_create(&thread_id, NULL, handle_client, pclient) != 0) {
            perror("Error al crear el hilo");
            close(client_socket);
            free(pclient);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_socket);
    return NULL;
}
