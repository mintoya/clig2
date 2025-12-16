#ifndef MY_TUI_H
#define MY_TUI_H
#include "wheels/arenaAllocator.h"
#include "wheels/fptr.h"
#include "wheels/print.h"
#include "wheels/types.h"
#include <stdio.h>

void List_wcs_addint(List *li, u32 in) {
  usize l = 1;
  while (l <= in / 10)
    l *= 10;
  while (l) {
    char c = in / l + '0';
    List_append(li, &c);
    in %= l;
    l /= 10;
  }
}
// data is unusable as wchar after
void quick_wchar_utf(wchar_t *data, size_t len) {
  char *utf8_ptr = (char *)data;
  size_t utf8_len = 0;

  for (int i = 0; i < len; i++) {
    mbstate_t mbs = {0};
    utf8_len += wcrtomb(utf8_ptr + utf8_len, data[i], &mbs);
  }

  fwrite(utf8_ptr, 1, utf8_len, stdout);
}
struct term_position {
  i32 row, col; // negetives not valid at all
};
struct term_color {
  union {
    struct {
      u8 r, g, b;
    };
    u8 colorIDX;
  } color;
  enum : u8 {
    term_color_def = 0,  // default / reset
    term_color_idx = 1,  // 256 bit color
    term_color_full = 2, // 24 byte color
  } tag;
};
struct term_cell {
  wchar c;
  struct term_color bg;
  struct term_color fg;
};
void term_render(void);
// clears the cell storage
// should use this if youre rendering everything every frame
void term_dump(void);
void term_setCell(struct term_position, struct term_cell cell);
void term_setCell_L(i32 row, i32 col, wchar character, u8 fgcolorLabel, u8 bgcolorLable);
void term_setCell_LL(i32 row, i32 col, wchar character, u8 fgr, u8 fgg, u8 fgb, u8 bgr, u8 bgg, u8 bgb);
struct term_position get_terminal_size(void);
#endif // MY_TUI_H

#if (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0)
#define MY_TUI_C
#endif
#ifdef MY_TUI_C
#if defined(_WIN32)
  #include <windows.h>
struct term_position get_terminal_size() {
  struct term_position size = {0, 0};

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    size.col = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    size.row = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  }

  return size;
}
  #include <fcntl.h>
  #include <io.h>
[[gnu::constructor]] void rawConsoleSetup() {
  _setmode(_fileno(stdout), _O_BINARY);
}
#elif defined(__linux__)
  #include <sys/ioctl.h>
  #include <unistd.h>
// #include <fcntl.h>
// [[gnu::constructor]] void rawConsoleSetup() {
//   fcntl(STDOUT_FILENO, F_SETFL, 0x4000);
// }
struct term_position get_terminal_size() {
  struct term_position size = {0, 0};

  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
    size.col = ws.ws_col;
    size.row = ws.ws_row;
  }

  return size;
}
#else
  #error "platform not supported"
#endif

static HHMap *back_buffer = NULL;
static List *dumpList = NULL;
static FILE *globalLog = NULL;

[[gnu::constructor]] void term_setup(void) {
  size_t key_size = sizeof(struct term_position);
  size_t value_size = sizeof(struct term_cell);
  size_t value_align = alignof(struct term_cell);
  size_t padded_key_size = (key_size + value_align - 1) & ~(value_align - 1);

  u32 r = get_terminal_size().row;
  u32 c = get_terminal_size().col;
  back_buffer = HHMap_new(
      padded_key_size,
      value_size,
      defaultAlloc,
      r * c * 5
  );
  dumpList = List_new(defaultAlloc, sizeof(struct term_position));
  globalLog = fopen("tui.log", "a");
}
[[gnu::destructor]] void term_cleanup(void) {
  HHMap_free(back_buffer);
}
void term_setCell(struct term_position pos, struct term_cell cell) {
  HHMap_fset(back_buffer, ((fptr){sizeof(struct term_position), (u8 *)&pos}), &cell);
}
void term_setCell_L(i32 row, i32 col, wchar character, u8 fgcolorLabel, u8 bgcolorLable) {
  term_setCell(
      (struct term_position){row, col},
      (struct term_cell){
          .c = character,
          .bg = (struct term_color){
              .tag = term_color_idx,
              .color = fgcolorLabel,
          },
          .fg = (struct term_color){
              .tag = term_color_idx,
              .color = bgcolorLable,
          },
      }
  );
}
void term_setCell_LL(i32 row, i32 col, wchar character, u8 fgr, u8 fgg, u8 fgb, u8 bgr, u8 bgg, u8 bgb) {
  term_setCell(
      (struct term_position){row, col},
      (struct term_cell){
          .c = character,
          .bg = (struct term_color){
              .tag = term_color_full,
              .color = {fgr, fgg, fgb},
          },
          .fg = (struct term_color){
              .tag = term_color_full,
              .color = {bgr, bgg, bgb},
          },
      }
  );
}
#include <time.h>
void term_render(void) {

  static struct term_position last = {0, 0};
  struct term_position recent = get_terminal_size();

  static List *printList = NULL;

  if (!printList)
    printList = List_new(defaultAlloc, sizeof(wchar));

  if (recent.row != last.row || recent.col != last.col) {
    fwrite("\033[0m\033[3J\033[2J\033[H", sizeof(char), 16, stdout);

    print_wfO(
        fileprint,
        globalLog,
        "cols:{} rows:{}\n",
        (uint)recent.col,
        (uint)recent.row
    );
  }

  for (u32 i = 0; i < List_length(dumpList); i++) {
    const struct term_position *position = (struct term_position *)List_getRef(dumpList, i);
    if (position->col < recent.col && position->row < recent.row && position->row > -1 && position->col > -1)
      print_wfO(asPrint, &printList, "\033[{};{}H\033[49m ", position->row + 1, position->col + 1);
  }
  dumpList->length = 0;
  for (u32 i = 0; i < HHMap_getMetaSize(back_buffer); i++) {
    for (u32 j = 0; j < HHMap_getBucketSize(back_buffer, i); j++) {
      void *it = HHMap_getCoord(back_buffer, i, j);
      if (!it)
        break;
      const struct term_position *position = (struct term_position *)it;
      const struct term_cell *cell = (struct term_cell *)((u8 *)it + sizeof(struct term_position));
      if (position->col < recent.col && position->row < recent.row && position->row > -1 && position->col > -1) {
        List_resize(printList, printList->length + 64);
        printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[%d;%dH", position->row + 1, position->col + 1);

        switch (cell->fg.tag) {
        case term_color_idx: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[38;5;%dm", (int)cell->fg.color.colorIDX);
        } break;
        case term_color_full: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[38;2;%d;%d;%dm", (int)cell->fg.color.r, (int)cell->fg.color.g, (int)cell->fg.color.b);
        }
        default: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[39m");
        } break;
        }

        switch (cell->bg.tag) {
        case term_color_idx: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[48;5;%dm", (int)cell->bg.color.colorIDX);
        } break;
        case term_color_full: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[48;2;%d;%d;%dm", (int)cell->bg.color.r, (int)cell->bg.color.g, (int)cell->bg.color.b);
        }
        default: {
          List_resize(printList, printList->length + 64);
          printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[49m");
        } break;
        }

        wchar_t wc = cell->c ? cell->c : L' ';
        List_resize(printList, printList->length + 64);
        printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"%lc", wc);
      }
    }
  }
  quick_wchar_utf((wchar *)printList->head, printList->length);

  fflush(stdout);
  printList->length = 0;
  last = recent;
}
void term_dump() {
  for (u32 i = 0; i < HHMap_getMetaSize(back_buffer); i++) {
    for (u32 j = 0; j < HHMap_getBucketSize(back_buffer, i); j++) {
      void *it = HHMap_getCoord(back_buffer, i, j);
      if (it)
        List_append(dumpList, it);
    }
  }
  HHMap_clear(back_buffer);
}
#endif // MY_TUI_C
