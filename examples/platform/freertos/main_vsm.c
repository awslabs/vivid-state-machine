#include "../../application.h"
#include <stdio.h>
#include <vivid/binding/freertos.h>

#define VSM_EVENT_MASK 1U

static void *m_application;

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
    TickType_t time = xTaskGetTickCount();
    printf("%s%u | %s | %s%s\n", level_color, (unsigned)time, level_string, message, (*level_string != '\0') ? "\x1B[0m" : "");
}

static void error_hook(void *app, vivid_error_t error)
{
    (void)app;
    (void)error;
    portDISABLE_INTERRUPTS();
    for (;;) { }
}

void main_uart_receive(char ch)
{
    static char buf[28U] = "input: ";
    static int offset = 7;
    if (ch == '\r') {
        printf("\n");
        buf[offset] = '\0';
        print_message(NULL, VIVID_LOG_LEVEL_INFO, buf);
        application_trigger(m_application, &buf[7U], offset - 7U);
        offset = 7;
        return;
    }
    printf("%c", ch);
    if (offset < (int)(sizeof(buf) - 1U)) {
        buf[offset++] = ch;
    }
}

static void main_task(void *params)
{
    vivid_binding_t *binding = *(vivid_binding_t **)params;
    for (;;) {
        uint32_t notify_mask;
        (void)xTaskNotifyWait(0U, UINT32_MAX, &notify_mask, portMAX_DELAY);
        if (notify_mask & VSM_EVENT_MASK) {
            vivid_binding_freertos_handle_event(binding);
        }
    }
}

void main_vsm(void)
{
    static vivid_binding_t *binding;
    TaskHandle_t task_handle = NULL;

    if (xTaskCreate(main_task, "main", 1024U, &binding, 1, &task_handle) != pdPASS) {
        goto out;
    }

    binding = vivid_binding_freertos_create(task_handle, VSM_EVENT_MASK VIVID_LOG_ARGS(, print_message, NULL));
    if (binding == NULL) {
        goto out;
    }
    binding->error_hook = error_hook;

    m_application = application_create(binding);
    if (m_application == NULL) {
        goto out;
    }

    vTaskStartScheduler();

out:
    application_destroy(m_application);
    vivid_binding_freertos_destroy(binding);
    if (task_handle != NULL) {
        vTaskDelete(task_handle);
    }
}
