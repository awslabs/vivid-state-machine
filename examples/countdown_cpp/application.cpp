#include "../application.h"
#include "countdown.h"

struct Application {
public:
    Application(vivid_binding_t *binding)
        : m_binding(binding)
        , m_countdown(binding)
    {
    }
    void trigger() { m_countdown.ev_start(); }

private:
    vivid_binding_t *m_binding;
    Countdown m_countdown;
};

void application_destroy(void *application)
{
    Application *me = (Application *)application;
    if (me == NULL) {
        return;
    }
    delete me;
}

void *application_create(vivid_binding_t *binding)
{
    try {
        return new Application(binding);
    } catch (...) {
        return NULL;
    }
}

void application_trigger(void *application, const char *value, size_t len)
{
    Application *me = (Application *)application;
    (void)value;
    (void)len;
    me->trigger();
}
