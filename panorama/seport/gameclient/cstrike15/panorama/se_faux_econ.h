//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: SE port - stand-in for the CS:GO econ layer that feeds the inventory UI.
//
//          CS:GO's inventory page is built from game/shared/econ/* (CEconItemView, the item
//          schema, CPlayerInventory's client view tiers) and those are filled from the GC
//          (public/gcsdk/gcclient/*).  This tree has neither: game/shared/econ/ holds only
//          ihasowner.h, there is no GC client stack, and the port runs without Steam.  What the
//          inventory UI actually consumes is small, so this file provides exactly that:
//
//            * item ids in the string form the JS layer already uses
//              ("se_store_<defindex>_<paintindex>", see se_session_sim.js::GetFauxItemIDFromDefAndPaintIndex)
//            * the category / subcategory / group membership the list walks
//              (CS:GO: CPlayerInventory::CClientInventoryViewTier)
//            * a rarity (the tile picks its rarity colour from it) and an icon path
//            * a sort order for the "inv_sort_*" entries the sort dropdown offers
//
//          The table below is DATA, and it mirrors what the CS:GO backend would deliver: the ids are
//          the ones the port's JS layer (se_session_sim.js) already names with tokens, so the UI code
//          keeps its shape and only the source of the data changes.  Adding an item means adding one
//          line here plus the matching answer in se_session_sim.js.
//
//=============================================================================//

#ifndef SE_PORT_FAUX_ECON_H
#define SE_PORT_FAUX_ECON_H

#pragma once

#include "tier1/utlstring.h"
#include "tier1/utlvector.h"

// CS:GO has this in game/shared/econ/econ_item_constants.h, a file this tree does not carry.  It is
// declared here for the ported UI code that still spells the type.
typedef uint64 itemid_t;

namespace se_faux_econ
{
	// The string form of a faux item id.  Must stay byte-identical to the JS side's
	// se_session_sim.js::InventoryAPI.GetFauxItemIDFromDefAndPaintIndex().
	void MakeItemID( char *pchOut, int nOutBufferSize, uint32 unDefIndex, uint32 unPaintKit );

	struct CatalogItem_t
	{
		uint32		m_unDefIndex;
		uint32		m_unPaintKit;
		const char *m_pchCategory;		// inventory category tag (a panel id in the content)
		const char *m_pchSubCategory;
		const char *m_pchGroup;			// the loadout slot for the real items ("secondary1"), "any" for the static ones
		uint32		m_unRarity;			// 0 (consumer) .. 6 (covert), 7 = gold/knife class
		const char *m_pchImage;			// "file://{images}/..." icon for the tile, may be NULL
		const char *m_pchTeam;			// "" = both teams, "t" / "ct" = that team only (real items)
	};

	int GetCatalogCount();
	const CatalogItem_t &GetCatalogItem( int nIndex );
	const CatalogItem_t *FindItemByID( const char *pchItemID );

	// SE port: the localization token of an item's name, the same ones the JS layer builds from
	// STORE_NAMES / EXTRA_NAMES in se_session_sim.js ("#SEPort_Store_Item_<def>", "#SEPort_Op_Q<n>_Name",
	// "#SEPort_Op_Reward_<n>").  Returns NULL for an unknown id.
	const char *GetItemNameToken( const char *pchItemID );

	// category / subcategory lists, in the order the UI should show them ("any" first)
	int GetCategoryCount();
	const char *GetCategory( int nIndex );
	int GetSubCategoryCount( const char *pchCategory );
	const char *GetSubCategory( const char *pchCategory, int nIndex );
	bool IsKnownCategory( const char *pchCategory );

	// the "view tier" walk CCSGO_InventoryItemList::SetCategorySortAndFilters() does before it fills
	// its item vector.  "any" matches everything; the group is the loadout slot (a request is a prefix
	// of the item's slot: "rifle" -> rifle0..5, "rifle1" -> just that slot); the team filter keeps
	// "" / "any" for both teams, otherwise the item has to be team-neutral or match.
	int CollectItemIDs( const char *pchCategory, const char *pchSubCategory, const char *pchGroup,
						CUtlVector<CUtlString> &vecOutIDs, const char *pchTeamFilter = NULL );

	// sorting: the sort string is one of the "inv_sort_*" names the content's dropdown offers
	// (CS:GO: CUiComponent_Inventory::SortFunc_Helper + InventorySortFromName).
	void SortItemIDs( CUtlVector<CUtlString> &vecIDs, const char *pchSort );

	int GetRarityForItemID( const char *pchItemID );

	// ------------------------------------------------------------------------------------------
	// The real half of the catalog: weapons + paint kits generated from the retail items_game.txt
	// (build/_gen_econ_real.ps1 -> scripts/items/se_econ_real.txt, loaded lazily on first use).
	// The static table above stays as the "store / collections" part of the inventory; the real
	// table adds the weapons, their skins and the loadout slots.
	// ------------------------------------------------------------------------------------------

	// One loadout slot entry (items_game.txt "item_sub_position", e.g. "rifle1").
	struct SlotItem_t
	{
		uint32		m_unDefIndex;
		const char *m_pchNameToken;		// "#SFUI_WPNHUD_AK47"
		const char *m_pchImage;			// "file://{images_econ}/econ/weapons/base_weapons/weapon_ak47.png"
		bool		m_bDefault;			// items_game "default_slot_item"
	};

	// Picks the item a loadout slot holds: the team's default item when there is one, else the other
	// team's, else whatever the slot lists first (CS:GO: the per-team default loadout).
	// nTeam is 2 = terrorist / 3 = counter-terrorist (SE_PORT_TEAM_*); anything else ignores the team.
	bool GetSlotItem( const char *pchSubPosition, int nTeam, SlotItem_t &outItem );

	int GetRealItemCount();
}

#endif // SE_PORT_FAUX_ECON_H
