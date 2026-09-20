//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_radial_selector.h - the
//          "CSGORadialSelector" panel type.  The content uses it for the loadout item wheel
//          (layout/loadout.xml, <CSGORadialSelector id="ItemWheel">) and the buy menu's radial
//          weapon picker; both layouts define the Radial_SixSlot / Radial_FourSlot snippets that
//          the constructor loads.
//
//          Detects mouse position and dispatches events to child panels.
//
//          Differences from CS:GO's header: none in the class shape.  The include set is the
//          port's (CS:GO got CDefaultInputCapture through cbase.h; here it is included directly).
//
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"
#include "panorama/iuipanel.h"
#include "panorama/controls/panelptr.h"
#include "panorama/input/iuiinput.h"

DECLARE_PANEL_EVENT1( RadialSelectorHoverPanelChange, panorama::CPanelPtr< panorama::CPanel2D > );

class CCSGO_RadialSelector : public panorama::CPanel2D, public panorama::CDefaultInputCapture
{
	DECLARE_PANEL2D( CCSGO_RadialSelector, panorama::CPanel2D );

public:

	enum ELayoutType
	{
		k_eSixChoices,
		k_eFourChoices
	};

	CCSGO_RadialSelector( panorama::CPanel2D *pParent, const char *pchID, ELayoutType nLayoutType = k_eSixChoices );
	~CCSGO_RadialSelector();

	void Enable( void );
	void Disable( void );

	// CDefaultInputCapture
	virtual bool OnCapturedMouseMove( panorama::IUIPanel *pPanel, float flMouseX, float flMouseY ) OVERRIDE;
	virtual bool OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code ) OVERRIDE;

	panorama::CPanel2D* GetSelectedPanel( void ) const { return m_hSelectedPanel.Get(); }

protected:
	bool BSetProperties(const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties) OVERRIDE;

	void UpdateHoverPanel( CPanel2D *pPrev, CPanel2D *pCurSelection );
	bool EventMouseOver( const panorama::CPanelPtr< panorama::IUIPanel > & pPanel );
	bool EventMouseOut( const panorama::CPanelPtr< panorama::IUIPanel > & pPanel );

	float	m_angStart;
	panorama::CPanelPtr< panorama::CPanel2D > m_hSelectedPanel;

	CUtlString m_rolloverSound, m_clickSound;
};
