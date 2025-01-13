#ifndef SERVER_H
#define SERVER_H

#include <semaphore.h>
#include "../Game_logic/game_logic.h"

// Makrá
#define PORT 45544
#define BUFFER_SIZE 1024

// Globálne premenné
extern Game *game;
extern sem_t *sem_game_update;

// Funkcie
void draw_game_to_buffer(const Game *game, char *buffer);
void *game_update_thread(void *arg);
void cleanup_resources(int server_fd, int client_socket);

#endif // SERVER_H
