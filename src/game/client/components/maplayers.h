/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#define GAME_CLIENT_COMPONENTS_MAPLAYERS_H
#include <base/tl/array.h>
#include <game/client/component.h>
#include <game/client/components/auto_tile.h>

class CMapLayers : public CComponent
{
	CLayers *m_pMenuLayers;
	IEngineMap *m_pMenuMap;
	// gamer
	CTilesetPainter* m_pTilesetPainter;
	CDoodadsPainter* m_pDoodadsPainter;
	CTile* m_pAutoTiles;
	CTile* m_pAutoDoodads;

	int m_Type;
	int m_CurrentLocalTick;
	int m_LastLocalTick;
	float m_OnlineStartTime;
	bool m_EnvelopeUpdate;
	bool m_AutolayerUpdate;

	array<CEnvPoint> m_lEnvPoints;
	array<CEnvPoint> m_lEnvPointsMenu;

	static void EnvelopeEval(float TimeOffset, int Env, float *pChannels, void *pUser);

	void LoadEnvPoints(const CLayers *pLayers, array<CEnvPoint>& lEnvPoints);
	void LoadBackgroundMap();
	void ReloadPainters();
	void LoadPainters(CLayers *pLayers);
	void LoadAutomapperRules(CLayers *pLayers, const char* pName);

public:
	enum
	{
		TYPE_BACKGROUND=0,
		TYPE_FOREGROUND,
	};

	CMapLayers(int Type);
	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnInit();
	virtual void OnRender();
	virtual void OnMapLoad();

	void EnvelopeUpdate();

	static void ConchainBackgroundMap(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainAutomapperReload(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void Con_Automap(IConsole::IResult *pResult, void *pUserData);

	virtual void OnConsoleInit();

	void BackgroundMapUpdate();

	bool MenuMapLoaded() { return m_pMenuMap ? m_pMenuMap->IsLoaded() : false; }
};

#endif
