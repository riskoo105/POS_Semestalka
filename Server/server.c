#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>
#include "../Game_logic/game_logic.h"

#define PORT 45544
#define BUFFER_SIZE 1024

Game *game;
sem_t *sem_game_update;

void draw_game_to_buffer(const Game *game, char *buffer) {
    int index = 0;
    for (int y = 0; y < game->height; y++) {
        for (int x = 0; x < game->width; x++) {
            if (x == 0 || x == game->width - 1 || y == 0 || y == game->height - 1) {
                buffer[index++] = '#';
            } else if (game->world_type == WORLD_WITH_OBSTACLES && game->obstacles[y][x] == 1) {
                buffer[index++] = '#';
            } else if (points_equal(game->fruit, (Point){x, y})) {
                buffer[index++] = 'F';
            } else {
                int is_snake = 0;
                for (int i = 0; i < game->snake.length; i++) {
                    if (points_equal(game->snake.body[i], (Point){x, y})) {
                        is_snake = 1;
                        break;
                    }
                }
                buffer[index++] = is_snake ? 'O' : '.';
            }
        }
        buffer[index++] = '\n'; // Ukončenie riadku
    }
    // Pridanie informácií o ovocí a dĺžke hry
    int fruits_eaten = game->snake.length - 1;
    int game_duration = (int)(difftime(time(NULL), game->start_time) - game->total_pause_time);

    index += snprintf(buffer + index, BUFFER_SIZE - index,
                      "Ovocie: %d\nDĺžka hry: %d sekúnd\n",
                      fruits_eaten, game_duration);

    buffer[index] = '\0'; // Null terminátor
}

// Thread to handle game updates
void *game_update_thread(void *arg) {
    int client_socket = *(int *)arg;
    char game_buffer[BUFFER_SIZE];
    printf("Game update thread started.\n");

    while (game->snake.alive) {
        sem_wait(sem_game_update);

        if (!game->player_status.active) {
            printf("Hráč sa odpojil. Had vymazaný.\n");
            sem_post(sem_game_update);
            break;
        }

        if (game->player_status.paused) {
            if (!game->paused_message_sent) {
                printf("Hra zastavená. Čakanie kým hráč obnoví hru...\n");
                game->paused_message_sent = 1;
                game->pause_start = time(NULL); // Zaznamenaj začiatok pauzy
            }
            sem_post(sem_game_update);
            sleep(1);
            continue;
        } else if (game->paused_message_sent) {
            time_t pause_end = time(NULL); // Zaznamenaj koniec pauzy
            game->total_pause_time += difftime(pause_end, game->pause_start); // Pripočítaj čas pauzy
            printf("Hra zastavená. Čakanie, pohyb začne o 3 sekundy...\n");
            game->paused_message_sent = 0;
            sem_post(sem_game_update);
            sleep(3);
            continue;
        }

        printf("Game update semaphore acquired.\n");

        printf("\033[H\033[J");

        if (game->mode == TIMED) {
            time_t current_time = time(NULL);
            if (difftime(current_time, game->start_time) >= game->time_limit) {
                printf("Čas vypršal! Hra skončila.\n");
                game->snake.alive = 0;
            }
        }

        if (game->snake.alive && !move_snake(game)) {
            printf("Hra skončila: Had narazil do prekážky alebo do seba.\n");
            game->snake.alive = 0;
        }

        if (!game->snake.alive) {
            snprintf(game_buffer, BUFFER_SIZE, "Hra skončila! Zjedeného ovocia: %d\n",
                     game->snake.length - 1);
            send(client_socket, game_buffer, strlen(game_buffer), 0);
            break;
        }

        if (points_equal(game->snake.body[0], game->fruit)) {
            game->snake.length += 1;
            generate_fruit(game);
        }

        draw_game_to_buffer(game, game_buffer);
        send(client_socket, game_buffer, strlen(game_buffer), 0); // Odoslanie hernej mapy

        sem_post(sem_game_update);
        sleep(2);
    }

    printf("Game update thread finished.\n");
    return NULL;
}

void cleanup_resources(int server_fd, int client_socket) {
    if (server_fd >= 0) close(server_fd);
    if (client_socket >= 0) close(client_socket);
    if (sem_game_update) {
        sem_close(sem_game_update);
        sem_unlink("/game_update");
    }
    if (game) {
        for (int i = 0; i < game->height; i++) {
            free(game->obstacles[i]);
        }
        free(game->obstacles);
        free(game);
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    setvbuf(stdout, NULL, _IONBF, 0);

    game = malloc(sizeof(Game));
    if (!game) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    sem_unlink("/game_update");

    sem_game_update = sem_open("/game_update", O_CREAT | O_EXCL, 0644, 1);
    if (sem_game_update == SEM_FAILED) {
        perror("sem_open failed");
        free(game);
        exit(EXIT_FAILURE);
    }
    printf("Semaphore initialized successfully.\n");

    sem_post(sem_game_update);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        cleanup_resources(-1, -1);
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        cleanup_resources(server_fd, -1);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        cleanup_resources(server_fd, -1);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        cleanup_resources(server_fd, -1);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("Accept failed");
        cleanup_resources(server_fd, -1);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
        int width, height, game_mode, time_limit, world_type;
        sscanf(buffer, "%d %d %d %d %d", &width, &height, &game_mode, &time_limit, &world_type);
        initialize_game(game, width, height, game_mode, time_limit, world_type);
        printf("Game initialized: Width=%d, Height=%d, Mode=%d, Time Limit=%d, World Type=%d\n",
               width, height, game_mode, time_limit, world_type);
    } else {
        printf("Failed to receive game settings from client.\n");
    }

    pthread_t game_thread;
    if (pthread_create(&game_thread, NULL, game_update_thread, &client_socket) != 0) {
        perror("Failed to create game update thread");
        cleanup_resources(server_fd, client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Game update thread created successfully.\n");

    while (game->snake.alive) {
        bytes_read = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Received from client: %s\n", buffer);

            if (strcmp(buffer, "pause") == 0) {
                sem_wait(sem_game_update);
                game->player_status.paused = 1;
                sem_post(sem_game_update);
            } else if (strcmp(buffer, "quit") == 0) {
                sem_wait(sem_game_update);
                game->player_status.active = 0;
                sem_post(sem_game_update);
                break;
            }  else if (strcmp(buffer, "resume") == 0) {
                sem_wait(sem_game_update);
                game->player_status.paused = 0;
                sem_post(sem_game_update);
                printf("Čakanie 3 sekundy pred obnovením hry...\n");
                sleep(3);
            }else {
                int new_direction = atoi(buffer);
                sem_wait(sem_game_update);
                change_direction(&game->snake, new_direction);
                sem_post(sem_game_update);
            }
        } else if (bytes_read == 0) {
            printf("Client disconnected.\n");
            break;
        } else {
            perror("Error reading from client");
            break;
        }

        snprintf(buffer, BUFFER_SIZE, "Had: (%d, %d), Ovocie: (%d, %d) - Status hry: %s",
                 game->snake.body[0].x, game->snake.body[0].y,
                 game->fruit.x, game->fruit.y,
                 game->snake.alive ? "Živý" : "Hra skončila");
        send(client_socket, buffer, strlen(buffer), 0);
    }

    snprintf(buffer, BUFFER_SIZE, " Hra skončila! Zjedeného ovocia: %d", game->snake.length - 1);
    send(client_socket, buffer, strlen(buffer), 0);

    pthread_join(game_thread, NULL);

    cleanup_resources(server_fd, client_socket);

    printf("Server shutdown.\n");
    return 0;
}