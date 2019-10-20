/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ENTITIES_H
#define GAME_CLIENT_COMPONENTS_ENTITIES_H
#include <game/client/component.h>

class CEntities : public CComponent
{
	bool m_Loaded;
	
public:
	CEntities();
	bool IsLoaded() const { return m_Loaded; }
	void DelayedInit(); // forwards to subcomponents
	void LoadEntities(); // forwards to subcomponents

	// game skins
	class CGameSkins : public CComponent
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
		IGraphics::CTextureHandle m_DefaultTexture;
		IGraphics::CTextureHandle m_InitialTexture; // store the texture at init there if it is custom (redundant)
		int m_Count;
		
		static int GameSkinScan(const char *pName, int IsDir, int DirType, void *pUser);
		bool LoadGameSkin(const char *pName, int DirType, IGraphics::CTextureHandle *pTexture);

	public:
		CGameSkins();
		void DelayedInit();
		void LoadEntities();
		IGraphics::CTextureHandle Get(int Index) const;
		IGraphics::CTextureHandle GetDefault() const;
		const char* GetName(int Index) const;
		int Num() const;
	} m_GameSkins;
};

#endif
