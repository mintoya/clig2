#ifndef MY_TUI_H
#define MY_TUI_H
#include "wheels/arenaAllocator.h"
#include "wheels/fptr.h"
#include "wheels/types.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

static FILE *globalLog = NULL;
typedef struct term_position {
  i64 row, col;
} term_position;
enum term_colorTag : u8 {
  term_color_def = 0,  // default / reset
  term_color_idx = 1,  // term colors
  term_color_full = 2, // 24 bit color
};
typedef struct term_color {
  union {
    struct {
      u8 r, g, b;
    };
    u8 colorIDX;
  } color;
  enum term_colorTag tag;
} term_color;

static inline term_color term_color_fromHex(u32 hex) {
  return (term_color){
      .color = {
          .r = (u8)((hex & 0xff0000) >> 0x10),
          .g = (u8)((hex & 0x00ff00) >> 0x08),
          .b = (u8)((hex & 0x0000ff) >> 0x00),
      },
      .tag = term_color_full,
  };
}
static inline term_color term_color_fromIdx(u8 hex) {
  return (term_color){
      .color = {.colorIDX = hex},
      .tag = term_color_idx,
  };
};
static inline term_color term_color_default() {
  return (term_color){.tag = term_color_def};
}

typedef struct __attribute__((aligned(16))) term_cell {
  wchar c;
  struct term_color bg;
  struct term_color fg;
  // clang-format off
  bool bold          : 1;
  bool italic        : 1;
  bool underline     : 1;
  bool blinking      : 1;
  bool strikethrough : 1;
  bool inverse       : 1;
  bool visible       : 1;
  // clang-format on
} term_cell;
static bool poseq(term_position a, term_position b) {
  return (a.col == b.col && a.row == b.row);
}
bool coloreq(struct term_color a, struct term_color b) {
  static_assert(sizeof(a) == sizeof(u32), "use memcmp");
  return (*(u32 *)&a == *(u32 *)&b);
}
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

#include "wheels/print.h"

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

int wwidth(wchar wc) {
  #pragma message "wide witdth on windows not implemented"
  return 1;
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
int wwidth(wchar wc) {
  if (!iswprint(cell->c))
    return 0;
  return wcwidth(wc);
}
#else
  #error "platform not supported"
#endif

#include "wheels/hhmap.h"
static HHMap *back_buffer = NULL;
static bool justdumped = false;

#ifdef _WIN32
  #include <io.h>
#endif
#include <time.h>
extern int nanosleep(const struct timespec *request, struct timespec *remain);
void convertwrite(wchar_t *data, usize len) {
  static char *u8data = NULL;
  static usize u8cap = 0;

  usize bufSize = lineup(len * MB_CUR_MAX, 4096);

  if (bufSize > u8cap) {
    if (u8data)
      aFree(defaultAlloc, u8data);
    u8data = (char *)aAlloc(defaultAlloc, bufSize);
    u8cap = bufSize;
    print_wfO(fileprint, globalLog, "maxwrite: {}\n", u8cap);
    print_wfO(fileprint, globalLog, "hmap size: {}\n", HHMap_footprint(back_buffer));
  }

  mbstate_t mbs = {0};
  usize wlen = 0;
  size_t n;
  for (usize i = 0; i < len; i++) {
    size_t n = wcrtomb(u8data + wlen, data[i], &mbs);
    assertMessage(n != ((size_t)-1), "wcrtomb failed?");
    wlen += n;
  }

#if defined(__linux__)
  write(STDOUT_FILENO, u8data, wlen);
#else
  _write(_fileno(stdout), u8data, wlen);
#endif
  // unbuffered
  // fflush(stdout);
  nanosleep(&(struct timespec){0, 1000}, NULL);
}
static_assert(sizeof(term_position) == sizeof(term_cell), "add alignment handling ");
[[gnu::constructor]] void term_setup(void) {
  u32 r = get_terminal_size().row;
  u32 c = get_terminal_size().col;
  back_buffer = HHMap_new(
      sizeof(term_position),
      sizeof(term_cell),
      defaultAlloc,
      r * c * 5
  );
  setvbuf(stdout, NULL, _IONBF, 0);
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
void list_addfgColor(List *printList, struct term_color fg) {
  typeof(fg) currentfg = fg;
  switch (currentfg.tag) {
    case term_color_idx: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[38;5;%dm", (int)currentfg.color.colorIDX);
    } break;
    case term_color_full: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf(
          (wchar *)List_getRefForce(printList, printList->length),
          64,
          L"\033[38;2;%d;%d;%dm",
          (int)currentfg.color.r,
          (int)currentfg.color.g,
          (int)currentfg.color.b
      );
    } break;
    default: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[39m");
    } break;
  }
}
void list_addbgColor(List *printList, struct term_color bg) {
  typeof(bg) currentbg = bg;
  switch (currentbg.tag) {
    case term_color_idx: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[48;5;%dm", (int)currentbg.color.colorIDX);
    } break;
    case term_color_full: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[48;2;%d;%d;%dm", (int)currentbg.color.r, (int)currentbg.color.g, (int)currentbg.color.b);
    } break;
    default: {
      List_resize(printList, printList->length + 64);
      printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[49m");
    } break;
  }
}
bool styleeq(term_cell a, term_cell b) {
  return (
      a.bold == b.bold &&
      a.italic == b.italic &&
      a.underline == b.underline &&
      a.blinking == b.blinking &&
      a.strikethrough == b.strikethrough
  );
}
void L_addStyle(List *l, term_cell c) {
  bool hasStyle = false;
  if (c.bold) {
    List_appendFromArr(l, L"\033[1m", 4);
    hasStyle = true;
  }
  if (c.italic) {
    List_appendFromArr(l, L"\033[3m", 4);
    hasStyle = true;
  }
  if (c.underline) {
    List_appendFromArr(l, L"\033[4m", 4);
    hasStyle = true;
  }
  if (c.blinking) {
    List_appendFromArr(l, L"\033[5m", 4);
    hasStyle = true;
  }
  if (c.strikethrough) {
    List_appendFromArr(l, L"\033[9m", 4);
    hasStyle = true;
  }
  if (!hasStyle) {
    List_appendFromArr(l, L"\033[0m", 4);
  }
}
void term_render(void) {
  static struct term_position last = {0, 0};
  struct term_position recent = get_terminal_size();

  static List *printList = NULL;

  if (!printList)
    printList = List_new(defaultAlloc, sizeof(wchar));

  if (recent.row != last.row || recent.col != last.col) {
    List_appendFromArr(printList, L"\033[0m\033[2J", 8);

    print_wfO(
        fileprint,
        globalLog,
        "cols:{} rows:{}\n",
        (uint)recent.col,
        (uint)recent.row
    );
  }

  List_appendFromArr(printList, L"\033[3J", 4);
  if (justdumped) {
    List_appendFromArr(printList, L"\033[0m\033[2J", 8);
    justdumped = false;
  }

  term_color lastbg = {0};
  term_color lastfg = {0};
  term_cell lastCel = {0};
  for (u32 i = 0; i < HHMap_getMetaSize(back_buffer); i++) {
    for (u32 j = 0; j < HHMap_getBucketSize(back_buffer, i); j++) {
      void *it = HHMap_getCoord(back_buffer, i, j);
      term_position *position = (struct term_position *)it;
      term_cell *cell = (struct term_cell *)((u8 *)it + sizeof(struct term_position));

      term_color currentbg = cell->bg;
      term_color currentfg = cell->fg;

      if (
          cell->visible &&
          position->col < recent.col && position->row < recent.row &&
          position->row > -1 && position->col > -1
      ) {
        List_resize(printList, printList->length + 64);
        printList->length += swprintf((wchar *)List_getRefForce(printList, printList->length), 64, L"\033[%d;%dH", position->row + 1, position->col + 1);

        if (cell->inverse) {
          term_color t = currentbg;
          currentbg = currentfg;
          currentfg = t;
        }
        if (!styleeq(lastCel, *cell)) {
          L_addStyle(printList, *cell);
          lastCel = *cell;
        }
        if (!coloreq(currentbg, lastbg) || !coloreq(currentfg, lastfg)) {
          list_addfgColor(printList, currentfg);
          list_addbgColor(printList, currentbg);
          lastbg = currentbg;
          lastfg = currentfg;
        }
        // TODO move char check to setcell
        wchar_t wc = cell->c && wwidth(cell->c) == 1 ? cell->c : L' ';
        List_append(printList, &wc);
      }
    }
  }
  convertwrite((wchar *)printList->head, printList->length);
  printList->length = 0;
  last = recent;
}
void term_dump() {
  justdumped = true;
  HHMap_clear(back_buffer);
}
#endif // MY_TUI_C
