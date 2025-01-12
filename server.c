#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include "game_logic.h"

#define PORT 8080
#define BUFFER_SIZE 1024

Game *game;
sem_t *sem_game_update;

// Thread to handle periodic game updates
void *game_update_thread(void *arg) {
    printf("Game update thread started.\n");

    while (game->snake.alive) {
        printf("Waiting for game update semaphore...\n");
        if (sem_wait(sem_game_update) == 0) {
            printf("Game update semaphore acquired.\n");

            // Clear console for new game state
            printf("\033[H\033[J");

            // Check for timed game mode
            if (game->mode == TIMED) {
                time_t current_time = time(NULL);
                if (difftime(current_time, game->start_time) >= game->time_limit) {
                    printf("Time's up! Game over.\n");
                    game->snake.alive = 0;
                }
            }

            // Move snake and handle collisions
            if (game->snake.alive && !move_snake(game)) {
                printf("Game over: Snake hit an obstacle or itself.\n");
                game->snake.alive = 0;
            }

            // Check if the snake eats the fruit
            if (points_equal(game->snake.body[0], game->fruit)) {
                game->snake.length += 1;
                generate_fruit(game);
            }

            // Draw the current game state
            draw_game(game);

            printf("Releasing game update semaphore...\n");
            sem_post(sem_game_update);
        } else {
            perror("sem_wait failed");
        }

        sleep(2);
    }

    printf("Game update thread finished.\n");
    return NULL;
}


int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    // Unbuffered output
    setvbuf(stdout, NULL, _IONBF, 0);

    // Initialize shared memory
    game = mmap(NULL, sizeof(Game), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (game == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // Ak semafor existuje, odstráňte ho
    sem_unlink("/game_update");

    // Inicializácia semaforu
    sem_game_update = sem_open("/game_update", O_CREAT | O_EXCL, 0644, 1);
    if (sem_game_update == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }
    printf("Semaphore initialized successfully.\n");

    // Pred spustením vlákna na aktualizáciu hry semafor manuálne nastavíme:
    sem_post(sem_game_update);
    printf("Semaphore set to initial value.\n");

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    // Accept client connection
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    // Receive game settings from the client
    int bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read > 0) {
        int game_mode, time_limit, world_type;
        sscanf(buffer, "%d %d %d", &game_mode, &time_limit, &world_type);
        initialize_game(game, 20, 10, game_mode, time_limit, world_type);
        printf("Game initialized: Mode=%d, Time Limit=%d, World Type=%d\n",
               game_mode, time_limit, world_type);
    } else {
        printf("Failed to receive game settings from client.\n");
    }

    // Start game update thread
    pthread_t game_thread;
    if (pthread_create(&game_thread, NULL, game_update_thread, NULL) != 0) {
        perror("Failed to create game update thread");
        exit(EXIT_FAILURE);
    }

    printf("Game update thread created successfully.\n");

    // Main loop to handle client communication
    while (game->snake.alive) {
        bytes_read = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            int new_direction = atoi(buffer);
            printf("Received input from client: %d\n", new_direction);
            sem_wait(sem_game_update);
            change_direction(&game->snake, new_direction);
            sem_post(sem_game_update);
        }

        // Send game state to the client
        snprintf(buffer, BUFFER_SIZE, "Snake: (%d, %d), Fruit: (%d, %d) - Game Status: %s",
                 game->snake.body[0].x, game->snake.body[0].y,
                 game->fruit.x, game->fruit.y,
                 game->snake.alive ? "Alive" : "Game Over");
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // Clean up resources
    pthread_join(game_thread, NULL);
    close(client_socket);
    close(server_fd);

    sem_close(sem_game_update);
    sem_unlink("/game_update");
    munmap(game, sizeof(Game));

    printf("Server shutdown.\n");
    return 0;
}
