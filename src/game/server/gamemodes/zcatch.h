/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_ZCATCH_H
#define GAME_SERVER_GAMEMODES_ZCATCH_H
#include <game/server/gamecontroller.h>

class CGameControllerZCATCH : public IGameController
{
public:
	CGameControllerZCATCH(class CGameContext *pGameServer);

	// game
	virtual void Tick();
	virtual void DoWincheckRound();
	
	virtual void OnPlayerConnect(class CPlayer *pPlayer);
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer);

	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);

	virtual void DoTeamChange(class CPlayer *pPlayer, int Team, bool DoChatMsg=true);

	virtual void EndRound();
private:

	/**
	 * Used to track, whether we reached the players count needed to
	 * end a round or whether we went below that treshold.
	 */
	int m_PreviousIngamePlayerCount;
	int m_IngamePlayerCount;
	
	
	/**
	 * As EndRound() should only be called from within DoWinCheck(), 
	 * We want to enforce Ending a round in order to revert back to 
	 * a warmup if there are not enough players to end a round.
	 */
	bool m_ForcedEndRound;
};

#endif
