// #include "term_input.h"
#include "term_screen.h"
#include "wheels/my-list.h"
#include "wheels/print.h"
#ifdef _WIN32
  #include "wheels/dirent/dirent.h"
#else
  #include <dirent.h>
#endif
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static pEsc green = ((pEsc){.fg = {50, 150, 50}, .fgset = 1});
static pEsc yello = ((pEsc){.fg = {200, 200, 0}, .fgset = 1});
static pEsc reset = ((pEsc){.reset = 1});

typedef struct {
  char *line;
  usize delta; ///< contained newlines
               ///< starts at 0
  usize len;
} line;
struct {
  char *filename;
  FILE *file;
  HHMap *known; ///<  Map<usize,line*>
                ///<   maps rows to edited lines
  usize row;
  usize col;
} editorData = {};
void beginEdit(
    char *filename
) {
  term_position ts = term_getTsize();
  editorData.filename = filename;
#ifdef _WIN32
  editorData.file = fopen(filename, "rt+, ccs=UTF-8");
#else
  editorData.file = fopen(filename, "rt+");
#endif
  // flockfile(editorData.file);
  // _lock_file(editorData.file);
  editorData.known = HHMap_new(sizeof(usize), sizeof(line), defaultAlloc, 1000);
  usize nCurrentLine = 0;
  List currentLine;
  List_makeNew(defaultAlloc, &currentLine, sizeof(wchar), 1);
  for (
      wint_t c = fgetwc(editorData.file);
      c != EOF && nCurrentLine < ts.row;
      c = fgetwc(editorData.file)
  ) {
    switch ((wchar)c) {
      case L'\r':
      case L'\n': {
        wchar next = fgetwc(editorData.file);
        if ((c == L'\r' && next != L'\n') || (c == L'\n' && next != L'\r')) {
          ungetwc(next, editorData.file);
        }
        line l = (line){(char *)currentLine.head, 0, currentLine.length};
        HHMap_set(editorData.known, &nCurrentLine, &l);
        nCurrentLine++;
        List_makeNew(defaultAlloc, &currentLine, sizeof(wchar), 1);
      } break;
      default:
        List_append(&currentLine, &(wchar){c});
    }
  }
  line l = (line){(char *)currentLine.head, 0, currentLine.length};
  HHMap_set(editorData.known, &nCurrentLine, &l);

  usize n = 0;
  line *ln = HHMap_get(editorData.known, REF(usize, 0));

  while (ln) {
    println("{}{}{}{fptr<wchar>}", yello, n, reset, ((fptr){ln->len * sizeof(wchar), (u8 *)ln->line}));
    n++;
    ln = HHMap_get(editorData.known, REF(usize, n));
  }
}
void draw_editor() {}

int main(int narg, char **arg) {
  char *pwd = getenv("PWD");
  if (narg < 2) {
    println(
        "{}supply file argument{}",
        ((pEsc){
            .fg.r = 255,
            .fgset = 1,
        }),
        ((pEsc){
            .reset = 1
        })
    );
    return 1;
  }
  DIR *Dir = opendir(pwd);
  struct dirent *d = readdir(Dir);
  while (d && strcmp(d->d_name, arg[1])) {
    d = readdir(Dir);
    println("{cstr}", (char *)d->d_name);
  }
  if (!d) {
    println("couldnt find file");
    return 1;
  }
  if (d->d_type != DT_REG) {
    println("unsupported file type");
    return 1;
  }
  beginEdit(d->d_name);
  return 0;
}
