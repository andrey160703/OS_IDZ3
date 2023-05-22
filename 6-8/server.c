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
pthread_mutex_t mutex_log;
pthread_mutex_t mutex_actual_index;

#define MAX_MESSAGES 10000
#define MAX_MESSAGE_LENGTH 100

char messages[MAX_MESSAGES][MAX_MESSAGE_LENGTH];

int message_count = 0, current_index = 0;

// Добавление сообщения в массив
void add_message(const char* message) {
    if (message_count < MAX_MESSAGES) {
        strncpy(messages[message_count], message, MAX_MESSAGE_LENGTH - 1);
        messages[message_count][MAX_MESSAGE_LENGTH - 1] = '\0';
        message_count++;
        if (message_count % 20 == 0 && current_index > 0) {
            current_index += 10;
        }
    }
}

// Получение сообщения по индексу
const char* get_message(int index) {
    if (index >= 0 && index < message_count) {
        return messages[index];
    }
    return NULL;
}

// Очистка массива сообщений
void clear_messages() {
    message_count = 0;
}

struct Island {
    int x, y;
    bool is_found;
};

struct Island island;
int length, width, group = 0;

typedef struct {
    int x;
    int y;
} CoordinatePair;

CoordinatePair process_data(char *buffer, int group_) {
    CoordinatePair result;
    pthread_mutex_lock(&mutex);
    if (strcmp(buffer, "-1") != 0) {
        char message[100];
        snprintf(message, sizeof(message), "Группа %d нашла %s! денег\n", group_, buffer);
        pthread_mutex_lock(&mutex_log);
        add_message(message);
        pthread_mutex_unlock(&mutex_log);
        island.is_found = true;
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

void *handle_client(void *arg) {
    int new_socket = *(int *) arg;
    char buffer[BUFFER_SIZE] = {0};

    int group_ = 1;
    pthread_mutex_lock(&mutex_log);
    add_message("Подключение нового клиента..\n");
    pthread_mutex_unlock(&mutex_log);
    bool is_searchGroup = false;
    {
        pthread_mutex_lock(&mutex_log);
        add_message("Ожидание статуса клиента\n");
        pthread_mutex_unlock(&mutex_log);
        memset(buffer, 0, sizeof(buffer));
        if (recv(new_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Ошибка при получении данных");
            close(new_socket);
            pthread_exit(NULL);
        }

        int status = atoi(buffer);

        pthread_mutex_lock(&mutex_log);
        if (status == 1) {
            is_searchGroup = true;
            add_message("Клиент - поисковая группа\n");
        } else {
            add_message("Клиент - Терминал\n");
        }
        pthread_mutex_unlock(&mutex_log);
    }
    if (is_searchGroup) {
        {
            pthread_mutex_lock(&mutex_group);
            group++;
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

            char message[100];

            snprintf(message, sizeof(message), "Получено от %d группы: %s\n", group_, buffer);
            pthread_mutex_lock(&mutex_log);
            add_message(message);
            pthread_mutex_unlock(&mutex_log);
            CoordinatePair result = process_data(buffer, group_);
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "%d %d", result.x, result.y);

            if (send(new_socket, response, strlen(response), 0) < 0) {
                perror("Ошибка при отправке ответа");
                close(new_socket);
                pthread_exit(NULL);
            }

            printf("Отправлено группе %d: %s\n", group_, response);

            snprintf(message, sizeof(message), "Отправлено группе %d: %s\n", group_, response);
            pthread_mutex_lock(&mutex_log);
            add_message(message);
            pthread_mutex_unlock(&mutex_log);
            if (result.x == -1 && result.y == -1) {
                pthread_mutex_lock(&mutex_group);
                group--;
                if (group == 0) {
                    printf("Сервер закончил работу");
                    pthread_mutex_lock(&mutex_log);
                    add_message("Сервер закончил работу");
                    pthread_mutex_unlock(&mutex_log);
                    sleep(3);
                    exit(0);
                }
                pthread_mutex_unlock(&mutex_group);
                break;
            }
        }
    } else {
        if (send(new_socket, "Привет!\n", strlen("Привет!\n"), 0) < 0) {
            perror("Ошибка при отправке ответа");
            close(new_socket);
            pthread_exit(NULL);
        }
        int ind = current_index;
        while (1) {
            pthread_mutex_lock(&mutex_log);
            int end = message_count;
            pthread_mutex_unlock(&mutex_log);
            for (int i = ind; i < end; i++) {
                if (send(new_socket, messages[i], strlen(messages[i]), 0) < 0) {
                    perror("Ошибка при отправке ответа");
                    close(new_socket);
                    pthread_exit(NULL);
                }
            }
            if (ind == end) {
                if (send(new_socket, "----------------\n", strlen("----------------\n"), 0) < 0) {
                    perror("Ошибка при отправке ответа");
                    close(new_socket);
                    pthread_exit(NULL);
                }
            }
            ind = message_count;
            sleep(1);
        }
    }
    // Закрываем сокет клиента
    if (close(new_socket) < 0) {
        perror("Ошибка при закрытии сокета клиента");
        pthread_exit(NULL);
    }

    pthread_exit(NULL);
}

void initialize_island_struct(struct Island *s) {
    s->x = 0;
    s->y = 0;
    s->is_found = false;
}

int main(int argc, char *argv[]) {
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
    pthread_mutex_init(&mutex_log, NULL);
    pthread_mutex_init(&mutex_actual_index, NULL);


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
    pthread_mutex_destroy(&mutex_log);
    pthread_mutex_destroy(&mutex_actual_index);
    return 0;
}
