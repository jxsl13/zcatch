/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "projectile.h"
#include  "game/server/player.h"
#include "engine/shared/config.h"


CProjectile::CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Type = Type;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_OwnerTeam = GameServer()->m_apPlayers[Owner]->GetTeam();
	m_Force = Force;
	m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	// at this point we cannot yet loose the owner, thus the m_Owner value here is correct.
	m_IsPunished = GameServer()->m_apPlayers[m_Owner] && GameServer()->m_apPlayers[m_Owner]->GetPunishmentLevel() > CPlayer::PunishmentLevel::NONE;

	GameWorld()->InsertEntity(this);
	FillValidTargets();
}

void CProjectile::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CProjectile::LoseOwner()
{
	if(m_OwnerTeam == TEAM_BLUE)
		m_Owner = PLAYER_TEAM_BLUE;
	else
		m_Owner = PLAYER_TEAM_RED;
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	// TODO: make projectile stop moving when game's paused.
	if (GameServer()->m_pController->IsGamePaused())
	{
		return;
	}
	
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &CurPos, 0);
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *TargetChr = GameServer()->m_World.IntersectCharacter(PrevPos, CurPos, 6.0f, CurPos, OwnerChar);
	
	CPlayer *Owner = nullptr;
	if(m_Owner >= 0 && m_Owner < MAX_CLIENTS)
		Owner =	GameServer()->m_apPlayers[m_Owner];

	m_LifeSpan--;

	if(TargetChr || Collide || m_LifeSpan < 0 || GameLayerClipped(CurPos))
	{
		if((m_LifeSpan >= 0 || m_Weapon == WEAPON_GRENADE) && !m_IsPunished)
			GameServer()->CreateSound(CurPos, m_SoundImpact);

		if(m_Explosive)
		{
			// if the owner respawns before the projectile hits, invalidate the projectile
			if (Owner && Owner->m_LastRespawnedTick <= m_StartTick && !m_IsPunished)
			{
				GameServer()->CreateExplosion(CurPos, m_Owner, m_Weapon, m_Damage, &m_ValidTargets);
			}
		}
		else if(TargetChr && IsValidTarget(TargetChr->GetPlayer()->GetCID()))
			TargetChr->TakeDamage(m_Direction * max(0.001f, m_Force), m_Direction*-1, m_Damage, m_Owner, m_Weapon);	

		GameServer()->m_World.DestroyEntity(this);
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
	{
		return;
	}
	else if(SnappingClient != m_Owner && m_IsPunished)
	{
		// don't send projectile to players other 
		// than the owner, if the owner if being punished
		return;
	}

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}

void CProjectile::FillValidTargets()
{	
	// if owner is being punished, don't add any valid enemies to the valid targets.
	if (m_IsPunished)
		return;
	
	class CGameContext *pGameServer = GameServer();
	if (pGameServer)
	{	
		
		class CPlayer *pPlayer;		
		int tmpID;
		for (int i : pGameServer->PlayerIDs())
		{
			pPlayer = pGameServer->m_apPlayers[i];

			if (pPlayer && pPlayer->IsNotCaught())
			{
				tmpID = pPlayer->GetCID();

				// in vicinity
				if(pPlayer->GetCharacter() && 
					(distance(m_Pos, pPlayer->GetCharacter()->GetPos()) <= g_Config.m_SvSprayProtectionRadius))
				{
					// inserts if not already a valid target
					m_ValidTargets.insert(tmpID);
				} 
			}
			pPlayer = nullptr;
		}

		class CPlayer *pOwner = GameServer()->m_apPlayers[m_Owner];
		class CCharacter *pOwnerCharacter = pOwner ? pOwner->GetCharacter() : nullptr;

		// add hooked player to valid targets
		if (pOwnerCharacter)
		{
			tmpID = pOwnerCharacter->GetHookedPlayer();
			m_ValidTargets.insert(tmpID);
		}
	}
}

bool CProjectile::IsValidTarget(int TargetID)
{
	return m_ValidTargets.count(TargetID) > 0;
}
