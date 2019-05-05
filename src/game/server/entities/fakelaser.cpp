/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include "fakelaser.h"
#include <iostream>

CFakeLaser::CFakeLaser(CGameWorld *pGameWorld, vec2 FromPos, vec2 ToPos, int Owner, int visibleFor)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER)
{	
	m_Pos = ToPos;
	m_From = FromPos;
	m_Owner = Owner;
	m_Traget = visibleFor;
	m_EvalTick = Server()->Tick();
	GameWorld()->InsertEntity(this);
}

void CFakeLaser::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CFakeLaser::Tick(){ }

void CFakeLaser::TickPaused(){ }

void CFakeLaser::Snap(int SnappingClient)
{
	/**
	 * We want only the m_Target to see the laser
	 * otherwise do nothing
	 */
	if (SnappingClient != m_Traget)
	{
		return;
	}
	/*
	 * Testing, whether velocity changes much of the correct laser positioning
	 */
	vec2 velocity(0.0, 0.0);

	/**
	 * getting velocity of the laser owner for some movement compensation
	 */
	CCharacterCore *pCharCore = (&GameWorld()->m_Core)->m_apCharacters[m_Owner];

	if (pCharCore)
	{
		velocity = pCharCore->m_Vel;
	}

	/**
	 * Add a new laser object to the snapshot
	 */
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_Owner, sizeof(CNetObj_Laser)));
	if(!pObj)
	{
		return;
	}
	
	/**
	 * Set laser start and end position
	 */
	pObj->m_X = (int)(m_Pos.x + velocity.x);
	pObj->m_Y = (int)(m_Pos.y + velocity.y);
	pObj->m_FromX = (int)(m_From.x + velocity.x);
	pObj->m_FromY = (int)(m_From.y + velocity.y);
	pObj->m_StartTick = Server()->Tick();
	
	/**
	 * Destroy laser entity.
	 */
	Reset();
}
