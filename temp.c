#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Goal of this file is to create a simple level

// Maybe make the level dynamically generated based off another file
// Maybe make a level editor
struct winsize getWindowSize();

int main(int argc, char **argv) {

  // struct winsize ws = getWindowSize();
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
    return 1;
  }

  int wlength = ws.ws_col;
  int wheight = ws.ws_row;

  for (int i = 0; i < wheight; i++) {
    for (int j = 0; j < wlength; j++) {
      printf("#");
    }
  }
  // Move cursor up n lines to overwrite screen
  printf("\033[%dA\r", wheight);

  fflush(stdout);
  sleep(1);

  for (int i = 0; i < wheight; i++) {
    for (int j = 0; j < wlength; j++) {
      printf("o");
    }
  }
}
