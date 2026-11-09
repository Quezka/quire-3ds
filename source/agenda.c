#include "agenda.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *str(const cJSON *o, const char *name) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static int num(const cJSON *o, const char *name, int fallback) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    return cJSON_IsNumber(v) ? v->valueint : fallback;
}

static void put(char *dest, size_t size, const char *src) {
    snprintf(dest, size, "%s", src ? src : "");
}

#define EACH_LIVE(store, kind, uidvar, datavar)                                                    \
    for (cJSON *_b = store_kind(store, kind), *_e = _b ? _b->child : NULL; _e; _e = _e->next)       \
        for (cJSON *datavar = cJSON_GetObjectItemCaseSensitive(_e, "data"); datavar; datavar = NULL) \
            for (const char *uidvar = _e->string; uidvar; uidvar = NULL)

static int by_start(const void *a, const void *b) {
    const Item *x = a, *y = b;
    return x->start != y->start ? x->start - y->start : strcmp(x->title, y->title);
}

static int add(Item *out, int n, int max, int start, int end, const char *title, const char *detail, char kind) {
    if (n >= max) return n;
    out[n].start = start;
    out[n].end = end;
    put(out[n].title, sizeof out[n].title, title);
    put(out[n].detail, sizeof out[n].detail, detail);
    out[n].kind = kind;
    return n + 1;
}

// A weekly shift is off on a day the person skipped it.
static int skipped(const cJSON *job, const char *day, int start) {
    const cJSON *skips = cJSON_GetObjectItemCaseSensitive(job, "skips");
    const cJSON *s;
    cJSON_ArrayForEach(s, skips) {
        const cJSON *d = cJSON_GetArrayItem(s, 0), *m = cJSON_GetArrayItem(s, 1);
        if (cJSON_IsString(d) && strcmp(d->valuestring, day) == 0 && cJSON_IsNumber(m) && m->valueint == start) return 1;
    }
    return 0;
}

int agenda_today(const Store *s, const char *day, int weekday, Item *out, int max) {
    int n = 0;
    EACH_LIVE(s, "event", uid, d) {
        (void)uid;
        if (strcmp(str(d, "day"), day) == 0) n = add(out, n, max, num(d, "start", 0), num(d, "end", -1), str(d, "title"), str(d, "details"), 'e');
    }
    EACH_LIVE(s, "course", uid, d) {
        (void)uid;
        const cJSON *slot;
        cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(d, "slots")) {
            if (cJSON_GetArrayItem(slot, 0) && cJSON_GetArrayItem(slot, 0)->valueint == weekday) {
                const cJSON *room = cJSON_GetArrayItem(slot, 3);
                const char *where = cJSON_IsString(room) && room->valuestring[0] ? room->valuestring : str(d, "room");
                n = add(out, n, max, cJSON_GetArrayItem(slot, 1)->valueint, cJSON_GetArrayItem(slot, 2)->valueint, str(d, "name"), where, 'c');
            }
        }
    }
    EACH_LIVE(s, "job", uid, d) {
        (void)uid;
        const cJSON *p;
        cJSON_ArrayForEach(p, cJSON_GetObjectItemCaseSensitive(d, "patterns")) {
            const cJSON *wd = cJSON_GetArrayItem(p, 0), *st = cJSON_GetArrayItem(p, 1), *du = cJSON_GetArrayItem(p, 2);
            const cJSON *since = cJSON_GetArrayItem(p, 4), *until = cJSON_GetArrayItem(p, 5);
            if (!cJSON_IsNumber(wd) || wd->valueint != weekday) continue;
            if (cJSON_IsString(since) && strcmp(since->valuestring, day) > 0) continue;
            if (cJSON_IsString(until) && strcmp(until->valuestring, day) < 0) continue;
            if (skipped(d, day, st->valueint)) continue;
            n = add(out, n, max, st->valueint, st->valueint + du->valueint, str(d, "name"), "Work", 'w');
        }
    }
    EACH_LIVE(s, "shift", uid, d) {
        (void)uid;
        if (strcmp(str(d, "day"), day) != 0) continue;
        const cJSON *job = store_get(s, "job", str(d, "job"));
        int start = num(d, "start", 0);
        n = add(out, n, max, start, start + num(d, "duration", 0), job ? str(job, "name") : "Shift", str(d, "notes")[0] ? str(d, "notes") : "Work", 'w');
    }
    EACH_LIVE(s, "routine", uid, d) {
        (void)uid;
        const cJSON *w;
        cJSON_ArrayForEach(w, cJSON_GetObjectItemCaseSensitive(d, "weekdays")) {
            if (cJSON_IsNumber(w) && w->valueint == weekday) {
                n = add(out, n, max, num(d, "start", 0), -1, str(d, "title"), "Routine", 'r');
                break;
            }
        }
    }
    qsort(out, n, sizeof *out, by_start);
    return n;
}

static int by_due(const void *a, const void *b) {
    const TaskRow *x = a, *y = b;
    if (!x->due[0] != !y->due[0]) return x->due[0] ? -1 : 1;  // undated last
    int c = strcmp(x->due, y->due);
    return c ? c : strcmp(x->title, y->title);
}

int agenda_tasks(const Store *s, const char *day, TaskRow *out, int max) {
    int n = 0;
    EACH_LIVE(s, "task", uid, d) {
        (void)uid;
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(d, "done")) || n >= max) continue;
        put(out[n].title, sizeof out[n].title, str(d, "title"));
        put(out[n].due, sizeof out[n].due, str(d, "due"));
        out[n].overdue = out[n].due[0] && strcmp(out[n].due, day) < 0;
        out[n].today = strcmp(out[n].due, day) == 0;
        n++;
    }
    qsort(out, n, sizeof *out, by_due);
    return n;
}

static void first_line(const char *body, char *dest, size_t size) {
    while (*body == '#' || *body == ' ') body++;
    size_t i = 0;
    while (body[i] && body[i] != '\n' && i + 1 < size) {
        dest[i] = body[i];
        i++;
    }
    dest[i] = 0;
}

static int by_note(const void *a, const void *b) {
    const NoteRow *x = a, *y = b;
    if (x->pinned != y->pinned) return y->pinned - x->pinned;
    return strcmp(y->when, x->when);  // newest first
}

int agenda_notes(const Store *s, NoteRow *out, int max) {
    int n = 0;
    EACH_LIVE(s, "note", uid, d) {
        if (n >= max) break;
        put(out[n].uid, sizeof out[n].uid, uid);
        if (str(d, "title")[0]) {
            put(out[n].title, sizeof out[n].title, str(d, "title"));
        } else {
            first_line(str(d, "body"), out[n].title, sizeof out[n].title);
        }
        if (!out[n].title[0]) put(out[n].title, sizeof out[n].title, "(no title)");
        snprintf(out[n].when, sizeof out[n].when, "%.10s", str(d, "updated"));
        out[n].pinned = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(d, "pinned"));
        n++;
    }
    qsort(out, n, sizeof *out, by_note);
    return n;
}

int agenda_water_ml(const Store *s, const char *day) {
    int total = 0;
    EACH_LIVE(s, "water", uid, d) {
        (void)uid;
        if (strcmp(str(d, "day"), day) == 0) total += num(d, "ml", 0);
    }
    return total;
}

// Copies `src` without the inline marks ** * ` and pictures, appending to dest.
static size_t inline_plain(const char *src, size_t len, char *dest) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if (c == '*' || c == '`' || c == '_') continue;
        if (c == '!' && i + 1 < len && src[i + 1] == '[') {  // ![alt](quire-image:...) -> [picture]
            const char *close = memchr(src + i, ')', len - i);
            if (close) {
                memcpy(dest + n, "[picture]", 9);
                n += 9;
                i = close - src;
                continue;
            }
        }
        dest[n++] = c;
    }
    return n;
}

char *agenda_note_text(const Store *s, const char *uid) {
    const cJSON *d = store_get(s, "note", uid);
    const char *body = d ? str(d, "body") : "";
    size_t size = strlen(body) * 2 + 64;
    char *out = malloc(size);
    if (!out) return NULL;
    size_t n = 0;
    const char *line = body;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        const char *p = line;
        size_t indent = 0;
        while (indent < len && (p[indent] == ' ' || p[indent] == '\t')) indent++;
        p += indent;
        len -= indent;
        for (size_t k = 0; k < indent / 2; k++) out[n++] = ' ', out[n++] = ' ';
        if (len >= 6 && (!strncmp(p, "- [ ] ", 6) || !strncmp(p, "- [x] ", 6) || !strncmp(p, "- [X] ", 6))) {
            // Checkboxes as plain marks the system font has.
            const char *box = p[3] == ' ' ? "[ ] " : "[x] ";
            memcpy(out + n, box, 4);
            n += 4;
            p += 6;
            len -= 6;
        } else if (len >= 2 && (p[0] == '-' || p[0] == '*') && p[1] == ' ') {
            memcpy(out + n, "\xE2\x80\xA2 ", 4);  // •
            n += 4;
            p += 2;
            len -= 2;
        } else if (len >= 2 && p[0] == '>' ) {
            memcpy(out + n, "| ", 2);
            n += 2;
            p += 1;
            len -= 1;
            while (len && *p == ' ') p++, len--;
        } else {
            while (len && *p == '#') p++, len--;  // headings keep their words
            while (len && *p == ' ' && p != line + indent) p++, len--;
        }
        n += inline_plain(p, len, out + n);
        out[n++] = '\n';
        if (!end) break;
        line = end + 1;
    }
    out[n] = 0;
    return out;
}
