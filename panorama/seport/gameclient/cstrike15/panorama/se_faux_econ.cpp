//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - the data behind se_faux_econ.h (see that file for why this exists).
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/se_faux_econ.h"
#include "tier1/keyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace se_faux_econ;

namespace
{
	// The category tags are the "Inv_Category_*" names the content localizes; "any" is what CS:GO's
	// own inventory shows first and is also the tag the list class treats as "everything".
	const char *k_Categories[] =
	{
		"inv_category_any",
		"inv_category_melee",
		"inv_category_secondary",
		"inv_category_smg",
		"inv_category_rifle",
		"inv_category_heavy",
		"inv_category_tools",
		"inv_category_container",
		"inv_category_collections",
	};

	const char *k_SubCategories[] =
	{
		"any",
	};

	// The catalog.  Item ids are "se_store_<def>_<paint>", the same form the JS layer builds and names
	// (se_session_sim.js: STORE_NAMES / the "SEPort_Op_*" tokens), so the tiles' names resolve through
	// the port's localization file.  The icons are CS:GO's own item art: {images_econ} is the mod's
	// resource/flash directory (panorama.cfg), and the files below are the ones the CS:GO install
	// ships (econ/tools, econ/status_icons, econ/weapon_cases and the generated weapon renders).
	CatalogItem_t k_ExtraCatalog[] =
	{
		// --- the store's own entries (Paris 2023 + the operations) -------------------------------
		{ 4883, 0, "inv_category_tools",       "any", "any", 4, "file://{images_econ}/econ/status_icons/blast_pickem_2023_pass_large.png" },
		{ 4888, 0, "inv_category_container",   "any", "any", 3, "file://{images_econ}/econ/weapon_cases/atlanta2017_bundleofall.png" },
		{ 6732, 0, "inv_category_container",   "any", "any", 3, "file://{images_econ}/econ/tools/sticker_crate_key.png" },

		// Operation Payback .. Vanguard have no coin art in this content; campaign.png is the generic
		// operation icon.  Bloodhound (6) .. Riptide (11) match the operation_<n>_gold coins the
		// install ships - the numbers are CS:GO's own operation numbers.
		{ 9101, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9102, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9103, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9104, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9105, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/tools/campaign.png" },
		{ 9106, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_6_gold_large.png" },
		{ 9107, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_7_gold_large.png" },
		{ 9108, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_8_gold_large.png" },
		{ 9109, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_9_gold_large.png" },
		{ 9110, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_10_gold_large.png" },
		{ 9111, 0, "inv_category_collections", "any", "any", 4, "file://{images_econ}/econ/status_icons/operation_11_gold_large.png" },

		// --- the operation's quests and rewards --------------------------------------------------
		{ 9201, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },
		{ 9202, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },
		{ 9203, 0, "inv_category_tools",       "any", "any", 3, "file://{images_econ}/econ/tools/mission.png" },

		{ 9301, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_cu_ak47_asiimov_light_large.png" },
		{ 9302, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_cu_ak47_anubis_light_large.png" },
		{ 9303, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_bloodsport_light_large.png" },
		{ 9304, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_empress_light_large.png" },
		{ 9305, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_ak47_gs_ak47_nibbler_light_large.png" },
		{ 9306, 0, "inv_category_collections", "any", "any", 5, "file://{images_econ}/econ/default_generated/weapon_knife_karambit_am_fade_light_large.png" },
	};

	const int k_nExtraCatalogCount = V_ARRAYSIZE( k_ExtraCatalog );

	// ------------------------------------------------------------------------------------------
	// The real catalog, generated from the retail items_game.txt (build/_gen_econ_real.ps1).
	// Everything below is loaded lazily by EnsureRealLoaded() from scripts/items/se_econ_real.txt.
	// ------------------------------------------------------------------------------------------
	struct RealDef_t
	{
		uint32		m_unDefIndex;
		CUtlString	m_strClass;
		CUtlString	m_strNameToken;
		CUtlString	m_strBaseImage;		// "econ/weapons/base_weapons/weapon_ak47" (no prefix/extension)
		CUtlString	m_strSubPosition;	// "rifle1"
		CUtlString	m_strTeam;			// "" = both, "t" / "ct" = that team only
	};

	struct RealItem_t
	{
		uint32		m_unDefIndex;
		uint32		m_unPaintKit;
		int			m_nRarity;
		CUtlString	m_strImage;			// full "file://{images_econ}/..." path, already sized
	};

	struct RealSlot_t
	{
		CUtlString	m_strSubPosition;
		uint32		m_unDefIndex;
		bool		m_bT;
		bool		m_bCT;
		bool		m_bDefault;
	};

	CUtlVector< RealDef_t >	s_RealDefs;
	CUtlVector< RealItem_t >	s_RealItems;
	CUtlVector< RealSlot_t >	s_RealSlots;
	CUtlVector< CatalogItem_t > s_RealCatalog;	// the real items that map to an inventory category
	bool					s_bRealLoaded = false;
	bool					s_bRealLoadFailed = false;

	const char *k_pchEconImagePrefix = "file://{images_econ}/";

	const RealDef_t *FindRealDef( uint32 unDefIndex )
	{
		for ( int i = 0; i < s_RealDefs.Count(); ++i )
		{
			if ( s_RealDefs[i].m_unDefIndex == unDefIndex )
				return &s_RealDefs[i];
		}

		return NULL;
	}

	// The inventory category a loadout slot belongs to (the content localizes these as
	// Inv_Category_melee / _secondary / _smg / _rifle / _heavy).
	const char *CategoryFromSubPosition( const char *pchSubPosition )
	{
		if ( !pchSubPosition )
			return NULL;

		if ( !V_strnicmp( pchSubPosition, "melee", 5 ) )		return "inv_category_melee";
		if ( !V_strnicmp( pchSubPosition, "secondary", 9 ) )	return "inv_category_secondary";
		if ( !V_strnicmp( pchSubPosition, "smg", 3 ) )			return "inv_category_smg";
		if ( !V_strnicmp( pchSubPosition, "rifle", 5 ) )		return "inv_category_rifle";
		if ( !V_strnicmp( pchSubPosition, "heavy", 5 ) )		return "inv_category_heavy";

		return NULL;
	}

	void BuildFullImagePath( const char *pchRelative, char *pchOut, int nOutSize )
	{
		V_snprintf( pchOut, nOutSize, "%s%s.png", k_pchEconImagePrefix, pchRelative );
	}

	bool BuildRealCatalogItem( const RealItem_t &item, CatalogItem_t &outItem );

	void EnsureRealLoaded()
	{
		if ( s_bRealLoaded || s_bRealLoadFailed )
			return;

		KeyValues *pKV = new KeyValues( "econ_real" );
		if ( !pKV->LoadFromFile( g_pFullFileSystem, "scripts/items/se_econ_real.txt", "GAME" ) )
		{
			// the mod directory is also reachable through the plain (no path id) search
			if ( !pKV->LoadFromFile( g_pFullFileSystem, "scripts/items/se_econ_real.txt", NULL ) )
			{
				Warning( "se_port econ: scripts/items/se_econ_real.txt not found - the weapon catalog "
						 "stays empty (run build/_gen_econ_real.ps1 and deploy it)\n" );
				s_bRealLoadFailed = true;
				pKV->deleteThis();
				return;
			}
		}

		// items/<def> -> class / name token / base icon / slot
		KeyValues *pItems = pKV->FindKey( "items" );
		if ( pItems )
		{
			for ( KeyValues *pItem = pItems->GetFirstSubKey(); pItem; pItem = pItem->GetNextKey() )
			{
				const char *pchDef = pItem->GetName();
				if ( !pchDef || !pchDef[0] )
					continue;

				RealDef_t def;
				def.m_unDefIndex = ( uint32 )V_atoi64( pchDef );
				def.m_strClass = pItem->GetString( "cls", "" );
				def.m_strNameToken = pItem->GetString( "name", "" );
				def.m_strBaseImage = pItem->GetString( "img", "" );
				def.m_strSubPosition = pItem->GetString( "sub", "" );
				def.m_strTeam = pItem->GetString( "team", "" );
				s_RealDefs.AddToTail( def );
			}
		}

		// paints/<def>/<paintkit> -> name token / icon / rarity
		KeyValues *pPaints = pKV->FindKey( "paints" );
		if ( pPaints )
		{
			for ( KeyValues *pDef = pPaints->GetFirstSubKey(); pDef; pDef = pDef->GetNextKey() )
			{
				const uint32 unDefIndex = ( uint32 )V_atoi64( pDef->GetName() );
				const RealDef_t *pRealDef = FindRealDef( unDefIndex );
				if ( !pRealDef )
					continue;

				for ( KeyValues *pPaint = pDef->GetFirstSubKey(); pPaint; pPaint = pPaint->GetNextKey() )
				{
					const char *pchImage = pPaint->GetString( "i", NULL );
					if ( !pchImage || !pchImage[0] )
						continue;

					RealItem_t item;
					item.m_unDefIndex = unDefIndex;
					item.m_unPaintKit = ( uint32 )V_atoi64( pPaint->GetName() );
					item.m_nRarity = pPaint->GetInt( "r", 0 );
					char szFullImage[ 512 ];
					BuildFullImagePath( pchImage, szFullImage, sizeof( szFullImage ) );
					item.m_strImage = CUtlString( szFullImage );
					s_RealItems.AddToTail( item );
				}
			}
		}

		// slots/<subposition>/<def> -> team flags + default flag
		KeyValues *pSlots = pKV->FindKey( "slots" );
		if ( pSlots )
		{
			for ( KeyValues *pSub = pSlots->GetFirstSubKey(); pSub; pSub = pSub->GetNextKey() )
			{
				for ( KeyValues *pEntry = pSub->GetFirstSubKey(); pEntry; pEntry = pEntry->GetNextKey() )
				{
					RealSlot_t slot;
					slot.m_strSubPosition = pSub->GetName();
					slot.m_unDefIndex = ( uint32 )V_atoi64( pEntry->GetName() );
					slot.m_bT = pEntry->GetInt( "t", 0 ) != 0;
					slot.m_bCT = pEntry->GetInt( "ct", 0 ) != 0;
					slot.m_bDefault = pEntry->GetInt( "dflt", 0 ) != 0;
					s_RealSlots.AddToTail( slot );
				}
			}
		}

		pKV->deleteThis();

		// the filtered catalog: only the items that map to an inventory category
		for ( int i = 0; i < s_RealItems.Count(); ++i )
		{
			CatalogItem_t item;
			if ( BuildRealCatalogItem( s_RealItems[ i ], item ) )
				s_RealCatalog.AddToTail( item );
		}

		s_bRealLoaded = true;

		Msg( "se_port econ: loaded %d items, %d weapon/skin entries (%d in the inventory catalog), "
			 "%d loadout slots\n",
			s_RealDefs.Count(), s_RealItems.Count(), s_RealCatalog.Count(), s_RealSlots.Count() );
	}

	// The catalog entry a real (def, paint) pair maps to - categories come from the loadout slot the
	// item sits in, so the real weapons land in the same tabs CS:GO's inventory uses.
	bool BuildRealCatalogItem( const RealItem_t &item, CatalogItem_t &outItem )
	{
		const RealDef_t *pDef = FindRealDef( item.m_unDefIndex );
		if ( !pDef )
			return false;

		const char *pchCategory = CategoryFromSubPosition( pDef->m_strSubPosition.Get() );
		if ( !pchCategory )
			return false;		// grenades / c4 / equipment are not inventory catalog items here

		outItem.m_unDefIndex = item.m_unDefIndex;
		outItem.m_unPaintKit = item.m_unPaintKit;
		outItem.m_pchCategory = pchCategory;
		outItem.m_pchSubCategory = "any";
		// The group tier carries the item's real loadout slot ("secondary1", "rifle4", ...): the loadout
		// page filters by asking for a slot name, and ItemMatchesTier treats the request as a prefix of
		// the group - so "rifle" selects rifle0..5 and "rifle1" selects just that slot.
		outItem.m_pchGroup = ( pDef->m_strSubPosition.Get() && pDef->m_strSubPosition.Get()[ 0 ] )
								? pDef->m_strSubPosition.Get() : "any";
		outItem.m_pchTeam = ( pDef->m_strTeam.Get() && pDef->m_strTeam.Get()[ 0 ] )
								? pDef->m_strTeam.Get() : "";
		outItem.m_unRarity = ( uint32 )item.m_nRarity;
		outItem.m_pchImage = item.m_strImage.Get();
		return true;
	}

	bool ItemMatchesTier( const CatalogItem_t &item, const char *pchCategory, const char *pchSubCategory, const char *pchGroup,
						 const char *pchTeamFilter )
	{
		// "any" is the wildcard in every tier, exactly like CS:GO's client inventory view treats it.
		const bool bCategoryMatches = ( !V_stricmp( pchCategory, "any" ) || !V_stricmp( pchCategory, "inv_category_any" ) )
									  || !V_stricmp( pchCategory, item.m_pchCategory );
		if ( !bCategoryMatches )
			return false;

		const bool bSubMatches = ( !pchSubCategory || !V_stricmp( pchSubCategory, "any" ) )
								 || !V_stricmp( pchSubCategory, item.m_pchSubCategory );
		if ( !bSubMatches )
			return false;

		// The group is the loadout slot: an exact "any" matches everything, otherwise the request has
		// to be a prefix of the item's slot name ("rifle" -> rifle0..rifle5, "rifle1" -> that slot).
		if ( pchGroup && V_stricmp( pchGroup, "any" ) )
		{
			if ( !item.m_pchGroup )
				return false;

			if ( V_strnicmp( item.m_pchGroup, pchGroup, V_strlen( pchGroup ) ) != 0 )
				return false;
		}

		// Team: "any" keeps both, otherwise the item has to be team-neutral or match.
		if ( pchTeamFilter && pchTeamFilter[ 0 ] && V_stricmp( pchTeamFilter, "any" ) )
		{
			if ( item.m_pchTeam && item.m_pchTeam[ 0 ] && V_stricmp( item.m_pchTeam, pchTeamFilter ) )
				return false;
		}

		return true;
	}
}

void se_faux_econ::MakeItemID( char *pchOut, int nOutBufferSize, uint32 unDefIndex, uint32 unPaintKit )
{
	V_snprintf( pchOut, nOutBufferSize, "se_store_%u_%u", unDefIndex, unPaintKit );
}

int se_faux_econ::GetCatalogCount()
{
	EnsureRealLoaded();
	return k_nExtraCatalogCount + s_RealCatalog.Count();
}

const CatalogItem_t &se_faux_econ::GetCatalogItem( int nIndex )
{
	EnsureRealLoaded();

	const int nTotalCount = k_nExtraCatalogCount + s_RealCatalog.Count();
	if ( nIndex < 0 || nIndex >= nTotalCount )
		nIndex = 0;

	if ( nIndex < k_nExtraCatalogCount )
		return k_ExtraCatalog[ nIndex ];

	return s_RealCatalog[ nIndex - k_nExtraCatalogCount ];
}

// "se_store_<def>_<paint>" -> def index + paint kit index (the id form the JS layer builds).
static bool ParseFauxItemID( const char *pchItemID, uint32 &unDefIndex, uint32 &unPaintKit )
{
	if ( !pchItemID )
		return false;

	const char *pchPrefix = "se_store_";
	const size_t nPrefixLen = V_strlen( pchPrefix );
	if ( V_strnicmp( pchItemID, pchPrefix, ( int )nPrefixLen ) )
		return false;

	const char *pchDef = pchItemID + nPrefixLen;
	const char *pchRest = V_strstr( pchDef, "_" );
	if ( !pchRest )
		return false;

	unDefIndex = ( uint32 )V_atoi( pchDef );
	unPaintKit = ( uint32 )V_atoi( pchRest + 1 );
	return true;
}

static const CatalogItem_t *FindRealCatalogItem( uint32 unDefIndex, uint32 unPaintKit )
{
	for ( int i = 0; i < s_RealCatalog.Count(); ++i )
	{
		if ( s_RealCatalog[ i ].m_unDefIndex == unDefIndex && s_RealCatalog[ i ].m_unPaintKit == unPaintKit )
			return &s_RealCatalog[ i ];
	}

	return NULL;
}

const CatalogItem_t *se_faux_econ::FindItemByID( const char *pchItemID )
{
	if ( !pchItemID )
		return NULL;

	EnsureRealLoaded();

	uint32 unDefIndex = 0, unPaintKit = 0;
	if ( ParseFauxItemID( pchItemID, unDefIndex, unPaintKit ) )
	{
		const CatalogItem_t *pRealItem = FindRealCatalogItem( unDefIndex, unPaintKit );
		if ( pRealItem )
			return pRealItem;
	}

	char szItemID[ 64 ];
	for ( int i = 0; i < k_nExtraCatalogCount; ++i )
	{
		MakeItemID( szItemID, sizeof( szItemID ), k_ExtraCatalog[ i ].m_unDefIndex, k_ExtraCatalog[ i ].m_unPaintKit );
		if ( !V_stricmp( pchItemID, szItemID ) )
			return &k_ExtraCatalog[ i ];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: the localization token of an item's name (see the header).
//-----------------------------------------------------------------------------
const char *se_faux_econ::GetItemNameToken( const char *pchItemID )
{
	static char s_szToken[ 256 ];

	// The real items name themselves with CS:GO's own tokens ("#SFUI_WPNHUD_AK47"), which the game's
	// localization carries - the skin part is composed by the JS layer (content/common/iteminfo.js).
	uint32 unDefIndex = 0, unPaintKit = 0;
	if ( ParseFauxItemID( pchItemID, unDefIndex, unPaintKit ) )
	{
		EnsureRealLoaded();

		if ( const RealDef_t *pDef = FindRealDef( unDefIndex ) )
		{
			if ( pDef->m_strNameToken.Get() && pDef->m_strNameToken.Get()[ 0 ] )
				return pDef->m_strNameToken.Get();
		}
	}

	const CatalogItem_t *pItem = FindItemByID( pchItemID );
	if ( !pItem )
		return NULL;

	unDefIndex = pItem->m_unDefIndex;
	if ( unDefIndex >= 9201 && unDefIndex <= 9203 )
		V_snprintf( s_szToken, sizeof( s_szToken ), "#SEPort_Op_Q%u_Name", unDefIndex - 9200 );
	else if ( unDefIndex >= 9301 && unDefIndex <= 9306 )
		V_snprintf( s_szToken, sizeof( s_szToken ), "#SEPort_Op_Reward_%u", unDefIndex - 9300 );
	else
		V_snprintf( s_szToken, sizeof( s_szToken ), "#SEPort_Store_Item_%u", unDefIndex );

	return s_szToken;
}

int se_faux_econ::GetCategoryCount()
{
	return V_ARRAYSIZE( k_Categories );
}

const char *se_faux_econ::GetCategory( int nIndex )
{
	if ( nIndex < 0 || nIndex >= V_ARRAYSIZE( k_Categories ) )
		return k_Categories[ 0 ];

	return k_Categories[ nIndex ];
}

int se_faux_econ::GetSubCategoryCount( const char *pchCategory )
{
	// Every category currently exposes a single "any" tier - the UI builds one tab per entry, and
	// "any" is the label the content localizes as Inv_Category_any.
	( void )pchCategory;
	return V_ARRAYSIZE( k_SubCategories );
}

const char *se_faux_econ::GetSubCategory( const char *pchCategory, int nIndex )
{
	( void )pchCategory;

	if ( nIndex < 0 || nIndex >= V_ARRAYSIZE( k_SubCategories ) )
		return k_SubCategories[ 0 ];

	return k_SubCategories[ nIndex ];
}

bool se_faux_econ::IsKnownCategory( const char *pchCategory )
{
	for ( int i = 0; i < V_ARRAYSIZE( k_Categories ); ++i )
	{
		if ( !V_stricmp( pchCategory, k_Categories[ i ] ) )
			return true;
	}

	return false;
}

int se_faux_econ::CollectItemIDs( const char *pchCategory, const char *pchSubCategory, const char *pchGroup,
								  CUtlVector<CUtlString> &vecOutIDs, const char *pchTeamFilter )
{
	EnsureRealLoaded();
	vecOutIDs.RemoveAll();

	char szItemID[ 64 ];
	for ( int i = 0; i < k_nExtraCatalogCount; ++i )
	{
		if ( !ItemMatchesTier( k_ExtraCatalog[ i ], pchCategory, pchSubCategory, pchGroup, pchTeamFilter ) )
			continue;

		MakeItemID( szItemID, sizeof( szItemID ), k_ExtraCatalog[ i ].m_unDefIndex, k_ExtraCatalog[ i ].m_unPaintKit );
		vecOutIDs.AddToTail( CUtlString( szItemID ) );
	}

	for ( int i = 0; i < s_RealCatalog.Count(); ++i )
	{
		if ( !ItemMatchesTier( s_RealCatalog[ i ], pchCategory, pchSubCategory, pchGroup, pchTeamFilter ) )
			continue;

		MakeItemID( szItemID, sizeof( szItemID ), s_RealCatalog[ i ].m_unDefIndex, s_RealCatalog[ i ].m_unPaintKit );
		vecOutIDs.AddToTail( CUtlString( szItemID ) );
	}

	return vecOutIDs.Count();
}

int se_faux_econ::GetRarityForItemID( const char *pchItemID )
{
	uint32 unDefIndex = 0, unPaintKit = 0;
	if ( ParseFauxItemID( pchItemID, unDefIndex, unPaintKit ) )
	{
		EnsureRealLoaded();

		const CatalogItem_t *pRealItem = FindRealCatalogItem( unDefIndex, unPaintKit );
		if ( pRealItem )
			return ( int )pRealItem->m_unRarity;
	}

	for ( int i = 0; i < k_nExtraCatalogCount; ++i )
	{
		char szItemID[ 64 ];
		MakeItemID( szItemID, sizeof( szItemID ), k_ExtraCatalog[ i ].m_unDefIndex, k_ExtraCatalog[ i ].m_unPaintKit );
		if ( !V_stricmp( pchItemID, szItemID ) )
			return ( int )k_ExtraCatalog[ i ].m_unRarity;
	}

	return 0;
}

int se_faux_econ::GetRealItemCount()
{
	EnsureRealLoaded();
	return s_RealCatalog.Count();
}

//-----------------------------------------------------------------------------
// Purpose: the loadout wheel asks this for each slot (see the header).
//-----------------------------------------------------------------------------
bool se_faux_econ::GetSlotItem( const char *pchSubPosition, int nTeam, SlotItem_t &outItem )
{
	EnsureRealLoaded();

	if ( !pchSubPosition || !pchSubPosition[ 0 ] )
		return false;

	const bool bWantT = ( nTeam == 2 );
	const bool bWantCT = ( nTeam == 3 );

	const RealSlot_t *pBest = NULL;
	int nBestScore = -1;

	for ( int i = 0; i < s_RealSlots.Count(); ++i )
	{
		const RealSlot_t &slot = s_RealSlots[ i ];
		if ( V_stricmp( slot.m_strSubPosition.Get(), pchSubPosition ) )
			continue;

		int nScore = 0;
		if ( bWantT && slot.m_bT )
			nScore += 4;
		if ( bWantCT && slot.m_bCT )
			nScore += 4;
		if ( slot.m_bDefault )
			nScore += 2;

		if ( nScore > nBestScore )
		{
			nBestScore = nScore;
			pBest = &slot;
		}
	}

	if ( !pBest )
		return false;

	const RealDef_t *pDef = FindRealDef( pBest->m_unDefIndex );
	if ( !pDef || !pDef->m_strNameToken.Get() )
		return false;

	static CUtlString s_strSlotImage;	// returned through the caller's SlotItem_t, read right away
	if ( pDef->m_strBaseImage.Get() && pDef->m_strBaseImage.Get()[ 0 ] )
	{
		char szFullImage[ 512 ];
		BuildFullImagePath( pDef->m_strBaseImage.Get(), szFullImage, sizeof( szFullImage ) );
		s_strSlotImage = CUtlString( szFullImage );
	}
	else
	{
		s_strSlotImage.Clear();
	}

	outItem.m_unDefIndex = pDef->m_unDefIndex;
	outItem.m_pchNameToken = pDef->m_strNameToken.Get();
	outItem.m_pchImage = s_strSlotImage.Get();
	outItem.m_bDefault = pBest->m_bDefault;
	return true;
}

void se_faux_econ::SortItemIDs( CUtlVector<CUtlString> &vecIDs, const char *pchSort )
{
	if ( !pchSort || !pchSort[ 0 ] || !V_stricmp( pchSort, "inv_sort_age" ) )
		return;		// the catalog order is the "age" order (newest additions last)

	if ( !V_stricmp( pchSort, "inv_sort_rarity" ) || !V_stricmp( pchSort, "inv_sort_quality" ) )
	{
		vecIDs.SortPredicate( []( const CUtlString &lhs, const CUtlString &rhs )
		{
			const int nLHS = GetRarityForItemID( lhs.Get() );
			const int nRHS = GetRarityForItemID( rhs.Get() );
			if ( nLHS != nRHS )
				return nLHS > nRHS;

			return V_stricmp( lhs.Get(), rhs.Get() ) < 0;
		} );
		return;
	}

	if ( !V_stricmp( pchSort, "inv_sort_alpha" ) )
	{
		vecIDs.SortPredicate( []( const CUtlString &lhs, const CUtlString &rhs )
		{
			return V_stricmp( lhs.Get(), rhs.Get() ) < 0;
		} );
		return;
	}

	// inv_sort_slot / inv_sort_collection / inv_sort_equipped / inv_sort_paint / inv_sort_wear have no
	// meaning for this data set (no loadout slots, no collections, no wear), so they keep the catalog
	// order instead of pretending to sort.
}
