#include "countdown.h"
#include <stdio.h>

VIVID_DECLARE_STATE(root);
VIVID_DECLARE_STATE(idle);
VIVID_DECLARE_STATE(timing);
VIVID_DECLARE_STATE(cond_done);
VIVID_DECLARE_STATE(ringing);

VIVID_EVENT_PUBLIC(countdown_t, countdown, ev_start, vsm);

VIVID_STATE(countdown_t, root)
{
    VIVID_SUB_STATE(idle);
    VIVID_SUB_STATE(timing);
    VIVID_SUB_CONDITION(cond_done);
    VIVID_SUB_STATE(ringing);
    VIVID_DEFAULT(idle, VIVID_NO_ACTION);
}

VIVID_STATE(countdown_t, idle)
{
    VIVID_ON_ENTRY(printf("# Welcome to the countdown example. Press enter to trigger the countdown!\n"););
    VIVID_ON_EVENT(ev_start, true, timing, me->counter = 10; printf("# STARTING COUNTDOWN\n"););
}

VIVID_STATE(countdown_t, timing)
{
    VIVID_ON_ENTRY(printf("# %d\n", me->counter););
    VIVID_ON_TIMEOUT(tm_ready, 1.0, true, cond_done, me->counter--;);
}

VIVID_STATE(countdown_t, cond_done)
{
    VIVID_JUMP(me->counter == 0, ringing, VIVID_NO_ACTION);
    VIVID_JUMP(true, timing, VIVID_NO_ACTION);
}

VIVID_STATE(countdown_t, ringing)
{
    VIVID_ON_ENTRY(printf("# SOUNDER ON\n"););
    VIVID_ON_EXIT(printf("# SOUNDER OFF\n"););
    VIVID_ON_TIMEOUT(tm_ringing, 3.0, true, idle, VIVID_NO_ACTION);
}

bool countdown_init(countdown_t *me, vivid_binding_t *binding)
{
    me->vsm = VIVID_CREATE_SM(binding, "countdown", root, 16U, me);
    if (me->vsm == NULL) {
        return false;
    }
    vivid_save_uml(me->vsm, "countdown.puml");
    return true;
}

void countdown_deinit(countdown_t *me)
{
    vivid_destroy_sm(me->vsm);
}
