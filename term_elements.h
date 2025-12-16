#include "term_input.h"
#include "term_screen.h"
#include "wheels/fptr.h"
typedef struct term_element term_element;
typedef struct {
  u32 len;
  struct term_cell *cells;
  struct term_position *positions;
} term_element_slice;
typedef term_element_slice (*render_term_element)(term_element *, const term_keyboard *);
typedef struct term_element {
  void *arb;
  render_term_element render;
} term_element;

#include "wheels/hmap.h"
static List *global_element_list = NULL;
void add_element(term_element *el) {
  List_append(global_element_list, &el);
}
[[gnu::constructor]] void elementInit() {
  global_element_list = List_new(defaultAlloc, sizeof(term_element *));
}
void term_renderElements(term_keyboard input) {
  term_dump();
  for (u32 i = 0; i < List_length(global_element_list); i++) {
    term_element **elem = (term_element **)List_getRef(global_element_list, i);
    term_element_slice cells = ((*elem)->render(*elem, &input));
    for (u32 i = 0; i < cells.len; i++)
      term_setCell(cells.positions[i], cells.cells[i]);
  }
  term_render();
}
// term_element_slice buttonRender(term_element *selfptr, const term_keyboard *inputState) {
//   return { 0, NULL, NULL };
// }
