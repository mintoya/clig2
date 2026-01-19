#include "term_input.h"
#include "term_screen.h"
#include "wheels/allocator.h"
#include <stdlib.h>

static struct golScreen {
  term_position size;
  u8 screens[];
} *golScreen = NULL;

void gol_setPoint(u32 row, u32 col, bool val) {
  if (row >= golScreen->size.row || col >= golScreen->size.col)
    return;

  u32 idx = row * golScreen->size.col + col;

  golScreen->screens[idx] &= ~1;
  if (val)
    golScreen->screens[idx] |= 1;
}
bool gol_getLastPoint(u32 row, u32 col) {
  if (row >= golScreen->size.row || col >= golScreen->size.col)
    return false;
  return (golScreen->screens[row * golScreen->size.col + col] & 2) && 1;
}
void gol_tick() {
  for (u32 i = 0; i < golScreen->size.col * golScreen->size.row; i++)
    golScreen->screens[i] <<= 1;
}

void draw_gol() {
  static bool hasrun = false;
  if (!golScreen) {
    term_position size = term_getTsize();
    golScreen = aAlloc(defaultAlloc, sizeof(*golScreen) + size.col * size.row);
    golScreen->size = size;
  }
  for (u32 i = 0; i < golScreen->size.row; i++) {
    u8 lastResult = 0;
    for (u32 j = 0; j * 2 < golScreen->size.col; j++) {
      u8 alive = 0;

      alive += gol_getLastPoint(i - 1, j - 1);
      alive += gol_getLastPoint(i - 1, j);
      alive += gol_getLastPoint(i - 1, j + 1);
      alive += gol_getLastPoint(i, j + 1);
      alive += gol_getLastPoint(i + 1, j - 1);
      alive += gol_getLastPoint(i + 1, j);
      alive += gol_getLastPoint(i + 1, j + 1);
      alive += gol_getLastPoint(i, j - 1);

      bool lastState = gol_getLastPoint(i, j);

      static bool stalive[9] = {0, 0, 1, 1, 0, 0, 0, 0, 0};
      static bool stdead[9] = {0, 0, 0, 1, 0, 0, 0, 0, 0};

      {
        if (hasrun)
          if (lastState) {
            gol_setPoint(i, j, stalive[alive]);
          } else {
            gol_setPoint(i, j, stdead[alive]);
          }
        else
          gol_setPoint(i, j, rand() % 100 < 10);
        if (lastState) {
          term_setCell_Ref(
              (term_position){i, j * 2},
              &(term_cell){
                  .fg = term_color_fromIdx(1),
                  .bg = term_color_fromIdx(200),
                  .c = L' ',
                  .visible = 1,
              }
          );
          term_setCell_Ref(
              (term_position){i, j * 2 + 1},
              &(term_cell){
                  .fg = term_color_fromIdx(1),
                  .bg = term_color_fromIdx(200),
                  .c = L' ',
                  .visible = 1,
              }
          );
        }
      }
    }
  }
  gol_tick();
  hasrun = true;
}
int main(void) {
  while (1) {
    // term_input ti = term_getInput(.01);
    draw_gol();
    term_render();
    term_dump();
  }
}
