#include "../../application.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <vivid/binding/libev.h>

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
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm = { 0 };
    (void)gmtime_r(&ts.tv_sec, &tm);
    char time_string[20]; // "1970-01-01 00:00:00"
    strftime(time_string, sizeof(time_string), "%F %T", &tm);
    printf("%s%s.%03ld | %s | %s%s\n", level_color, time_string, ts.tv_nsec / 1000000, level_string, message, (*level_string != '\0') ? "\x1B[0m" : "");
}

static void exit_handler(struct ev_loop *loop, ev_signal *w, int revents)
{
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopping");
    ev_break(loop, EVBREAK_ALL);
}

static void trigger_handler(struct ev_loop *loop, ev_io *w, int revents)
{
    char buf[28U] = "input: ";
    size_t len = fread(&buf[7U], sizeof(char), 20U, stdin);
    if (len <= 0U) {
        return;
    }
    buf[6U + len] = '\0';
    print_message(NULL, VIVID_LOG_LEVEL_INFO, buf);
    application_trigger(w->data, &buf[7U], len - 1U);
}

static void error_hook(void *app, vivid_error_t error)
{
    abort();
}

int main(int argc, char **argv)
{
    int res = 0;
    vivid_binding_t *binding = NULL;
    void *application = NULL;

    binding = vivid_binding_libev_create(EV_DEFAULT VIVID_LOG_ARGS(, print_message, NULL));
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

    // Setup handlers:
    ev_signal sigint_watcher = { 0 };
    ev_signal_init(&sigint_watcher, exit_handler, SIGINT);
    ev_signal_start(EV_DEFAULT, &sigint_watcher);

    ev_signal sigterm_watcher = { 0 };
    ev_signal_init(&sigterm_watcher, exit_handler, SIGTERM);
    ev_signal_start(EV_DEFAULT, &sigterm_watcher);

    ev_io stdin_watcher = { 0 };
    stdin_watcher.data = application;
    fcntl(fileno(stdin), F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    ev_io_init(&stdin_watcher, trigger_handler, fileno(stdin), EV_READ);
    ev_io_start(EV_DEFAULT, &stdin_watcher);

    // Run the loop:
    ev_run(EV_DEFAULT, 0);

out:
    application_destroy(application);
    vivid_binding_libev_destroy(binding);
    ev_loop_destroy(EV_DEFAULT);
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopped");
    return res;
}
