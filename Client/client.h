#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>

// Makrá
#define PORT 45544
#define BUFFER_SIZE 1024

// Globálne premenné
extern int sock; // Socket zdieľaný medzi vláknami
extern pthread_mutex_t send_mutex; // Mutex pre odosielanie správ
extern int game_active; // Indikátor aktívnej hry

// Funkcie
void enable_raw_mode();
void disable_raw_mode();
void *receive_updates(void *arg);
void *send_updates(void *arg);
void start_new_game();
void main_menu();

#endif // CLIENT_H
