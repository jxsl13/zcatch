#ifndef DELETION_REQUEST_H
#define DELETION_REQUEST_H

#include <string>
#include <functional>

class CRankDeletionRequest
{
   public:

    enum DeletionType
    {
        NO_REQUEST = 0,
        DELETION = 1,
        SCORE_RESET = 2,
        SCORE_AND_WIN_RESET = 3,
        UPPER_LIMIT,
    };

    using cb_delreq_t = std::function<void(DeletionType, std::string)>;

   private:
    // Step one: the admin requests a deletion of some rank
    bool m_Requested;
    int m_RequestTick;
    DeletionType m_DeletionType;
    std::string m_Nickname;

    // the admin has to confirm the deletion within the
    int m_ConfirmationTimeoutTicks;

    // step two: the admin confirms the deletion within the expiration time slot.
    bool m_Confirmed;
    int m_ConfirmationTick;

    // this callback is executed when the deletion request is properly requested and confirmed.
    cb_delreq_t m_DeletionCallback;

   public:

    CRankDeletionRequest();

    // Requires a deletion callback handler that is executed when the 
    // deletion request is properly requested and confirmed afterwards within the appropriate 
    // timout time.
    CRankDeletionRequest(std::function<void(DeletionType, std::string)> Callback);


    // simple setter to change the default 60 seconds value of the confiration timeout.
    void SetConfirmationTimeout(int Seconds) noexcept;

    // First step for deleting an account
    void RequestDeletion(DeletionType Type, std::string& Nickname, int CurretTick);

    // returns true if the request timed out nad neds to be retried again.
    bool IsRequestTimeouted(int CurrentTick) const;

    // Throws errors depending on state.
    // second step, confirm the deletion within the given time.
    void Confirm(int CurrentTick);

    // Abort the deletion request of a pending request
    void Abort(int CurrentTick);

    std::string Nickname() const { return m_Nickname; }

    // can be executed every second in the game tick functon.
    // actually processed the request to delete a rank.
    void ProcessDeletion();

    // resets the internal values to the state where no deletion was requested.
    void Reset() noexcept;
};

#endif // DELETION_REQUEST_H
