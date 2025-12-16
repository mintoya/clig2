#include <math.h>
#include <stdio.h>
#define LIST_NOCHECKS
#define MY_TERM_INPUT_C
#include "input.h"
#define MY_TUI_C
#include "tui.h"
#include "wheels/print.h"
#include "wheels/wheels.h"

#include <time.h>
#include <wchar.h>

void draw_box(u32 row, u32 col, u32 rows, u32 cols) {
  if (rows == 0 || cols == 0)
    return;

  wchar top_left = L'┌';
  wchar top_right = L'┐';
  wchar bottom_left = L'└';
  wchar bottom_right = L'┘';
  wchar horizontal = L'─';
  wchar vertical = L'│';

  u8 fg = 15; // example: white
  u8 bg = 0;  // example: black

  // Top and bottom edges
  term_setCell_L(row, col, top_left, fg, bg);
  term_setCell_L(row, col + cols - 1, top_right, fg, bg);
  term_setCell_L(row + rows - 1, col, bottom_left, fg, bg);
  term_setCell_L(row + rows - 1, col + cols - 1, bottom_right, fg, bg);

  for (u32 c = 1; c < cols - 1; ++c) {
    term_setCell_L(row, col + c, horizontal, fg, bg);
    term_setCell_L(row + rows - 1, col + c, horizontal, fg, bg);
  }

  // Left and right edges
  for (u32 r = 1; r < rows - 1; ++r) {
    term_setCell_L(row + r, col, vertical, fg, bg);
    term_setCell_L(row + r, col + cols - 1, vertical, fg, bg);
  }
}
struct term_color term_distance_color(i32 row, i32 col) {
  struct term_color c;
  c.tag = term_color_full;

  i32 d = row * row + col * col;
  u8 hue = (u8)(d >> 3);

  u8 r;
  u8 region = hue / 43;
  u8 remainder = (hue - region * 43) * 6;

  u8 p = 0;
  u8 q = 255 - remainder;
  u8 t = remainder;

  switch (region) {
  case 0:
    r = 255;
    break;
  case 1:
    r = q;
    break;
  case 2:
    r = p;
    break;
  case 3:
    r = p;
    break;
  case 4:
    r = t;
    break;
  default:
    r = 255;
    break;
  }

  c.tag = term_color_idx;
  c.color.colorIDX = r;

  return c;
}

#ifdef _WIN32
extern int nanosleep(const struct timespec *request, struct timespec *remain);
#endif
int main(void) {
  print("hellow utf8  {wcstr}", (wchar *)L"");
  nanosleep(&(struct timespec){2, 0}, NULL);
  i64 row = 0;
  i64 col = 0;
  while (1) {
    term_keyboard k = term_getInput(100);
    if (k.kind == term_keyboard_mouse) {
      row = k.data.mouse.row;
      col = k.data.mouse.col;
      struct term_position screen = get_terminal_size();
      for (int i = 0; i < screen.row; i++) {
        for (int j = 0; j < screen.col; j++) {
          term_setCell(
              (struct term_position){i, j},
              (struct term_cell){
                  L'┘',
                  term_distance_color(i - k.data.mouse.row, j - k.data.mouse.col),
                  term_distance_color(255 - i - k.data.mouse.row, j - k.data.mouse.col),
              }
          );
        }
      }
    }
    term_render();
    // term_dump();
    nanosleep(&(struct timespec){0, 10000000}, NULL);
  }

  return 0;
}
