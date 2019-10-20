/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_ENTITIES_H
#define GAME_CLIENT_COMPONENTS_ENTITIES_H
#include <game/client/component.h>

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
