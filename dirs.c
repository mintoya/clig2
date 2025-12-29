#include "wheels/arenaAllocator.h"
#include "wheels/fptr.h"
#include "wheels/my-list.h"
#include "wheels/omap.h"
#include "wheels/print.h"
#include <time.h>
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

void dircleanup(DIR **d) {
  if (d && *d) {
    closedir(*d);
    *d = NULL;
  }
}
#define DIR_scoped [[gnu::cleanup(dircleanup)]] DIR

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

stringList *parsePath(char *ds, AllocatorV allocator) {
  stringList *res = stringList_new(allocator);
  while (*ds) {
    char *chr = ds;
    while (*chr && *chr != JOINER)
      chr++;
    stringList_append(res, fptr_fromPL(ds, chr - ds));
    ds = chr;
    ds += *ds ? 1 : 0;
  }

  return res;
}
void assemblePath(stringList *sl, List *clist) {
  assertMessage(clist && clist->width == sizeof(char));
  clist->length = 0;
  usize len = stringList_length(sl);
  for (usize i = 0; i < len; i++) {
    fptr d = stringList_get(sl, i);
    if (i)
      List_append(clist, REF(char, JOINER));
    List_appendFromArr(clist, d.ptr, d.width);
  }
}
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
void dirBox(term_position pos, term_position size, stringList *directory, fptr selected) {
  Arena_scoped *arenav = arena_new_ext(defaultAlloc, 1024);
  List *pathc = List_new(arenav, sizeof(char));
  assemblePath(directory, pathc);
  List_append(pathc, NULL);
  DIR_scoped *dir = opendir((char *)pathc->head);
  draw_box(
      pos, size,
      *term_makeCell(
          L' ',
          term_color_fromHex(0xffffff),
          (term_color){0},
          term_cell_VISIBLE
      ),
      L'╭', L'╮', L'╰', L'╯',
      L'─', L'│'
  );
  term_position place = (term_position){pos.row + 1, pos.col + 1};
  for (struct dirent *d = readdir(dir);
       d;
       d = readdir(dir)) {
    fptr name = fptr_fromCS(d->d_name);
    bool same = fptr_eq(selected, name);
    term_setLineM(
        place,
        *term_makeCell(
            L' ',
            term_color_fromHex(0xffffff),
            (term_color){0},
            term_cell_VISIBLE |
                (same ? term_cell_INVERSE : 0)
        ),
        (char *)name.ptr,
        name.width
    );
    place.row++;
  }
}
void drawSuper(stringList *path) {
  stringList_scoped *paths = stringList_remake(path);
  stringList_remove(paths, stringList_length(paths) - 1);

  term_position size = term_getTsize();
  size.col--;
  size.row--;
  size.col /= 3;
  dirBox((term_position){0}, size, paths, stringList_get(path, stringList_length(path) - 1));
}
void drawPath(stringList *path) {
  term_position size = term_getTsize();
  size.col--;
  size.row--;
  size.col /= 3;
  term_position place = (term_position){0, size.col + 1};
  dirBox(place, size, path, stringList_get(path, 0));
}
void drawSub(stringList *path, char *folderName) {
  stringList_scoped *paths = stringList_remake(path);
  stringList_append(paths, fptr_fromCS(folderName));
}
int main(int nargs, char **args) {
  Arena_scoped *local = arena_new_ext(defaultAlloc, 1024);

  char pwd[300];
  getcwd(pwd, 300);
  stringList_scoped *path = parsePath(pwd, local);

  while (1) {
    drawSuper(path);
    drawPath(path);
    term_render();
    term_dump();
    term_getInput(1);
  }
}
