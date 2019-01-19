#ifndef GAME_EDITOR_AUTO_TILE_H
#define GAME_EDITOR_AUTO_TILE_H

#include <base/tl/array.h>
#include <base/vmath.h>

#include <engine/external/json-parser/json.h>
#include <game/editor/editor.h>
#include <game/layers.h>

// this tiles the game layer
class IAutoTiler
{
protected:
	typedef CMapItemLayerTilemap gamelayer_t;
	gamelayer_t *m_pGameLayer;
	class CLayers *m_pLayers;
	CTile *m_pGameTiles;
	int m_Type;

public:
	enum
	{
		TYPE_TILESET,
		TYPE_DOODADS,

		MAX_RULES=256
	};

	//
	IAutoTiler(CLayers *pLayers, int Type) : m_pGameLayer(pLayers->GameLayer()), m_pLayers(pLayers), m_Type(Type)
		{ dbg_assert(pLayers && pLayers->Map(), "automapper: no map provided"); m_pGameTiles = (CTile *)pLayers->Map()->GetData(m_pGameLayer->m_Data); }
	virtual ~IAutoTiler() {};
	virtual void Load(const json_value &rElement) = 0;
	// virtual void Proceed(class CLayerTiles *pLayer, int ConfigID) {}
	virtual void Proceed(CTile* pTiles, int Width, int Height, int ConfigID) {}
	virtual void Proceed(CTile* pTiles, int Width, int Height, int ConfigID, int Amount) {}
	// virtual void Proceed(class CLayerTiles *pLayer, int ConfigID, int Amount) {} // for convenience purposes

	virtual int RuleSetNum() = 0;
	virtual const char* GetRuleSetName(int Index) const = 0;

	//
	int GetType() const { return m_Type; }

	static bool Random(int Value)
	{
		// return (((random_int() + Value) % 2) == 1);
		return (((random_int()) % Value) == 0);
	}

	static const char *GetTypeName(int Type)
	{
		if(Type == TYPE_TILESET)
			return "tileset";
		else if(Type == TYPE_DOODADS)
			return "doodads";
		else
			return "";
	}

	static int CompareRules(const void *a, const void *b);
};

class CTilesetPainter: public IAutoTiler
{
	struct CRuleCondition
	{
		int m_X;
		int m_Y;
		int m_Value;

		enum
		{
			EMPTY=-2,
			FULL=-1
		};
	};

	struct CRule
	{
		int m_Index;
		int m_HFlip;
		int m_VFlip;
		int m_Random;
		int m_Rotation;

		array<CRuleCondition> m_aConditions;
	};

	struct CRuleSet
	{
		char m_aName[128];
		int m_BaseTile;

		array<CRule> m_aRules;
	};

	array<CRuleSet> m_aRuleSets;

public:
	CTilesetPainter(CLayers *pLayers) : IAutoTiler(pLayers, TYPE_TILESET) { m_aRuleSets.clear(); }

	virtual void Load(const json_value &rElement);
	// virtual void Proceed(class CLayerTiles *pLayer, int ConfigID);
	virtual void Proceed(CTile* pTiles, int Width, int Height, int ConfigID);

	virtual int RuleSetNum() { return m_aRuleSets.size(); }
	virtual const char* GetRuleSetName(int Index) const;
};

class CDoodadsPainter: public IAutoTiler
{
public:
	struct CRule
	{
		ivec2 m_Rect;
		ivec2 m_Size;
		ivec2 m_RelativePos;

		int m_Location;
		int m_Random;

		int m_HFlip;
		int m_VFlip;

		enum
		{
			FLOOR=0,
			CEILING,
			WALLS
		};
	};

	struct CRuleSet
	{
		char m_aName[128];

		array<CRule> m_aRules;
	};

	CDoodadsPainter(CLayers *pLayers) :  IAutoTiler(pLayers, TYPE_DOODADS) { m_aRuleSets.clear(); }

	virtual void Load(const json_value &rElement);
	virtual void Proceed(CTile* pTiles, int Width, int Height, int ConfigID, int Amount);
	void AnalyzeGameLayer();

	virtual int RuleSetNum() { return m_aRuleSets.size(); }
	virtual const char* GetRuleSetName(int Index) const;

private:
	void PlaceDoodads(CTile* pTiles, int Width, int Height, CRule *pRule, array<array<int> > *pPositions, int Amount, int LeftWall = 0);

	array<CRuleSet> m_aRuleSets;

	array<array<int> > m_FloorIDs;
	array<array<int> > m_CeilingIDs;
	array<array<int> > m_RightWallIDs;
	array<array<int> > m_LeftWallIDs;
};

#endif
