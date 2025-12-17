#include <string.h>
#define LIST_NOCHECKS

#define MY_TERM_ELEMENTS_C
#include "term_elements.h"
#define MY_TERM_INPUT_C
#include "term_input.h"
#define MY_TUI_C
#include "term_screen.h"

#include "wheels/print.h"
#include "wheels/wheels.h"

#ifdef _WIN32
extern int nanosleep(const struct timespec *request, struct timespec *remain);
#endif
#include <time.h>

struct textBox_userData {
  term_cell defaultCell;
  term_position position;
  term_position size;
  u8 *text;
  List *cells;
  List *positions;
};
void draw_box_around(List *cells, List *positions, term_position topLeft, int width, int height, term_cell defaultCell, wchar_t tl, wchar_t tr, wchar_t bl, wchar_t br, wchar_t hor, wchar_t ver) {
  topLeft.col--;
  topLeft.row--;
  width += 2;
  height += 2;
  for (int col = 0; col < width; col++) {
    term_cell cell = defaultCell;
    term_position pos = topLeft;
    pos.col += col;

    if (col == 0)
      cell.c = tl;
    else if (col == width - 1)
      cell.c = tr;
    else
      cell.c = hor;

    List_append(cells, &cell);
    List_append(positions, &pos);
  }

  // Vertical borders
  for (int row = 1; row < height - 1; row++) {
    for (int col = 0; col < width; col++) {
      term_cell cell = defaultCell;
      term_position pos = topLeft;
      pos.row += row;
      pos.col += col;

      if (col == 0)
        cell.c = ver;
      else if (col == width - 1)
        cell.c = ver;
      else
        continue; // leave inner cells untouched

      List_append(cells, &cell);
      List_append(positions, &pos);
    }
  }

  // Bottom border
  for (int col = 0; col < width; col++) {
    term_cell cell = defaultCell;
    term_position pos = topLeft;
    pos.row += height - 1;
    pos.col += col;

    if (col == 0)
      cell.c = bl;
    else if (col == width - 1)
      cell.c = br;
    else
      cell.c = hor;

    List_append(cells, &cell);
    List_append(positions, &pos);
  }
}

term_element_slice textBox_render(term_element *elptr, const term_keyboard *inputstate) {
  struct textBox_userData *selfptr = (struct textBox_userData *)elptr->arb;

  selfptr->cells->length = 0;
  selfptr->positions->length = 0;

  for (int i = 0; i < selfptr->size.col && selfptr->text[i]; i++) {
    term_cell thiscell = selfptr->defaultCell;
    term_position thispos = selfptr->position;

    thiscell.c = (wchar)(selfptr->text[i]);
    thispos.col += i;

    List_append(selfptr->cells, &thiscell);
    List_append(selfptr->positions, &thispos);
  }
  draw_box_around(
      selfptr->cells,
      selfptr->positions,
      selfptr->position,
      selfptr->size.col,
      selfptr->size.row,
      selfptr->defaultCell,
      L'┌', L'┐', L'└', L'┘', L'─', L'│'
  );

  return ((term_element_slice){
      selfptr->cells->length,
      (term_cell *)selfptr->cells->head,
      (term_position *)selfptr->positions->head
  });
}
term_element_slice textBox_render_track(term_element *elptr, const term_keyboard *inputstate) {
  struct textBox_userData *selfptr = (struct textBox_userData *)elptr->arb;
  if (inputstate->kind == term_keyboard_mouse) {
    selfptr->position.col = inputstate->data.mouse.col;
    selfptr->position.row = inputstate->data.mouse.row;
    selfptr->defaultCell.inverse =
        (inputstate->data.mouse.code.known == term_mouse_left &&
         inputstate->data.mouse.state == term_mouse_down);
  }
  return textBox_render(elptr, inputstate);
}
term_element *textBox_newElement(u8 *text, u32 row, u32 col, term_color fg, term_color bg) {
  term_element *res = (term_element *)aAlloc(defaultAlloc, sizeof(term_element));
  struct textBox_userData *ud =
      (struct textBox_userData *)aAlloc(defaultAlloc, sizeof(struct textBox_userData));
  *ud = (struct textBox_userData){
      .defaultCell = (term_cell){
          L' ',
          fg, bg,
          0, 0, 0, 1, 0, 0, 1
      },
      .position = {row, col},
      .size = {1, strlen((char *)text)},
      .cells = mList(defaultAlloc, term_cell),
      .positions = mList(defaultAlloc, term_position),
      .text = text,
  };
  *res = (term_element){
      .arb = ud,
      .render = textBox_render,
  };
  return res;
}
term_element *textBox_newElement_track(u8 *text, u32 row, u32 col, term_color fg, term_color bg) {
  term_element *res = (term_element *)aAlloc(defaultAlloc, sizeof(term_element));
  struct textBox_userData *ud =
      (struct textBox_userData *)aAlloc(defaultAlloc, sizeof(struct textBox_userData));
  *ud = (struct textBox_userData){
      .defaultCell = (term_cell){
          L' ',
          fg, bg,
          0, 0, 0, 1, 0, 0, 1
      },
      .position = {row, col},
      .size = {1, strlen((char *)text)},
      .cells = mList(defaultAlloc, term_cell),
      .positions = mList(defaultAlloc, term_position),
      .text = text,
  };
  *res = (term_element){
      .arb = ud,
      .render = textBox_render_track,
  };
  return res;
}

int main(int argc, char *argv[]) {
  add_element(
      textBox_newElement(
          (u8 *)"hello world test",
          1, 1, term_color_fromIdx(10),
          term_color_fromIdx(15)
      )
  );
  add_element(
      textBox_newElement(
          (u8 *)"hello world",
          10, 12, term_color_fromHex(0xffff77),
          term_color_fromHex(0xff00ff)
      )
  );
  add_element(
      textBox_newElement_track(
          (u8 *)"sticky",
          11, 30, term_color_fromHex(0x6767ff),
          term_color_fromHex(0x00ffff)
      )
  );
  while (1) {
    term_renderElements(term_getInput(1));
    nanosleep(&(struct timespec){0, 10000000}, NULL);
  }
}
