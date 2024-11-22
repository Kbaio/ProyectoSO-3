#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <regex.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define PORT 8899
#define BUFFER_SIZE 1024

void wildcard_to_regex(const char *wildcard, char *regex) {
    const char *w = wildcard;
    char *r = regex;

    while (*w) {
        switch (*w) {
            case '*':
                *r++ = '.';
                *r++ = '*';
                break;
            case '?':
                *r++ = '.';
                break;
            case '.':
            case '\\':
            case '^':
            case '$':
            case '[':
            case ']':
            case '(':
            case ')':
            case '{':
            case '}':
            case '|':
            case '+':
                *r++ = '\\';
                *r++ = *w;
                break;
            default:
                *r++ = *w;
                break;
        }
        w++;
    }

    *r = '\0';
}

void processCommand(int client_socket, char *command, char *directory, char *pattern) {
    DIR *dir;
    struct dirent *entry;
    char buffer[BUFFER_SIZE];
    regex_t regex;
    ssize_t bytes_received;

    // Convertir el patrón de estilo comodín a una expresión regular
    char regex_pattern[BUFFER_SIZE];
    wildcard_to_regex(pattern, regex_pattern);

    // Se compila la expresión regular
    if (regcomp(&regex, regex_pattern, REG_EXTENDED | REG_NOSUB) != 0) {
        perror("Error al compilar la expresión regular");
        return;
    }

    if ((dir = opendir(directory)) == NULL) {
        perror("Error al abrir el directorio");
        return;
    }

    // Si es el comando search, se envían los archivos que coincidan con el patrón
    if (strcmp(command, "search") == 0) {
        while ((entry = readdir(dir)) != NULL) {
            if (regexec(&regex, entry->d_name, 0, NULL, 0) == 0) {
                snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
                send(client_socket, buffer, strlen(buffer), 0);
            }
        }
        closedir(dir);
        regfree(&regex);

    // Si es el comando get, se envían los archivos que coincidan con el patrón
    } else if (strcmp(command, "get") == 0) {
        while ((entry = readdir(dir)) != NULL) {
            if (regexec(&regex, entry->d_name, 0, NULL, 0) == 0) {
                snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
                send(client_socket, buffer, strlen(buffer), 0);

                // Enviar el contenido del archivo
                char filepath[BUFFER_SIZE];
                snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);
                FILE *file = fopen(filepath, "rb");

                if (file == NULL) {
                    perror("Error al abrir el archivo");
                    continue;
                }
                while ((bytes_received = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                    send(client_socket, buffer, bytes_received, 0);
                }

                fclose(file);
            }
        }
        closedir(dir);
        regfree(&regex);
    }
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while (1) {
        // Recibir comando o nombre de archivo del cliente
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            // Si se recibe 0 o un valor negativo, se cierra la conexión
            break;
        }
        buffer[bytes_received] = '\0';

        // Procesar el comando recibido
        char command[BUFFER_SIZE];
        char directory[BUFFER_SIZE];
        char pattern[BUFFER_SIZE];
        sscanf(buffer, "%s %s %s", command, directory, pattern);

        processCommand(client_socket, command, directory, pattern);
    }

    close(client_socket);
    printf("Conexión cerrada por el cliente\n");
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("Error en listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Servidor escuchando en el puerto %d\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error en accept");
            continue;
        }

        printf("Conexión aceptada\n");

        pthread_t thread;
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;
        pthread_create(&thread, NULL, handle_client, pclient);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}