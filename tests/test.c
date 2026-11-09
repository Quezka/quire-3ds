// Host tests for the parts that don't need a 3DS:  make -f tests/Makefile
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/agenda.h"
#include "../source/link.h"
#include "../source/store.h"

static cJSON *record(const char *kind, const char *uid, const char *modified, const char *data) {
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "kind", kind);
    cJSON_AddStringToObject(r, "uid", uid);
    cJSON_AddStringToObject(r, "modified", modified);
    if (data) {
        cJSON_AddFalseToObject(r, "deleted");
        cJSON_AddItemToObject(r, "data", cJSON_Parse(data));
    } else {
        cJSON_AddTrueToObject(r, "deleted");
        cJSON_AddNullToObject(r, "data");
    }
    return r;
}

static void apply(Store *s, const char *kind, const char *uid, const char *modified, const char *data) {
    cJSON *r = record(kind, uid, modified, data);
    store_apply(s, r);
    cJSON_Delete(r);
}

int main(int argc, char **argv) {
    // The setup code, as the desktop makes it (version 2, a quire-server).
    QuireLink link;
    const char *code = argc > 1 ? argv[1] : NULL;
    if (code) {
        assert(link_parse(code, &link) == NULL);
        assert(strcmp(link.url, "https://sync.example.com") == 0);
        assert(strcmp(link.refresh, "r3fr3sh") == 0);
    }
    assert(link_parse("hello", &link) != NULL);
    char out[64];
    assert(base64url_decode("aGk", out, sizeof out) == 2 && strcmp(out, "hi") == 0);

    Store s;
    store_init(&s);
    apply(&s, "task", "t1", "2026-11-09T10:00:00.000Z", "{\"title\":\"Essay\",\"due\":\"2026-11-08\",\"done\":false}");
    apply(&s, "task", "t2", "2026-11-09T10:00:00.000Z", "{\"title\":\"Read\",\"due\":null,\"done\":false}");
    apply(&s, "task", "t3", "2026-11-09T10:00:00.000Z", "{\"title\":\"Done one\",\"due\":\"2026-11-09\",\"done\":true}");
    // An older copy never wins over a newer one.
    apply(&s, "task", "t1", "2026-11-01T10:00:00.000Z", "{\"title\":\"Old\",\"due\":\"2026-11-08\",\"done\":false}");
    TaskRow tasks[8];
    int n = agenda_tasks(&s, "2026-11-09", tasks, 8);
    assert(n == 2 && strcmp(tasks[0].title, "Essay") == 0 && tasks[0].overdue && strcmp(tasks[1].title, "Read") == 0);

    apply(&s, "event", "e1", "2026-11-09T10:00:00.000Z", "{\"day\":\"2026-11-09\",\"start\":600,\"end\":660,\"title\":\"Dentist\",\"details\":\"\"}");
    apply(&s, "course", "c1", "2026-11-09T10:00:00.000Z", "{\"name\":\"Maths\",\"room\":\"B2\",\"slots\":[[0,480,540,\"\"],[2,480,540,\"A1\"]]}");
    apply(&s, "job", "j1", "2026-11-09T10:00:00.000Z", "{\"name\":\"Bar\",\"patterns\":[[0,1080,240,0,\"2026-01-01\",null]],\"skips\":[]}");
    apply(&s, "routine", "r1", "2026-11-09T10:00:00.000Z", "{\"title\":\"Gym\",\"start\":420,\"weekdays\":[0,2]}");
    apply(&s, "water", "w1", "2026-11-09T10:00:00.000Z", "{\"day\":\"2026-11-09\",\"at\":600,\"ml\":250}");
    Item items[16];
    n = agenda_today(&s, "2026-11-09", 0, items, 16);  // a Monday
    assert(n == 4);
    assert(strcmp(items[0].title, "Gym") == 0 && strcmp(items[1].title, "Maths") == 0 && strcmp(items[1].detail, "B2") == 0);
    assert(strcmp(items[2].title, "Dentist") == 0 && strcmp(items[3].title, "Bar") == 0 && items[3].end == 1320);
    assert(agenda_water_ml(&s, "2026-11-09") == 250);

    apply(&s, "job", "j1", "2026-11-09T11:00:00.000Z", "{\"name\":\"Bar\",\"patterns\":[[0,1080,240,0,\"2026-01-01\",null]],\"skips\":[[\"2026-11-09\",1080]]}");
    assert(agenda_today(&s, "2026-11-09", 0, items, 16) == 3);

    apply(&s, "note", "n1", "2026-11-09T10:00:00.000Z", "{\"title\":\"\",\"body\":\"# Cells\\n\\nThe **powerhouse**\\n- [x] ATP\\n- item\\n![a](quire-image:x)\",\"pinned\":false,\"updated\":\"2026-11-09T09:00:00\"}");
    apply(&s, "note", "n2", "2026-11-09T10:00:00.000Z", "{\"title\":\"Pinned\",\"body\":\"x\",\"pinned\":true,\"updated\":\"2026-01-01T00:00:00\"}");
    NoteRow notes[4];
    n = agenda_notes(&s, notes, 4);
    assert(n == 2 && strcmp(notes[0].title, "Pinned") == 0 && strcmp(notes[1].title, "Cells") == 0);
    char *text = agenda_note_text(&s, "n1");
    assert(strstr(text, "Cells\n") && strstr(text, "The powerhouse") && strstr(text, "[x] ATP") && strstr(text, "[picture]"));
    free(text);

    // Deleted on another device: gone here too, and it survives a save and load.
    apply(&s, "note", "n2", "2026-11-09T12:00:00.000Z", NULL);
    store_set_cursor(&s, "c42");
    assert(store_save(&s, "build-tests/store.json"));
    Store t;
    store_init(&t);
    assert(store_load(&t, "build-tests/store.json"));
    assert(strcmp(store_cursor(&t), "c42") == 0);
    assert(agenda_notes(&t, notes, 4) == 1);
    store_free(&t);
    store_free(&s);
    puts("all tests passed");
    return 0;
}
