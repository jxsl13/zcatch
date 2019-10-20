/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#define GAME_CLIENT_COMPONENTS_MAPIMAGES_H
#include <game/client/component.h>

class CMapImages : public CComponent
{
	enum
	{
		MAX_TEXTURES=64,

		MAP_TYPE_GAME=0,
		MAP_TYPE_MENU,
		NUM_MAP_TYPES
	};
	struct
	{
		IGraphics::CTextureHandle m_aTextures[MAX_TEXTURES];
		int m_Count;
	} m_Info[NUM_MAP_TYPES];
	IGraphics::CTextureHandle m_EntitiesTexture;
	IGraphics::CTextureHandle m_AutoEntitiesTexture;
	IGraphics::CTextureHandle m_TeleEntitiesTexture;
	IGraphics::CTextureHandle m_AutoTilesTexture;
	IGraphics::CTextureHandle m_AutoDoodadsTexture;

	IGraphics::CTextureHandle m_EasterTexture;
	bool m_EasterIsLoaded;

	void LoadMapImages(class IMap *pMap, class CLayers *pLayers, int MapType);

public:
	CMapImages();

	IGraphics::CTextureHandle Get(int Index) const;
	IGraphics::CTextureHandle GetEntities() const;
	IGraphics::CTextureHandle GetAutoEntities() const;
	IGraphics::CTextureHandle GetTeleEntities() const;
	IGraphics::CTextureHandle GetAutoTiles() const;
	IGraphics::CTextureHandle GetAutoDoodads() const;
	int Num() const;

	virtual void OnMapLoad();
	void OnMenuMapLoad(class IMap *pMap);
	
	IGraphics::CTextureHandle GetEasterTexture();
	void LoadAutoMapres();
};

class CEntities : public CComponent
{
	enum
	{
		MAX_TEXTURES=64,
	};
	struct
	{
		IGraphics::CTextureHandle m_aTextures;
		char m_aName[256];
	} m_Info[MAX_TEXTURES];
	bool m_Loaded;
	int m_Count;
	static int EntityScan(const char *pName, int IsDir, int DirType, void *pUser);

public:
	CEntities() { }
	bool IsLoaded() const { return m_Loaded; }
	void OnInit();
	void LoadEntities();
	IGraphics::CTextureHandle Get(int Index) const;
	const char* GetName(int Index) const;
	int Num() const;
};

#endif
