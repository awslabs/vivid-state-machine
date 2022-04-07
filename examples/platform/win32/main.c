#include "../../application.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vivid/binding/win32.h>
#include <windows.h>

#define QUIT_INDEX 0U
#define CONSOLE_INDEX 1U
#define VSM_INDEX 2U
#define NUM_HANDLES 3U

static HANDLE m_quit_event_handle;

static void print_message(void *logger, vivid_log_level_t level, const char *message)
{
    (void)logger;
    if (level == VIVID_LOG_LEVEL_NONE) {
        return;
    }
    const char *level_string = "?????";
    const char *level_color = "";
    switch (level) {
    case VIVID_LOG_LEVEL_ERROR:
        level_string = "ERROR";
        level_color = "\x1B[31m"; // red
        break;
    case VIVID_LOG_LEVEL_WARN:
        level_string = "WARN ";
        level_color = "\x1B[33m"; // yellow
        break;
    case VIVID_LOG_LEVEL_INFO:
        level_string = "INFO ";
        level_color = "";
        break;
    case VIVID_LOG_LEVEL_DEBUG:
        level_string = "DEBUG";
        level_color = "\x1B[34m"; // blue
        break;
    case VIVID_LOG_LEVEL_NONE:
    default:
        break;
    }
    struct timespec ts = { 0 };
    (void)timespec_get(&ts, TIME_UTC);
    struct tm tm = { 0 };
    (void)gmtime_s(&tm, &ts.tv_sec);
    char time_string[20]; // "1970-01-01 00:00:00"
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", &tm);
    printf("%s%s.%03ld | %s | %s%s\n", level_color, time_string, ts.tv_nsec / 1000000, level_string, message, (*level_string != '\0') ? "\x1B[0m" : "");
}

static void console_process(void *application)
{
    static char buf[28U] = "input: ";
    static int offset = 7;
    INPUT_RECORD record;
    DWORD count;
    if (!ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1U, &count)) {
        return;
    }
    if (record.EventType != KEY_EVENT) {
        return;
    }
    if (!record.Event.KeyEvent.bKeyDown) {
        return;
    }
    char ch = record.Event.KeyEvent.uChar.AsciiChar;
    if (ch == '\r') {
        putc('\n', stdout);
        buf[offset] = '\0';
        print_message(NULL, VIVID_LOG_LEVEL_INFO, buf);
        application_trigger(application, &buf[7U], offset - 7U);
        offset = 7;
        return;
    }
    putc(ch, stdout);
    if (offset < (sizeof(buf) - 1U)) {
        buf[offset++] = ch;
    }
}

static BOOL ctrl_handler(DWORD signal)
{
    if (signal != CTRL_C_EVENT) {
        return FALSE;
    }
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopping");
    SetEvent(m_quit_event_handle);
    return TRUE;
}

static void error_hook(void *app, vivid_error_t error)
{
    abort();
}

int main(int argc, char **argv)
{
    int res = 0;
    HANDLE handles[NUM_HANDLES] = { 0 };
    void *application = NULL;
    vivid_binding_t *binding = NULL;

    handles[QUIT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (handles[QUIT_INDEX] == NULL) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not create event");
        res = -1;
        goto out;
    }
    m_quit_event_handle = handles[QUIT_INDEX];
    if (!SetConsoleCtrlHandler(ctrl_handler, TRUE)) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not set console control handler");
        res = -1;
        goto out;
    }
    handles[CONSOLE_INDEX] = GetStdHandle(STD_INPUT_HANDLE);

    binding = vivid_binding_win32_create(&handles[VSM_INDEX] VIVID_LOG_ARGS(, print_message, NULL));
    if (binding == NULL) {
        res = -1;
        goto out;
    }
    binding->error_hook = error_hook;

    application = application_create(binding);
    if (application == NULL) {
        res = -1;
        goto out;
    }

    bool quit = false;
    while (!quit) {
        DWORD result = WaitForMultipleObjects(NUM_HANDLES, handles, FALSE, INFINITE);
        if (result < 0) {
            print_message(NULL, VIVID_LOG_LEVEL_ERROR, "error waiting on objects");
            res = -1;
            break;
        }
        if (result < WAIT_OBJECT_0) {
            print_message(NULL, VIVID_LOG_LEVEL_ERROR, "unexpected wait object");
            res = -1;
            break;
        }

        switch (result) {
        case WAIT_OBJECT_0 + QUIT_INDEX:
            quit = true;
            break;
        case WAIT_OBJECT_0 + CONSOLE_INDEX:
            console_process(application);
            break;
        case WAIT_OBJECT_0 + VSM_INDEX:
            vivid_binding_win32_handle_event(binding);
            break;
        default:
            print_message(NULL, VIVID_LOG_LEVEL_ERROR, "unexpected wait object");
            res = -1;
            break;
        }
    }

out:
    if (handles[QUIT_INDEX] != NULL) {
        (void)CloseHandle(handles[QUIT_INDEX]);
    }
    application_destroy(application);
    vivid_binding_win32_destroy(binding);
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopped");
    return res;
}
