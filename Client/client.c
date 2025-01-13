#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>

#define PORT 45544
#define BUFFER_SIZE 1024

int sock; // Socket zdieľaný medzi vláknami
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex pre odosielanie správ
int game_active = 0; // Indikátor aktívnej hry

// Konfigurácia terminálu na raw mode
void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Vypnutie kanonického módu a echo
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Obnovenie normálneho režimu terminálu
void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Funkcia pre prijímanie správ od servera
void *receive_updates(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int bytes_read = read(sock, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) {
            printf("Server odpojený\n");
            close(sock); // Uzavretie socketu
            pthread_exit(NULL); // Ukončenie vlákna
        }
        buffer[bytes_read] = '\0';

        // Vymaž obrazovku a vykresli hernú mapu
        printf("\033[H\033[J"); // Escape sekvencie na vyčistenie terminálu
        printf("%s\n", buffer);

        // Kontrola ukončenia hry
        if (strstr(buffer, "Hra skončila") != NULL) {
            printf("%s\n", buffer);
            printf("Hra skončila. Ukončujem aplikáciu...\n");
            game_active = 0;
            close(sock);
            exit(0);
        }
    }
    return NULL;
}


// Funkcia pre odosielanie vstupov serveru
void *send_updates(void *arg) {
    enable_raw_mode();
    printf("Ovládajte hada pomocou W (hore), A (vľavo), S (dole), D (vpravo).\n");
    printf("Stlačte 'p' pre pozastavenie a 'q' pre ukončenie hry.\n");

    char ch;
    char buffer[BUFFER_SIZE];
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == 'q') {
            snprintf(buffer, BUFFER_SIZE, "quit");
            send(sock, buffer, strlen(buffer), 0);
            game_active = 0;
            break;
        } else if (ch == 'p') {
            snprintf(buffer, BUFFER_SIZE, "pause");
            send(sock, buffer, strlen(buffer), 0);
            break;
        } else if (ch == 'r') {
            snprintf(buffer, BUFFER_SIZE, "resume");
            send(sock, buffer, strlen(buffer), 0);
            sleep(3); // Čakanie pred obnovením hry
        } else {
            int direction = -1;

            switch (ch) {
                case 'w': direction = 0; break;
                case 'd': direction = 1; break;
                case 's': direction = 2; break;
                case 'a': direction = 3; break;
            }

            if (direction != -1) {
                snprintf(buffer, BUFFER_SIZE, "%d", direction);
                pthread_mutex_lock(&send_mutex);
                send(sock, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&send_mutex);
            }
        }
    }

    disable_raw_mode();
    return NULL;
}

void start_new_game() {
    int width, height, game_mode, world_type, time_limit = 0;
    printf("Zadajte šírku herného sveta: ");
    scanf("%d", &width);
    printf("Zadajte výšku herného sveta: ");
    scanf("%d", &height);
    printf("Vyberte režim hry (0 = Štandardný, 1 = Na čas): ");
    scanf("%d", &game_mode);

    if (game_mode == 1) {
        printf("Zadajte časový limit v sekundách: ");
        scanf("%d", &time_limit);
    }

    printf("Vyberte typ sveta (0 = Bez prekážok, 1 = S prekážkami): ");
    scanf("%d", &world_type);

    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%d %d %d %d %d", width, height, game_mode, time_limit, world_type);
    pthread_mutex_lock(&send_mutex);
    send(sock, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&send_mutex);
    game_active = 1;
}

void main_menu() {
    int choice;
    pthread_t send_thread;

    while (1) {
        printf("\n--- Hlavné Menu ---\n");
        printf("1. Nová hra\n");
        printf("2. Pokračovať v hre\n");
        printf("3. Skonči\n");
        printf("Vaša voľba: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                start_new_game();
                pthread_create(&send_thread, NULL, send_updates, NULL);
                pthread_join(send_thread, NULL);

                // Po skončení hry sa vráťte do hlavného menu
                if (!game_active) {
                    printf("Vraciame sa do hlavného menu...\n");
                }
                break;
            case 2:
                if (game_active) {
                    printf("Obnovujem hru...\n");
                    char buffer[BUFFER_SIZE];
                    snprintf(buffer, BUFFER_SIZE, "resume");
                    pthread_mutex_lock(&send_mutex);
                    send(sock, buffer, strlen(buffer), 0);
                    pthread_mutex_unlock(&send_mutex);

                    pthread_t resume_thread;
                    pthread_create(&resume_thread, NULL, send_updates, NULL);
                    pthread_join(resume_thread, NULL);
                } else {
                    printf("Nie je aktívna žiadna hra na pokračovanie.\n");
                }
                break;
            case 3:
                printf("Ukončujem aplikáciu...\n");

                // Uvoľnenie zdrojov
                pthread_mutex_destroy(&send_mutex);
                close(sock);
                exit(0);
            default:
                printf("Neplatná voľba. Skúste znova.\n");
        }
    }
}

int main() {
    struct sockaddr_in serv_addr;
    pthread_t receive_thread;

    // Skúste sa pripojiť k serveru
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Connected to server\n");
    pthread_create(&receive_thread, NULL, receive_updates, NULL);

    main_menu();

    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);
    pthread_mutex_destroy(&send_mutex);
    close(sock);
    return 0;
}