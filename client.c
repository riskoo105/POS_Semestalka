#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int sock; // Socket zdieľaný medzi vláknami
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex pre odosielanie správ

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
            printf("Server disconnected\n");
            exit(0);
        }
        buffer[bytes_read] = '\0';
        printf("\nServer: %s\n", buffer);
    }
    return NULL;
}

// Funkcia pre odosielanie vstupov serveru
void *send_updates(void *arg) {
    enable_raw_mode();
    printf("Ovládajte hada pomocou W (hore), A (vľavo), S (dole), D (vpravo). Stlačte 'q' pre ukončenie.\n");

    char ch;
    char buffer[BUFFER_SIZE];
    while (read(STDIN_FILENO, &ch, 1) == 1 && ch != 'q') {
        int direction = -1;

        // Mapovanie vstupov na smery
        switch (ch) {
            case 'w': direction = 0; break; // Hore
            case 'd': direction = 1; break; // Vpravo
            case 's': direction = 2; break; // Dole
            case 'a': direction = 3; break; // Vľavo
        }

        if (direction != -1) {
            snprintf(buffer, BUFFER_SIZE, "%d", direction);
            pthread_mutex_lock(&send_mutex);
            send(sock, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&send_mutex);
        }
    }

    disable_raw_mode();
    close(sock);
    exit(0); // Ukončenie aplikácie po stlačení 'q'
    return NULL;
}

int main() {
    struct sockaddr_in serv_addr;
    pthread_t receive_thread, send_thread;

    // Vytvorenie socketu
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Konverzia IPv4 adresy
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        return -1;
    }

    // Pripojenie k serveru
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }

    printf("Pripojené k serveru\n");

    // Získanie nastavení hry od používateľa
    int game_mode, world_type, time_limit = 0;
    printf("Vyberte režim hry (0 = Štandardný, 1 = Na čas): ");
    scanf("%d", &game_mode);

    if (game_mode == 1) { // Časový režim
        printf("Zadajte časový limit v sekundách: ");
        scanf("%d", &time_limit);
    }

    printf("Vyberte typ sveta (0 = Bez prekážok, 1 = S prekážkami): ");
    scanf("%d", &world_type);

    // Odoslanie nastavení hry serveru
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%d %d %d", game_mode, time_limit, world_type);
    send(sock, buffer, strlen(buffer), 0);

    // Vytvorenie vlákien na odosielanie a prijímanie
    pthread_create(&receive_thread, NULL, receive_updates, NULL);
    pthread_create(&send_thread, NULL, send_updates, NULL);

    // Čakanie na ukončenie vlákien
    pthread_join(send_thread, NULL);
    pthread_cancel(receive_thread);

    // Uvoľnenie zdrojov
    pthread_mutex_destroy(&send_mutex);
    close(sock);
    return 0;
}
