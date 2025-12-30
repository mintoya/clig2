#include "wheels/fptr.h"
#include "wheels/stringList.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#define MY_TUI_C
#include "term_screen.h"
#define MY_TERM_INPUT_C
#include "term_input.h"
#if defined(_WIN32) || defined(_WIN64)
  #define JOINER '\\'
  #include "wheels/dirent/dirent.h"
  #include <direct.h>
#else
  #define JOINER '/'
  #include <dirent.h>
#endif
#include "wheels/wheels.h"

REGISTER_SPECIAL_PRINTER("dirent", struct dirent *, {
  PUTS(L"{");
  USETYPEPRINTER(fptr, fptr_fromCS(in->d_name));
  PUTS(L":");
  switch (in->d_type) {
    case DT_DIR: {
      PUTS(L"directory");
    } break;
    case DT_REG: {
      PUTS(L"file");
    } break;
    default: {
      PUTS(L"other");
    } break;
  }
  PUTS(L"}");
});
void draw_box(
    term_position position,
    term_position size,
    term_cell style,
    wchar topLeft,
    wchar topRight,
    wchar bottomLeft,
    wchar bottomRight,
    wchar horizontal,
    wchar vertical
) {
  usize top = position.row;
  usize bottom = position.row + size.row;
  usize left = position.col;
  usize right = position.col + size.col;
  style.c = topLeft;
  term_setCell((term_position){top, left}, style);
  style.c = topRight;
  term_setCell((term_position){top, right}, style);
  style.c = bottomLeft;
  term_setCell((term_position){bottom, left}, style);
  style.c = bottomRight;
  term_setCell((term_position){bottom, right}, style);

  style.c = horizontal;
  for (usize col = left + 1; col < right; col++) {
    term_setCell((term_position){top, col}, style);
    term_setCell((term_position){bottom, col}, style);
  }

  style.c = vertical;
  for (usize row = top + 1; row < bottom; row++) {
    term_setCell((term_position){row, left}, style);
    term_setCell((term_position){row, right}, style);
  }
}
// static return
char *assembleDir(stringList *path) {
  static List *l = NULL;
  if (!l)
    l = List_new(defaultAlloc, sizeof(char));
  l->length = 0;
  u32 len = stringList_length(path);
  for (u32 i = 0; i < len; i++) {
    fptr it = stringList_get(path, i);
    List_appendFromArr(l, it.ptr, it.width);
    // if (i + 1 != len)
    List_append(l, REF(char, JOINER));
  }
  return (char *)l->head;
}
struct dirent *drawMainBox(DIR *directory, i8 direction) {
  static DIR *last = NULL;

  static List *dirList = NULL;
  if (!dirList)
    dirList = List_new(defaultAlloc, sizeof(struct dirent));

  static i32 selected = 0;
  if (last != directory) {
    selected = 0;
    dirList->length = 0;

    struct dirent *d;
    while ((d = readdir(directory))) {
      List_append(dirList, d);
    }

    rewinddir(directory);
  }
  selected += direction;
  if (dirList->length) {
    if (selected > (i64)List_length(dirList)) {
      selected = 0;
    } else if (selected < 0) {
      selected = List_length(dirList) - 1;
    }
  }

  term_position size = term_getTsize();
  size.col /= 3;
  size.col--;
  size.row--;
  term_position position = (term_position){0, size.col};
  term_cell style =
      *term_makeCell(
          L' ',
          term_color_fromHex(0xffffff),
          (term_color){},
          term_cell_VISIBLE
      );
  term_cell selectedStyle = style;
  selectedStyle.bg = term_color_fromHex(0xffffff);
  selectedStyle.fg = term_color_fromHex(0x1);
  draw_box(
      position, size, style,
      L'╭', L'╮',
      L'╰', L'╯',
      L'─', L'│'
  );

  term_position pos = (term_position){position.row + 1, position.col + 1};
  for (u32 i = 0; i < List_length(dirList); i++) {
    wchar w[sizeof(((struct dirent *)NULL)->d_name) / sizeof(char)];
    struct dirent *this = List_getRef(dirList, i);
    mbstowcs(w, this->d_name, sizeof((struct dirent *)NULL)->d_name);
    term_setLine(
        pos, i == selected ? selectedStyle : style,
        w, wcslen(w), size.col - 1
    );
    pos.row++;
  }

  last = directory;
  return List_getRef(dirList, selected);
}
void drawSubBox(DIR *directory, struct dirent *name, char *supername) {
  if (!name)
    return;
  term_position size = term_getTsize();
  size.col /= 3;
  size.col--;
  size.row--;
  term_position position = (term_position){0, size.col * 2 + 1};
  term_cell style =
      *term_makeCell(
          L' ',
          term_color_fromHex(0xffffff),
          (term_color){},
          term_cell_VISIBLE
      );
  draw_box(
      position, size, style,
      L'╭', L'╮',
      L'╰', L'╯',
      L'─', L'│'
  );
  term_position pos = (term_position){position.row + 1, position.col + 1};
  print_wfO(fileprint, globalLog, "{dirent}\n", name);
  if (name->d_type == DT_DIR) {
    usize superlen = strlen(supername);
    usize sublen = strlen(name->d_name);
    char sname[superlen + sublen + 1] = {};
    memcpy(sname, supername, superlen);
    memcpy(sname + superlen, name->d_name, sublen);
    print_wfO(fileprint, globalLog, "opening {cstr}\n", (char *)sname);
    DIR *sub = opendir(sname);
    struct dirent *d = NULL;
    while ((d = readdir(sub))) {
      wchar w[sizeof(((struct dirent *)NULL)->d_name) / sizeof(char)];
      mbstowcs(w, d->d_name, sizeof((struct dirent *)NULL)->d_name);
      term_setLine(
          pos, style,
          w, wcslen(w), size.col - 1
      );
      pos.row++;
    }
    closedir(sub);
  }
}
void drawSuperBox(DIR *directory) {
  static DIR *last = NULL;

  static List *dirList = NULL;
  if (!dirList)
    dirList = List_new(defaultAlloc, sizeof(struct dirent));
  if (last != directory) {
    dirList->length = 0;

    struct dirent *d;
    while ((d = readdir(directory))) {
      List_append(dirList, d);
    }

    rewinddir(directory);
  }

  term_position size = term_getTsize();
  size.col /= 3;
  size.col-=2;
  size.row--;
  term_position position = (term_position){0, 0};
  term_cell style =
      *term_makeCell(
          L' ',
          term_color_fromHex(0xffffff),
          (term_color){},
          term_cell_VISIBLE
      );
  term_cell selectedStyle = style;
  selectedStyle.bg = term_color_fromHex(0xffffff);
  selectedStyle.fg = term_color_fromHex(0x1);
  draw_box(
      position, size, style,
      L'╭', L'╮',
      L'╰', L'╯',
      L'─', L'│'
  );

  term_position pos = (term_position){position.row + 1, position.col + 1};
  for (u32 i = 0; i < List_length(dirList); i++) {
    wchar w[sizeof(((struct dirent *)NULL)->d_name) / sizeof(char)];
    struct dirent *this = List_getRef(dirList, i);
    mbstowcs(w, this->d_name, sizeof((struct dirent *)NULL)->d_name);
    term_setLine(
        pos, style,
        w, wcslen(w), size.col - 1
    );
    pos.row++;
  }

  last = directory;
}
int main(void) {
  char pwd[100];
  assertMessage(getcwd(pwd, sizeof(pwd)), "pwd buffer too small probalby");

  stringList *splitPath = stringList_new(defaultAlloc);
  for (usize i = 0, j = 0; pwd[i]; i++) {
    if (!pwd[i] || pwd[i] == JOINER) {
      usize len = i - j;
      len = len ? len - 1 : len;
      stringList_append(splitPath, fptr_fromPL(pwd + j + 1, len));
      j = i;
    }
    if (!pwd[i + 1]) {
      usize len = i - j;
      stringList_append(splitPath, fptr_fromPL(pwd + j + 1, len));
    }
  }

  splitPath->List_stringMetaData.length--;
  DIR *super = opendir(assembleDir(splitPath));
  splitPath->List_stringMetaData.length++;
  char *direname = assembleDir(splitPath);
  DIR *main = opendir(direname);
  term_input in = {0};
  while (1) {
    assertMessage(main);
    i8 direction = 0;
    switch (in.kind) {
      case term_input_arrow: {
        switch (in.data.arrow) {
          case term_input_down:
            direction = 1;
            break;
          case term_input_up:
            direction = -1;
            break;
          default:;
        }
      } break;
      default:;
    }
    struct dirent *file = drawMainBox(main, direction);
    drawSubBox(main, file, direname);
    drawSuperBox(super);
    term_render();
    in = term_getInput(10);
    term_dump();
  }
  return 0;
}
