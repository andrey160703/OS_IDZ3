#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>


#include <sys/mman.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    srand((int) time(0));
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

    int answer = -1;

    // Говорим что мы клиент
    if (send(client_socket, "1", strlen("1"), 0) < 0) {
        perror("Ошибка при отправке данных на сервер");
        exit(EXIT_FAILURE);
    }

    // получаем первую точку для раскопое
    memset(buffer, 0, sizeof(buffer));
    if (recv(client_socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("Ошибка при получении данных от сервера");
        exit(EXIT_FAILURE);
    }
    int x = -1, y = -1, number;

    sscanf(buffer, "%d", &number);

    printf("Группе присвоен номер %d\n", number);

    while (1) {
        answer = (rand() % 40 == 0 ? rand() % 12345 : -1);
        if (answer < 0) {
            printf("Клад не найден (\n");
        } else {
            printf("Нашли клад - %d денег\n", answer);
        }
        snprintf(buffer, BUFFER_SIZE, "%d", answer);
        // Отправляем сообщение на сервер
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Ошибка при отправке данных на сервер");
            exit(EXIT_FAILURE);
        }
        // Получаем ответ от сервера
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Ошибка при получении данных от сервера");
            exit(EXIT_FAILURE);
        }

        int x, y;
        sscanf(buffer, "%d %d", &x, &y);

        if (x == -1 || y == -1) {
            break;
        }

        printf("Флинт послал искать в {%d:%d}\n", x, y);
        sleep(rand() % 2 + 1);

    }
    printf("Группа %d закончила раскопки острова", number);
    // Закрываем соединение с сервером
    close(client_socket);

    return 0;
}
