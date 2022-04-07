#include "countdown.h"
#include <iostream>
#include <stdexcept>
#include <string>

VIVID_EVENT_CPP(Countdown, ev_start, m_vsm);

VIVID_STATE_CPP(Countdown, root)
{
    VIVID_SUB_STATE(idle);
    VIVID_SUB_STATE(timing);
    VIVID_SUB_CONDITION(cond_done);
    VIVID_SUB_STATE(ringing);
    VIVID_DEFAULT(idle, VIVID_NO_ACTION);
}

VIVID_STATE_CPP(Countdown, idle)
{
    VIVID_ON_ENTRY(std::cout << "# Welcome to the countdown example. Press enter to trigger the countdown!\n";);
    VIVID_ON_EVENT(ev_start, true, timing, m_counter = 10; std::cout << "# STARTING COUNTDOWN\n";);
}

VIVID_STATE_CPP(Countdown, timing)
{
    VIVID_ON_ENTRY(std::cout << "# " << std::to_string(m_counter) << "\n";);
    VIVID_ON_TIMEOUT(tm_ready, 1.0, true, cond_done, m_counter--;);
}

VIVID_STATE_CPP(Countdown, cond_done)
{
    VIVID_JUMP(m_counter == 0, ringing, VIVID_NO_ACTION);
    VIVID_JUMP(true, timing, VIVID_NO_ACTION);
}

VIVID_STATE_CPP(Countdown, ringing)
{
    VIVID_ON_ENTRY(std::cout << "# SOUNDER ON\n";);
    VIVID_ON_EXIT(std::cout << "# SOUNDER OFF\n";);
    VIVID_ON_TIMEOUT(tm_ringing, 3.0, true, idle, VIVID_NO_ACTION);
}

Countdown::Countdown(vivid_binding_t *binding)
{
    m_vsm = VIVID_CREATE_SM(binding, "countdown", root, 16U, this);
    if (m_vsm == NULL) {
        throw std::runtime_error("could not create vsm");
    }
    vivid_save_uml(m_vsm, "countdown_cpp.puml");
}

Countdown::~Countdown()
{
    vivid_destroy_sm(m_vsm);
}
