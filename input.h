#ifndef MY_TERM_INPUT_H
#define MY_TERM_INPUT_H
#include "wheels/fptr.h"
#include "wheels/print.h"

typedef struct {
  u32 row, col;
  union {
    enum : u8 {
      term_mouse_left = 0,
      term_mouse_middle = 1,
      term_mouse_right = 2,
      term_mouse_move = 3,
    } known;
    u8 unknown;
  } code;
  enum : u8 {
    term_mouse_up = 0,
    term_mouse_down = 1,
  } state;
  bool term_mouse_ctrl : 1;
  bool term_mouse_drag : 1;
  bool term_mouse_alt : 1;
} term_mouse;

typedef struct {
  enum : u8 {
    term_keyboard_raw,
    term_keyboard_ctrl,
    term_keyboard_alt,
    term_keyboard_arrow,
    term_keyboard_function,
    term_keyboard_mouse,
    term_keyboard_none, // NEW: No input available
    term_keyboard_unknown,
  } kind;
  union {
    term_mouse mouse;
    u8 raw;
    struct {
      u8 key;
      bool ctrl;
      bool shift;
      bool alt;
    } modified;
    enum : u8 {
      term_keyboard_up = 'A',
      term_keyboard_down = 'B',
      term_keyboard_right = 'C',
      term_keyboard_left = 'D',
      term_keyboard_home = 'H',
      term_keyboard_end = 'F',
    } arrow;
    u8 function_key; // F1-F12
  } data;
} term_keyboard;

term_keyboard term_getInput(float timeout_seconds);
#endif
#ifdef MY_TERM_INPUT_C

#if defined(__linux__)
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

static struct termios old;

[[gnu::constructor]] static void raw_on(void) {
  struct termios t;
  tcgetattr(STDIN_FILENO, &old);
  t = old;
  t.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &t);
  write(STDOUT_FILENO, "\033[?1003h\033[?1015h\033[?1006h", 24);
}

[[gnu::destructor]] static void raw_off(void) {
  write(STDOUT_FILENO, "\033[?1003l\033[?1015l\033[?1006l", 24);
  tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

term_mouse mouse(u8 *buf, u8 len) {
  int i = 0;
  for (; buf[i] != ';'; i++)
    ;
  uint codeint = fptr_toUint(((fptr){i, buf}));
  buf += i + 1;
  len -= i + 1;
  i = 0;
  for (; buf[i] != ';'; i++)
    ;
  uint col = fptr_toUint(((fptr){i, buf}));
  buf += i + 1;
  len -= i + 1;
  i = 0;
  for (; buf[i] != 'm' && buf[i] != 'M'; i++)
    ;
  uint row = fptr_toUint(((fptr){i, buf}));
  bool drag = codeint & 32;
  bool ctrl = codeint & 16;
  bool alt = codeint & 8;
  codeint &= ~32;
  codeint &= ~16;
  codeint &= ~8;
  return (term_mouse){
      .row = row,
      .col = col,
      .term_mouse_drag = drag,
      .term_mouse_ctrl = ctrl,
      .term_mouse_alt = alt,
      .code.unknown = codeint,
      .state = buf[i] != 'm',
  };
}

term_keyboard term_getInput(float timeout_seconds) {
  term_keyboard res = {0};

  // Use select() to wait for input with timeout
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = (long)timeout_seconds;
  timeout.tv_usec = (long)((timeout_seconds - (long)timeout_seconds) * 1000000);

  int select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

  if (select_result == 0) {
    // Timeout - no input available
    res.kind = term_keyboard_none;
    return res;
  }

  if (select_result < 0) {
    // Error
    res.kind = term_keyboard_unknown;
    return res;
  }

  // Input is available, read it
  uchar buf[32];
  usize n = read(STDIN_FILENO, buf, sizeof(buf));

  if (n == 0) {
    res.kind = term_keyboard_unknown;
    return res;
  }

  // Handle escape sequences
  if (buf[0] == 27) {
    if (n == 1) {
      // Just ESC key pressed
      res.kind = term_keyboard_raw;
      res.data.raw = 27;
    } else if (n >= 2 && buf[1] == '[') {
      // CSI sequence: ESC [
      if (n >= 3 && buf[2] == '<') {
        // Mouse input: ESC [
        res.kind = term_keyboard_mouse;
        res.data.mouse = mouse(buf + 3, n - 3);
      } else if (n == 3 && buf[2] >= 'A' && buf[2] <= 'H') {
        // Simple arrow keys or Home/End: ESC [ A/B/C/D/H/F
        res.kind = term_keyboard_arrow;
        res.data.arrow = buf[2];
      } else if (n >= 6 && buf[2] == '1' && buf[3] == ';') {
        // Modified keys: ESC [ 1 ; modifier final
        u8 modifier = buf[4] - '1'; // 0-based
        u8 final = buf[5];

        res.kind = term_keyboard_arrow;
        res.data.modified.key = final;
        res.data.modified.shift = (modifier & 1) != 0;
        res.data.modified.alt = (modifier & 2) != 0;
        res.data.modified.ctrl = (modifier & 4) != 0;
      } else if (n >= 4 && buf[n - 1] == '~') {
        // Function keys and special: ESC [ num ~
        u8 num = 0;
        for (int i = 2; i < n - 1; i++) {
          if (buf[i] >= '0' && buf[i] <= '9') {
            num = num * 10 + (buf[i] - '0');
          } else if (buf[i] == ';') {
            break; // modifier follows
          }
        }
        res.kind = term_keyboard_function;
        res.data.function_key = num;
      } else {
        // Unknown CSI sequence
        res.kind = term_keyboard_unknown;
      }
    } else if (n == 3 && buf[1] == 'O') {
      // SS3 sequence: ESC O (alternate function/arrow keys)
      if (buf[2] >= 'P' && buf[2] <= 'S') {
        // F1-F4
        res.kind = term_keyboard_function;
        res.data.function_key = buf[2] - 'P' + 1;
      } else if (buf[2] >= 'A' && buf[2] <= 'D') {
        // Arrow keys in application mode
        res.kind = term_keyboard_arrow;
        res.data.arrow = buf[2];
      } else {
        res.kind = term_keyboard_unknown;
      }
    } else if (n == 2) {
      // Alt + key: ESC key
      res.kind = term_keyboard_alt;
      res.data.modified.alt = true;
      res.data.modified.key = buf[1] & ~32; // uppercase
    } else {
      // Unknown escape sequence
      res.kind = term_keyboard_unknown;
    }
  } else if (buf[0] < 32 && buf[0] != '\n' && buf[0] != '\r' &&
             buf[0] != '\t') {
    // Control character (Ctrl+A through Ctrl+Z = 1-26)
    res.kind = term_keyboard_ctrl;
    res.data.modified.ctrl = true;
    res.data.modified.key =
        buf[0] + '@'; // Convert to letter (1='A', 2='B', etc)
  } else {
    // Regular printable key
    res.kind = term_keyboard_raw;
    res.data.raw = buf[0];
  }

  return res;
}
#endif

#if defined(_WIN32)
#include <windows.h>

static DWORD old;

[[gnu::constructor]] static void raw_on(void) {
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(hInput, &old);

  DWORD mode = old;
  mode |= ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
  mode &= ~ENABLE_QUICK_EDIT_MODE;

  SetConsoleMode(hInput, mode);
}

[[gnu::destructor]] static void raw_off(void) {
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(hInput, old);
}

term_keyboard term_getInput(float timeout_seconds) {
  term_keyboard res = {0};
  HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);

  // Convert timeout to milliseconds
  DWORD timeout_ms = (DWORD)(timeout_seconds * 1000);

  // Wait for input with timeout
  DWORD wait_result = WaitForSingleObject(hInput, timeout_ms);

  if (wait_result == WAIT_TIMEOUT) {
    // Timeout - no input available
    res.kind = term_keyboard_none;
    return res;
  }

  if (wait_result != WAIT_OBJECT_0) {
    // Error
    res.kind = term_keyboard_unknown;
    return res;
  }

  // Input is available, read it
  INPUT_RECORD ir;
  DWORD read_count;

  if (!ReadConsoleInput(hInput, &ir, 1, &read_count) || read_count == 0) {
    res.kind = term_keyboard_unknown;
    return res;
  }

  if (ir.EventType == MOUSE_EVENT) {
    MOUSE_EVENT_RECORD mer = ir.Event.MouseEvent;
    res.kind = term_keyboard_mouse;
    res.data.mouse = (term_mouse){
        .row = mer.dwMousePosition.Y + 1,
        .col = mer.dwMousePosition.X + 1,
        .term_mouse_ctrl = (mer.dwControlKeyState &
                            (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0,
        .term_mouse_alt = (mer.dwControlKeyState &
                           (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0,
        .term_mouse_drag = false,
        .state = term_mouse_up,
    };

    switch (mer.dwEventFlags) {
    case 0: // Button press/release
      if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
        res.data.mouse.code.known = term_mouse_left;
        res.data.mouse.state = term_mouse_down;
      } else if (mer.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
        res.data.mouse.code.known = term_mouse_right;
        res.data.mouse.state = term_mouse_down;
      } else if (mer.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) {
        res.data.mouse.code.known = term_mouse_middle;
        res.data.mouse.state = term_mouse_down;
      } else {
        res.data.mouse.code.known = term_mouse_left;
        res.data.mouse.state = term_mouse_up;
      }
      break;
    case MOUSE_MOVED:
      res.data.mouse.code.known = term_mouse_move;
      res.data.mouse.term_mouse_drag = (mer.dwButtonState != 0);
      break;
    case DOUBLE_CLICK:
      res.data.mouse.code.known = term_mouse_left;
      res.data.mouse.state = term_mouse_down;
      break;
    case MOUSE_WHEELED:
    case MOUSE_HWHEELED:
      res.data.mouse.code.unknown =
          64 + (HIWORD(mer.dwButtonState) > 0 ? 0 : 1);
      break;
    }
    return res;
  }

  if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
    // Not a key press event, recurse with remaining timeout
    // (This could be improved to track elapsed time)
    return term_getInput(timeout_seconds);
  }

  KEY_EVENT_RECORD ker = ir.Event.KeyEvent;
  DWORD ctrl_state = ker.dwControlKeyState;
  bool ctrl = (ctrl_state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
  bool alt = (ctrl_state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
  bool shift = (ctrl_state & SHIFT_PRESSED) != 0;

  // Handle special keys
  if (ker.wVirtualKeyCode >= VK_F1 && ker.wVirtualKeyCode <= VK_F12) {
    res.kind = term_keyboard_function;
    res.data.function_key = ker.wVirtualKeyCode - VK_F1 + 1;
    return res;
  }

  if (ker.wVirtualKeyCode >= VK_LEFT && ker.wVirtualKeyCode <= VK_DOWN) {
    res.kind = term_keyboard_arrow;
    res.data.modified.ctrl = ctrl;
    res.data.modified.shift = shift;
    res.data.modified.alt = alt;

    switch (ker.wVirtualKeyCode) {
    case VK_UP:
      res.data.modified.key = 'A';
      break;
    case VK_DOWN:
      res.data.modified.key = 'B';
      break;
    case VK_RIGHT:
      res.data.modified.key = 'C';
      break;
    case VK_LEFT:
      res.data.modified.key = 'D';
      break;
    }
    return res;
  }

  if (ker.wVirtualKeyCode == VK_HOME) {
    res.kind = term_keyboard_arrow;
    res.data.arrow = term_keyboard_home;
    return res;
  }

  if (ker.wVirtualKeyCode == VK_END) {
    res.kind = term_keyboard_arrow;
    res.data.arrow = term_keyboard_end;
    return res;
  }

  // Handle character input
  WCHAR ch = ker.uChar.UnicodeChar;

  if (ch == 0) {
    res.kind = term_keyboard_unknown;
    return res;
  }

  if (ch > 127) {
    res.kind = term_keyboard_raw;
    res.data.raw = '?';
    return res;
  }

  u8 ascii = (u8)ch;

  if (ctrl && !alt && ascii >= 1 && ascii <= 26) {
    res.kind = term_keyboard_ctrl;
    res.data.modified.ctrl = true;
    res.data.modified.shift = shift;
    res.data.modified.key = ascii + '@';
    return res;
  }

  if (alt && ascii >= 32) {
    res.kind = term_keyboard_alt;
    res.data.modified.alt = true;
    res.data.modified.ctrl = ctrl;
    res.data.modified.shift = shift;
    res.data.modified.key = ascii & ~32;
    return res;
  }

  res.kind = term_keyboard_raw;
  res.data.raw = ascii;
  return res;
}
#endif
REGISTER_PRINTER(term_keyboard, {
  PUTS(L"{");
  switch (in.kind) {
  case term_keyboard_raw:
    PUTS(L"raw:");
    USETYPEPRINTER(char, (char)in.data.raw);
    break;
  case term_keyboard_ctrl:
    PUTS(L"ctrl:");
    if (in.data.modified.shift)
      PUTS(L"S+");
    if (in.data.modified.alt)
      PUTS(L"A+");
    USETYPEPRINTER(char, (char)in.data.modified.key);
    break;
  case term_keyboard_alt:
    PUTS(L"alt:");
    if (in.data.modified.ctrl)
      PUTS(L"C+");
    if (in.data.modified.shift)
      PUTS(L"S+");
    USETYPEPRINTER(char, (char)in.data.modified.key);
    break;
  case term_keyboard_arrow:
    PUTS(L"arrow:");
    if (in.data.modified.ctrl)
      PUTS(L"C+");
    if (in.data.modified.shift)
      PUTS(L"S+");
    if (in.data.modified.alt)
      PUTS(L"A+");
    switch (in.data.arrow) {
    case term_keyboard_up:
      PUTS(L"↑");
      break;
    case term_keyboard_down:
      PUTS(L"↓");
      break;
    case term_keyboard_left:
      PUTS(L"←");
      break;
    case term_keyboard_right:
      PUTS(L"→");
      break;
    case term_keyboard_home:
      PUTS(L"Home");
      break;
    case term_keyboard_end:
      PUTS(L"End");
      break;
    default:
      USETYPEPRINTER(char, (char)in.data.arrow);
    }
    break;
  case term_keyboard_function:
    PUTS(L"F");
    USETYPEPRINTER(int, in.data.function_key);
    break;
  case term_keyboard_mouse:
    PUTS(L"mouse:");
    USENAMEDPRINTER("term_mouse", in.data.mouse);
    break;
  case term_keyboard_none:
    PUTS(L"none");
    break;
  case term_keyboard_unknown:
    PUTS(L"unknown");
    break;
  }
  PUTS(L"}");
});

REGISTER_PRINTER(term_mouse, {
  PUTS(L"{");
  PUTS(L"(");
  USETYPEPRINTER(int, in.row);
  PUTS(L",");
  USETYPEPRINTER(int, in.col);
  PUTS(L")");
  PUTS(L",");
  switch (in.code.unknown) {
  case term_mouse_left:
    PUTS(L"left");
    break;
  case term_mouse_middle:
    PUTS(L"middle");
    break;
  case term_mouse_right:
    PUTS(L"right");
    break;
  case term_mouse_move:
    PUTS(L"move");
    break;
  default:
    PUTS(L"unknown(");
    USETYPEPRINTER(int, in.code.unknown);
    PUTS(L")");
  }
  PUTS(L",");
  switch (in.state) {
  case term_mouse_up:
    PUTS(L"up");
    break;
  default:
    PUTS(L"down");
  }
  PUTS(L"(");
  if (in.term_mouse_alt)
    PUTS(L"A");
  if (in.term_mouse_ctrl)
    PUTS(L"C");
  if (in.term_mouse_drag)
    PUTS(L"D");
  PUTS(L")");
  PUTS(L"}");
});
#endif // MY_TERM_INPUT_C
