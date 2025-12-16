#include <math.h>
#include <stdio.h>
#define LIST_NOCHECKS
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

struct buttonstate {
  term_position position;
  term_position size;
  term_cell inactive;
  term_cell onHover;
  term_cell onClick;
  usize bufferlen;
  u8 *buffer;
};
u32 udist(term_position a, term_position b) {
  i32 ax = a.row;
  i32 ay = a.col;
  i32 bx = b.row;
  i32 by = b.col;

  i32 x = ax - bx;
  i32 y = ay - by;
  x = x < 1 ? -x : x;
  y = y < 1 ? -y : y;

  return (u32)(x * x + y * y);
}
term_element_slice button_render(term_element *selfptr, const term_keyboard *inputstate) {
  struct buttonstate *selfPos = (struct buttonstate *)selfptr->arb;
  term_cell *respos = &selfPos->inactive;
  if (inputstate->kind == term_keyboard_mouse) {
    if (udist(selfPos->position, (term_position){inputstate->data.mouse.row, inputstate->data.mouse.col}) < 20) {
      if (inputstate->data.mouse.code.known == term_mouse_left && inputstate->data.mouse.state == term_mouse_down) {
        respos = &selfPos->onClick;
      } else {
        respos = &selfPos->onHover;
      }
    }
  }
  return (term_element_slice){
      .len = 1,
      .cells = respos,
      .positions = &selfPos->position,
  };
}
term_element *button(void) {
  term_element *res = (term_element *)aAlloc(defaultAlloc, sizeof(term_element));
  struct buttonstate *p = (struct buttonstate *)aAlloc(defaultAlloc, sizeof(struct buttonstate));
  p->position = (term_position){10, 10};
  p->inactive = (term_cell){
      L'B',
      term_color_fromHex(0xff0000),
      term_color_fromIdx(0),
      .visible = 1
  };

  p->onHover = p->inactive;
  p->onHover.inverse = true;

  p->onClick = (term_cell){
      L'C',
      term_color_fromIdx(160),
      term_color_fromIdx(15),
      .visible = 1
  };
  *res = (term_element){
      .render = button_render,
      .arb = p,
  };
  return res;
}

int main(int argc, char *argv[]) {
  add_element(button());
  while (1)
    term_renderElements(term_getInput(1));
}
