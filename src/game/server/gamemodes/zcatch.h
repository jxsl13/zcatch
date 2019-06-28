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
	virtual void DoWincheckRound();
};

#endif
