#include "./string.c"
#include <bits/time.h>
#include <bits/types/struct_timeval.h>
#include <math.h>
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
#define PLAYER_INITIAL_X_POS 0
#define PLAYER_INITIAL_Y_POS 5
#define MAX_PLAYER_VELOCITY 1
#define GRAVITY_ACCELERATION 0
#define PLAYER_MOVEMENT_SPEED 1
#define PLAYER_JUMP_VELOCITY 10

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
  Vector2 *points;
  u_int num_points;
} Hitbox;

typedef struct {
  Sprite sprite;
  Hitbox hitbox;
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
int draw_level_to_buffer(TwoDCharBuffer *buffer, Level *level_data,
                         WinSize *ws);
int render_pixel_row(TwoDCharBuffer *buffer, Vector2 pos, char *pixels,
                     WinSize *ws);
void move_cursor_to_pos(u_int x, u_int y, WinSize *ws);
void disable_canonical_mode();
int check_input();
Vector2 convert_world_pos_to_screen_pos(Vector2 *pos, WinSize *ws);
int draw_sprite_to_buffer(TwoDCharBuffer *buffer, Sprite *sprite, Vector2 pos,
                          WinSize *ws);
void draw_hitbox_to_buffer(TwoDCharBuffer *buffer, Hitbox *hitbox, WinSize *ws);
void draw_line_to_buffer(TwoDCharBuffer *buffer, Vector2 *point1,
                         Vector2 *point2, WinSize *ws);
bool is_colliding(Hitbox *hitbox1, Hitbox *hitbox2);
int init_player(Player *p, WinSize *ws, Level level_data);
void handle_player(Player *player, TwoDCharBuffer *buffer, WinSize *ws);
void add_assign_vector2(Vector2 *this, Vector2 *other);
Vector2 new_Vector2(int x, int y);
Vector2 add_vector2(Vector2 *first, Vector2 *second);
int load_sprite_texture(Sprite *sprite, char *filename);
int max_arr(int *arr, int len);
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

    draw_level_to_buffer(&next, &level_data, &ws);

    if (check_input() != 0) {
      char input = getchar();
      switch (input) {
      case 'a':
        player1.position.x -= PLAYER_MOVEMENT_SPEED;
        break;
      case 'd':
        player1.position.x += PLAYER_MOVEMENT_SPEED;
        break;
      case 's':
        player1.position.y -= PLAYER_MOVEMENT_SPEED;
        break;
      case 'w':
        player1.position.y += PLAYER_MOVEMENT_SPEED;
        break;
      case ' ':
        player1.velocity.y = PLAYER_JUMP_VELOCITY;
      default:
        break;
      }
    }

    handle_player(&player1, &next, &ws);
    draw_sprite_to_buffer(&next, &player1.sprite, player1.position, &ws);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double duration = ((double)(now.tv_sec - prev_frame.tv_sec) +
                       ((double)(now.tv_nsec - prev_frame.tv_nsec) / 1e9));
    double fps = 1 / duration;
    prev_frame = now;
    sprintf(next.buffer, "Fps: %f Player {Pos x : %d, y %d} {Vel x: %d, y %d}",
            fps, player1.position.x, player1.position.y, player1.velocity.x,
            player1.velocity.y);
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
  pos->x = PLAYER_INITIAL_X_POS;
  pos->y = PLAYER_INITIAL_Y_POS;

  player->acceleration.y = GRAVITY_ACCELERATION;
  Vector2 *points = malloc(sizeof(Vector2) * 4);

  // Vector2 top_left_corner = new_Vector2(-(int)player->sprite.lengthpx / 2 -
  // 1,
  //                                       player->sprite.heightpx + 1);
  // points[0] = top_left_corner;
  // Vector2 top_right_corner = new_Vector2((int)player->sprite.lengthpx / 2 +
  // 1,
  //                                        player->sprite.heightpx + 1);
  // points[1] = top_right_corner;
  // Vector2 bottom_right_corner =
  //     new_Vector2((int)player->sprite.lengthpx / 2 + 1, -1);
  // points[2] = bottom_right_corner;
  // Vector2 bottom_left_corner =
  //     new_Vector2(-(int)player->sprite.lengthpx / 2 - 1, -1);
  // points[3] = bottom_left_corner;

  Vector2 top_left_corner = new_Vector2(-(int)player->sprite.lengthpx / 2,
                                        player->sprite.heightpx - 1);
  points[0] = top_left_corner;
  Vector2 top_right_corner = new_Vector2((int)player->sprite.lengthpx / 2,
                                         player->sprite.heightpx - 1);
  points[1] = top_right_corner;
  Vector2 bottom_right_corner =
      new_Vector2((int)player->sprite.lengthpx / 2, 0);
  points[2] = bottom_right_corner;
  Vector2 bottom_left_corner =
      new_Vector2(-(int)player->sprite.lengthpx / 2, 0);
  points[3] = bottom_left_corner;

  player->hitbox.points = points;
  player->hitbox.num_points = 4;

  return 0;
}

void handle_player(Player *player, TwoDCharBuffer *buffer, WinSize *ws) {
  add_assign_vector2(&player->velocity, &player->acceleration);
  add_assign_vector2(&player->position, &player->velocity);
  if (player->velocity.x > MAX_PLAYER_VELOCITY) {
    player->velocity.x = MAX_PLAYER_VELOCITY;
  }

  if (player->velocity.x < -MAX_PLAYER_VELOCITY) {
    player->velocity.x = -MAX_PLAYER_VELOCITY;
  }

  if (player->velocity.y > MAX_PLAYER_VELOCITY) {
    player->velocity.y = MAX_PLAYER_VELOCITY;
  }

  if (player->velocity.y < -MAX_PLAYER_VELOCITY) {
    player->velocity.y = -MAX_PLAYER_VELOCITY;
  }

  Vector2 floor_points[2];
  floor_points[0] = new_Vector2(-(int)ws->cols / 2, 0);
  floor_points[1] = new_Vector2(ws->cols / 2, 0);
  Hitbox floor_hitbox = {
      .points = floor_points,
      .num_points = 2,
  };

  Vector2 ceiling_points[2];
  ceiling_points[0] = new_Vector2(-(int)ws->cols / 2, 20);
  ceiling_points[1] = new_Vector2(ws->cols / 2, 20);
  Hitbox ceiling_hitbox = {
      .points = ceiling_points,
      .num_points = 2,
  };

  Vector2 left_wall_points[2];
  left_wall_points[0] = new_Vector2(-10, 0);
  left_wall_points[1] = new_Vector2(-10, ws->rows - 1);
  Hitbox left_wall = {
      .points = left_wall_points,
      .num_points = 2,
  };

  Vector2 player_points[4];
  for (int i = 0; i < player->hitbox.num_points; i++) {
    player_points[i] =
        add_vector2(&player->position, &player->hitbox.points[i]);
  }

  Hitbox player_hitbox = {.num_points = 4, .points = player_points};
  // draw_hitbox_to_buffer(buffer, &player_hitbox, ws);
  // draw_hitbox_to_buffer(buffer, &floor_hitbox, ws);
  // draw_hitbox_to_buffer(buffer, &ceiling_hitbox, ws);
  // draw_hitbox_to_buffer(buffer, &left_wall, ws);

  if (is_colliding(&floor_hitbox, &player_hitbox)) {
    printf("Colliding \n");
    // player->velocity.y = 0;
    player->position.y = 1;
  }

  if (is_colliding(&ceiling_hitbox, &player_hitbox)) {
    printf("Colliding \n");
    // player->velocity.y = 0;
    player->position.y = 20 - player->sprite.heightpx;
  }

  if (is_colliding(&left_wall, &player_hitbox)) {
    printf("Colliding \n");
    // player->velocity.y = 0;
    player->position.x = -8;
  }
}

int min(int a, int b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

int max(int a, int b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

void draw_hitbox_to_buffer(TwoDCharBuffer *buffer, Hitbox *hitbox,
                           WinSize *ws) {
  for (int i = 0; i < hitbox->num_points; i++) {
    Vector2 *point1 = &hitbox->points[i];
    Vector2 *point2 = &hitbox->points[(i + 1) % hitbox->num_points];
    draw_line_to_buffer(buffer, point1, point2, ws);
  }
}

bool is_colliding_vertical_and_horizontal(Vector2 *vertical1,
                                          Vector2 *vertical2, Vector2 *other1,
                                          Vector2 *other2) {
  if (vertical1->x < min(other1->x, other2->x) ||
      vertical1->x > max(other1->x, other2->x)) {
    return false;
  }
  if (min(vertical1->y, vertical2->y) > max(other1->y, other2->y) ||
      max(vertical1->y, vertical2->y) < min(other1->y, other2->y)) {
    return false;
  }
  return true;
}

void draw_line_to_buffer(TwoDCharBuffer *buffer, Vector2 *point1,
                         Vector2 *point2, WinSize *ws) {
  int distance = (int)ceil(sqrt(pow((double)(point2->y - point1->y), 2.0) +
                                pow((point2->x - point1->x), 2.0)));

  Vector2 screen_point1 = convert_world_pos_to_screen_pos(point1, ws);
  Vector2 screen_point2 = convert_world_pos_to_screen_pos(point2, ws);
  if (screen_point2.x - screen_point1.x == 0) {
    if (screen_point1.y > screen_point2.y) {
      Vector2 temp = screen_point1;
      screen_point1 = screen_point2;
      screen_point2 = temp;
    }
    for (int i = 0; i <= distance; i++) {
      // Calc where to draw point
      Vector2 new_point = new_Vector2(screen_point1.x, screen_point1.y + i);
      // Draw point there
      printToBuffer(buffer, "*", new_point.x, new_point.y);
    }
    return;
  }

  if (screen_point1.x > screen_point2.x) {
    Vector2 temp = screen_point1;
    screen_point1 = screen_point2;
    screen_point2 = temp;
  }

  int slope =
      (screen_point2.y - screen_point1.y) / (screen_point2.x - screen_point1.x);
  int y_intercept = screen_point2.y - slope * screen_point2.x;

  for (int i = 0; i < distance; i++) {
    // Calc where to draw screen_point
    int new_x = screen_point1.x + i;
    Vector2 new_point = new_Vector2(new_x, slope * new_x + y_intercept);
    // Draw screen_point there
    printToBuffer(buffer, "*", new_point.x, new_point.y);
  }
}

bool is_colliding(Hitbox *hitbox1, Hitbox *hitbox2) {
  for (int i = 0; i < hitbox1->num_points; i++) {
    Vector2 *point1 = &hitbox1->points[i];
    Vector2 *point2 = &hitbox1->points[(i + 1) % hitbox1->num_points];

    for (int j = 0; j < hitbox2->num_points; j++) {
      Vector2 *point3 = &hitbox2->points[j];
      Vector2 *point4 = &hitbox2->points[(j + 1) % hitbox2->num_points];
      if (max(point1->x, point2->x) < min(point3->x, point4->x) ||
          max(point3->x, point4->x) < min(point1->x, point2->x)) {
        continue;
      }

      if ((point2->x - point1->x) == 0 || (point4->x - point3->x) == 0) {
        // Two cases: Both lines are vertical -> check if they have the same x
        // One of the lines is vertical -> check if the vertical line intersects
        // the non-vertical line The vertical line just has one x and a y range
        // If the x is between the x's of the non-vertical line and the y ranges
        // overlap, these lines must be intersecting
        bool vertical1 = (point2->x - point1->y) == 0;
        bool vertical2 = (point4->x - point3->y) == 0;
        if (vertical1 && vertical2 && point1->x == point3->x) {
          printf("Vertical ");
          return true;
        }

        if (vertical1) {
          if (!is_colliding_vertical_and_horizontal(point1, point2, point3,
                                                    point4)) {
            continue;
          }
        } else {
          if (!is_colliding_vertical_and_horizontal(point3, point4, point1,
                                                    point2)) {
            continue;
          }
        }

        printf("Vertical ");
        return true;
      }

      int slope1 = (point2->y - point1->y) / (point2->x - point1->x);
      int y_intercept1 = point2->y - slope1 * point2->x;

      int slope2 = (point4->y - point3->y) / (point4->x - point3->x);
      int y_intercept2 = point4->y - slope2 * point4->x;

      if (slope1 == slope2) {
        if (y_intercept1 == y_intercept2) {
          printf("Overlap ");
          return true;
        }
        continue;
      }

      int x = (y_intercept1 - y_intercept2) / (slope2 - slope1);
      int min_x = max(min(point1->x, point2->x), min(point3->x, point4->x));
      int max_x = min(max(point1->x, point2->x), max(point3->x, point4->x));

      if (x < min_x || x > max_x) {
        continue;
      } else {
        printf("Single Point ");
        return true;
      }
    }
  }

  return false;
}

void add_assign_vector2(Vector2 *this, Vector2 *other) {
  this->x += other->x;
  this->y += other->y;
}

Vector2 new_Vector2(int x, int y) {
  Vector2 res = {.x = x, .y = y};
  return res;
}

Vector2 add_vector2(Vector2 *first, Vector2 *second) {
  Vector2 res = new_Vector2(first->x + second->x, first->y + second->y);
  return res;
}

Hitbox new_Hitbox(Vector2 *points, u_int num_points) {
  Hitbox res = {
      .points = points,
      .num_points = num_points,
  };

  return res;
}

int max_arr(int *arr, int len) {
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
  int max_line_length = max_arr(line_lengths, line_count);

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

int draw_level_to_buffer(TwoDCharBuffer *buffer, Level *level_data,
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

int draw_sprite_to_buffer(TwoDCharBuffer *buffer, Sprite *sprite, Vector2 pos,
                          WinSize *ws) {
  // Sprite should be rendered with the middle of the sprite at the x value
  // Sprite should be rendered with the bottom of the sprite at the y value
  Vector2 temppos = new_Vector2(pos.x + (ws->cols / 2 - sprite->lengthpx / 2),
                                ws->rows - (pos.y + sprite->heightpx));
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

Vector2 convert_world_pos_to_screen_pos(Vector2 *pos, WinSize *ws) {
  Vector2 temppos = new_Vector2(pos->x + ws->cols / 2, ws->rows - pos->y - 1);
  return temppos;
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
