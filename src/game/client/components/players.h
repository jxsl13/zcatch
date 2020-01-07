/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PLAYERS_H
#define GAME_CLIENT_COMPONENTS_PLAYERS_H
#include <game/client/component.h>

class CPlayers : public CComponent
{
	CTeeRenderInfo m_aRenderInfo[MAX_CLIENTS];
	void RenderPlayer(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CNetObj_PlayerInfo *pPrevInfo,
		const CNetObj_PlayerInfo *pPlayerInfo,
		int ClientID
	);
	void RenderHook(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CNetObj_PlayerInfo *pPrevInfo,
		const CNetObj_PlayerInfo *pPlayerInfo,
		int ClientID
	);
	void RenderHealthBar(vec2 Position, int hp, int armor, int Ammo, int m_Weapon);

	class CHealthBarStartInfo
	{
	private:
		enum
		{
			UNINITIALIZED = -1,
			CHANGED = -2,
		};
		int m_HealthAtStart;
		int m_ArmorAtStart;
		int m_AmmoAtStart;

	public:
		CHealthBarStartInfo() { m_HealthAtStart = UNINITIALIZED; m_ArmorAtStart = UNINITIALIZED; m_AmmoAtStart = UNINITIALIZED; }
		bool HealthHasChanged() { return m_HealthAtStart == CHANGED; }
		bool ArmorHasChanged() { return m_ArmorAtStart == CHANGED; }
		bool AmmoHasChanged() { return m_AmmoAtStart == CHANGED; }
		void Feed(int Health, int Armor, int Ammo) {
			Update(&m_HealthAtStart, Health);
			Update(&m_ArmorAtStart, Armor);
			Update(&m_AmmoAtStart, Ammo);
		}
	private:
		void Update(int* pItem, int NewValue) { 
			NewValue = clamp(NewValue, 0, 10);
			if(*pItem == UNINITIALIZED) *pItem = NewValue;
			if(*pItem != NewValue) *pItem = CHANGED;
		}
	} m_HealthBarStartInfo;

public:
	virtual void OnRender();
	virtual void OnEnterGame();
};

#endif
