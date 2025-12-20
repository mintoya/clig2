// #define LIST_NOCHECKS
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define MY_TERM_ELEMENTS_C
#include "term_elements.h"
#define MY_TERM_INPUT_C
#include "term_input.h"
#define MY_TUI_C
#include "term_screen.h"
#include "wheels/wheels.h"

typedef struct golScreen {
  term_position size;
  u8 screens[];
} golScreen;

void gol_setPoint(golScreen *s, u32 row, u32 col, bool val) {
  if (row >= s->size.row || col >= s->size.col)
    return;

  u32 idx = row * s->size.col + col;

  s->screens[idx] &= ~1;
  if (val)
    s->screens[idx] |= 1;
}

bool gol_getLastPoint(golScreen *s, u32 row, u32 col) {
  if (row >= s->size.row || col >= s->size.col)
    return false;
  return (s->screens[row * s->size.col + col] & 2) && 1;
}
void gol_tick(golScreen *s) {
  for (u32 i = 0; i < s->size.col * s->size.row; i++)
    s->screens[i] <<= 1;
}

void draw_gol(term_element *objptr, const term_keyboard *input) {
  static bool hasrun = false;
  golScreen *selfptr = (golScreen *)objptr->arb;
  for (u32 i = 0; i < selfptr->size.row; i++)
    for (u32 j = 0; j * 2 < selfptr->size.col; j++) {
      u8 alive = 0;

      alive += gol_getLastPoint(selfptr, i - 1, j - 1);
      alive += gol_getLastPoint(selfptr, i - 1, j);
      alive += gol_getLastPoint(selfptr, i - 1, j + 1);
      alive += gol_getLastPoint(selfptr, i, j + 1);
      alive += gol_getLastPoint(selfptr, i + 1, j - 1);
      alive += gol_getLastPoint(selfptr, i + 1, j);
      alive += gol_getLastPoint(selfptr, i + 1, j + 1);
      alive += gol_getLastPoint(selfptr, i, j - 1);

      bool lastState = gol_getLastPoint(selfptr, i, j);

      static bool stalive[9] = {0, 0, 1, 1, 0, 0, 0, 0, 0};
      static bool stdead[9] = {0, 0, 0, 1, 0, 0, 0, 0, 0};

      if (hasrun)
        if (lastState) {
          gol_setPoint(selfptr, i, j, stalive[alive]);
        } else {
          gol_setPoint(selfptr, i, j, stdead[alive]);
        }
      else
        gol_setPoint(selfptr, i, j, rand() % 100 < 10);

      term_setCell(
          (term_position){i, j * 2},
          (term_cell){
              .bg = term_color_fromIdx(0),
              .fg = term_color_fromIdx(255),
              .c = L' ',
              .visible = 1,
              .inverse = lastState,
          }
      );
      term_setCell(
          (term_position){i, j * 2 + 1},
          (term_cell){
              .bg = term_color_fromIdx(0),
              .fg = term_color_fromIdx(255),
              .c = L' ',
              .visible = 1,
              .inverse = lastState,
          }
      );
    }
  gol_tick(selfptr);
  hasrun = true;
}
term_element *gol(void) {
  term_position screenSize = get_terminal_size();
  term_element *res = aCreate(defaultAlloc, term_element);

  golScreen *ptr = aAlloc(
      defaultAlloc,
      sizeof(golScreen) +
          sizeof(ptr->screens[0]) * screenSize.col * screenSize.row
  );
  ptr->size = screenSize;

  *res = (term_element){
      .render = draw_gol,
      .arb = ptr,
  };
  return res;
}
int main(void) {
  add_element(gol());
  while (1) {
    term_renderElements(term_getInput(10));
  }
}
