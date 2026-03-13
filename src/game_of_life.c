/*
 * Game of Life
 * Компиляция: gcc game_of_life.c -o game_of_life -lncurses
 * Запуск:     ./game_of_life < patterns/glider.txt
 * Управление: A - быстрее, Z - медленнее, Space - выход
 */

#include <stdio.h>
#include <ncurses.h>

#define ROWS       25
#define COLS       80
#define DELAY_MIN  50
#define DELAY_MAX  900
#define DELAY_DEF  200
#define DELAY_STEP 50

/* ─── Типы ──────────────────────────────────────────────────────── */

typedef struct {
    int cells[ROWS][COLS];
} Board;

typedef struct {
    int step;
    int delay_ms;
    int running;
} GameState;

/* ─── Операции с полем ──────────────────────────────────────────── */

static void board_clear(Board *b)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            b->cells[r][c] = 0;
}

static int board_cell(const Board *b, int r, int c)
{
    return b->cells[(r + ROWS) % ROWS][(c + COLS) % COLS];
}

static int board_count_neighbors(const Board *b, int r, int c)
{
    int count = 0;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++)
            if (dr != 0 || dc != 0)
                count += board_cell(b, r + dr, c + dc);
    return count;
}

static int next_cell_state(int alive, int neighbors)
{
    if (alive)
        return (neighbors == 2 || neighbors == 3) ? 1 : 0;
    return (neighbors == 3) ? 1 : 0;
}

static void board_next_gen(const Board *src, Board *dst)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int nb = board_count_neighbors(src, r, c);
            dst->cells[r][c] = next_cell_state(src->cells[r][c], nb);
        }
}

static void board_copy(const Board *src, Board *dst)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            dst->cells[r][c] = src->cells[r][c];
}

/* ─── Загрузка из файла ─────────────────────────────────────────── */

static void board_load(Board *b, const char *path)
{
    FILE *f = fopen(path, "r");
    char  line[COLS + 4];
    board_clear(b);
    if (!f) return;
    for (int r = 0; r < ROWS; r++) {
        if (!fgets(line, (int)sizeof(line), f)) break;
        for (int c = 0; line[c] && line[c] != '\n' && c < COLS; c++) {
            char ch = line[c];
            b->cells[r][c] = (ch == 'O' || ch == '*' || ch == '1') ? 1 : 0;
        }
    }
    fclose(f);
}

/* ─── Отрисовка ─────────────────────────────────────────────────── */

static void draw_border(void)
{
    mvhline(0,        0, ACS_HLINE, COLS + 2);
    mvhline(ROWS + 1, 0, ACS_HLINE, COLS + 2);
    mvvline(1,        0, ACS_VLINE, ROWS);
    mvvline(1, COLS + 1, ACS_VLINE, ROWS);
    mvaddch(0,        0,        ACS_ULCORNER);
    mvaddch(0,        COLS + 1, ACS_URCORNER);
    mvaddch(ROWS + 1, 0,        ACS_LLCORNER);
    mvaddch(ROWS + 1, COLS + 1, ACS_LRCORNER);
}

static void draw_cells(const Board *b)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            mvaddch(r + 1, c + 1, b->cells[r][c] ? 'O' : ' ');
}

static void draw_status(const GameState *gs)
{
    int speed = (DELAY_MAX - gs->delay_ms) / DELAY_STEP + 1;
    mvprintw(ROWS + 2, 0,
             "Step: %-6d  Speed: %-2d  [A] faster  [Z] slower  [Space] quit",
             gs->step, speed);
}

static void render(const Board *b, const GameState *gs)
{
    erase();
    draw_border();
    draw_cells(b);
    draw_status(gs);
    refresh();
}

/* ─── Управление ────────────────────────────────────────────────── */

static void speed_up(GameState *gs)
{
    if (gs->delay_ms > DELAY_MIN) gs->delay_ms -= DELAY_STEP;
}

static void speed_down(GameState *gs)
{
    if (gs->delay_ms < DELAY_MAX) gs->delay_ms += DELAY_STEP;
}

static void flush_and_process(GameState *gs)
{
    int key;
    while ((key = getch()) != ERR) {
        if (key == 'a' || key == 'A') speed_up(gs);
        else if (key == 'z' || key == 'Z') speed_down(gs);
        else if (key == ' ') gs->running = 0;
    }
}

/* ─── ncurses ───────────────────────────────────────────────────── */

static void ncurses_init(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
}

static void ncurses_shutdown(void)
{
    endwin();
}

/* ─── Игровой цикл ──────────────────────────────────────────────── */

static void game_init(GameState *gs)
{
    gs->step     = 0;
    gs->delay_ms = DELAY_DEF;
    gs->running  = 1;
}

static void game_tick(Board *board, GameState *gs)
{
    Board next;
    board_next_gen(board, &next);
    board_copy(&next, board);
    gs->step++;
}

static void game_loop(Board *board, GameState *gs)
{
    while (gs->running) {
        render(board, gs);
        napms(gs->delay_ms);
        flush_and_process(gs);  /* читаем ВСЕ накопившиеся клавиши */
        if (gs->running)
            game_tick(board, gs);
    }
}

/* ─── Точка входа ───────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    Board     board;
    GameState gs;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pattern_file>\n", argv[0]);
        return 1;
    }

    board_load(&board, argv[1]);
    game_init(&gs);

    ncurses_init();
    game_loop(&board, &gs);
    ncurses_shutdown();

    printf("Game over. Steps: %d\n", gs.step);
    return 0;
}
