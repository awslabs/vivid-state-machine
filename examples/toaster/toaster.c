#include "toaster.h"
#include <stdio.h>

VIVID_DECLARE_STATE(root);
VIVID_DECLARE_STATE(off);
VIVID_DECLARE_STATE(on);

VIVID_EVENT_PUBLIC(toaster_t, toaster, ev_button_press, vsm);

VIVID_STATE(toaster_t, root)
{
    VIVID_SUB_STATE(off);
    VIVID_SUB_STATE(on);
    VIVID_DEFAULT(off, VIVID_NO_ACTION);
}

VIVID_STATE(toaster_t, off)
{
    VIVID_ON_ENTRY(printf("Toaster is off. Press enter to start!\n"););
    VIVID_ON_EVENT(ev_button_press, true, on, VIVID_NO_ACTION);
}

VIVID_STATE(toaster_t, on)
{
    VIVID_ON_ENTRY(printf("Toaster is on. Press enter to cancel, or wait for it to pop up...\n"););
    VIVID_ON_EVENT(ev_button_press, true, off, printf("Toasting cancelled\n"););
    VIVID_ON_TIMEOUT(tm_popup, 10.0, true, off, printf("Toast is ready!\n"););
}

bool toaster_init(toaster_t *me, vivid_binding_t *binding)
{
    me->vsm = VIVID_CREATE_SM(binding, "toaster", root, 16U, me);
    if (me->vsm == NULL) {
        return false;
    }
    vivid_save_uml(me->vsm, "toaster.puml");
    return true;
}

void toaster_deinit(toaster_t *me)
{
    vivid_destroy_sm(me->vsm);
}
