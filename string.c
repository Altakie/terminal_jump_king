#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
typedef struct {
  char *pointer;
  u_int len;
  u_int capacity;
} StackString;
typedef struct {
  u_int rows;
  u_int cols;
  char *buffer;
} TwoDCharBuffer;

StackString newString(u_int capacity);
int appendToString(StackString *string, char *buffer, u_int len);
int write_to_string_at_pos(StackString *string, char *buffer, u_int len,
                           u_int pos);
void clearString(StackString *string);

// int main() {
//   StaticString hello = newString(6);
//   appendToString(&hello, "hello\n", 6);
//   printf("%s", hello.pointer);
//   printf("Len: %d\n", hello.len);
//   printf("Cap: %d\n", hello.capacity);
//   return 0;
// }

StackString newString(u_int capacity) {
  char pointer[capacity + 1];
  pointer[0] = '\0';
  StackString newString = {.pointer = pointer, .len = 0, .capacity = capacity};

  return newString;
}

int appendToString(StackString *string, char *buffer, u_int len) {
  int new_len = string->len + len;
  if (new_len > string->capacity) {
    fprintf(stderr, "String cannot fit this");
    return 1;
  }
  int old_len = string->len;
  for (int i = 0; i < len; i++) {
    string->pointer[old_len + i] = buffer[i];
  }
  string->len = new_len;
  string->pointer[new_len] = '\0';
  return 0;
}

int write_to_string_at_pos(StackString *string, char *buffer, u_int len,
                           u_int pos) {
  if (len + pos > string->capacity) {
    fprintf(stderr, "String cannot fit this");
    return 1;
  }
  string->len = pos;
  return appendToString(string, buffer, len);
}

void clearString(StackString *string) {
  string->len = 0;
  string->pointer[0] = '\0';
}

void clearBuffer(TwoDCharBuffer *buffer) {
  for (int i = 0; i < buffer->cols * buffer->rows; i++) {
    buffer->buffer[i] = 'o';
  }
}

TwoDCharBuffer newBuffer(u_int cols, u_int rows) {
  char *buffer = malloc(rows * cols);
  TwoDCharBuffer res = {.buffer = buffer, .rows = rows, .cols = cols};
  clearBuffer(&res);
  return res;
}

char *getRow(TwoDCharBuffer *buffer, u_int y) {
  return &buffer->buffer[buffer->cols * y];
}

char *getPos(TwoDCharBuffer *buffer, u_int x, u_int y) {
  return &buffer->buffer[buffer->cols * y + x];
}

void printToBuffer(TwoDCharBuffer *buffer, char *string, u_int x, u_int y) {
  u_int buff_index = buffer->cols * y + x;
  u_int string_index = 0;

  while (string[string_index] != '\0') {
    buffer->buffer[buff_index] = string[string_index];
    string_index++;
    buff_index++;
  }
}
