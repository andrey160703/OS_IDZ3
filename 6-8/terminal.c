#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Использование: %s <адрес сервера> <порт сервера>\n", argv[0]);
        return 1;
    }

    const char* server_address = argv[1];
    int server_port = atoi(argv[2]);

    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Создаем сокет
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Сокет не создан");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Преобразуем адрес сервера из строки в структуру
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        perror("Ошибка преобразования адреса сервера");
        exit(EXIT_FAILURE);
    }

    // Устанавливаем соединение с сервером
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка соединения с сервером");
        exit(EXIT_FAILURE);
    }

    printf("Подключение к серверу %s:%d\n", server_address, server_port);

    // Говорим что мы терминал
    if (send(client_socket, "2", strlen("1"), 0) < 0) {
        perror("Ошибка при отправке данных на сервер");
        exit(EXIT_FAILURE);
    }

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Ошибка при получении данных от сервера");
            exit(EXIT_FAILURE);
        } else if (bytes_received == 0) {
            printf("Соединение с сервером закрыто\n");
            break;  // Выход из цикла while
        }
        printf("%s\n", buffer);
    }
    // Закрываем соединение с сервером
    close(client_socket);

    return 0;
}
