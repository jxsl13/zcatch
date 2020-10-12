

#include <string>
#include <sstream>
#include <stdexcept>
#include <game/server/gamecontext.h>
#include "deletionrequest.h"
#include <base/system.h>
 


CRankDeletionRequest::CRankDeletionRequest()
{
    m_DeletionCallback = [](CRankDeletionRequest::DeletionType Type, std::string Nickname){};
    m_Nickname.reserve(MAX_NAME_LENGTH);
    Reset();
    SetConfirmationTimeout(0);
}

// Requires a callback
CRankDeletionRequest::CRankDeletionRequest(cb_delreq_t Callback) : m_DeletionCallback(Callback)
{
    m_Nickname.reserve(MAX_NAME_LENGTH);
    Reset();
    SetConfirmationTimeout(0);
}

void CRankDeletionRequest::SetConfirmationTimeout(int Seconds) noexcept
{
    if (Seconds <= 0)
    {
        m_ConfirmationTimeoutTicks = SERVER_TICK_SPEED * 60;
        return;
    }

    m_ConfirmationTimeoutTicks = SERVER_TICK_SPEED * Seconds;
}

void CRankDeletionRequest::RequestDeletion(DeletionType Type, std::string& Nickname, int CurretTick)
{
    if (Type <= 0 || DeletionType::UPPER_LIMIT <= Type)
    {
        throw std::invalid_argument("invalid DeletionType passed");
    }

    if (CurretTick <= 0)
    {
        throw std::invalid_argument("invalid CurrentTick passed");
    }

    m_Requested = true;
    m_RequestTick = CurretTick;
    m_Nickname = Nickname;

    m_DeletionType = Type;
}

bool CRankDeletionRequest::IsRequestTimeouted(int CurrentTick) const
{
    return m_RequestTick + m_ConfirmationTimeoutTicks < CurrentTick;
}

void CRankDeletionRequest::Confirm(int CurrentTick)
{
    if (!m_Requested)
    {
        throw std::logic_error("You need to request the deletion of a nickname before confirming the deletion.");
    }

    if (IsRequestTimeouted(CurrentTick))
    {
        std::stringstream ss;
        ss << "Confirmation timeout of " << (m_ConfirmationTimeoutTicks / SERVER_TICK_SPEED) << " seconds reached, please request again.";
        auto err = ss.str();
        throw std::logic_error(err);
    }

    m_Confirmed = true;
    m_ConfirmationTick = CurrentTick;
}

void CRankDeletionRequest::Abort(int CurrentTick)
{
    if (!m_Requested)
    {
        throw std::logic_error("You need to request the deletion of a nickname before aborting the deletion.");
    }

    if (IsRequestTimeouted(CurrentTick))
    {
        std::stringstream ss;
        ss << "Abortion timeout of " << (m_ConfirmationTimeoutTicks / SERVER_TICK_SPEED) << " seconds reached, request was aborted automatically.";
        throw std::logic_error(ss.str());
    }

    Reset();
}

void CRankDeletionRequest::ProcessDeletion()
{
    if (m_DeletionType < 0)
    {
        return;
    }

    if (!(m_Requested && m_Confirmed))
    {
        return;
    }

    if (IsRequestTimeouted(m_ConfirmationTick))
    {
        Reset();
        return;
    }

    // call the function that handles the deletion and explicitly copy the nickname value, 
    // in order to avoid data races.
    m_DeletionCallback(m_DeletionType, m_Nickname);
    // deletion was properly handled, reset the state machine to the initial empty state.
    Reset();
}

void CRankDeletionRequest::Reset() noexcept
{
    m_DeletionType = NO_REQUEST;
    m_Requested = false;
    m_RequestTick = 0;

    m_Confirmed = false;
    m_ConfirmationTick = 0;
    m_Nickname = "";
}
