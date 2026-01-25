#ifndef MY_TUI_H
#define MY_TUI_H
#include "wheels/types.h"
#include <stdio.h>

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

typedef struct __attribute__((aligned(16))) term_cell {
  wchar c;
  term_color bg;
  term_color fg;
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

typedef enum : u8 {
  // clang-format off
  term_cell_VISIBLE       = 0b1,
  term_cell_BOLD          = 0b10,
  term_cell_DIM           = 0b100,
  term_cell_ITALIC        = 0b1000,
  term_cell_UNDERLINE     = 0b10000,
  term_cell_BLINKING      = 0b100000,
  term_cell_STRIKETHROUGH = 0b1000000,
  term_cell_INVERSE       = 0b10000000,
  // clang-format on
} term_cell_flags;

// workaround for bitfield
// make a copy
const term_cell *term_makeCell(wchar character, term_color fg, term_color bg, term_cell_flags f);
term_color term_color_fromIdx(u8 hex);
term_color term_color_fromHex(u32 hex);
bool coloreq(struct term_color a, struct term_color b);
void term_render(void);
// clears the cell storage
// should use this if youre rendering everything every frame
void term_dump(void);

__attribute__((hot)) void term_setCell(struct term_position, term_cell cell);
__attribute__((hot)) void term_setCell_Ref(struct term_position pos, const struct term_cell *cell);

typedef enum {
  term_line_center,
} term_line_opt;
void term_setLine(term_position start, term_cell design, wchar *ptr, usize length, usize max);

struct term_position term_getTsize(void);
#endif // MY_TUI_H

#if (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0)
#define MY_TUI_C
#endif
#ifdef MY_TUI_C

void term_setLine(term_position start, term_cell design, wchar *ptr, usize length, usize max) {
  i64 i = start.col;
  for (; ptr[i - start.col]; i++) {
    design.c = ptr[i - start.col];
    term_setCell((term_position){start.row, i}, design);
  }
  for (; i < start.col + max; i++) {
    design.c = L' ';
    term_setCell((term_position){start.row, i}, design);
  }
}

const term_cell *term_makeCell(wchar character, term_color fg, term_color bg, term_cell_flags f) {
  static thread_local term_cell res = {0};
  res.c = character;
  res.fg = fg;
  res.bg = bg;

  // clang-format off
  res.bold          = f & term_cell_BOLD          ;
  res.dim           = f & term_cell_DIM           ;
  res.italic        = f & term_cell_ITALIC        ;
  res.underline     = f & term_cell_UNDERLINE     ;
  res.blinking      = f & term_cell_BLINKING      ;
  res.strikethrough = f & term_cell_STRIKETHROUGH ;
  res.inverse       = f & term_cell_INVERSE       ;
  res.visible       = f & term_cell_VISIBLE       ;
  // clang-format on

  return &res;
}

static bool poseq(term_position a, term_position b) {
  return (a.col == b.col && a.row == b.row);
}
term_color term_color_fromHex(u32 hex) {
  return (term_color){
      .color = {
          .r = (u8)((hex & 0xff0000) >> 0x10),
          .g = (u8)((hex & 0x00ff00) >> 0x08),
          .b = (u8)((hex & 0x0000ff) >> 0x00),
      },
      .tag = term_color_full,
  };
}
term_color term_color_fromIdx(u8 hex) {
  return (term_color){
      .color = {.colorIDX = hex},
      .tag = term_color_idx,
  };
};
static inline term_color term_color_default() {
  return (term_color){.tag = term_color_def};
}
#include "wheels/arenaAllocator.h"
#include "wheels/fptr.h"
#include "wheels/types.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

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
#include "wheels/print.h"

#if defined(_WIN32)
  #include <windows.h>
struct term_position term_getTsize() {
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
struct term_position term_getTsize() {
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
static mHmap(term_position, term_cell) back_buffer = NULL;
static bool justdumped = false;

#ifdef _WIN32
  #include <io.h>
#endif
#include <time.h>
extern int nanosleep(const struct timespec *request, struct timespec *remain);
// not benchmarked
// assuming MB_CUR_MAX never changes
__attribute__((hot)) void convertwrite(wchar_t *data, usize len) {
  static char *u8data = NULL;
  static usize u8cap = 0;

  usize bufSize = lineup(24 + len * MB_CUR_MAX, 4096);

  if (bufSize > u8cap) {
    if (u8data)
      aFree(defaultAlloc, u8data);
    u8data = (char *)aAlloc(defaultAlloc, bufSize);
    u8cap = bufSize;
    print_wfO(fileprint, globalLog, "maxwrite: {}\n", u8cap);
    print_wfO(fileprint, globalLog, "{} by {}\n", term_getTsize().row, term_getTsize().col);
    print_wfO(fileprint, globalLog, "hmap size: {}\n", HMap_footprint((HMap *)back_buffer));
    print_wfO(fileprint, globalLog, "hmap keys: {}\n", (usize)HMap_count((HMap *)back_buffer));
    print_wfO(fileprint, globalLog, "hmap collisions: {}\n", (usize)HMap_countCollisions((HMap *)back_buffer));
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
    wlen += wcrtomb((char *)(u8data + wlen), data[i], &mbs);
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
  u32 r = term_getTsize().row;
  u32 c = term_getTsize().col;
  back_buffer = mHmap_init(
      arena_new_ext(pageAllocator, 2048), // dont intend to free
      term_position,
      term_cell,
      r * c / 3 + 1
  );
  setvbuf(stdout, NULL, _IONBF, 0);
  globalLog = fopen("tui.log", "a");
}
[[gnu::destructor]] void term_cleanup(void) {
  mHmap_deinit(back_buffer);
}
__attribute__((hot)) void term_setCell_Ref(struct term_position pos, const struct term_cell *cell) {
  mHmap_set(back_buffer, pos, *cell);
}
__attribute__((hot)) void term_setCell(struct term_position pos, term_cell cell) {
  mHmap_set(back_buffer, pos, cell);
}
// will break if mp_cur_max ever changes
// appends wchar to place+len, then increments len
#include <time.h>
void L_addUint(List *l, u32 i) {
  static wchar look[10];
  u8 len = 0;
  for (len = 0; i; len++, i /= 10)
    look[9 - len] = L'0' + i % 10;
  if (len)
    List_appendFromArr(l, look + 10 - len, (usize)len);
  else
    List_append(l, L"0");
}
void L_fgcolor(List *printList, struct term_color fg) {
  typeof(fg) currentfg = fg;
  switch (currentfg.tag) {
    case term_color_idx: {
      List_resize(printList, printList->length + 18);
      List_appendFromArr(printList, L"\033[38;5;", 7);
      L_addUint(printList, currentfg.color.colorIDX);
      List_appendFromArr(printList, L"m", 1);
    } break;
    case term_color_full: {
      List_appendFromArr(printList, L"\033[38;2;", 7);
      L_addUint(printList, currentfg.color.r);
      List_append(printList, L";");
      L_addUint(printList, currentfg.color.g);
      List_append(printList, L";");
      L_addUint(printList, currentfg.color.b);
      List_append(printList, L"m");
    } break;
    default: {
      List_appendFromArr(printList, L"\033[39m", 5);
    } break;
  }
}
void L_bgcolor(List *printList, struct term_color bg) {
  typeof(bg) currentbg = bg;
  switch (currentbg.tag) {
    case term_color_idx: {
      List_appendFromArr(printList, L"\033[48;5;", 7);
      L_addUint(printList, currentbg.color.colorIDX);
      List_appendFromArr(printList, L"m", 1);
    } break;
    case term_color_full: {
      List_appendFromArr(printList, L"\033[48;2;", 7);
      L_addUint(printList, currentbg.color.r);
      List_append(printList, L";");
      L_addUint(printList, currentbg.color.g);
      List_append(printList, L";");
      L_addUint(printList, currentbg.color.b);
      List_append(printList, L"m");
    } break;
    default: {
      List_appendFromArr(printList, L"\033[49m", 5);
    } break;
  }
}
bool styleeq(term_cell a, term_cell b) {
  return (
      a.dim == b.dim &&
      a.bold == b.bold &&
      a.italic == b.italic &&
      a.underline == b.underline &&
      a.blinking == b.blinking &&
      a.inverse == b.inverse &&
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
void L_addPos(List *l, term_position p) {
  List_appendFromArr(l, L"\033[", 2);
  L_addUint(l, p.row + 1);
  List_append(l, L";");
  L_addUint(l, p.col + 1);
  List_append(l, L"H");
}
void term_render(void) {
  static struct term_position last = {0, 0};
  struct term_position recent = term_getTsize();

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
  mHmap_foreach(
      back_buffer,
      term_position, position,
      term_cell, cell, {
        term_color currentbg = cell.bg;
        term_color currentfg = cell.fg;

        if (
            cell.visible &&
            position.col < recent.col && position.row < recent.row &&
            position.row > -1 && position.col > -1
        ) {
          L_addPos(printList, position);
          if (cell.inverse) {
            term_color t = currentbg;
            currentbg = currentfg;
            currentfg = t;
          }
          if (!styleeq(lastCel, cell)) {
            L_addStyle(printList, cell, lastCel);
            lastCel = cell;
          }
          if (!coloreq(currentbg, lastbg) || !coloreq(currentfg, lastfg)) {
            L_fgcolor(printList, currentfg);
            L_bgcolor(printList, currentbg);
          }
          lastbg = currentbg;
          lastfg = currentfg;
          // TODO move char check to setcell
          wchar_t wc = cell.c && wwidth(cell.c) == 1 ? cell.c : L' ';
          List_append(printList, &wc);
        }
      }
  );
  convertwrite((wchar *)printList->head, printList->length);

  printList->length = 0;
  last = recent;
}
void term_dump() {
  justdumped = true;
  HMap_clear((HMap *)back_buffer);
}
#endif // MY_TUI_C
