//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_radial_selector.cpp - see the header.
//
//          The body is CS:GO's, with these port notes:
//            * CDefaultInputCapture / MouseData_t come from panorama/input/iuiinput.h and the mouse
//              constants are plain enums here (CS:GO: MouseCode::MOUSE_LEFT, port: MOUSE_LEFT).
//            * Min() / RAD2DEG() / fmod() are written out longhand so the file does not depend on
//              the mathlib macros.
//            * The sounds go through the port's UISoundSystem (wrap_sound.cpp), which is what
//              CS:GO's UIPanorama.loadout_sector_* events resolve to as well.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "panorama/csgo_radial_selector.h"
#include "panorama/ui_root.h"
#include "panorama/uijsregistration.h"
#include "panorama/iuisoundsystem.h"

#include <math.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

DEFINE_PANORAMA_EVENT( RadialSelectorHoverPanelChange );

DECLARE_PANEL_EVENT0( RadialSelectorMouseOver )
DEFINE_PANORAMA_EVENT( RadialSelectorMouseOver )

DECLARE_PANEL_EVENT0( RadialSelectorMouseOut )
DEFINE_PANORAMA_EVENT( RadialSelectorMouseOut )

REGISTER_PANEL2D_FACTORY( CCSGO_RadialSelector, CSGORadialSelector )

CCSGO_RadialSelector::CCSGO_RadialSelector( CPanel2D *pParent, const char *pchID, ELayoutType nLayoutType /*= k_eSixChoices*/ )
: CPanel2D( pParent, pchID )
, m_angStart( 0 )
{
	if ( nLayoutType == k_eSixChoices )
	{
		BLoadLayoutSnippet( "Radial_SixSlot" );
		m_angStart = -150.f;
	}
	else if ( nLayoutType == k_eFourChoices )
	{
		BLoadLayoutSnippet( "Radial_FourSlot" );
		m_angStart = -135.f;
	}
	else
	{
		Assert( 0 ); // Needs custom layout snippet in xml
	}

	// We handle hover and click events for contained children, deny them hittest directly
	SetHitTestEnabled( true );
	SetHitTestChildrenEnabled( false );

	SetOnMouseOverEvent( RadialSelectorMouseOver::MakeEvent( this ) );
	SetOnMouseOutEvent( RadialSelectorMouseOut::MakeEvent( this ) );

	RegisterEventHandler( RadialSelectorMouseOver(), this, &CCSGO_RadialSelector::EventMouseOver );
	RegisterEventHandler( RadialSelectorMouseOut(), this, &CCSGO_RadialSelector::EventMouseOut );

	SetVisible( false );
}

CCSGO_RadialSelector::~CCSGO_RadialSelector()
{
	if ( UIInputEngine()->GetInputCapture().HasElement( this ) )
		UIInputEngine()->ReleaseInputCapture( this );
}

bool CCSGO_RadialSelector::BSetProperties(const CUtlVector< panorama::ParsedPanelProperty_t > &vecProperties)
{
	static CPanoramaSymbol k_symSoundClick("sound_click");
	static CPanoramaSymbol k_symSoundRollover("sound_rollover");


	FOR_EACH_VEC(vecProperties, i)
	{
		const ParsedPanelProperty_t &prop = vecProperties[i];
		if(prop.m_symName == k_symSoundClick)
		{
			m_clickSound = prop.m_pchValue;
		}
		else if(prop.m_symName == k_symSoundRollover)
		{
			m_rolloverSound = prop.m_pchValue;
		}
	}

	return true;
}

void CCSGO_RadialSelector::Enable( void )
{
	if ( !BIsVisible() )
	{
		UpdateHoverPanel( nullptr, m_hSelectedPanel.Get() );
		SetVisible( true );
		UIInputEngine()->SetInputCapture( this );
	}
}

void CCSGO_RadialSelector::Disable( void )
{
	if ( BIsVisible() )
	{
		UpdateHoverPanel( m_hSelectedPanel.Get(), nullptr );
		SetVisible( false );
		m_hSelectedPanel.Clear();
		UIInputEngine()->ReleaseInputCapture( this );
	}
}

bool CCSGO_RadialSelector::OnCapturedMouseMove( IUIPanel *pPanel, float flMouseX, float flMouseY )
{
	CPanel2D* pPrevSelection = m_hSelectedPanel.Get();
	m_hSelectedPanel.Clear();
	if ( pPanel == UIPanel() )
	{
		Vector2D vecCenter( GetActualLayoutWidth() / 2, GetActualLayoutHeight() / 2 );
		// SE port: Min() longhand (mathlib's Min is a macro in some configurations).
		float flRadius = vecCenter.x < vecCenter.y ? vecCenter.x : vecCenter.y;
		Vector2D vecMouse( flMouseX - vecCenter.x, flMouseY - vecCenter.y );
		float distSq = vecMouse.LengthSqr();
		if ( distSq < flRadius*flRadius )
		{
			// SE port: RAD2DEG / fmod longhand.
			float ang = atan2f( vecMouse.y, vecMouse.x ) * ( 180.0f / 3.14159265358979323846f );
			ang -= m_angStart;
			if ( ang < 0.f )
				ang += 360.f;
			ang = fmodf( ang, 360.f );

			int nPanelIdx = (int)( ang / ( 360.0f / (float)GetChildCount() ) );

			if ( nPanelIdx >= GetChildCount() )
				nPanelIdx = GetChildCount() - 1; // SE port: guard the rounding edge (360.f == ang)

			Assert( GetChild( nPanelIdx ) );
			m_hSelectedPanel = GetChild( nPanelIdx );
		}
	}

	if ( pPrevSelection != m_hSelectedPanel.Get() )
	{
		UpdateHoverPanel( pPrevSelection, m_hSelectedPanel.Get() );
	}

	return false;
}

void CCSGO_RadialSelector::UpdateHoverPanel( CPanel2D *pPrev, CPanel2D *pCurSelection )
{
	static CPanoramaSymbol k_symRadialHover( "radial-select-hover" );
	static CPanoramaSymbol k_symPropertyOnMouseOver( "onmouseover" );
	static CPanoramaSymbol k_symPropertyOnMouseOut( "onmouseout" );
	if ( pPrev )
	{
		pPrev->RemoveClass( k_symRadialHover );
		pPrev->DispatchPanelEvent( k_symPropertyOnMouseOut );
	}
	if ( pCurSelection )
	{
		pCurSelection->AddClass( k_symRadialHover );
		pCurSelection->DispatchPanelEvent( k_symPropertyOnMouseOver );
	}
	DispatchEvent( RadialSelectorHoverPanelChange(), GetParent(), pCurSelection );

	if(m_rolloverSound.Length() > 0)
	{
		UIEngine()->UISoundSystem()->PlaySound(m_rolloverSound.Get(), nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f);
	}
}



bool CCSGO_RadialSelector::OnCapturedMouseButtonUp( panorama::IUIPanel *pPanel, const panorama::MouseData_t &code )
{
	// SE port: the enum constant is qualified because ::MOUSE_LEFT (SDL) is visible here too.
	if ( m_hSelectedPanel.Get() && m_hSelectedPanel->IsEnabled() && code.m_MouseCode == panorama::MOUSE_LEFT )
	{
		DispatchEvent( Activated(), m_hSelectedPanel.Get(), k_ePanelEventSourceProgram );

		if( m_clickSound.Length() > 0 )
		{
			UIEngine()->UISoundSystem()->PlaySound(m_clickSound.Get(), nullptr, panorama::k_ESoundType_Effects, 1.0f, 0.5f, 0.0f);
		}

	}
	return false;
}

bool CCSGO_RadialSelector::EventMouseOver( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	static CPanoramaSymbol k_symPropertyOnMouseOver( "onmouseover" );
	if ( m_hSelectedPanel.Get() )
		m_hSelectedPanel->DispatchPanelEvent( k_symPropertyOnMouseOver );

	return false;
}

bool CCSGO_RadialSelector::EventMouseOut( const panorama::CPanelPtr< panorama::IUIPanel > &pPanel )
{
	static CPanoramaSymbol k_symPropertyOnMouseOut( "onmouseout" );
	if ( m_hSelectedPanel.Get() )
		m_hSelectedPanel->DispatchPanelEvent( k_symPropertyOnMouseOut );

	return false;
}
