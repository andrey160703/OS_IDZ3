#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>


#include <sys/mman.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>

#define BUFFER_SIZE 1024

pthread_mutex_t mutex;
pthread_mutex_t mutex_group;

struct Island {
    int x, y;
    bool is_found;
};

struct Island island;
int length, width, treasure = -1, group = 0;

typedef struct {
    int x;
    int y;
} CoordinatePair;

CoordinatePair process_data(char* buffer, int group_) {
    CoordinatePair result;
    pthread_mutex_lock(&mutex);
    if (strcmp(buffer, "-1") != 0) {
        printf("Группа %d нашла %s! денег\n", group_, buffer);
        island.is_found = true;
        treasure = atoi(buffer);
    }
    if (island.is_found) {
        result.x = -1;
        result.y = -1;
    } else {
        result.x = island.x;
        result.y = island.y;
    }
    island.x++;
    if (island.x == length) {
        island.x = 0;
        island.y++;
    }
    if (!island.is_found && island.y == width) {
        result.x = island.x = 0;
        result.y = island.y = 0;
    }
    pthread_mutex_unlock(&mutex);
//    printf("Завернул: %d %d\n", result.x, result.y);
    return result;
}

void* handle_client(void* arg) {
    int new_socket = *(int*)arg;
    char buffer[BUFFER_SIZE] = {0};

    int group_ = 1;

    printf("Подключение нового клиента..\n");

    bool is_searchGroup = false;
    {
        printf("Ожидание статуса клиента\n");

        memset(buffer, 0, sizeof(buffer));
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Ошибка при получении данных");
            close(new_socket);
            pthread_exit(NULL);
        }

        int status = atoi(buffer);

        if (status == 1) {
            is_searchGroup = true;
            printf("Клиент - поисковая группа\n");
        } else {
            printf("Клиент - Терминал\n");
        }
    }
    if (is_searchGroup) {
        {
            pthread_mutex_lock(&mutex_group);
            group_ = group;
            pthread_mutex_unlock(&mutex_group);
            char group_str[20];  // Буфер для хранения строки
            snprintf(group_str, sizeof(group_str), "%d", group_);  // Преобразование значения group в строку
            if (send(new_socket, group_str, strlen(group_str), 0) < 0) {
                perror("Ошибка при отправке ответа");
                close(new_socket);
                pthread_exit(NULL);
            }
        }

        while (1) {
            // Получаем данные от клиента
            memset(buffer, 0, sizeof(buffer));
            if (recv(new_socket, buffer, BUFFER_SIZE, 0) < 0) {
                perror("Ошибка при получении данных");
                close(new_socket);
                pthread_exit(NULL);
            }

            printf("Получено от %d группы: %s\n", group_,  buffer);

            CoordinatePair result = process_data(buffer, group_);
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "%d %d", result.x, result.y);

            if (send(new_socket, response, strlen(response), 0) < 0) {
                perror("Ошибка при отправке ответа");
                close(new_socket);
                pthread_exit(NULL);
            }

            printf("Отправлено группе %d: %s\n", group_, response);
            if (result.x == -1 && result.y == -1) {
                pthread_mutex_lock(&mutex_group);
                group--;
                if (group == 0) {
                    printf("Сервер закончил работу");
                    exit(0);
                }
                pthread_mutex_unlock(&mutex_group);
                break;
            }
        }
    } else {

    }
    // Закрываем сокет клиента
    if (close(new_socket) < 0) {
        perror("Ошибка при закрытии сокета клиента");
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

void initialize_island_struct(struct Island* s) {
    s->x = 0;
    s->y = 0;
    s->is_found = false;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Использование: %s <адрес> <порт>\n", argv[0]);
        return 1;
    }

    srand((int) time(0));
    length = rand() % 10 + 2, width = rand() % 10 + 2;

    printf("Прибыли на остров {%dx%d}\n", length, width);

    initialize_island_struct(&island);

    // Инициализируем мьютекс
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_group, NULL);


    const char *address = argv[1];
    int port = atoi(argv[2]);

    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    int opt = 1;
    int addrlen = sizeof(client_addr);

    // Создаем сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Сокет не создан");
        exit(EXIT_FAILURE);
    }

    // Устанавливаем опцию переиспользования адреса и порта
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Ошибка установки опции переиспользования адреса и порта");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(address);
    server_addr.sin_port = htons(port);

    // Привязываем сокет к адресу и порту
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки сокета к адресу и порту");
        exit(EXIT_FAILURE);
    }

    // Ожидаем подключения клиентов
    if (listen(server_fd, 5) < 0) {
        perror("Ошибка ожидания подключений");
        exit(EXIT_FAILURE);
    }

    printf("Ожидание подключений на адресе %s:%d...\n", address, port);
    while (1) {
        // Принимаем подключение клиента
        if ((new_socket = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen)) < 0) {
            perror("Ошибка принятия подключения");
            continue;
        }

        pthread_mutex_lock(&mutex_group);
        group++;
        pthread_mutex_unlock(&mutex_group);

        // Создаем новый поток для обработки клиента
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, &new_socket) != 0) {
            perror("Ошибка создания потока для клиента");
            close(new_socket);
            continue;
        }

        // Освобождаем ресурсы потока
        pthread_detach(tid);

        sleep(1);
        pthread_mutex_lock(&mutex_group);
        if (group == 0 && island.is_found) {
            printf("------------------------------------------------------");
            pthread_mutex_unlock(&mutex_group);
            break;
        }
        pthread_mutex_unlock(&mutex_group);
    }


    // Уничтожаем мьютекс
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&mutex_group);

    return 0;
}
