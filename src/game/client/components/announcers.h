/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ANNOUNCERS_H
#define GAME_CLIENT_COMPONENTS_ANNOUNCERS_H
#include <game/client/component.h>

class CAnnouncers : public CComponent
{
	// broadcasts
	char m_aAnnouncerText[1024];
	char m_aAnnouncerLegend[128];
	int64 m_AnnouncerTime;
	float m_AnnouncerRenderOffset;
	float m_AnnouncerDelay;
	float m_AnnouncerSpeed;

public:
	virtual void OnReset();
	virtual void OnRender();
	// virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual void Announce(const char* Message, const char* Legend = "", float AnnouncerSpeed = 4.0f);
};

#endif
