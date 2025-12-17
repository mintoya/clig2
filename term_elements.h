#ifndef MY_TERM_ELEMENTS_H
#define MY_TERM_ELEMENTS_H
#include "term_input.h"
#include "term_screen.h"
// object based element system
typedef struct term_element term_element;
typedef void (*render_term_element)(term_element *, const term_keyboard *);
typedef struct term_element {
  void *arb;
  render_term_element render;
  bool delete_flag;
} term_element;

#include "wheels/hmap.h"
void term_renderElements(term_keyboard input);
term_element *add_element(term_element *el);

#endif // MY_TERM_ELEMENTS_H
#ifdef MY_TERM_ELEMENTS_C
static List *global_element_list = NULL;
[[gnu::constructor]] void elementInit() {
  global_element_list = List_new(defaultAlloc, sizeof(term_element *));
}
[[gnu::destructor]] void elementdeInit() {
  List_free(global_element_list);
}
term_element *add_element(term_element *el) {
  List_append(global_element_list, &el);
  term_element *res = *(term_element **)List_getRef(global_element_list, global_element_list->length - 1);
  assertMessage(res == el);
  return res;
}
void term_renderElements(term_keyboard input) {
  term_dump();
  for (u32 i = 0; i < List_length(global_element_list); i++) {
    term_element **elem = (term_element **)List_getRef(global_element_list, i);
    if ((*elem)->delete_flag) {
      List_remove(global_element_list, i);
      i--;
      break;
    }
    ((*elem)->render(*elem, &input));
  }
  term_render();
}
#endif
