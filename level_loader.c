#include "./string.c"
#include <bits/time.h>
#include <bits/types/struct_timeval.h>
#include <iso646.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define LEVEL_FILE "level1"
#define PLAYER_SPRITE "default.sprite"

/* TODO
 * - Make renderer separate structure that is passed into functions
 * - Make physics engine
 * - Add collisions
 * - Refactor level loading
 *  - Add animations (swapping between sprites when velocity is 0)
 *  - Refactor input handling and movement system
 */
// Goal of this file is to create a simple level

// Maybe make the level dynamically generated based off another file
// Maybe make a level editor

typedef struct {
  int x;
  int y;
} Vector2;

typedef struct {
  char **texture;
  uint lengthpx;
  uint heightpx;
} Sprite;

typedef struct {
  Sprite texture;

} Level;

typedef struct {
  Vector2 position;
  Vector2 dimensions;
} Hitbox;

typedef struct {
  Sprite sprite;
  Vector2 position;
  Vector2 velocity;
  Vector2 acceleration;
} Player;

struct winsize getWindowSize();

typedef struct {
  u_int cols;
  u_int rows;
} WinSize;

void init();
void handle_exit();
void handle_exit_signal(int sig);
int load_level(char *file_name, Level *level_data);
int print_level_to_buffer(TwoDCharBuffer *buffer, Level *level_data,
                          WinSize *ws);
int render_pixel_row(TwoDCharBuffer *buffer, Vector2 pos, char *pixels,
                     WinSize *ws);
void move_cursor_to_pos(u_int x, u_int y, WinSize *ws);
void disable_canonical_mode();
int check_input();
int render_sprite(TwoDCharBuffer *buffer, Sprite *sprite, Vector2 pos,
                  WinSize *ws);
int init_player(Player *p, WinSize *ws, Level level_data);
void handle_player(Player *player, WinSize *ws);
void add_assign_vector2(Vector2 *this, Vector2 *other);
int load_sprite_texture(Sprite *sprite, char *filename);
int max(int *arr, int len);
void printNext(TwoDCharBuffer *currentBuffer, TwoDCharBuffer *nextBuffer);

WinSize getWinSize() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
    exit(1);
  }
  WinSize res = {.cols = ws.ws_col, .rows = ws.ws_row};

  return res;
}

int main(int argc, char **argv) {
  init();
  signal(SIGTERM, handle_exit_signal);
  atexit(handle_exit);
  // Should use another thread for input handling

  // struct winsize ws = getWindowSize();
  // if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
  //   return 1;
  // }

  WinSize ws = getWinSize();

  TwoDCharBuffer current = newBuffer(ws.cols, ws.rows);
  TwoDCharBuffer next = newBuffer(ws.cols, ws.rows);
  printf("Allocated buffers");

  Level level_data = {0};

  load_level(LEVEL_FILE, &level_data);
  printf("Loaded level");

  // Recursively initializes the player on the stack
  Player player1 = {0};
  init_player(&player1, &ws, level_data);

  struct timespec prev_frame;
  clock_gettime(CLOCK_MONOTONIC, &prev_frame);

  while (true) {

    print_level_to_buffer(&next, &level_data, &ws);

    if (check_input() != 0) {
      char input = getchar();
      switch (input) {
      case 'a':
        player1.position.x -= 1;
        break;
      case 'd':
        player1.position.x += 1;
        break;
      case ' ':
        player1.velocity.y = 10;
      default:
        break;
      }
    }

    handle_player(&player1, &ws);
    render_sprite(&next, &player1.sprite, player1.position, &ws);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double duration = ((double)(now.tv_sec - prev_frame.tv_sec) +
                       ((double)(now.tv_nsec - prev_frame.tv_nsec) / 1e9));
    double fps = 1 / duration;
    prev_frame = now;
    sprintf(next.buffer, "Fps: %f", fps);
    move_cursor_to_pos(1, 1, &ws);
    printNext(&current, &next);
    TwoDCharBuffer temp = current;
    current = next;
    next = temp;

    // struct timespec sleeptime;
    // sleeptime.tv_sec = 0;
    // sleeptime.tv_nsec = 5 * 1000000;
    // nanosleep(&sleeptime, &sleeptime);
  }

  free(current.buffer);
  free(next.buffer);

  // Move cursor up n lines to overwrite screen
  // printf("\033[%dA\r", wheight);
  //
  //
  // fflush(stdout);
  // sleep(1);
  // Reset Cursor to top left and clear screen
  printf("\033[1;1H\r");
}

void init() {
  system("stty -echo");
  disable_canonical_mode();
  printf("\033[?25l"); // hide cursor
}

void handle_exit() {
  system("stty echo");
  printf("\033[?25h"); // show cursor again
}

void handle_exit_signal(int sig) { handle_exit(); }

int init_player(Player *player, WinSize *ws, Level level_data) {
  load_sprite_texture(&player->sprite, PLAYER_SPRITE);

  // Set default position for player
  Vector2 *pos = &player->position;
  pos->x = 0;
  pos->y = 1;

  player->acceleration.y = -10;

  return 0;
}

void handle_player(Player *player, WinSize *ws) {
  add_assign_vector2(&player->velocity, &player->acceleration);
  add_assign_vector2(&player->position, &player->velocity);
}

void add_assign_vector2(Vector2 *this, Vector2 *other) {
  this->x += other->x;
  this->y += other->y;
}

int max(int *arr, int len) {
  if (len == 0) {
    return 0;
  }

  int max = arr[0];
  for (int i = 1; i < len; i++) {
    if (max < arr[i]) {
      max = arr[i];
    }
  }

  return max;
}

int load_sprite_texture(Sprite *sprite, char *filename) {
  FILE *sprite_file;
  sprite_file = fopen(filename, "r");

  if (sprite_file == NULL) {
    printf("error opening sprite file");
    return 1;
  }

  // Find size of file
  if (fseek(sprite_file, 0L, SEEK_END) != 0) {
    perror("Error seeking to EOF");
    return 2;
  }
  long size = ftell(sprite_file);

  // Reset to start of file
  fseek(sprite_file, 0L, SEEK_SET);

  char *file_buffer = malloc(size + 1);

  if (fread(file_buffer, 1, size, sprite_file) < size) {
    printf("error reading file\n");
    return 3;
  }
  file_buffer[size] = '\0';

  // Count lines and line lengths
  int line_count = 0;
  for (int i = 0; i < size; i++) {
    char currchar = file_buffer[i];
    if (currchar == '\n') {
      line_count++;
    }
  }

  int line_lengths[line_count];
  int curr_line = 0;
  int line_length = 0;
  for (int i = 0; i < size; i++) {
    char currchar = file_buffer[i];
    if (currchar == '\n' || currchar == '\0') {
      // Save length of this line
      line_lengths[curr_line] = line_length;

      // Reset
      line_length = 0;
      curr_line++;
    } else {
      line_length++;
    }
  }

  // Only really want largest line length
  int max_line_length = max(line_lengths, line_count);

  // Format sprite into 2D array
  char **texture = malloc(sizeof(char *) * line_count);
  int pos = 0;
  for (int i = 0; i < line_count; i++) {
    char *line = malloc(sizeof(char) * (max_line_length + 1));
    int curr_line_len = line_lengths[i];
    // Fill up the line
    for (int j = 0; j < curr_line_len; j++) {
      char currchar = file_buffer[pos];
      line[j] = currchar;
      pos++;
    }

    for (int j = curr_line_len; j < max_line_length; j++) {
      line[j] = ' ';
    }

    pos++; // To skip newline

    line[max_line_length] = '\0';

    texture[i] = line;
  }

  sprite->texture = texture;
  sprite->heightpx = line_count;
  sprite->lengthpx = max_line_length;

  // Cleanup
  free(file_buffer);
  fclose(sprite_file);

  return 0;
}

int load_level(char *file_name, Level *level_data) {
  load_sprite_texture(&level_data->texture, LEVEL_FILE);
  return 0;
}

int pad_left(char *buffer, int px) {
  if (px < 0) {
    return 1;
  }

  for (int i = 0; i < px; i++) {
    buffer[i] = ' ';
  }

  return 0;
}

int pad_top_bottom(TwoDCharBuffer *buffer, u_int cols, u_int rows) {
  if (rows < 0) {
    return 1;
  }
  for (int i = 0; i < rows; i++) {
    char *row = getRow(buffer, i);
    pad_left(row, cols);
  }

  return 0;
}

int print_level_to_buffer(TwoDCharBuffer *buffer, Level *level_data,
                          WinSize *ws) {
  u_int padding = ws->rows - level_data->texture.heightpx;
  pad_top_bottom(buffer, buffer->cols, padding);
  for (int i = 0; i < level_data->texture.heightpx; i++) {
    u_int row_num = padding + i;
    char *buffer_row = getRow(buffer, row_num);
    char *row = level_data->texture.texture[i];
    pad_left(buffer_row, (ws->cols - level_data->texture.lengthpx) / 2);
    printToBuffer(buffer, row, (ws->cols - level_data->texture.lengthpx) / 2,
                  row_num);
    pad_left(getPos(buffer,
                    (ws->cols - level_data->texture.lengthpx) / 2 +
                        level_data->texture.lengthpx,
                    row_num),
             (ws->cols - level_data->texture.lengthpx) / 2);
    // printToBuffer(buffer, "\n", ws->cols - 1, row_num);
    // if ((i + 1) != level_data->texture.heightpx) {
    //   sprintf(buffer_row, "\n");
    // }
  }

  return 0;
}

int render_pixel_row(TwoDCharBuffer *buffer, Vector2 pos, char *pixels,
                     WinSize *ws) {
  printToBuffer(buffer, pixels, pos.x, pos.y);

  return 0;
}

void move_cursor_to_pos(u_int x, u_int y, WinSize *ws) {
  // Convert vector2 into screen coordinates
  // ANSI Escape codes think 1,1 is top left
  // Want 0,0 to be bottom left of our map
  printf("\033[%d;%dH", y, x);
}

void disable_canonical_mode() {
  struct termios attr;
  tcgetattr(STDIN_FILENO, &attr);

  attr.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &attr);
}

int check_input() {
  fd_set file_descriptor_set;
  struct timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  FD_ZERO(&file_descriptor_set);
  FD_SET(STDIN_FILENO, &file_descriptor_set);

  if (select(STDIN_FILENO + 1, &file_descriptor_set, NULL, NULL, &timeout) ==
      -1) {
    return 0;
  }

  return FD_ISSET(STDIN_FILENO, &file_descriptor_set);
}

int render_sprite(TwoDCharBuffer *buffer, Sprite *sprite, Vector2 pos,
                  WinSize *ws) {
  Vector2 temppos;
  // Sprite should be rendered with the middle of the sprite at the x value
  temppos.x = pos.x + (ws->cols / 2 - sprite->lengthpx / 2);
  // Sprite should be rendered with the bottom of the sprite at the y value
  temppos.y = ws->rows - (pos.y + sprite->heightpx);
  char **texture = sprite->texture;
  int heightpx = sprite->heightpx;
  if (temppos.y < 0 || (temppos.y + sprite->heightpx) > ws->rows) {
    return 1;
  }
  for (int i = 0; i < heightpx; i++) {
    render_pixel_row(buffer, temppos, texture[i], ws);
    temppos.y++;
  }

  return 0;
}

void printNext(TwoDCharBuffer *currentBuffer, TwoDCharBuffer *nextBuffer) {
  u_int rows = currentBuffer->rows;
  u_int cols = currentBuffer->cols;

  // write(STDOUT_FILENO, nextBuffer->buffer, rows * cols);
  fflush(stdout);

  for (int y = 0; y < rows; y++) {
    char *row = getRow(nextBuffer, y);
    printf("\033[%d;1H", y + 1); // move to start of row y
    write(STDOUT_FILENO, row, cols);
    // for (int x = 0; x < cols; x++) {
    //   char currChar = getPos(currentBuffer, x, y)[0];
    //   char nextChar = getPos(nextBuffer, x, y)[0];
    //
    //   if (currChar != nextChar) {
    //     WinSize ws = {.rows = rows, .cols = cols};
    //     move_cursor_to_pos(x, y, &ws);
    //     printf("%c", nextChar);
    //   }
    // }
  }
}
