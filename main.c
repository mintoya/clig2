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

int main(void) {
  // srand((unsigned)time(NULL));
  //
  // const char charset[] =
  //     "0123456789"
  //     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  //     "abcdefghijklmnopqrstuvwxyz";

  // while (1) {
  //   struct term_position size = get_terminal_size();
  //
  //   for (u32 row = 0; row < size.row; row++) {
  //     for (u32 col = 0; col < size.col; col++) {
  //       char c = charset[rand() % (sizeof(charset) - 1)];
  //       u8 fg = rand() % 256;
  //       u8 bg = rand() % 256;
  //       term_setCell_L(row, col, c, fg, bg);
  //     }
  //   }
  //   term_keyboard k = term_getInput(100);
  //   print_wfO(
  //       fileprint,
  //       globalLog,
  //       "{term_keyboard}\n", k
  //   );
  //   term_render();
  //   term_dump();
  // }
  i64 row = 0;
  i64 col = 0;
  while (1) {
    draw_box(row, col, 10, 10);
    term_render();
    term_dump();
    term_keyboard k = term_getInput(100);
    print_wfO(
        fileprint,
        globalLog,
        "{term_keyboard}\n", k
    );
    if (k.kind == term_keyboard_mouse) {
      row = k.data.mouse.row;
      col = k.data.mouse.col;
    }
  }

  return 0;
}
