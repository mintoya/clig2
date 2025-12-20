#ifndef MY_TERM_ELEMENTS_H
#define MY_TERM_ELEMENTS_H
#include "term_input.h"
#include "term_screen.h"
#include "wheels/fptr.h"
// object based element system
typedef struct term_element term_element;
typedef void (*render_term_element)(term_element *, const term_keyboard *);
typedef struct term_element {
  void *arb;
  render_term_element render;
  bool delete_flag : 1;
} term_element;

#include "wheels/hmap.h"
void term_renderElements(term_keyboard input);
term_element *add_element(term_element *el);

#endif // MY_TERM_ELEMENTS_H
#if (defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0)
#define MY_TERM_ELEMENTS_C (1)
#endif
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
  return res;
}
void term_renderElements(term_keyboard input) {
  term_dump();
  for (u32 i = 0; i < List_length(global_element_list); i++) {
    term_element **elem = (term_element **)List_getRef(global_element_list, i);
    assertMessage(*elem);
    if ((*elem)->delete_flag) {
      List_remove(global_element_list, i);
      i--;
      break;
    }
    ((*elem)->render(*elem, &input));
  }
  term_render();
}

// typedef struct term_element_nested {
//   enum { nested_parent,
//          root_parent } kind;
//   union {
//     term_element *rootParent;
//     struct term_element_nested *nestedParent;
//   } parent;
//   term_position position; //
//   term_position size;     //
//   usize childrenLength;
//   struct term_element_nested *children;
// } term_element_nested;

#endif
