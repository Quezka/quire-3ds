// What the screens show, worked out from the stored records: today's plan, open tasks, notes.
// Pure C on top of the store, so it's tested on the computer too (tests/test.c).
#pragma once
#include "store.h"

typedef struct {
    int start, end;      // minutes after midnight; end -1 when it has no end
    char title[96];
    char detail[96];
    char kind;           // 'e' event, 'c' class, 'w' work, 'r' routine
} Item;

typedef struct {
    char title[128];
    char due[11];        // YYYY-MM-DD or ""
    int overdue, today;
} TaskRow;

typedef struct {
    char uid[130];
    char title[96];
    char when[11];       // the day it was last changed
    int pinned;
} NoteRow;

// `day` is YYYY-MM-DD, `weekday` 0 = Monday (as the desktop counts).
int agenda_today(const Store *s, const char *day, int weekday, Item *out, int max);
int agenda_tasks(const Store *s, const char *day, TaskRow *out, int max);
int agenda_notes(const Store *s, NoteRow *out, int max);
int agenda_water_ml(const Store *s, const char *day);
// The note's text without Markdown marks, ready to show: headings, bullets and checkboxes kept readable.
char *agenda_note_text(const Store *s, const char *uid);  // malloc'd; free it
