#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include <vivid/sm.h>

struct Countdown {
public:
    Countdown(vivid_binding_t *binding);
    ~Countdown();
    VIVID_DECLARE_EVENT_CPP(ev_start);

private:
    vivid_sm_t *m_vsm;
    int m_counter {};
    VIVID_DECLARE_STATE_CPP(root);
    VIVID_DECLARE_STATE_CPP(idle);
    VIVID_DECLARE_STATE_CPP(timing);
    VIVID_DECLARE_STATE_CPP(cond_done);
    VIVID_DECLARE_STATE_CPP(ringing);
};

#endif
