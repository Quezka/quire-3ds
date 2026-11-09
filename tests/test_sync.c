// Pulls from a running quire-server:  ./test_sync <url> <refresh token>   (see tests/README)
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../source/agenda.h"
#include "../source/sync.h"

static void progress(int n, void *ctx) {
    (void)ctx;
    printf("  %d records\n", n);
}

int main(int argc, char **argv) {
    QuireLink link = {0};
    snprintf(link.url, sizeof link.url, "%s", argv[1]);
    snprintf(link.refresh, sizeof link.refresh, "%s", argv[2]);
    Store s;
    store_init(&s);
    const char *problem = sync_pull(&link, &s, NULL, progress, NULL);
    if (problem) printf("problem: %s\n", problem);
    assert(!problem);
    TaskRow tasks[8];
    assert(agenda_tasks(&s, "2026-11-09", tasks, 8) == 2);
    NoteRow notes[8];
    assert(agenda_notes(&s, notes, 8) == 1);
    // Nothing new the second time; the cursor carries on.
    assert(!sync_pull(&link, &s, NULL, progress, NULL));
    snprintf(link.refresh, sizeof link.refresh, "wrong");
    problem = sync_pull(&link, &s, NULL, NULL, NULL);
    assert(problem);
    printf("bad token says: %s\n", problem);
    snprintf(link.url, sizeof link.url, "http://127.0.0.1:1");
    printf("no server says: %s\n", sync_pull(&link, &s, NULL, NULL, NULL));
    store_free(&s);
    puts("sync ok");
    return 0;
}
