#include <string.h>

#include <base/color.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>
#include <engine/graphics.h>

#include <generated/client_data.h>

#include <game/version.h>
#include <game/client/ui.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/client/animstate.h>

#include "binds.h"
#include "menus.h"
#include "skins.h"
#include "items.h"

#include <game/client/teecomp.h>

#define TEECOMP_VERSION "1.0"

void CMenus::RenderRgbSliders(CUIRect* pMainView, CUIRect* pButton, int &r, int &g, int &b, bool Enabled)
{
	const char *pLabels[] = {"R.", "G.", "B."};
	int *pColorSlider[3] = {&r, &g, &b};
	for(int i=0; i<3; i++)
	{
		CUIRect Text;
		pMainView->HSplitTop(19.0f, pButton, pMainView);
		pButton->VMargin(15.0f, pButton);
		pButton->VSplitLeft(30.0f, &Text, pButton);
		pButton->VSplitRight(5.0f, pButton, 0);
		pButton->HSplitTop(4.0f, 0, pButton);

		if(Enabled)
		{
			float k = (*pColorSlider[i]) / 255.0f;
			k = DoScrollbarH(pColorSlider[i], pButton, k);
			*pColorSlider[i] = (int)(k*255.0f);
		}
		else
			DoScrollbarH(pColorSlider[i], pButton, 0);
		UI()->DoLabel(&Text, pLabels[i], 15.0f, CUI::ALIGN_LEFT);
	}
}

void CMenus::RenderSettingsTeecomp(CUIRect MainView)
{
	CUIRect Button;
	static int s_SettingsPage = 0;
						
	// Tabs (Teecomp pattern)
	MainView.HSplitBottom(80.0f, &MainView, 0);
	MainView.HSplitTop(14.0f, 0, &MainView);

	// if(s_SettingsPage != 3)
	{
		MainView.HSplitBottom(20.0f, 0, &Button);
		Button.VSplitLeft(MainView.w/3, &Button, 0);
		static CButtonContainer s_DefaultButton;
		if(DoButton_Menu(&s_DefaultButton, Localize("Reset to defaults"), 0, &Button))
			CTeecompUtils::ResetConfig();

		MainView.HSplitBottom(10.0f, &MainView, &Button);
		MainView.HSplitBottom(10.0f, &MainView, &Button);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("Teeworlds %s with TeeComp %s"), GAME_VERSION, TEECOMP_VERSION);
		UI()->DoLabel(&Button, aBuf, 10.0f, CUI::ALIGN_RIGHT);
		MainView.HSplitBottom(10.0f, &MainView, 0);
	}

	// render background
	CUIRect Tabbar;
	MainView.HSplitTop(24.0f, &Tabbar, &MainView);

	const char *pTabs[] = { Localize("Skins"), Localize("Stats"), Localize("Misc")/*, Localize("About")*/ };
	int NumTabs = (int)(sizeof(pTabs)/sizeof(*pTabs));

	RenderTools()->DrawUIRect(&MainView, vec4(0.0f, 0.0f, 0.0f, 0.5f), CUI::CORNER_ALL, 10.0f);
	for(int i=0; i<NumTabs; i++)
	{
		Tabbar.VSplitLeft(10.0f, &Button, &Tabbar);
		Tabbar.VSplitLeft(80.0f, &Button, &Tabbar);
				
		static CButtonContainer s_Buttons[3];
		if (DoButton_MenuTabTop(&s_Buttons[i], pTabs[i], s_SettingsPage == i, &Button, 1.0f, 1.0f, CUI::CORNER_T, 5.0f, 0.25f))
			s_SettingsPage = i;
	}
	MainView.Margin(10.0f, &MainView);
	
	if(s_SettingsPage == 0)
		RenderSettingsTeecompSkins(MainView);
	else if(s_SettingsPage == 1)
		RenderSettingsTeecompStats(MainView);
	else if(s_SettingsPage == 2)
		RenderSettingsTeecompMisc(MainView);
	// else if(s_SettingsPage == 3)
	// 	RenderSettingsTeecompAbout(MainView);
}

// TODO Teecomp port
int CMenus::DoButton_ListRow(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	if(Checked)
	{
		CUIRect sr = *pRect;
		sr.Margin(1.5f, &sr);
		RenderTools()->DrawUIRect(&sr, vec4(1,1,1,0.5f), CUI::CORNER_ALL, 4.0f);
	}
	UI()->DoLabel(pRect, pText, pRect->h*ms_FontmodHeight, CUI::ALIGN_LEFT);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}
// TODO Teecomp port
void CMenus::UiDoKeybinder(CKeyInfo& pKey, CUIRect* r)
{
	CUIRect Label, Button;
	r->HSplitTop(20.0f, &Button, r);
	Button.VSplitRight(5.0f, &Button, 0);
	Button.VSplitLeft(180.0f, &Label, &Button);

	UI()->DoLabel(&Label, pKey.m_Name, 14.0f, CUI::ALIGN_LEFT);
	int OldId = pKey.m_KeyId, OldModifier = pKey.m_Modifier, NewModifier;
	int NewId = DoKeyReader(&pKey.m_BC, &Button, OldId, OldModifier, &NewModifier);
	if(NewId != OldId || NewModifier != OldModifier)
	{
		if(OldId != 0 || NewId == 0)
			m_pClient->m_pBinds->Bind(OldId, OldModifier, "");
		if(NewId != 0)
			m_pClient->m_pBinds->Bind(NewId, NewModifier, pKey.m_pCommand);
	}
	r->HSplitTop(5.0f, 0, r);
}


void CMenus::RenderSettingsTeecompSkins(CUIRect MainView)
{
	CUIRect Button, LeftView, RightView;
	MainView.VSplitLeft(MainView.w/2, &LeftView, &RightView);

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcColoredTeesMethod, Localize("Enemy based colors"), g_Config.m_TcColoredTeesMethod, &Button))
		g_Config.m_TcColoredTeesMethod ^= 1;

	// Colors team 1

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), Localize("Use DM colors for team %s"), (g_Config.m_TcColoredTeesMethod)?Localize("mates"):"1");
	if(DoButton_CheckBox(&g_Config.m_TcDmColorsTeam1, aBuf, g_Config.m_TcDmColorsTeam1, &Button))
		g_Config.m_TcDmColorsTeam1 ^= 1;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	UI()->DoLabel(&Button, (g_Config.m_TcColoredTeesMethod)?Localize("Team mates"):Localize("Team 1"), 14.0f, CUI::ALIGN_LEFT);
	int r1, g1, b1, r2, g2, b2;
	r1 = g_Config.m_TcColoredTeesTeam1>>16;
	g1 = (g_Config.m_TcColoredTeesTeam1>>8)&0xff;
	b1 = g_Config.m_TcColoredTeesTeam1&0xff;
	RenderRgbSliders(&LeftView, &Button, r1, g1, b1, !g_Config.m_TcDmColorsTeam1);
	g_Config.m_TcColoredTeesTeam1 = (r1<<16) + (g1<<8) + b1;

	// teecomp modification: now using HSL picker to get realistic values
	// {
	// 	bool Modified;
	// 	vec3 OriginalHSLvecColor = RgbToHsl(vec3(r1/255.f, g1/255.f, b1/255.f));
	// 	int OriginalHSLColor = (int(OriginalHSLvecColor.h*255)<<16) + (int(OriginalHSLvecColor.s*255)<<8) + int(OriginalHSLvecColor.l*255);
	// 	ivec4 HSLAColor = RenderHSLPicker(LeftView, OriginalHSLColor, false, Modified);
	// 	if(Modified)
	// 	{
	// 		vec3 HSLColor = vec3(HSLAColor.x/255.f, HSLAColor.y/255.f, HSLAColor.z/255.f);
	// 		vec3 RGBColor = HslToRgb(HSLColor);
	// 		g_Config.m_TcColoredTeesTeam1 = (int(255*RGBColor.r)<<16) + (int(255*RGBColor.g)<<8) + int(255*RGBColor.b);
	// 	}
	// }

	const CSkins::CSkin *s = m_pClient->m_pSkins->Get(max(0, m_pClient->m_pSkins->Find(g_Config.m_TcForcedSkin1, false)));
	CTeeRenderInfo Info;
	if(!g_Config.m_TcDmColorsTeam1)
	{
		for(int i = 0; i < NUM_SKINPARTS; i++)
		{
			Info.m_aTextures[i] = s->m_apParts[i]->m_ColorTexture;
			vec3 RGBColor = vec3(r1/255.0f, g1/255.0f, b1/255.0f);
			vec3 HSLColor = RgbToHsl(RGBColor);
			vec3 HSLColorFiltered = m_pClient->m_pSkins->GetBasicTeamColor(HSLColor);
			vec3 RGBColorFiltered = HslToRgb(HSLColorFiltered);
			Info.m_aColors[i] = vec4(RGBColorFiltered.r, RGBColorFiltered.g, RGBColorFiltered.b, 1.0f);
		}
		// Info.m_ColorBody = vec4(r1/255.0f, g1/255.0f, b1/255.0f, 1.0f);
		// Info.m_ColorFeet = vec4(r1/255.0f, g1/255.0f, b1/255.0f, 1.0f);
	}
	else
	{
		for(int i = 0; i < NUM_SKINPARTS; i++)
		{
			Info.m_aTextures[i] = s->m_apParts[i]->m_OrgTexture;
			Info.m_aColors[i] = vec4(1.0f, 1.0f, 1.0f, 1.0f);;
		}
		// Info.m_ColorBody = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		// Info.m_ColorFeet = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	Info.m_Size = UI()->Scale()*50.0f;

	Button.HSplitTop(70.0f, 0, &Button);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, 0, vec2(1, 0), vec2(Button.x, Button.y+Button.h/2));
	LeftView.HSplitTop(50.0f, 0, &LeftView);

///////////////////////////////////////////////////////
// teecomp debug, remove!!!
// return;

	// Colors team 2

	RightView.HSplitTop(20.0f, 0, &RightView);
	RightView.HSplitTop(20.0f, &Button, &RightView);
	str_format(aBuf, sizeof(aBuf), Localize("Use DM colors for %s"), (g_Config.m_TcColoredTeesMethod)?Localize("enemies"):Localize("team 2"));
	if(DoButton_CheckBox(&g_Config.m_TcDmColorsTeam2, aBuf, g_Config.m_TcDmColorsTeam2, &Button))
		g_Config.m_TcDmColorsTeam2 ^= 1;

	RightView.HSplitTop(20.0f, &Button, &RightView);
	UI()->DoLabel(&Button, (g_Config.m_TcColoredTeesMethod)?Localize("Enemies"):Localize("Team 2"), 14.0f, CUI::ALIGN_LEFT);
	r2 = g_Config.m_TcColoredTeesTeam2>>16;
	g2 = (g_Config.m_TcColoredTeesTeam2>>8)&0xff;
	b2 = g_Config.m_TcColoredTeesTeam2&0xff;
	RenderRgbSliders(&RightView, &Button, r2, g2, b2, !g_Config.m_TcDmColorsTeam2);
	g_Config.m_TcColoredTeesTeam2 = (r2<<16) + (g2<<8) + b2;

	s = m_pClient->m_pSkins->Get(max(0, m_pClient->m_pSkins->Find(g_Config.m_TcForcedSkin2, false)));
	if(!g_Config.m_TcDmColorsTeam2)
	{
		for(int i = 0; i < NUM_SKINPARTS; i++)
		{
			Info.m_aTextures[i] = s->m_apParts[i]->m_ColorTexture;
			Info.m_aColors[i] = vec4(r2/255.0f, g2/255.0f, b2/255.0f, 1.0f);
		}
	}
	else
	{
		for(int i = 0; i < NUM_SKINPARTS; i++)
		{
			Info.m_aTextures[i] = s->m_apParts[i]->m_OrgTexture;
			Info.m_aColors[i] = vec4(1.0f, 1.0f, 1.0f, 1.0f);;
		}
	}

	Button.HSplitTop(70.0f, 0, &Button);
	RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, 0, vec2(1, 0), vec2(Button.x, Button.y+Button.h/2));
	RightView.HSplitTop(50.0f, 0, &RightView);

	// Force skins team 1

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcForcedSkinsMethod, Localize("Enemy based skins"), g_Config.m_TcForcedSkinsMethod, &Button))
		g_Config.m_TcForcedSkinsMethod ^= 1;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	str_format(aBuf, sizeof(aBuf), Localize("Force team %s/FFA skins"), (g_Config.m_TcForcedSkinsMethod)?Localize("mates"):"1");
	if(DoButton_CheckBox(&g_Config.m_TcForceSkinTeam1, aBuf, g_Config.m_TcForceSkinTeam1, &Button))
		g_Config.m_TcForceSkinTeam1 ^= 1;

	CUIRect SkinSelection, Scroll;
	LeftView.Margin(10.0f, &SkinSelection);

	SkinSelection.HSplitTop(20, &Button, &SkinSelection);
	RenderTools()->DrawUIRect(&Button, vec4(1,1,1,0.25f), CUI::CORNER_T, 5.0f); 
	UI()->DoLabel(&Button, Localize("Forced skin"), 14.0f, CUI::ALIGN_CENTER);

	RenderTools()->DrawUIRect(&SkinSelection, vec4(0,0,0,0.15f), 0, 0);
	SkinSelection.VSplitRight(15, &SkinSelection, &Scroll);

	CUIRect List = SkinSelection;
	List.HSplitTop(20, &Button, &List);
	
	int Num = (int)(SkinSelection.h/Button.h);
	static float Scrollvalue = 0;
	static int Scrollbar = 0;
	Scroll.HMargin(5.0f, &Scroll);
	Scrollvalue = DoScrollbarV(&Scrollbar, &Scroll, Scrollvalue);

	int Start = (int)((m_pClient->m_pSkins->Num()-Num)*Scrollvalue);
	if(Start < 0)
		Start = 0;

	for(int i=Start; i<Start+Num && i<m_pClient->m_pSkins->Num(); i++)
	{
		const CSkins::CSkin *s = m_pClient->m_pSkins->Get(i);

		str_format(aBuf, sizeof(aBuf), "%s", s->m_aName);
		int Selected = 0;
		if(str_comp(s->m_aName, g_Config.m_TcForcedSkin1) == 0)
			Selected = 1;

		if(DoButton_ListRow(s+m_pClient->m_pSkins->Num(), "", Selected, &Button))
			str_copy(g_Config.m_TcForcedSkin1, s->m_aName, sizeof(g_Config.m_TcForcedSkin1));

		Button.VMargin(5.0f, &Button);
		Button.HSplitTop(1.0f, 0, &Button);
		UI()->DoLabel(&Button, aBuf, 14.0f, CUI::ALIGN_LEFT);

		List.HSplitTop(20.0f, &Button, &List);
	}

	// Forced skin team 2

	RightView.HSplitTop(20.0f, 0, &RightView);
	RightView.HSplitTop(20.0f, &Button, &RightView);
	str_format(aBuf, sizeof(aBuf), Localize("Force %s skins"), (g_Config.m_TcForcedSkinsMethod)?Localize("enemies"):Localize("team 2"));
	if(DoButton_CheckBox(&g_Config.m_TcForceSkinTeam2, aBuf, g_Config.m_TcForceSkinTeam2, &Button))
		g_Config.m_TcForceSkinTeam2 ^= 1;

	RightView.Margin(10.0f, &SkinSelection);

	SkinSelection.HSplitTop(20, &Button, &SkinSelection);
	RenderTools()->DrawUIRect(&Button, vec4(1,1,1,0.25f), CUI::CORNER_T, 5.0f); 
	UI()->DoLabel(&Button, Localize("Forced skin"), 14.0f, CUI::ALIGN_CENTER);

	RenderTools()->DrawUIRect(&SkinSelection, vec4(0,0,0,0.15f), 0, 0);
	SkinSelection.VSplitRight(15, &SkinSelection, &Scroll);

	List = SkinSelection;
	List.HSplitTop(20, &Button, &List);
	
	Num = (int)(SkinSelection.h/Button.h);
	static float Scrollvalue2 = 0;
	static int Scrollbar2 = 0;
	Scroll.HMargin(5.0f, &Scroll);
	Scrollvalue2 = DoScrollbarV(&Scrollbar2, &Scroll, Scrollvalue2);

	Start = (int)((m_pClient->m_pSkins->Num()-Num)*Scrollvalue2);
	if(Start < 0)
		Start = 0;

	for(int i=Start; i<Start+Num && i<m_pClient->m_pSkins->Num(); i++)
	{
		const CSkins::CSkin *s = m_pClient->m_pSkins->Get(i);

		str_format(aBuf, sizeof(aBuf), "%s", s->m_aName);
		int Selected = 0;
		if(str_comp(s->m_aName, g_Config.m_TcForcedSkin2) == 0)
			Selected = 1;

		if(DoButton_ListRow(s+m_pClient->m_pSkins->Num(), "", Selected, &Button))
			str_copy(g_Config.m_TcForcedSkin2, s->m_aName, sizeof(g_Config.m_TcForcedSkin2));

		Button.VMargin(5.0f, &Button);
		Button.HSplitTop(1.0f, 0, &Button);
		UI()->DoLabel(&Button, aBuf, 14.0f, CUI::ALIGN_LEFT);

		List.HSplitTop(20.0f, &Button, &List);
	}
}

void CMenus::RenderSettingsTeecompStats(CUIRect MainView)
{
	CUIRect Button, LeftView;

	MainView.VSplitLeft(MainView.w/2, &LeftView, &MainView);
	LeftView.VSplitRight(15.0f, &LeftView, 0);

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "%s:",  Localize("Show in global statboard"));
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	UI()->DoLabel(&Button, aBuf, 16.0f, CUI::ALIGN_LEFT);

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos, Localize("Frags"), g_Config.m_TcStatboardInfos & TC_STATS_FRAGS, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_FRAGS;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+1, Localize("Deaths"), g_Config.m_TcStatboardInfos & TC_STATS_DEATHS, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_DEATHS;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+2, Localize("Suicides"), g_Config.m_TcStatboardInfos & TC_STATS_SUICIDES, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_SUICIDES;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+3, Localize("Ratio"), g_Config.m_TcStatboardInfos & TC_STATS_RATIO, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_RATIO;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+4, Localize("Net score"), g_Config.m_TcStatboardInfos & TC_STATS_NET, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_NET;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+5, Localize("Frags per minute"), g_Config.m_TcStatboardInfos & TC_STATS_FPM, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_FPM;
		
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+6, Localize("Current spree"), g_Config.m_TcStatboardInfos & TC_STATS_SPREE, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_SPREE;
		
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+7, Localize("Best spree"), g_Config.m_TcStatboardInfos & TC_STATS_BESTSPREE, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_BESTSPREE;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+9, Localize("Weapons stats"), g_Config.m_TcStatboardInfos & TC_STATS_WEAPS, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_WEAPS;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+8, Localize("Flag grabs"), g_Config.m_TcStatboardInfos & TC_STATS_FLAGGRABS, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_FLAGGRABS;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcStatboardInfos+10, Localize("Flag captures"), g_Config.m_TcStatboardInfos & TC_STATS_FLAGCAPTURES, &Button))
		g_Config.m_TcStatboardInfos ^= TC_STATS_FLAGCAPTURES;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	UI()->DoLabel(&Button, Localize("Key bindings"), 16.0f, CUI::ALIGN_LEFT);
	char aaBuf[3][32];
	str_format(aaBuf[0], sizeof(aaBuf[0]), "%s:", Localize("Global statboard"));
	str_format(aaBuf[1], sizeof(aaBuf[1]), "%s:", Localize("Player board"));
	str_format(aaBuf[2], sizeof(aaBuf[2]), "%s:", Localize("Next player"));
	CKeyInfo pKeys[] = {{ aaBuf[0], "+stats 1", 0},
		{ aaBuf[1], "+stats 2", 0},
		{ aaBuf[2], "+next_stats", 0}};

	for(int pKeyid=0; pKeyid < KEY_LAST; pKeyid++)
	{
		const char *Bind = m_pClient->m_pBinds->Get(pKeyid, 0); // no modifier for + commands
		if(!Bind[0])
			continue;

		for(unsigned int i=0; i<sizeof(pKeys)/sizeof(CKeyInfo); i++)
			if(strcmp(Bind, pKeys[i].m_pCommand) == 0)
			{
				pKeys[i].m_KeyId = pKeyid;
				break;
			}
	}

	for(unsigned int i=0; i<sizeof(pKeys)/sizeof(CKeyInfo); i++)
		UiDoKeybinder(pKeys[i], &MainView);
}

void CMenus::RenderLaser(const struct CNetObj_Laser *pCurrent)
{

	vec2 Pos = vec2(pCurrent->m_X, pCurrent->m_Y);
	vec2 From = vec2(pCurrent->m_FromX, pCurrent->m_FromY);
	vec2 Dir = normalize(Pos-From);

	float Ticks = Client()->GameTick() + Client()->IntraGameTick() - pCurrent->m_StartTick;
	float Ms = (Ticks/50.0f) * 1000.0f;
	float a =  Ms / m_pClient->m_Tuning.m_LaserBounceDelay;
	a = clamp(a, 0.0f, 1.0f);
	float Ia = 1-a;
	
	vec2 Out, Border;
	
	Graphics()->BlendNormal();
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	
	//vec4 inner_color(0.15f,0.35f,0.75f,1.0f);
	//vec4 outer_color(0.65f,0.85f,1.0f,1.0f);
	vec4 OuterColor(
		(g_Config.m_TcLaserColorOuter>>16)/255.0f,
		((g_Config.m_TcLaserColorOuter>>8)&0xff)/255.0f,
		(g_Config.m_TcLaserColorOuter&0xff)/255.0f, 1.0f);
	Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
	Out = vec2(Dir.y, -Dir.x) * (7.0f*Ia);

	IGraphics::CFreeformItem Freeform(
			From.x-Out.x, From.y-Out.y,
			From.x+Out.x, From.y+Out.y,
			Pos.x-Out.x, Pos.y-Out.y,
			Pos.x+Out.x, Pos.y+Out.y);
	Graphics()->QuadsDrawFreeform(&Freeform, 1);

	// do inner	
	// vec4 InnerColor(0.5f, 0.5f, 1.0f, 1.0f);
	vec4 InnerColor(
		(g_Config.m_TcLaserColorInner>>16)/255.0f,
		((g_Config.m_TcLaserColorInner>>8)&0xff)/255.0f,
		(g_Config.m_TcLaserColorInner&0xff)/255.0f, 1.0f);
	Out = vec2(Dir.y, -Dir.x) * (5.0f*Ia);
	Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f); // center
	
	Freeform = IGraphics::CFreeformItem(
			From.x-Out.x, From.y-Out.y,
			From.x+Out.x, From.y+Out.y,
			Pos.x-Out.x, Pos.y-Out.y,
			Pos.x+Out.x, Pos.y+Out.y);
	Graphics()->QuadsDrawFreeform(&Freeform, 1);
		
	Graphics()->QuadsEnd();
	
	// render head
	{
		Graphics()->BlendNormal();
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_PARTICLES].m_Id);
		Graphics()->QuadsBegin();

		int Sprites[] = {SPRITE_PART_SPLAT01, SPRITE_PART_SPLAT02, SPRITE_PART_SPLAT03};
		RenderTools()->SelectSprite(Sprites[Client()->GameTick()%3]);
		Graphics()->QuadsSetRotation(Client()->GameTick());
		Graphics()->SetColor(OuterColor.r, OuterColor.g, OuterColor.b, 1.0f);
		IGraphics::CQuadItem QuadItem(Pos.x, Pos.y, 24, 24);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->SetColor(InnerColor.r, InnerColor.g, InnerColor.b, 1.0f);
		QuadItem = IGraphics::CQuadItem(Pos.x, Pos.y, 20, 20);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	
	Graphics()->BlendNormal();	
}

void CMenus::RenderSettingsTeecompMisc(CUIRect MainView)
{
	CUIRect LeftView, RightView, LaserView, LLeftView, LRightView, Button;

	MainView.HSplitTop(MainView.h/2, &MainView, &LaserView);
	MainView.VSplitLeft(MainView.w/2, &LeftView, &RightView);

	// Left
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	UI()->DoLabel(&Button, Localize("HUD/Flag"), 16.0f, CUI::ALIGN_LEFT);

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcHudMatch, Localize("Make HUD match tees colors"), g_Config.m_TcHudMatch, &Button))
		g_Config.m_TcHudMatch ^= 1;

	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	if(DoButton_CheckBox(&g_Config.m_TcColoredFlags, Localize("Make flags match tees colors"), g_Config.m_TcColoredFlags, &Button))
		g_Config.m_TcColoredFlags ^= 1;

	int FakeSpeedMeter = 0;
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	DoButton_CheckBox(&FakeSpeedMeter, Localize("Display speed meter (TODO)"), FakeSpeedMeter, &Button);

	int FakeAccelMeter = 0;
	LeftView.HSplitTop(20.0f, &Button, &LeftView);
	Button.VSplitLeft(15.0f, 0, &Button);
	DoButton_CheckBox(&FakeAccelMeter, Localize("Speed meter show acceleration (TODO)"), FakeAccelMeter, &Button);

	// Right
	RightView.HSplitTop(20.0f, &Button, &RightView);
	UI()->DoLabel(&Button, Localize("Name plates"), 16.0f, CUI::ALIGN_LEFT);
	
	RightView.HSplitTop(20.0f, &Button, &RightView);
	if(DoButton_CheckBox(&g_Config.m_TcNameplateScore, Localize("Show score in name plate"), g_Config.m_TcNameplateScore, &Button))
		g_Config.m_TcNameplateScore ^= 1;

	RightView.HSplitTop(20.0f, 0, &RightView);
	RightView.HSplitTop(20.0f, &Button, &RightView);
	UI()->DoLabel(&Button, Localize("Other"), 16.0f, CUI::ALIGN_LEFT);

	RightView.HSplitTop(20.0f, &Button, &RightView);
	if(DoButton_CheckBox(&g_Config.m_TcHideCarrying, Localize("Hide flag while carrying it"), g_Config.m_TcHideCarrying, &Button))
		g_Config.m_TcHideCarrying ^= 1;

	/*
	RightView.HSplitTop(20.0f, &Button, &RightView);
	if(DoButton_CheckBox(&g_Config.m_TcStatScreenshot, Localize("Automatically take game over stat screenshot"), g_Config.m_TcStatScreenshot, &Button))
		g_Config.m_TcStatScreenshot ^= 1;

	char aBuf[64];
	RightView.HSplitTop(10.0f, 0, &RightView);
	RightView.VSplitLeft(20.0f, 0, &RightView);
	RightView.HSplitTop(20.0f, &Label, &Button);
	Button.VSplitRight(20.0f, &Button, 0);
	Button.HSplitTop(20.0f, &Button, 0);
	if(g_Config.m_TcStatScreenshotMax)
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max Screenshots"), g_Config.m_TcStatScreenshotMax);
	else
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Max Screenshots"), Localize("no limit"));
		
	UI()->DoLabelScaled(&Label, aBuf, 13.0f, CUI::ALIGN_LEFT);
	g_Config.m_TcStatScreenshotMax = static_cast<int>(DoScrollbarH(&g_Config.m_TcStatScreenshotMax, &Button, g_Config.m_TcStatScreenshotMax/1000.0f)*1000.0f+0.1f);
	*/
	
	// laser
	LaserView.HSplitTop(20.0f, &Button, &LaserView);
	UI()->DoLabel(&Button, "Laser", 16.0f, CUI::ALIGN_CENTER);
	LaserView.VSplitLeft(LaserView.w/2, &LLeftView, &LRightView);
	
	int lri, lro, lgi, lgo, lbi, lbo;
	lri = g_Config.m_TcLaserColorInner>>16;
	lgi = (g_Config.m_TcLaserColorInner>>8)&0xff;
	lbi = g_Config.m_TcLaserColorInner&0xff;
	
	LLeftView.HSplitTop(20.0f, &Button, &LLeftView);
	UI()->DoLabel(&Button, Localize("Laser inner color"), 14.0f, CUI::ALIGN_LEFT);
	RenderRgbSliders(&LLeftView, &Button, lri, lgi, lbi, true);
	g_Config.m_TcLaserColorInner = (lri<<16) + (lgi<<8) + lbi;
	
	
	lro = g_Config.m_TcLaserColorOuter>>16;
	lgo = (g_Config.m_TcLaserColorOuter>>8)&0xff;
	lbo = g_Config.m_TcLaserColorOuter&0xff;
	
	LRightView.HSplitTop(20.0f, &Button, &LRightView);
	UI()->DoLabel(&Button, Localize("Laser outer color"), 14.0f, CUI::ALIGN_LEFT);
	
	RenderRgbSliders(&LRightView, &Button, lro, lgo, lbo, true);
	g_Config.m_TcLaserColorOuter = (lro<<16) + (lgo<<8) + lbo;
	
	{ 
		CUIRect LBut, RBut;
		CUIRect screen = *UI()->Screen();
		
		LLeftView.HSplitTop(20.0f, &LBut, &LLeftView);
		LRightView.HSplitTop(20.0f, &RBut, &LRightView);
		
		// Calculate world screen mapping
		float aPoints[4];
		RenderTools()->MapScreenToWorld(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, Graphics()->ScreenAspect(), 1.0f, aPoints);
		aPoints[2] = aPoints[2] - aPoints[0];
		aPoints[3] = aPoints[3] - aPoints[1];
		aPoints[0] = 0.0f;
		aPoints[1] = 0.0f;
		
		// factor between world and menu screen mappings
		float fact_x, fact_y;
		fact_x = (aPoints[2]-aPoints[0])/screen.w;
		fact_y = (aPoints[3]-aPoints[1])/screen.h;
		
		struct CNetObj_Laser Laser;
		// we want to draw a beam from under the center of one sliders section to the center of the other sliders section
		Laser.m_FromX = (LBut.x + LBut.w/2)*fact_x;
		Laser.m_FromY = (LBut.y + LBut.h/2)*fact_y;
		Laser.m_X = (RBut.x + RBut.w/2)*fact_x;
		Laser.m_Y = (RBut.y + RBut.h/2)*fact_y;
		Laser.m_StartTick = Client()->GameTick() + Client()->IntraGameTick();
		// apply world screen mapping (beam is bigger in menu mapping)
		Graphics()->MapScreen(aPoints[0], aPoints[1], aPoints[2], aPoints[3]);
		// draw laser
		RenderLaser(&Laser);
		// restore menu screen mapping
		Graphics()->MapScreen(screen.x, screen.y, screen.w, screen.h); 
	}
}
/*
void CMenus::RenderSettingsTeecompAbout(CUIRect MainView)
{
	CUIRect Button;
	
	char aBuf[32];
	MainView.HSplitTop(52.0f, &Button, &MainView);
	UI()->DoLabel(&Button, "TeeComp", 48.0f, CUI::ALIGN_CENTER);

	NewLine(&Button, &MainView);
	Button.VSplitRight(Button.w/3, 0, &Button);
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("Version"), TEECOMP_VERSION);
	UI()->DoLabel(&Button, aBuf, 14.0f, CUI::ALIGN_LEFT);
	NewLine();
	Button.VSplitRight(Button.w/3, 0, &Button);
	str_format(aBuf, sizeof(aBuf), "%s %s", Localize("For Teeworlds"), GAME_VERSION);
	UI()->DoLabel(&Button, aBuf, 14.0f, CUI::ALIGN_LEFT);
	NewLine();
	Button.VSplitRight(Button.w/3, 0, &Button);
	str_format(aBuf, sizeof(aBuf), "%s %s %s", Localize("Compiled"), __DATE__, __TIME__);
	UI()->DoLabel(&Button, aBuf, 14.0f, CUI::ALIGN_LEFT);

	NewLine();
	NewLine();
	NewLine();
	UI()->DoLabel(&Button, Localize("By Alban 'spl0k' FERON"), 14.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, "http://spl0k.unreal-design.com/", 14.0f, CUI::ALIGN_CENTER);

	NewLine();
	NewLine();
	UI()->DoLabel(&Button, Localize("Special thanks to:"), 16.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, "Sd`", 14.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, "Tho", 14.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, "Eve", 14.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, Localize("some other MonkeyStyle members"), 14.0f, CUI::ALIGN_CENTER);
	NewLine();
	UI()->DoLabel(&Button, Localize("and the Teeworlds.com community"), 14.0f, CUI::ALIGN_CENTER);

	MainView.HSplitBottom(10.0f, &MainView, &Button);
	UI()->DoLabel(&Button, Localize("so you can set while u set while u set options"), 10.0f, CUI::ALIGN_LEFT);
	MainView.HSplitBottom(10.0f, &MainView, &Button);
	UI()->DoLabel(&Button, Localize("Yo dawg I herd you like tabs so we put tabs in yo tabs in yo tabs"), 10.0f, CUI::ALIGN_LEFT);
	NewLine(NULL, NULL);
}

void CMenus::NewLine(CUIRect *pButton, CUIRect *pView)
{
	m_pNewLineButton = pButton;
	m_pNewLineView = pView;
	
	if(m_pNewLineButton == NULL || m_pNewLineView == NULL)
		return;
	NewLine();
}

void CMenus::NewLine()
{
	if(m_pNewLineButton == NULL || m_pNewLineView == NULL)
		return;
	m_pNewLineView->HSplitTop(20.0f, m_pNewLineButton, m_pNewLineView);
}
*/
