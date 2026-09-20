//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Menu to change player loadout
//
// SE port: CS:GO's game/client/cstrike15/panorama/csgo_loadout.* with the loadout-state pieces
// dropped.  In CS:GO this panel is fed by CStrikeLoadout / CUiComponent_Loadout (which loadout item
// is equipped in which slot, per team) and it hosts a radial selector (CCSGO_RadialSelector, ported
// in csgo_radial_selector.*).  What is left here is the panel itself: it loads loadout.xml, fills the
// radial selector's wedges the way CS:GO does (ItemWedge snippet + equip_slot + activate event +
// item icon - only the item data comes from the port's faux catalog), and answers the two events the
// content raises (ShowLoadout / Loadout_FilterForPosition) by pointing its item list at that catalog.
// "Which item is equipped" stays unimplemented (LoadoutAPI is the JS layer's job).
//
//=============================================================================//

#ifndef CSGO_LOADOUT_H
#define CSGO_LOADOUT_H

#pragma once

#include "panorama/controls/panel2d.h"
#include "tier1/utlstring.h"

class CCSGO_InventoryItemList;
class CCSGO_RadialSelector;

DECLARE_PANORAMA_EVENT2( ShowLoadout, const char *, int );
DECLARE_PANORAMA_EVENT2( Loadout_FilterForPosition, int, int );

class CCSGO_Loadout : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_Loadout, panorama::CPanel2D );

public:
	CCSGO_Loadout( panorama::CPanel2D *pParent, const char *pchID );

	bool EventShowLoadout( const char *szLoadoutSlot, int nTeam );
	bool EventFilterForPosition( int nTeam, int nPosition );

	// SE port: called once the panel description that created this class has been applied, which is
	// when the wheel child is guaranteed to exist in every loading order (see EnsureWheelReady).
	virtual void OnInitializedFromLayout() OVERRIDE;

private:
	// CS:GO holds these as CPanelPtr (weak references); plain pointers are enough here - the loadout
	// panel and its children live as long as the main menu does.
	panorama::CPanel2D *m_pTeamLogo;
	CCSGO_RadialSelector *m_pItemWheel;
	CCSGO_InventoryItemList *m_pItemList;

	// the slot group the wheel shows ("secondary" / "smg" / "rifle" / "heavy") and the team it was
	// last filtered for - EventFilterForPosition() names the wedge the user picked from these two.
	CUtlString m_strSlotGroup;
	int m_nTeam;

	// Every "slot" (secondary / smg / rifle / heavy / gear) filters the same faux catalog in this
	// build - there is no loadout state to restrict it to.
	void ShowItemsForSlot( const char *szLoadoutSlot, int nTeam );

	// CS:GO: UpdatePanelsForLoadout() walks the wheel's wedges for one team's loadout position range
	// (equip_slot attribute + activate event + item icon).  The port fills the same fields from the
	// faux catalog instead of the real loadout state.
	void UpdatePanelsForLoadout( int nTeam, const char *szSlotGroup );
	void FillWheelFromCatalog();

	// SE port: the wheel can still be missing when the constructor runs (the panel description that
	// creates it may be applied after this class is constructed), so every entry point re-runs the
	// setup until it sticks.  See EnsureWheelReady() in the .cpp.
	bool EnsureWheelReady();
};

#endif // CSGO_LOADOUT_H
