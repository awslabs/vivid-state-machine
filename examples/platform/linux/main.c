#include "../../application.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>
#include <vivid/binding/linux.h>

#define QUIT_ID 0U
#define INPUT_ID 1U
#define VSM_ID 2U

static int m_quit_event_fd;

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

static bool add_fd(int efd, int fd, uint32_t id)
{
    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN;
    ev.data.u32 = id;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not add fd");
        return false;
    }
    return true;
}

static void handle_input(void *application)
{
    char buf[28U] = "input: ";
    size_t len = fread(&buf[7U], sizeof(char), 20U, stdin);
    if (len <= 0U) {
        return;
    }
    buf[6U + len] = '\0';
    print_message(NULL, VIVID_LOG_LEVEL_INFO, buf);
    application_trigger(application, &buf[7U], len - 1U);
}

static void handle_signal(int signum)
{
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopping");
    uint64_t val = 1U;
    if (write(m_quit_event_fd, &val, sizeof(val)) < sizeof(val)) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not set event fd");
    }
}

static void error_hook(void *app, vivid_error_t error)
{
    abort();
}

int main(int argc, char **argv)
{
    int res = 0;
    int efd = 0;
    vivid_binding_t *binding = NULL;
    void *application = NULL;

    m_quit_event_fd = eventfd(0, EFD_NONBLOCK);
    if (m_quit_event_fd < 0) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not create event");
        res = -1;
        goto out;
    }
    if ((signal(SIGINT, handle_signal) == SIG_ERR)
        || (signal(SIGTERM, handle_signal) == SIG_ERR)) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not set signal handler");
        res = -1;
        goto out;
    }

    int vsm_fd;
    binding = vivid_binding_linux_create(&vsm_fd VIVID_LOG_ARGS(, print_message, NULL));
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

    efd = epoll_create1(0);
    if (efd < 0) {
        print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not create efd");
        res = -1;
        goto out;
    }

    fcntl(fileno(stdin), F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    if (!add_fd(efd, m_quit_event_fd, QUIT_ID)
        || !add_fd(efd, fileno(stdin), INPUT_ID)
        || !add_fd(efd, vsm_fd, VSM_ID)) {
        res = -1;
        goto out;
    }

    bool quit = false;
    while (!quit) {
        struct epoll_event ev;
        int num_events = epoll_wait(efd, &ev, 1U, -1);
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            print_message(NULL, VIVID_LOG_LEVEL_ERROR, "could not wait on efd");
            res = -1;
            break;
        }

        switch (ev.data.u32) {
        case QUIT_ID:
            quit = true;
            break;
        case INPUT_ID:
            handle_input(application);
            break;
        case VSM_ID:
            vivid_binding_linux_handle_event(binding);
            break;
        default:
            print_message(NULL, VIVID_LOG_LEVEL_ERROR, "unexpected id");
            res = -1;
            break;
        }
    }

out:
    (void)close(efd);
    (void)close(m_quit_event_fd);
    application_destroy(application);
    vivid_binding_linux_destroy(binding);
    print_message(NULL, VIVID_LOG_LEVEL_INFO, "stopped");
    return res;
}
