#ifndef APP_TERMINAL_H
#define APP_TERMINAL_H

#include "kernel.h"

#define TERM_LINES 16
#define TERM_COLS  52

extern int term_active;

void term_init(void);
void term_clear_screen(void);
void term_scroll_up(void);
void term_putchar(char c);
void term_print(const char* s);
void term_dec(u32 v);
void term_exec(const char* cmd);
void draw_terminal(struct Window* w);
void terminal_key(char c);

#endif
