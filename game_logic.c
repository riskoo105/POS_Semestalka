#include "game_logic.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Helper to check if two points are equal
int points_equal(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}

void initialize_game(Game *game, int width, int height, int mode, int time_limit, int world_type) {
    game->width = width;
    game->height = height;
    game->snake.length = 1;
    game->snake.body[0].x = width / 2;
    game->snake.body[0].y = height / 2;
    game->snake.direction = 1;
    game->snake.alive = 1;
    game->mode = mode;
    game->time_limit = time_limit;
    game->start_time = time(NULL);
    game->world_type = world_type;

    srand(time(NULL));

    // Allocate memory for the obstacle array
    game->obstacles = malloc(height * sizeof(int *));
    for (int i = 0; i < height; i++) {
        game->obstacles[i] = calloc(width, sizeof(int));
    }

    if (world_type == WORLD_WITH_OBSTACLES) {
        for (int i = 0; i < width * height / 10; i++) {
            int x, y;
            do {
                x = rand() % (width - 2) + 1; // Avoid borders
                y = rand() % (height - 2) + 1; // Avoid borders
            } while (points_equal((Point){x, y}, game->snake.body[0]) || game->obstacles[y][x] == 1);

            game->obstacles[y][x] = 1; // Place obstacle
        }
    }

    generate_fruit(game);
}

int move_snake(Game *game) {
    if (!game->snake.alive) return 0;

    Point head = game->snake.body[0];

    switch (game->snake.direction) {
        case 0: head.y -= 1; break; // Up
        case 1: head.x += 1; break; // Right
        case 2: head.y += 1; break; // Down
        case 3: head.x -= 1; break; // Left
    }

    if (head.x < 0 || head.x >= game->width || head.y < 0 || head.y >= game->height) {
        if (game->world_type == WORLD_NO_OBSTACLES) {
            if (head.x < 0) head.x = game->width - 1;
            if (head.x >= game->width) head.x = 0;
            if (head.y < 0) head.y = game->height - 1;
            if (head.y >= game->height) head.y = 0;
        } else {
            game->snake.alive = 0;
            return 0;
        }
    }

    if (game->world_type == WORLD_WITH_OBSTACLES && game->obstacles[head.y][head.x] == 1) {
        game->snake.alive = 0;
        return 0;
    }

    for (int i = game->snake.length; i > 0; i--) {
        game->snake.body[i] = game->snake.body[i - 1];
    }

    game->snake.body[0] = head;

    if (check_collision(game)) {
        game->snake.alive = 0;
        return 0;
    }

    return 1;
}

void change_direction(Snake *snake, int new_direction) {
    // Prevent reversing direction
    if ((snake->direction + 2) % 4 != new_direction) {
        snake->direction = new_direction;
    }
}

int check_collision(const Game *game) {
    Point head = game->snake.body[0];

    // Check if the snake hits itself
    for (int i = 1; i < game->snake.length; i++) {
        if (points_equal(head, game->snake.body[i])) {
            return 1;
        }
    }

    return 0;
}

void generate_fruit(Game *game) {
    int x, y;
    int collision;

    do {
        collision = 0;
        x = rand() % game->width;
        y = rand() % game->height;

        // Ensure fruit is not placed on the snake or on the walls
        if (x == 0 || x == game->width - 1 || y == 0 || y == game->height - 1) {
            collision = 1; // Avoid walls
        }

        for (int i = 0; i < game->snake.length; i++) {
            if (game->snake.body[i].x == x && game->snake.body[i].y == y) {
                collision = 1; // Avoid snake body
                break;
            }
        }
    } while (collision);

    game->fruit.x = x;
    game->fruit.y = y;
}

void draw_game(const Game *game) {
    printf("Drawing game...\n");
    for (int y = 0; y < game->height; y++) {
        for (int x = 0; x < game->width; x++) {
            if (x == 0 || x == game->width - 1 || y == 0 || y == game->height - 1) {
                printf("#");
            } else if (game->world_type == WORLD_WITH_OBSTACLES && game->obstacles[y][x] == 1) {
                printf("#");
            } else if (points_equal(game->fruit, (Point){x, y})) {
                printf("F");
            } else {
                int is_snake = 0;
                for (int i = 0; i < game->snake.length; i++) {
                    if (points_equal(game->snake.body[i], (Point){x, y})) {
                        is_snake = 1;
                        break;
                    }
                }
                if (is_snake) {
                    printf("O");
                } else {
                    printf(".");
                }
            }
        }
        printf("\n");
    }
    printf("Game state drawn.\n");
}
