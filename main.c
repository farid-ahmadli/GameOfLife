#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>
#include <SDL_ttf.h>

const char *CONFIG_FILE = "resources/game.cfg";
const char *FINAL_OUTPUT = "game.out";
const char *FONT_PATH = "resources/block.ttf";
int FONT_SIZE = 28;

typedef struct GameOfLife {
    int grid_width;
    int grid_height;
    int max_evolution_steps;
    char **grid;
    int window_width;
    int window_height;
    int grid_cell_size;
    int evolution_step;
    SDL_bool evolution_active;

    SDL_Color grid_background_color;
    SDL_Color grid_line_color;
    SDL_Color grid_live_cell_color;
    SDL_Color grid_dead_cell_color;
} GameOfLife;


GameOfLife *loadGame(const char *filename);

void evolveToNextGeneration(GameOfLife *game);

void renderGameWorld(GameOfLife *game, SDL_Renderer *renderer);

void quitGame(GameOfLife *game, SDL_bool saveGame);

/**
 * This helper function reads three unsigned int values from the given file
 * @param file
 * @param color
 */
int readColor(FILE *file, SDL_Color *color) {
    if (file == NULL || color == NULL) return 0;
    unsigned int r, g, b;
    if (fscanf(file, "%u %u %u", &r, &g, &b) < 3)
        return 0;
    color->r = r;
    color->g = g;
    color->b = b;
    color->a = 255;
    return 1;
}

/**
 * Loads game from the given configuration file
 * @param filename
 * @return
 */
GameOfLife *loadGame(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file)
        return NULL;

    GameOfLife *game = (GameOfLife *) malloc(sizeof(GameOfLife));

    int gameLoaded = 0;

    while (!gameLoaded) {
        if (fscanf(file, "%u %u %u", &game->grid_height, &game->grid_width, &game->max_evolution_steps) < 3)
            break;

        game->evolution_step = 0;
        game->evolution_active = SDL_FALSE;

        int h = game->grid_height + 2;
        int w = game->grid_width + 2;

        game->grid = (char **) malloc(h * sizeof(char *));
        for (int i = 0; i < h; i++)
            game->grid[i] = (char *) malloc(w * sizeof(char));

        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                int cell;

                if (i == 0 || i == h - 1 || j == 0 || j == w - 1)
                    cell = 0;
                else {
                    fscanf(file, "%u", &cell);
                }

                game->grid[i][j] = (char) cell;
            }
        }

        if (fscanf(file, "%u %u %u", &game->window_width, &game->window_height, &game->grid_cell_size) < 3)
            break;

        // in case if grid is too large to fit the screen
        if (game->window_width < game->grid_width * game->grid_cell_size + 200)
            game->window_width = game->grid_width * game->grid_cell_size + 200;

        if (game->window_height < game->grid_height * game->grid_cell_size + 200)
            game->window_height = game->grid_height * game->grid_cell_size + 200;

        // adjust font size
        FONT_SIZE = game->window_height / 20;

        if (!readColor(file, &game->grid_background_color)
            || !readColor(file, &game->grid_line_color)
            || !readColor(file, &game->grid_live_cell_color)
            || !readColor(file, &game->grid_dead_cell_color))
            break;

        gameLoaded = 1;
    }

    fclose(file);

    // if game is not loaded, quit the game
    if (!gameLoaded) {
        quitGame(game, 0);
        game = NULL;
    }

    return game;
}

/**
 * Game evolution step
 * @param game
 */
void evolveToNextGeneration(GameOfLife *game) {
    if (game == NULL) return;

    int h = game->grid_height + 2;
    int w = game->grid_width + 2;

    char prevGen[h][w];

    for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
            prevGen[i][j] = game->grid[i][j];

    for (int i = 1; i <= game->grid_height; i++)
        for (int j = 1; j <= game->grid_width; j++) {
            int neighbors = prevGen[i - 1][j - 1]
                            + prevGen[i - 1][j]
                            + prevGen[i - 1][j + 1]
                            + prevGen[i][j + 1]
                            + prevGen[i + 1][j + 1]
                            + prevGen[i + 1][j]
                            + prevGen[i + 1][j - 1]
                            + prevGen[i][j - 1];

            if (game->grid[i][j]) {
                if (neighbors < 2 || neighbors > 3)
                    game->grid[i][j] = 0;
            }
            else {
                if (neighbors == 3)
                    game->grid[i][j] = 1;
            }
        }

    game->evolution_step++;
}

void setDrawColor(SDL_Renderer *renderer, const SDL_Color *color) {
    SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
}

/**
 * Draws text at position (x, y). If alignCenter is true, center of the text will be aligned with (x, y).
 * @param renderer
 * @param x
 * @param y
 * @param text
 * @param alignCenter
 */
void drawText(SDL_Renderer *renderer, int x, int y, const char *text, SDL_bool alignCenter) {
    if (!TTF_WasInit())
        TTF_Init();

    TTF_Font *sans = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!sans) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
        return;
    }

    SDL_Color black = {0, 0, 0};

    SDL_Surface *surfaceMessage = TTF_RenderText_Solid(sans, text, black);

    SDL_Texture *message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

    SDL_Rect rect;
    TTF_SizeText(sans, text, &rect.w, &rect.h);

    rect.x = x;
    if (alignCenter)
        rect.x -= rect.w / 2;
    rect.y = y;

    SDL_RenderCopy(renderer, message, NULL, &rect);

    SDL_FreeSurface(surfaceMessage);
    SDL_DestroyTexture(message);
}

void renderGameWorld(GameOfLife *game, SDL_Renderer *renderer) {
    // Draw grid background.
    setDrawColor(renderer, &game->grid_background_color);
    SDL_RenderClear(renderer);

    int cell_size = game->grid_cell_size;
    int center_x = game->window_width / 2;
    int center_y = game->window_height / 2;
    int grid_area_width = game->grid_width * cell_size;
    int grid_area_height = game->grid_height * cell_size;

    int left = center_x - grid_area_width / 2;
    int right = left + grid_area_width;
    int top = center_y - grid_area_height / 2;
    int bottom = top + grid_area_height;

    if (!game->evolution_active) {
        if (game->evolution_step < game->max_evolution_steps) {
            drawText(renderer, 10, 10, "Press any key", SDL_FALSE);
            drawText(renderer, 10, 40, "to START the evolution", SDL_FALSE);
        }
        else {
            drawText(renderer, 10, 10, "Evolution STOPPED", SDL_FALSE);
            drawText(renderer, 10, 40, "Press ESC to QUIT", SDL_FALSE);
        }
    }
    else {
        drawText(renderer, 10, 10, "Evolution STARTED", SDL_FALSE);
    }

    setDrawColor(renderer, &game->grid_line_color);

    for (int x = left; x <= right; x += cell_size)
        SDL_RenderDrawLine(renderer, x, top, x, bottom);

    for (int y = top; y <= bottom; y += cell_size)
        SDL_RenderDrawLine(renderer, left, y, right, y);

    SDL_Rect square = {
            .x = 0,
            .y = 0,
            .w = cell_size - 1,
            .h = cell_size - 1,
    };

    printf("Rendering evolution step: %d\n", game->evolution_step); // for debugging purposes
    char caption[50];
    sprintf(caption, "Evolution step: %d", game->evolution_step);
    drawText(renderer, (left + right) / 2, bottom + 20, caption, SDL_TRUE);

    for (int i = 1; i <= game->grid_height; i++) {
        for (int j = 1; j <= game->grid_width; j++) {

            printf("%d", game->grid[i][j]); // for debugging purposes

            if (game->grid[i][j])
                setDrawColor(renderer, &game->grid_live_cell_color);
            else
                setDrawColor(renderer, &game->grid_dead_cell_color);

            square.x = left + (j - 1) * cell_size + 1;
            square.y = top + (i - 1) * cell_size + 1;

            SDL_RenderFillRect(renderer, &square);
        }
        putchar('\n');
    }

    SDL_RenderPresent(renderer);
}

void quitGame(GameOfLife *game, SDL_bool saveGame) {
    if (game == NULL) return;

    // save final game configuration to file before releasing it from memory
    if (saveGame) {
        FILE *file = fopen(FINAL_OUTPUT, "w");
        fprintf(file, "%u %u\n", game->grid_height, game->grid_width);
        for (int i = 1; i <= game->grid_height; i++) {
            for (int j = 1; j <= game->grid_width; j++) {
                fputc('0' + game->grid[i][j], file);
                if (j < game->grid_width)
                    fputc(' ', file);
            }
            fputc('\n', file);
        }
        fclose(file);
    }

    // release allocated memory
    for (int i = 0; i < game->grid_height + 2; i++)
        free(game->grid[i]);
    free(game->grid);
    free(game);
}

void initSDL(GameOfLife *game, SDL_Window **window, SDL_Renderer **renderer) {

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("[Error] SDL Init : %s \n", SDL_GetError());
        return;
    }

    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);

    if (SDL_CreateWindowAndRenderer(game->window_width, game->window_height, SDL_WINDOW_SHOWN, window, renderer) != 0) {
        fprintf(stderr, "[Error] SDL Create Window : %s \n", SDL_GetError());
        return;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    SDL_SetWindowTitle(*window, "Game Of Life");
}

int main() {

    GameOfLife *game = loadGame(CONFIG_FILE);

    if (!game) {
        fprintf(stderr, "Game config could not be loaded. Bye!");
        exit(1);
    }

    SDL_Window *window;
    SDL_Renderer *renderer;

    initSDL(game, &window, &renderer);
    if (!window || !renderer) {
        printf("SDL could not be loaded. Bye!");
        exit(1);
    }

    renderGameWorld(game, renderer);

    SDL_bool quit = SDL_FALSE;

    time_t lastRenderTime = 0;

    while (!quit) {
        SDL_Event event;
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = SDL_TRUE;
                    break;

                case SDL_KEYDOWN:
                    if (!game->evolution_active && game->evolution_step == game->max_evolution_steps)
                        if (event.key.keysym.sym == SDLK_ESCAPE)
                            quit = SDL_TRUE;

                case SDL_MOUSEBUTTONDOWN:
                    if (!game->evolution_active && game->evolution_step < game->max_evolution_steps) {
                        game->evolution_active = SDL_TRUE;
                        printf("Evolution started\n");
                    }
                    break;
            }
        }

        if (game->evolution_active && SDL_GetTicks() - lastRenderTime > 1000) {
            evolveToNextGeneration(game);
            renderGameWorld(game, renderer);
            lastRenderTime = SDL_GetTicks();
            if (game->evolution_step == game->max_evolution_steps) {
                game->evolution_active = SDL_FALSE;
                printf("Evolution stopped\n");
                renderGameWorld(game, renderer);
            }
        }

        SDL_Delay(10);
    }

    quitGame(game, SDL_TRUE);

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}