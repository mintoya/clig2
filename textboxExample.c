#include "wheels/fptr.h"
#include <wchar.h>
#define LIST_NOCHECKS

#define MY_TERM_ELEMENTS_C
#include "term_elements.h"
#define MY_TERM_INPUT_C
#include "term_input.h"
#define MY_TUI_C
#include "term_screen.h"

#include "wheels/print.h"
#include "wheels/wheels.h"

// doesnt own allocator, keep track
typedef struct LinkedList {
  wchar *characters;
  u32 length;
  struct LinkedList *prev;
  struct LinkedList *next;
} LinkedList;
LinkedList *LinkedList_insertNewElement(const My_allocator *allocator, LinkedList *next, LinkedList *prev) {
  LinkedList *res = (LinkedList *)aAlloc(allocator, sizeof(LinkedList));
  *res = (LinkedList){
      .next = next,
      .prev = prev,
  };
  if (next)
    next->prev = res;
  if (prev)
    prev->next = res;
  return res;
}
void LinkedList_removeElement(const My_allocator *allocator, LinkedList *el) {
  el->next->prev = el->prev;
  el->prev->next = el->next;
  aFree(allocator, el);
}
wchar LinkedList_getFromInnerList(LinkedList *el, u32 idx) {
  if (!el)
    return 0;
  if (idx > el->length)
    return 0;
  return el->characters[idx];
}
struct textBox {
  term_cell defaultCell;

  term_position position;
  term_position size;

  LinkedList *currentLinePlace;
  i32 currentCol;
  i32 currentLine;
};
void draw_box_around(term_position topLeft, int width, int height, term_cell defaultCell, wchar_t tl, wchar_t tr, wchar_t bl, wchar_t br, wchar_t hor, wchar_t ver) {
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

    term_setCell(pos, cell);
  }

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
        continue;

      term_setCell(pos, cell);
    }
  }

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

    term_setCell(pos, cell);
  }
}

bool within(term_position position, term_position size, term_position point) {
  if (!size.row)
    return false;
  if (!size.col)
    return false;
  if (point.row < position.row)
    return false;
  if (point.row > position.row + size.row - 1)
    return false;
  if (point.col < position.col)
    return false;
  if (point.col > position.col + size.col - 1)
    return false;
  return true;
}
void textbox_renderFn(term_element *selfElementPtr, const term_keyboard *lastInput) {
  struct textBox *selfptr = (struct textBox *)selfElementPtr->arb;
  if (lastInput->kind == term_keyboard_arrow) {
    switch (lastInput->data.arrow) {
      case term_keyboard_right:
        selfptr->currentCol += 2;
        break;
      case term_keyboard_left:
        selfptr->currentCol -= 2;
        break;
      case term_keyboard_down:
        selfptr->currentLine -= 2;
        break;
      case term_keyboard_up:
        selfptr->currentLine += 2;
        break;
    }
  }
  draw_box_around(
      selfptr->position,
      selfptr->size.col,
      selfptr->size.row,
      selfptr->defaultCell,
      L'╭', L'╮', L'╰', L'╯', L'─', L'│'
  );

  LinkedList *start = selfptr->currentLinePlace;
  if ((i32)selfptr->currentLine >= 0 && (i32)selfptr->currentLine < (i32)selfptr->size.row) {
    for (u32 j = 0; j < selfptr->size.col; j++) {
      term_cell l = selfptr->defaultCell;
      i32 diff = (i32)j + (i32)selfptr->currentCol;
      l.c = start ? LinkedList_getFromInnerList(start, diff) : l.c;
      term_position pdiff = selfptr->position;
      pdiff.row += selfptr->currentLine;
      pdiff.col += j;
      term_setCell(pdiff, l);
    }
  }
  {
    LinkedList *place = start ? start->prev : NULL;
    for (i32 i = (i32)selfptr->currentLine - 1; i >= 0; i--) {
      if (i < (i32)selfptr->size.row) {
        for (u32 j = 0; j < selfptr->size.col; j++) {
          term_cell l = selfptr->defaultCell;
          i32 diff = (i32)j + (i32)selfptr->currentCol;
          l.c = place ? LinkedList_getFromInnerList(place, diff) : l.c;
          term_position pdiff = selfptr->position;
          pdiff.row += i;
          pdiff.col += j;
          term_setCell(pdiff, l);
        }
      }
      place = place ? place->prev : NULL;
    }
  }
  {
    LinkedList *place = start ? start->next : NULL;
    for (i32 i = (i32)selfptr->currentLine + 1; i < (i32)selfptr->size.row; i++) {
      if (i >= 0) {
        for (u32 j = 0; j < selfptr->size.col; j++) {
          term_cell l = selfptr->defaultCell;
          i32 diff = (i32)j + (i32)selfptr->currentCol;
          l.c = place ? LinkedList_getFromInnerList(place, diff) : l.c;
          term_position pdiff = selfptr->position;
          pdiff.row += i;
          pdiff.col += j;
          term_setCell(pdiff, l);
        }
      }
      place = place ? place->next : NULL;
    }
  }
}
LinkedList *fromParagraph(const wchar_t *paragraph, const My_allocator *alloc) {
  if (paragraph == NULL || wcslen(paragraph) == 0) {
    return NULL;
  }
  LinkedList *head = NULL;
  LinkedList *current = NULL;

  size_t len = wcslen(paragraph);
  size_t lineStart = 0;

  for (size_t i = 0; i <= len; i++) {
    if (i == len || paragraph[i] == L'\n') {
      size_t lineLen = i - lineStart;

      if (lineLen == 0) {
        lineStart = i + 1;
        continue;
      }

      if (head == NULL) {
        head = LinkedList_insertNewElement(alloc, NULL, NULL);
        current = head;
      } else {
        current = LinkedList_insertNewElement(alloc, NULL, current);
      }

      current->characters = (wchar_t *)&paragraph[lineStart];
      current->length = lineLen - 1;

      lineStart = i + 1;
    }
  }

  return head->next;
}
LinkedList *testText(void) {
  return fromParagraph(
      // clang-format off
L"hello\n"
 "  world\n\n"
 "how do you do raw string \n"
 "  literals for wchars?"
      // clang-format on
      ,
      defaultAlloc
  );
}
term_element *textBox_newElement(const My_allocator *allocator) {
  term_element *selfptr = (term_element *)aAlloc(allocator, sizeof(term_element));
  struct textBox *userData = (struct textBox *)aAlloc(allocator, sizeof(struct textBox));

  *userData = (struct textBox){
      .defaultCell = (term_cell){
          0,
          term_color_fromIdx(0), term_color_fromIdx(255),
          0, 0, 0, 0, 0, 0, 1
      },
      .currentLinePlace = testText(),
      .position = {5, 5},
      .currentLine = 0,
      .currentCol = 0,
      .size = {20, 40},
  };
  *selfptr = (term_element){
      .arb = userData,
      .render = textbox_renderFn,
  };
  return selfptr;
}
int main() {
  add_element(textBox_newElement(defaultAlloc));
  while (1)
    term_renderElements(term_getInput(10));
}
