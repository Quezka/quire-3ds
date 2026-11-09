// Quire for Nintendo 3DS: a read-only window on your Quire, synced from your own quire-server.
// Set up by scanning the setup code Quire's desktop shows for phones (Settings > Sync, the phone's code).
//
// Top screen: what you're reading. Bottom screen: the tabs and what the buttons do.
// D-pad / Circle Pad: move · A: open · B: back · L/R or touch: tabs · X: sync · SELECT: sign out · START: quit.

#include <3ds.h>
#include <citro2d.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "agenda.h"
#include "camera.h"
#include "link.h"
#include "store.h"
#include "sync.h"

#define DIR "sdmc:/3ds/quire"
#define ACCOUNT DIR "/account.json"
#define DATA DIR "/data.json"
#define CA "romfs:/cacert.pem"

// The suite's dark palette, and large text: the owner doesn't see well.
#define BG C2D_Color32(0x11, 0x12, 0x14, 0xFF)
#define SURFACE C2D_Color32(0x26, 0x28, 0x2E, 0xFF)
#define TEXT C2D_Color32(0xEC, 0xED, 0xEF, 0xFF)
#define MUTED C2D_Color32(0xB0, 0xB6, 0xC0, 0xFF)
#define ACCENT C2D_Color32(0x81, 0x82, 0xF0, 0xFF)
#define ON_ACCENT C2D_Color32(0x10, 0x10, 0x30, 0xFF)
#define WARN C2D_Color32(0xFF, 0x9F, 0x6E, 0xFF)
#define BODY 0.62f
#define SMALL 0.52f
#define TITLE 0.85f
#define ROW 56

enum Screen { SETUP, SCAN, TODAY, TASKS, NOTES, NOTE, CONFIRM_SIGNOUT };

static C3D_RenderTarget *top, *bottom;
static C2D_TextBuf textbuf;
static Store store;
static QuireLink account;
static char status[200];

static Item items[128];
static TaskRow tasks[256];
static NoteRow notes[512];
static int item_count, task_count, note_count;
static char today[11];
static int weekday, water;
static char *note_text;
static char note_title[96];

static int screen = TODAY, selected = 0;
static float scroll = 0;

// ---- drawing ---------------------------------------------------------------------------------------------------

// Draws text; with a width it wraps. Returns roughly how tall it came out.
static float text(float x, float y, float scale, u32 color, float width, const char *s) {
    C2D_Text t;
    C2D_TextParse(&t, textbuf, s);
    C2D_TextOptimize(&t);
    float w = 0, h = 0;
    C2D_TextGetDimensions(&t, scale, scale, &w, &h);
    if (width > 0) {
        C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, 0.5f, scale, scale, color, width);
        int lines = 1 + (int)(w / width);
        for (const char *p = s; *p; p++) lines += *p == '\n';
        return lines * 30.0f * scale;
    }
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, scale, scale, color);
    return h;
}

static void clock_time(int m, char *out) { sprintf(out, "%02d:%02d", (m / 60) % 24, m % 60); }

static void begin(C3D_RenderTarget *target) {
    C2D_TargetClear(target, BG);
    C2D_SceneBegin(target);
}

// ---- data ------------------------------------------------------------------------------------------------------

static void work_out_day(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(today, sizeof today, "%Y-%m-%d", t);
    weekday = (t->tm_wday + 6) % 7;  // Monday = 0, as the desktop counts
}

static void refresh_lists(void) {
    work_out_day();
    item_count = agenda_today(&store, today, weekday, items, 128);
    task_count = agenda_tasks(&store, today, tasks, 256);
    note_count = agenda_notes(&store, notes, 512);
    water = agenda_water_ml(&store, today);
}

static int load_account(void) {
    FILE *f = fopen(ACCOUNT, "rb");
    if (!f) return 0;
    char buf[1200] = {0};
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[got] = 0;
    cJSON *j = cJSON_Parse(buf);
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(j, "url");
    const cJSON *ref = cJSON_GetObjectItemCaseSensitive(j, "refresh");
    const cJSON *mail = cJSON_GetObjectItemCaseSensitive(j, "email");
    int ok = cJSON_IsString(url) && cJSON_IsString(ref);
    if (ok) {
        snprintf(account.url, sizeof account.url, "%s", url->valuestring);
        snprintf(account.refresh, sizeof account.refresh, "%s", ref->valuestring);
        snprintf(account.email, sizeof account.email, "%s", cJSON_IsString(mail) ? mail->valuestring : "");
    }
    cJSON_Delete(j);
    return ok;
}

static void save_account(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(DIR, 0777);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "url", account.url);
    cJSON_AddStringToObject(j, "email", account.email);
    cJSON_AddStringToObject(j, "refresh", account.refresh);
    char *s = cJSON_PrintUnformatted(j);
    FILE *f = fopen(ACCOUNT, "wb");
    if (f && s) {
        fwrite(s, 1, strlen(s), f);
    }
    if (f) fclose(f);
    free(s);
    cJSON_Delete(j);
}

static void draw_message(const char *heading, const char *detail) {
    C2D_TextBufClear(textbuf);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    begin(top);
    text(20, 70, TITLE, TEXT, 0, heading);
    text(20, 110, BODY, MUTED, 360, detail);
    begin(bottom);
    C3D_FrameEnd(0);
}

static void sync_progress(int n, void *context) {
    (void)context;
    char line[64];
    sprintf(line, "%d changes so far", n);
    draw_message("Syncing\xE2\x80\xA6", line);
}

static void do_sync(void) {
    draw_message("Syncing\xE2\x80\xA6", account.url);
    mkdir("sdmc:/3ds", 0777);
    mkdir(DIR, 0777);
    const char *problem = sync_pull(&account, &store, CA, sync_progress, NULL);
    store_save(&store, DATA);
    if (problem) {
        snprintf(status, sizeof status, "%s", problem);
    } else {
        time_t now = time(NULL);
        strftime(status, sizeof status, "Synced at %H:%M", localtime(&now));
    }
    refresh_lists();
}

// ---- screens ---------------------------------------------------------------------------------------------------

static const char *TABS[] = {"Today", "Tasks", "Notes"};

static int current_tab(void) { return screen == NOTE ? 2 : screen - TODAY; }

static int list_length(void) {
    return screen == TODAY ? item_count : screen == TASKS ? task_count : screen == NOTES ? note_count : 0;
}

static void row_background(int i, float y) {
    if (i == selected) C2D_DrawRectSolid(8, y - 4, 0.2f, 384, ROW - 4, SURFACE);
}

static void draw_top(void) {
    begin(top);
    char line[200];
    float y = 10 - scroll;
    switch (screen) {
    case SETUP:
        text(20, 24, TITLE, TEXT, 0, "Quire");
        text(20, 64, BODY, MUTED, 360,
             "On your computer, open Quire's sync settings and show the setup code for the phone. "
             "Then press A here and hold the 3DS's outer camera up to it.");
        if (status[0]) text(20, 190, SMALL, WARN, 360, status);
        return;
    case SCAN:
        camera_draw(0, 0);  // the live picture, so you can aim
        C2D_DrawRectSolid(0, 212, 0.6f, 400, 28, C2D_Color32(0, 0, 0, 0xC0));
        text(10, 216, SMALL, TEXT, 0, "Point the outer camera at the code.  B: cancel");
        return;
    case CONFIRM_SIGNOUT:
        text(20, 70, TITLE, TEXT, 360, "Sign out?");
        return;
    case NOTE:
        y += text(16, y, TITLE, TEXT, 368, note_title) + 10;
        text(16, y, BODY, TEXT, 368, note_text ? note_text : "");
        return;
    case TODAY:
        sprintf(line, "Today \xC2\xB7 %s", today);
        y += text(16, y, TITLE, TEXT, 0, line) + 6;
        if (water) {
            sprintf(line, "Water: %d ml", water);
            y += text(16, y, SMALL, MUTED, 0, line) + 6;
        }
        if (!item_count) text(16, y, BODY, MUTED, 368, "Nothing planned today.");
        for (int i = 0; i < item_count; i++, y += ROW) {
            char a[8], b[8];
            clock_time(items[i].start, a);
            if (items[i].end >= 0) {
                clock_time(items[i].end, b);
                sprintf(line, "%s\xE2\x80\x93%s", a, b);
            } else {
                strcpy(line, a);
            }
            u32 tint = items[i].kind == 'c' ? ACCENT : items[i].kind == 'w' ? WARN : MUTED;
            row_background(i, y);
            text(16, y, SMALL, tint, 0, line);
            text(130, y, BODY, TEXT, 0, items[i].title);
            if (items[i].detail[0]) text(130, y + 24, SMALL, MUTED, 0, items[i].detail);
        }
        return;
    case TASKS:
        y += text(16, y, TITLE, TEXT, 0, "Tasks") + 8;
        if (!task_count) text(16, y, BODY, MUTED, 368, "Nothing to do. Nice.");
        for (int i = 0; i < task_count; i++, y += ROW) {
            row_background(i, y);
            text(16, y, BODY, TEXT, 0, tasks[i].title);
            if (tasks[i].due[0]) {
                sprintf(line, "%s%s", tasks[i].overdue ? "Late \xC2\xB7 " : tasks[i].today ? "Today \xC2\xB7 " : "Due ", tasks[i].due);
                text(16, y + 24, SMALL, tasks[i].overdue ? WARN : MUTED, 0, line);
            }
        }
        return;
    case NOTES:
        y += text(16, y, TITLE, TEXT, 0, "Notes") + 8;
        if (!note_count) text(16, y, BODY, MUTED, 368, "No notes yet.");
        for (int i = 0; i < note_count; i++, y += ROW) {
            row_background(i, y);
            sprintf(line, "%s%s", notes[i].pinned ? "\xE2\x98\x85 " : "", notes[i].title);
            text(16, y, BODY, TEXT, 0, line);
            text(16, y + 24, SMALL, MUTED, 0, notes[i].when);
        }
        return;
    }
}

static void draw_bottom(void) {
    begin(bottom);
    if (screen == SETUP || screen == SCAN) {
        text(16, 20, BODY, TEXT, 288, screen == SETUP ? "A: scan the setup code" : "Looking for the code\xE2\x80\xA6");
        text(16, 205, SMALL, MUTED, 0, "START: quit");
        return;
    }
    if (screen == CONFIRM_SIGNOUT) {
        text(16, 24, BODY, TEXT, 288, "Sign out and forget the data on this 3DS?");
        text(16, 120, BODY, ACCENT, 0, "A: sign out");
        text(16, 150, BODY, MUTED, 0, "B: keep");
        return;
    }
    int tab = current_tab();
    for (int i = 0; i < 3; i++) {
        float x = 8 + i * 104;
        C2D_DrawRectSolid(x, 8, 0.2f, 96, 44, i == tab ? ACCENT : SURFACE);
        text(x + 14, 18, BODY, i == tab ? ON_ACCENT : TEXT, 0, TABS[i]);
    }
    text(16, 66, SMALL, MUTED, 288,
         screen == NOTE ? "Up/Down: scroll \xC2\xB7 B: back" : "Up/Down: move \xC2\xB7 A: open a note \xC2\xB7 L/R: tabs");
    text(16, 120, SMALL, MUTED, 288, "X: sync \xC2\xB7 SELECT: sign out \xC2\xB7 START: quit");
    text(16, 160, SMALL, status[0] ? TEXT : MUTED, 288,
         status[0] ? status : "Read-only: change things in Quire on your computer or phone.");
    if (account.email[0]) text(16, 214, SMALL, MUTED, 0, account.email);
}

// ---- input -----------------------------------------------------------------------------------------------------

static void open_tab(int tab) {
    screen = TODAY + tab;
    selected = 0;
    scroll = 0;
}

static void open_note(int i) {
    free(note_text);
    note_text = agenda_note_text(&store, notes[i].uid);
    snprintf(note_title, sizeof note_title, "%s", notes[i].title);
    screen = NOTE;
    scroll = 0;
}

static void sign_out(void) {
    remove(ACCOUNT);
    remove(DATA);
    store_clear(&store);
    memset(&account, 0, sizeof account);
    status[0] = 0;
    refresh_lists();
    screen = SETUP;
}

static void move(int delta) {
    if (screen == NOTE) {
        scroll += delta * 40;
        if (scroll < 0) scroll = 0;
        return;
    }
    int n = list_length();
    selected += delta;
    if (selected > n - 1) selected = n - 1;
    if (selected < 0) selected = 0;
    // Keep the chosen row on screen (rows come after a heading about 50 px tall).
    float row_top = 50 + selected * ROW;
    if (row_top - scroll > 240 - ROW - 10) scroll = row_top - (240 - ROW - 10);
    if (row_top - scroll < 50) scroll = row_top - 50 < 0 ? 0 : row_top - 50;
}

int main(void) {
    romfsInit();
    u32 *soc = memalign(0x1000, 0x100000);
    int network = soc && R_SUCCEEDED(socInit(soc, 0x100000));
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(8192);  // the camera preview is drawn as a few thousand small squares
    C2D_Prepare();
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textbuf = C2D_TextBufNew(16384);

    store_init(&store);
    store_load(&store, DATA);
    int signed_in = load_account();
    refresh_lists();
    screen = signed_in ? TODAY : SETUP;
    if (!network) {
        snprintf(status, sizeof status, "No network: showing what was synced before.");
    } else if (signed_in) {
        do_sync();
    }

    u32 repeat = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        if (down & KEY_START) break;

        if (screen == SCAN) {
            static char code[2048];
            if (camera_scan(code, sizeof code)) {
                camera_stop();
                QuireLink scanned;
                memset(&scanned, 0, sizeof scanned);
                const char *problem = link_parse(code, &scanned);
                if (problem) {
                    snprintf(status, sizeof status, "%s", problem);
                    screen = SETUP;
                } else {
                    account = scanned;
                    save_account();
                    store_clear(&store);
                    open_tab(0);
                    if (network) do_sync();
                }
            } else if (down & KEY_B) {
                camera_stop();
                screen = SETUP;
            }
        } else if (screen == SETUP) {
            if (down & KEY_A) {
                status[0] = 0;
                if (camera_start()) {
                    screen = SCAN;
                } else {
                    snprintf(status, sizeof status, "The camera didn't start.");
                }
            }
        } else if (screen == CONFIRM_SIGNOUT) {
            if (down & KEY_A) sign_out();
            if (down & KEY_B) open_tab(0);
        } else {
            if ((down & KEY_X) && network) do_sync();
            if (down & KEY_SELECT) screen = CONFIRM_SIGNOUT;
            if (down & KEY_R) open_tab((current_tab() + 1) % 3);
            if (down & KEY_L) open_tab((current_tab() + 2) % 3);
            if ((down & KEY_B) && screen == NOTE) {
                screen = NOTES;
                scroll = 0;
                move(0);
            }
            if ((down & KEY_A) && screen == NOTES && note_count) open_note(selected);
            if (down & KEY_TOUCH) {
                touchPosition touch;
                hidTouchRead(&touch);
                if (touch.py < 56) open_tab(touch.px / 106 > 2 ? 2 : touch.px / 106);
            }
            // Holding a direction keeps moving, a little slower than the frame rate.
            int dir = (held & (KEY_DOWN | KEY_CPAD_DOWN)) ? 1 : (held & (KEY_UP | KEY_CPAD_UP)) ? -1 : 0;
            if (down & (KEY_DOWN | KEY_UP | KEY_CPAD_DOWN | KEY_CPAD_UP)) {
                move(dir);
                repeat = 15;
            } else if (dir && repeat && --repeat == 0) {
                move(dir);
                repeat = 5;
            }
        }

        C2D_TextBufClear(textbuf);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        draw_top();
        draw_bottom();
        C3D_FrameEnd(0);
    }

    camera_stop();
    free(note_text);
    store_free(&store);
    C2D_TextBufDelete(textbuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    if (network) socExit();
    romfsExit();
    return 0;
}
