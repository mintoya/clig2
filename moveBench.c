
#include "term_input.h"
#include "term_screen.h"
#include <stdint.h>

u8 udist(term_position a, term_position b) {
  i32 ac = a.col / 2;
  i32 bc = b.col / 2;
  i32 ar = a.row;
  i32 br = b.row;

  i32 c = ac - bc;
  i32 r = ar - br;
  return 255 - ((u8)((c * c + r * r) >> 5));
}
void render_circles(const term_input *input) {
  static term_position center = {0};
  bool click = false;
  if (input->kind == term_input_mouse) {
    center = (term_position){
        input->data.mouse.row,
        input->data.mouse.col,
    };
    if (input->data.mouse.code.known == term_mouse_left && input->data.mouse.state == term_mouse_down) {
      click = true;
    }
  }
  term_position termsize = term_getTsize();
  for (u32 i = 1; i < termsize.row; i++) {
    for (u32 j = 1; j < termsize.col; j++) {
      u8 dist = udist((term_position){i, j}, center);
      dist *= click ? -1 : 1;
      term_setCell_Ref(
          (term_position){i, j},
          term_makeCell(
              L'x', term_color_fromIdx(255-dist), term_color_fromIdx(dist),
              term_cell_VISIBLE
          )
      );
    }
  }
}
int main(void) {
  while (1) {
    const term_input ti = term_getInput(1);
    render_circles(&ti);
    term_render();
  }
}
