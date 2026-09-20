//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Menu to change player loadout
//
// SE port: see csgo_loadout.h for what was kept and what was dropped.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"

#include "panorama/csgo_loadout.h"
#include "panorama/csgo_inventory_item_list.h"
#include "panorama/csgo_item_image_panel.h"
#include "panorama/csgo_radial_selector.h"
#include "panorama/se_faux_econ.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

DEFINE_PANORAMA_EVENT( ShowLoadout );
DEFINE_PANORAMA_EVENT( Loadout_FilterForPosition );

REGISTER_PANEL2D_FACTORY( CCSGO_Loadout, CSGOLoadout );

using namespace panorama;

// CS:GO's loadout subposition names (game/shared/cstrike15/cstrike15_item_schema.cpp::
// g_szLoadoutStringsSubPositions).  The wheel shows one wedge per subposition of the slot group the
// content's slot radio picked; the remaining wedges of the six-slot snippet stay disabled.  The
// counts match CS:GO's GetLoadoutPositionRangeForSlot() (secondary/smg/heavy end at *4, rifles at *5).
enum { SE_PORT_TEAM_TERRORIST = 2, SE_PORT_TEAM_CT = 3 };

static const char *k_SE_LoadoutSubPositions_Secondary[] = { "secondary0", "secondary1", "secondary2", "secondary3", "secondary4" };
static const char *k_SE_LoadoutSubPositions_SMG[] = { "smg0", "smg1", "smg2", "smg3", "smg4" };
static const char *k_SE_LoadoutSubPositions_Rifle[] = { "rifle0", "rifle1", "rifle2", "rifle3", "rifle4", "rifle5" };
static const char *k_SE_LoadoutSubPositions_Heavy[] = { "heavy0", "heavy1", "heavy2", "heavy3", "heavy4" };

//-----------------------------------------------------------------------------
// Purpose: CS:GO: GetLoadoutPositionRangeForSlot(); returns the subposition names of a slot group.
//-----------------------------------------------------------------------------
static const char **SE_LoadoutSubPositionsForGroup( const char *szSlotGroup, int *pnOutCount )
{
	*pnOutCount = 0;
	if ( !szSlotGroup || !szSlotGroup[0] )
		return NULL;

	if ( V_strncmp( szSlotGroup, "secondary", 9 ) == 0 )
	{
		*pnOutCount = ARRAYSIZE( k_SE_LoadoutSubPositions_Secondary );
		return k_SE_LoadoutSubPositions_Secondary;
	}
	else if ( V_strncmp( szSlotGroup, "smg", 3 ) == 0 )
	{
		*pnOutCount = ARRAYSIZE( k_SE_LoadoutSubPositions_SMG );
		return k_SE_LoadoutSubPositions_SMG;
	}
	else if ( V_strncmp( szSlotGroup, "rifle", 5 ) == 0 )
	{
		*pnOutCount = ARRAYSIZE( k_SE_LoadoutSubPositions_Rifle );
		return k_SE_LoadoutSubPositions_Rifle;
	}
	else if ( V_strncmp( szSlotGroup, "heavy", 5 ) == 0 )
	{
		*pnOutCount = ARRAYSIZE( k_SE_LoadoutSubPositions_Heavy );
		return k_SE_LoadoutSubPositions_Heavy;
	}

	return NULL;
}

CCSGO_Loadout::CCSGO_Loadout( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_pTeamLogo( NULL )
	, m_pItemWheel( NULL )
	, m_pItemList( NULL )
	, m_nTeam( SE_PORT_TEAM_CT )
{
	SetInputNamespace( "loadout" );

	SetAcceptsInput( true );
	SetAcceptsFocus( true );

	// CS:GO starts on the CT secondary range (UpdatePanelsForLoadout( TEAM_CT, SECONDARY0, SECONDARY4 )).
	m_strSlotGroup.Set( "secondary" );

	BLoadLayout( "file://{resources}/layout/loadout.xml" );

	// CS:GO uses RequireChildInLayoutFile() for all three; the port only requires the item list (the
	// one child it actually needs) and looks the others up defensively, because the radial selector
	// is one of the CS:GO panel types this port still substitutes with a plain Panel.
	m_pTeamLogo = FindChildInLayoutFile( "TeamLogo" );
	m_pItemWheel = panel_cast< CCSGO_RadialSelector * >( FindChildInLayoutFile( "ItemWheel" ) );
	m_pItemList = panel_cast< CCSGO_InventoryItemList* >( FindChildInLayoutFile( "LoadoutItemList" ) );

	if ( !m_pItemList )
	{
		Warning( "CCSGO_Loadout: loadout.xml has no LoadoutItemList panel - the item list stays empty\n" );
	}

	// SE port: the wheel is set up here when it already exists, and again from every entry point
	// (see EnsureWheelReady) - in this port the layout description that creates it may still be
	// applied after this constructor runs, while CS:GO can rely on BLoadLayout having done it.
	EnsureWheelReady();

	if ( m_pItemList )
		ShowItemsForSlot( "any", m_nTeam );

	RegisterForUnhandledEvent( ShowLoadout(), this, &CCSGO_Loadout::EventShowLoadout );
	RegisterEventHandler( Loadout_FilterForPosition(), this, &CCSGO_Loadout::EventFilterForPosition );
}

//-----------------------------------------------------------------------------
// Purpose: point the item list at the faux catalog for the requested slot
//-----------------------------------------------------------------------------
void CCSGO_Loadout::ShowItemsForSlot( const char *szLoadoutSlot, int nTeam )
{
	if ( !m_pItemList )
		return;

	// The list shows the items that fit the selected slot: the catalog's group tier carries every
	// item's real loadout slot ("secondary1", "rifle0", ... - items_game.txt "item_sub_position"),
	// and the list matches the requested slot as a prefix - so a wedge ("secondary4") selects that
	// one slot while the panel's own range name ("secondary") selects secondary0..5.  The team filter
	// keeps the page to the team it is showing (items_game "used_by_classes") plus neutral items.
	const char *pchGroup = ( szLoadoutSlot && szLoadoutSlot[ 0 ] ) ? szLoadoutSlot : "any";
	m_pItemList->SetTeamFilter( ( nTeam == SE_PORT_TEAM_TERRORIST ) ? "t" : "ct" );
	m_pItemList->SetCategorySortAndFilters( "inv_category_any", "any", pchGroup, "inv_sort_age" );

	Msg( "[SE port] CCSGO_Loadout: showing items for slot '%s' (team %d) - %d item(s)\n",
		 pchGroup, nTeam, m_pItemList->GetItemCount() );
}

//-----------------------------------------------------------------------------
// Purpose: SE port: the panel description has been applied - the wheel child exists now.
//-----------------------------------------------------------------------------
void CCSGO_Loadout::OnInitializedFromLayout()
{
	BaseClass::OnInitializedFromLayout();

	EnsureWheelReady();
}

//-----------------------------------------------------------------------------
// Purpose: SE port - bring the radial selector up (CS:GO: the constructor's UpdatePanelsForLoadout +
//          Enable pair).  Returns true once the wheel has been found and switched on.
//          Loading the "ItemWedge" snippet into every wedge's Contents panel is CS:GO's step; the
//          wheel's own constructor loads the Radial_SixSlot snippet, this fills the slots.
//-----------------------------------------------------------------------------
bool CCSGO_Loadout::EnsureWheelReady()
{
	if ( !m_pItemWheel )
		m_pItemWheel = panel_cast< CCSGO_RadialSelector * >( FindChildInLayoutFile( "ItemWheel" ) );

	if ( !m_pItemWheel )
		return false;

	if ( m_pItemWheel->GetChildCount() > 0 && m_pItemWheel->GetChild( 0 )->FindChildTraverse( "ItemImage" ) == NULL )
	{
		// CS:GO: one "ItemWedge" (icon + label) per slot.
		m_pItemWheel->IterateChildren( [&]( CPanel2D *pPanel ) -> bool
		{
			CPanel2D *pContents = pPanel->FindChildTraverse( "Contents" );
			if ( pContents )
				pContents->RequireLoadLayoutSnippet( "ItemWedge" );
			return true;
		} );
	}

	UpdatePanelsForLoadout( m_nTeam, m_strSlotGroup.Get() );

	// Enable() is what makes the wheel visible and gives it the mouse capture (the CS:GO radial
	// selector hides itself in its constructor).
	m_pItemWheel->Enable();

	// SE port: the content's loadout.js unhides this container when the page is opened; doing it here
	// as well is what makes a direct "+panorama_menu" load usable.
	if ( CPanel2D *pContainer = FindChildInLayoutFile( "LoadoutRadialSelector" ) )
		pContainer->SetHasClass( "hidden", false );

	return true;
}

bool CCSGO_Loadout::EventShowLoadout( const char *szLoadoutSlot, int nTeam )
{
	EnsureWheelReady();

	// CS:GO: UpdatePanelsForLoadout( team, range ) + SetHasClass( "loadout--visible", true ) +
	// EventFilterForPosition() + SetFocus().  The slider group comes straight from the content (the
	// slot radios carry data-wedge/slot names "secondary" / "smg" / "rifle" / "heavy").
	UpdatePanelsForLoadout( nTeam, szLoadoutSlot );
	SetHasClass( "loadout--visible", true );
	SetFocus();

	ShowItemsForSlot( szLoadoutSlot, nTeam );
	return true;
}

bool CCSGO_Loadout::EventFilterForPosition( int nTeam, int nPosition )
{
	EnsureWheelReady();

	int nSubPositionCount = 0;
	const char **ppSubPositions = SE_LoadoutSubPositionsForGroup( m_strSlotGroup.Get(), &nSubPositionCount );

	if ( ppSubPositions && nPosition >= 0 && nPosition < nSubPositionCount )
	{
		Msg( "[SE port] CCSGO_Loadout: wedge %d selected -> slot '%s' (team %d)\n",
			 nPosition, ppSubPositions[ nPosition ], nTeam );
		ShowItemsForSlot( ppSubPositions[ nPosition ], nTeam );
	}
	else
	{
		Msg( "[SE port] CCSGO_Loadout: filter for position %d (team %d) - no wedge for it, keeping '%s'\n",
			 nPosition, nTeam, m_strSlotGroup.Get() );
		ShowItemsForSlot( m_strSlotGroup.Get(), nTeam );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: CS:GO's UpdatePanelsForLoadout() for the port: remember team + slot group, keep the team
//          logo in sync, then refill the wheel.
//-----------------------------------------------------------------------------
void CCSGO_Loadout::UpdatePanelsForLoadout( int nTeam, const char *szSlotGroup )
{
	static CPanoramaSymbol k_symTeam( "team" );

	m_nTeam = nTeam;
	if ( szSlotGroup && szSlotGroup[0] )
		m_strSlotGroup.Set( szSlotGroup );

	SetAttribute( k_symTeam, nTeam );

	if ( CImagePanel *pLogo = panel_cast< CImagePanel * >( m_pTeamLogo ) )
		pLogo->SetImage( CFmtStr( "file://{images}/icons/%s.svg", nTeam == SE_PORT_TEAM_TERRORIST ? "t_logo" : "ct_logo" ).Get() );

	FillWheelFromCatalog();
}

//-----------------------------------------------------------------------------
// Purpose: one wedge per loadout subposition: equip_slot + activate event + item icon.  CS:GO takes
//          the item from the equipped loadout; the port takes the slot's base weapon from the real
//          catalog (se_faux_econ, generated from items_game.txt) - so every wedge shows the weapon
//          that actually lives in that slot, with CS:GO's own name token and silhouette icon.
//-----------------------------------------------------------------------------
void CCSGO_Loadout::FillWheelFromCatalog()
{
	static CPanoramaSymbol k_symEquipSlot( "equip_slot" );
	static CPanoramaSymbol k_symLoadoutNoItem( "loadout--no-item" );

	if ( !m_pItemWheel )
		return;

	int nSubPositionCount = 0;
	const char **ppSubPositions = SE_LoadoutSubPositionsForGroup( m_strSlotGroup.Get(), &nSubPositionCount );

	int nWedge = 0;
	int nFilled = 0;
	m_pItemWheel->IterateChildren( [&]( CPanel2D *pPanel ) -> bool
	{
		CItemImagePanel *pImage = panel_cast< CItemImagePanel * >( pPanel->FindChildTraverse( "ItemImage" ) );

		const char *pchSubPosition = ( ppSubPositions != NULL && nWedge < nSubPositionCount ) ? ppSubPositions[ nWedge ] : NULL;

		se_faux_econ::SlotItem_t slotItem;
		const bool bHasItem = ( pchSubPosition != NULL ) && se_faux_econ::GetSlotItem( pchSubPosition, m_nTeam, slotItem );

		pPanel->SetEnabled( bHasItem );
		pPanel->SetHasClass( k_symLoadoutNoItem, !bHasItem );

		if ( bHasItem )
		{
			char szItemID[ 64 ];
			se_faux_econ::MakeItemID( szItemID, sizeof( szItemID ), slotItem.m_unDefIndex, 0 );

			pPanel->SetAttribute( k_symEquipSlot, pchSubPosition );
			pPanel->SetOnActivateEvent( Loadout_FilterForPosition::MakeEvent( this, m_nTeam, nWedge ) );
			++nFilled;

			if ( pImage )
			{
				pImage->SetItemID( CUtlString( szItemID ) );

				// The ItemWedge snippet's label is "{s:item_name}"; CS:GO sets that dialog variable from
				// the econ item, the port takes the item's own localization token
				// ("#SFUI_WPNHUD_AK47", straight out of items_game.txt).
				if ( slotItem.m_pchNameToken )
					pImage->SetDialogVariableLocString( "item_name", slotItem.m_pchNameToken );
			}
		}
		else
		{
			pPanel->RemoveAttribute( k_symEquipSlot );

			if ( pImage )
			{
				CUtlString strEmpty;
				pImage->SetItemID( strEmpty );
			}
		}

		++nWedge;
		return true;
	} );

	Msg( "[SE port] CCSGO_Loadout: wheel filled for slot group '%s' (team %d): %d of %d wedges\n",
		 m_strSlotGroup.Get(), m_nTeam, nFilled, nWedge );
}
