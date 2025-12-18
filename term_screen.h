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
  bool dim           : 1;
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
  if (a.tag != b.tag)
    return false;
  switch (a.tag) {
    case term_color_full:
      return (
          a.color.r == b.color.r &&
          a.color.g == b.color.g &&
          a.color.b == b.color.b
      );
    case term_color_idx:
      return a.color.colorIDX == b.color.colorIDX;
    default:
      return true;
  }
}
void term_render(void);
// clears the cell storage
// should use this if youre rendering everything every frame
void term_dump(void);
__attribute__((hot)) void term_setCell(struct term_position, struct term_cell cell);
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
  if (!iswprint(wc))
    return 0;
  #pragma message "wcwidth"
  return 1;
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
// not benchmarked
// assuming MB_CUR_MAX never changes
void wsc_add(u8 *place, usize *len, wchar wc) {
  mbstate_t mbs = {0};
  *len += wcrtomb((char *)(place + *len), wc, &mbs);
  return;
  // if (wc >= 0x10000) {
  //   mbstate_t mbs = {0};
  //   *len += wcrtomb((char *)(place + *len), wc, &mbs);
  //   return;
  // }
  // static u8 *storage[0x10000] = {0};
  // if (!storage[0]) {
  //   usize entrySize = MB_CUR_MAX + 2;
  //   storage[0] = (u8 *)aAlloc(defaultAlloc, entrySize * 0x10000);
  //   for (u32 i = 1; i < 0x10000; i++) {
  //     storage[i] = storage[0] + (i * entrySize);
  //     storage[i][0] = 0;
  //   }
  // }
  //
  // if (!storage[wc][0]) {
  //   mbstate_t mbs = {0};
  //   size_t n = wcrtomb((char *)(storage[wc] + 2), wc, &mbs);
  //   assertMessage(n < ((u8)-1));
  //   storage[wc][0] = 1;
  //   storage[wc][1] = (u8)n;
  // }
  // u8 length = storage[wc][1];
  // memcpy(place + *len, storage[wc] + 2, length);
  // *len += length;
}
void convertwrite(wchar_t *data, usize len) {
  static char *u8data = NULL;
  static usize u8cap = 0;

  usize bufSize = lineup(24 + len * MB_CUR_MAX, 4096);

  if (bufSize > u8cap) {
    if (u8data)
      aFree(defaultAlloc, u8data);
    u8data = (char *)aAlloc(defaultAlloc, bufSize);
    u8cap = bufSize;
    print_wfO(fileprint, globalLog, "maxwrite: {}\n", u8cap);
    print_wfO(fileprint, globalLog, "hmap size: {}\n", HHMap_footprint(back_buffer));
    print_wfO(fileprint, globalLog, "hmap keys: {}\n", (usize)HHMap_count(back_buffer));
    print_wfO(fileprint, globalLog, "hmap collisions: {}\n", (usize)HHMap_countCollisions(back_buffer));
  }

  usize wlen = 0;
  mbstate_t mbs = {0};
  size_t n;

#ifndef TERM_NOSYNC
  memcpy(u8data + wlen, "\033[?2026h", 8);
  wlen += 8;
#endif
  if (justdumped) {
    memcpy(u8data + wlen, "\033[0m\033[2J", 8);
    wlen += 8;
  }
  for (usize i = 0; i < len; i++)
    wsc_add((u8 *)u8data, &wlen, data[i]);
#ifndef TERM_NOSYNC
  memcpy(u8data + wlen, "\033[?2026l", 8);
  wlen += 8;
#endif

#if defined(__linux__)
  write(STDOUT_FILENO, u8data, wlen);
#else
  _write(_fileno(stdout), u8data, wlen);
#endif
  justdumped = false;
}
static_assert(sizeof(term_position) == sizeof(term_cell), "add alignment handling ");
[[gnu::constructor]] void term_setup(void) {
  u32 r = get_terminal_size().row;
  u32 c = get_terminal_size().col;
  back_buffer = HHMap_new(
      sizeof(term_position),
      sizeof(term_cell),
      defaultAlloc,
      r * c / 3
  );
  setvbuf(stdout, NULL, _IONBF, 0);
  globalLog = fopen("tui.log", "a");
}
[[gnu::destructor]] void term_cleanup(void) {
  HHMap_free(back_buffer);
}
__attribute__((hot)) void term_setCell(struct term_position pos, struct term_cell cell) {
  HHMap_set(back_buffer, &pos, &cell);
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
// will break if mp_cur_max ever changes
// appends wchar to place+len, then increments len
#include <time.h>
void L_fgcolor(List *printList, struct term_color fg) {
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
      List_appendFromArr(printList, L"\033[39m", 6);
    } break;
  }
}
void L_bgcolor(List *printList, struct term_color bg) {
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
  // return false;
  return (
      a.dim == b.dim &&
      a.bold == b.bold &&
      a.italic == b.italic &&
      a.underline == b.underline &&
      a.blinking == b.blinking &&
      a.strikethrough == b.strikethrough
  );
}
void L_addStyle(List *l, term_cell c, term_cell last) {
  bool hasStyle = false;
  if (c.bold)
    List_appendFromArr(l, L"\033[1m", 4);
  else if (last.bold)
    List_appendFromArr(l, L"\033[22m", 5);
  if (c.dim)
    List_appendFromArr(l, L"\033[2m", 4);
  else if (last.dim)
    List_appendFromArr(l, L"\033[22m", 5);
  if (c.italic)
    List_appendFromArr(l, L"\033[3m", 4);
  else if (last.italic)
    List_appendFromArr(l, L"\033[23m", 5);
  if (c.underline)
    List_appendFromArr(l, L"\033[4m", 4);
  else if (last.underline)
    List_appendFromArr(l, L"\033[24m", 5);
  if (c.blinking)
    List_appendFromArr(l, L"\033[5m", 4);
  else if (last.blinking)
    List_appendFromArr(l, L"\033[25m", 5);
  if (c.strikethrough)
    List_appendFromArr(l, L"\033[9m", 4);
  else if (last.strikethrough)
    List_appendFromArr(l, L"\033[29m", 5);
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

  term_color lastbg = {0};
  term_color lastfg = {0};
  term_cell lastCel = {0};
  const u32 metaSize = HHMap_getMetaSize(back_buffer);
  for (u32 i = 0; i < metaSize; i++) {
    const u32 bucketSize = HHMap_getBucketSize(back_buffer, i);
    for (u32 j = 0; j < bucketSize; j++) {
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
          L_addStyle(printList, *cell, lastCel);
          lastCel = *cell;
        }
        if (!coloreq(currentbg, lastbg) || !coloreq(currentfg, lastfg)) {
          L_fgcolor(printList, currentfg);
          L_bgcolor(printList, currentbg);
        }
        lastbg = currentbg;
        lastfg = currentfg;
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
