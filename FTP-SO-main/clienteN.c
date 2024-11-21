#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8899
#define BUFFER_SIZE 1024

int client_socket = -1;

void open_connection(const char *ip_address) {
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(client_socket);
        client_socket = -1;
        return;
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al conectar con el servidor");
        close(client_socket);
        client_socket = -1;
        return;
    }

    printf("Conexión establecida con %s\n", ip_address);
}

void close_connection() {
    if (client_socket != -1) {
        close(client_socket);
        client_socket = -1;
        printf("Conexión cerrada\n");
    } else {
        printf("No hay conexión activa para cerrar\n");
    }
}

void search_files(const char *directory, const char *pattern) {
    if (client_socket == -1) {
        printf("No hay conexión activa. Use '-ip <direccion-ip>' para conectar.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "search %s %s", directory, pattern);
    send(client_socket, buffer, strlen(buffer), 0);

    printf("Archivos encontrados:\n");
    while (recv(client_socket, buffer, BUFFER_SIZE, 0) > 0) {
        printf("%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);
    }
}

void get_files(const char *directory, const char *pattern) {
    if (client_socket == -1) {
        printf("No hay conexión activa. Use '-ip <direccion-ip>' para conectar.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "get %s %s", directory, pattern);
    send(client_socket, buffer, strlen(buffer), 0);

    printf("Descargando archivos:\n");
    while (recv(client_socket, buffer, BUFFER_SIZE, 0) > 0) {
        char *filename = strtok(buffer, "\n");
        while (filename != NULL) {
            FILE *file = fopen(filename, "wb");
            if (!file) {
                perror("Error al abrir el archivo local");
                return;
            }

            ssize_t bytes_received;
            while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
                fwrite(buffer, 1, bytes_received, file);
                if (bytes_received < BUFFER_SIZE) break; // Fin del archivo
            }

            fclose(file);
            printf("Archivo descargado: %s\n", filename);
            filename = strtok(NULL, "\n");
        }
    }
}

void usage() {
    printf("Uso:\n");
    printf("  -name <patron> -ip <direccion-ip> [-get]\n");
    printf("Ejemplos:\n");
    printf("  ./cliente -name \"*.c\" -ip 127.0.0.1\n");
    printf("  ./cliente -name \"*.c\" -ip 127.0.0.1 -get\n");
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        usage();
        return EXIT_FAILURE;
    }

    char *pattern = NULL, *ip = NULL, *directory = ".";
    int get_files_flag = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
            pattern = argv[++i];
        } else if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            ip = argv[++i];
        } else if (strcmp(argv[i], "-get") == 0) {
            get_files_flag = 1;
        }
    }

    if (!pattern || !ip) {
        usage();
        return EXIT_FAILURE;
    }

    open_connection(ip);

    if (get_files_flag) {
        get_files(directory, pattern);
    } else {
        search_files(directory, pattern);
    }

    close_connection();
    return 0;
}
