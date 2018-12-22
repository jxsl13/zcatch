/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
// #include <game/generated/protocol.h>
// #include <game/generated/client_data.h>

#include <game/client/gameclient.h>

#include "announcers.h"

void CAnnouncers::OnReset()
{
	m_AnnouncerTime = 0.f;
	m_AnnouncerDelay = 2.5f;
	m_AnnouncerSpeed = 4.0f;
}

void CAnnouncers::Announce(const char* Message, const char* Legend, float AnnouncerSpeed)
{
	if(!g_Config.m_ClAnnouncers)
		return;
		
	CTextCursor Cursor;
	
	str_copy(m_aAnnouncerText, Message, sizeof(m_aAnnouncerText));
	str_copy(m_aAnnouncerLegend, Legend, sizeof(m_aAnnouncerLegend));
	TextRender()->SetCursor(&Cursor, 0, 0, 12.0f, TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
	TextRender()->TextEx(&Cursor, m_aAnnouncerText, -1);
	m_AnnouncerRenderOffset = 150*Graphics()->ScreenAspect()-Cursor.m_X/2;
	m_AnnouncerTime = Client()->LocalTime()+m_AnnouncerDelay;
	m_AnnouncerSpeed = AnnouncerSpeed;
}

void CAnnouncers::OnRender()
{
	Graphics()->MapScreen(0, 0, 300*Graphics()->ScreenAspect(), 300);
		
	if(Client()->LocalTime() < m_AnnouncerTime)
	{
		float Factor = ((Client()->LocalTime()+m_AnnouncerDelay) - m_AnnouncerTime) / 0.3f;
		int txtSize = 20.0f*min(1.0f, Factor) + 45.0f*(1.0f-min(1.0f, Factor));
		char Text[1024];
		str_copy(Text, m_aAnnouncerText, sizeof(Text));
		int Pos = (int)(Factor * m_AnnouncerSpeed); // Originally 4.0f
		if(Pos < str_length(Text))
		{
			char FirstPart[512];
			char Letter;
			char SecondPart[512];
			str_copy(FirstPart, Text, sizeof(FirstPart));
			FirstPart[Pos] = 0;
			if(str_length(Text) == Pos)
				str_copy(SecondPart, "", sizeof(SecondPart));
			else
				str_copy(SecondPart, Text + Pos + 1, sizeof(SecondPart));
			Letter = Text[Pos];
			
			str_format(Text, sizeof(Text), "%s^h%c^r%s", FirstPart, Letter, SecondPart);
		}
		
		int ClTextColors = g_Config.m_ClTextColors;
		g_Config.m_ClTextColors = 1;
		
		float w = TextRender()->TextWidth(0, txtSize, Text, -1);
		
		if(!g_Config.m_ClAnnouncersShadows)
			TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f);


		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 0, 0, 12.0f, TEXTFLAG_STOP_AT_END);
		TextRender()->SetCursor(&Cursor, m_AnnouncerRenderOffset, 40.0f, txtSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = 300*Graphics()->ScreenAspect()-m_AnnouncerRenderOffset;

		// Draw the shadows
		if(false && g_Config.m_ClAnnouncersShadows)
		{
			TextRender()->TextColor(0.5f, 0.05f, 0.05f, 1.0f);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2, Cursor.m_Y, txtSize, m_aAnnouncerText, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2, Cursor.m_Y + 1, txtSize, m_aAnnouncerText, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2, Cursor.m_Y - 1, txtSize, m_aAnnouncerText, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2 + 2, Cursor.m_Y, txtSize, m_aAnnouncerText, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2 + 2, Cursor.m_Y + 1, txtSize, m_aAnnouncerText, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2 + 2, Cursor.m_Y - 1, txtSize, m_aAnnouncerText, -1);
		}
		/*
		else
		{
			TextRender()->TextColor(0.9f, 0.1f, 0.1f, 1.0f);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2, Cursor.m_Y, txtSize, Text, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2, Cursor.m_Y + 1, txtSize, Text, -1);
			TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2 + 1, Cursor.m_Y + 1, txtSize, Text, -1);			
		}
		*/
		
		// Draw the plain text
		TextRender()->TextColor(0.9f, 0.1f, 0.1f, 1.0f);
		TextRender()->Text(0, 150*Graphics()->ScreenAspect() - w/2 + 1, Cursor.m_Y, txtSize, Text, -1);
		
		// Draw the legend
		if(g_Config.m_ClAnnouncersLegend && m_aAnnouncerLegend[0])
		{
			TextRender()->Text(0, 200*Graphics()->ScreenAspect() - w/2, Cursor.m_Y + 25, 9.0f, m_aAnnouncerLegend, -1);
		}
		
		// TextRender()->TextEx(&Cursor, m_aAnnouncerText, -1);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
		g_Config.m_ClTextColors = ClTextColors;
	}
}
