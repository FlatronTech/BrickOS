#ifndef APP_NOTEPAD_H
#define APP_NOTEPAD_H

#include "kernel.h"

#define NOTE_LINES 25
#define NOTE_COLS  64

extern int note_active;
extern int note_dirty;
extern char current_note_file[16];

void note_init(void);
void note_clear(void);
void note_load_file(const char* filename);
void note_save(void);
void draw_notepad(struct Window* w);
void notepad_click(struct Window* w, int mx, int my);
void notepad_key(char c, struct Window* w);
int  notepad_is_hover_link(struct Window* w, int mx, int my);

#endif
