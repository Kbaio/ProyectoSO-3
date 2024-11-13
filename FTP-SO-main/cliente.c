#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8889
#define BUFFER_SIZE 1024

int client_socket = -1;  // Socket del cliente global para manejar la conexión actual
char current_directory[BUFFER_SIZE];

void list_local_files() {
    system("ls");
}

void change_local_directory(const char *path) {
    if (chdir(path) == 0) {
        printf("Directorio cambiado a: %s\n", path);
    } else {
        perror("Error al cambiar de directorio");
    }
}

void remote_cd(const char *directory) {
    if (client_socket == -1) {
        printf("No hay conexión activa. Use 'open <dirección-ip>' para iniciar una conexión.\n");
        return;
    }

    // Enviar el comando al servidor
    send(client_socket, directory, strlen(directory), 0);
}

void get_remote_files(){
    if (client_socket == -1) {
        printf("No hay conexión activa. Use 'open <direccion-ip>' para iniciar una conexión.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    fd_set readfds;
    struct timeval tv;

    // Enviar instrucción "ls" al servidor
    send(client_socket, "ls", strlen("ls"), 0);

    // Inicializar el conjunto de descriptores de lectura
    FD_ZERO(&readfds);
    FD_SET(client_socket, &readfds);

    // Establecer un tiempo de espera para select()
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    // Recibir y mostrar los archivos
    printf("Archivos en el Servidor:");
    while (1) {
        // Limpiar el buffer antes de recibir datos
        memset(buffer, 0, sizeof(buffer));

        // Usar select para esperar a que el socket esté listo para leer
        int retval = select(client_socket + 1, &readfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
            break;
        } else if (retval == 0) {
            break;
        } else {
            // Hay datos disponibles para leer
            bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            buffer[bytes_received] = '\0';
            printf("%s",buffer);
        }
    }
}

void send_file(const char *filename) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    //Si la conexion no existe
    if (client_socket == -1) {
        printf("No hay conexión activa. Use 'open <direccion-ip>' para iniciar una conexión.\n");
        return;
    }

    // Abrir el archivo para leer
    file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        return;
    }

    // Enviar el nombre del archivo
    send(client_socket, filename, sizeof(filename), 0);

    // Enviar el archivo
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
    printf("Archivo enviado con éxito: %s\n", filename);
}

void open_connection(const char *ip_address) {
    struct sockaddr_in server_addr;

    // Crear el socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(client_socket);
        client_socket = -1;
        return;
    }

    // Conectar al servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al conectar con el servidor");
        close(client_socket);
        client_socket = -1;
        return;
    }

    printf("Conexión establecida con %s\n", ip_address);
}

/*
 * Funcion: receive_file
 * Entradas:
 *  socket: descriptor de socket
 *  filename: nombre de archivo a enviar
 * 
 * Se encarga de recibir el archivo del servidor
*/

void receive_file(int socket, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("No se pudo abrir el archivo");
        return;
    }

    //escribir el archivo en el directorio actual
    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived;
    while ((bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytesReceived, file);
    }
    fclose(file);
}


//solicitar el archivo al servidor
void request_file( char *peer_ip, char *filename) {
    int client_socket;
    struct sockaddr_in peer_address;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    peer_address.sin_family = AF_INET;
    peer_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, peer_ip, &peer_address.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr *)&peer_address, sizeof(peer_address)) < 0) {
        perror("Error en connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    //hacer la solicitud al servidor y obtener archivo
    send(client_socket, filename, strlen(filename), 0);
    filename = filename + 4;
    receive_file(client_socket, filename);

    close(client_socket);
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
