#include <time.h>

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#define MAX_SNAKE_LENGTH 100
#define STANDARD 0
#define TIMED 1
#define WORLD_NO_OBSTACLES 0
#define WORLD_WITH_OBSTACLES 1

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point body[MAX_SNAKE_LENGTH];
    int length;
    int direction;
    int alive;
} Snake;

typedef struct {
    int paused; // Indikátor, či je hra pozastavená (1 = áno, 0 = nie)
    int active; // Indikátor, či je hráč aktívny (1 = áno, 0 = nie)
} PlayerStatus;

typedef struct {
    int width;
    int height;
    Snake snake;
    Point fruit;
    int mode;             // Game mode: STANDARD or TIMED
    int time_limit;       // Time limit in seconds (for timed mode)
    time_t start_time;    // Start time of the game
    int world_type;       // Type of world: WORLD_NO_OBSTACLES or WORLD_WITH_OBSTACLES
    int **obstacles;      // 2D array for obstacles
    PlayerStatus player_status; // Stav hráča
    int paused_message_sent;
    time_t pause_start; // Čas, kedy sa hra pozastavila
    time_t total_pause_time; // Celkový čas strávený v pauze
} Game;

int points_equal(Point a, Point b);

// Inicializuje hru so zadanou šírkou a výškou.
void initialize_game(Game *game, int width, int height, int mode, int time_limit, int world_type);

// Pohybuje hadom v aktuálnom smere. Vráti 1, ak had zje ovocie, 0 inak.
int move_snake(Game *game);

// Zmení smer pohybu hada.
void change_direction(Snake *snake, int new_direction);

// Skontroluje kolízie (had narazí do seba alebo steny).
int check_collision(const Game *game);

// Generuje nové ovocie na náhodnej pozícii.
void generate_fruit(Game *game);

// Deklarácia funkcie na vykreslenie hernej plochy (bez definície).
void draw_game(const Game *game);

#endif // GAME_LOGIC_H