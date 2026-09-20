//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of the "CountdownTimer" panel type - see the header for the contract the
//          newer CS:GO layouts expect.  The ticking follows CCountdown (panorama/controls/
//          countdown.cpp): a ReadyForDisplay handler starts it and an async event re-arms itself
//          every update-interval seconds.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "panorama/se_countdown_timer.h"

#include "uijsregistration.h"

#include <math.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace panorama;

REGISTER_PANEL2D_FACTORY( CSE_CountdownTimer, CountdownTimer );

DECLARE_PANORAMA_EVENT1( UpdateCountdownTimerInternal, int );
DEFINE_PANORAMA_EVENT( UpdateCountdownTimerInternal );

//-----------------------------------------------------------------------------
// Purpose: "7000s" / "500ms" / "12.5" -> seconds
//-----------------------------------------------------------------------------
static double SE_ParseCountdownTime( const char *pchValue )
{
	if ( !pchValue || !pchValue[0] )
		return 0.0;

	double flValue = V_atof( pchValue );
	if ( V_stristr( pchValue, "ms" ) != NULL )
		return flValue / 1000.0;

	return flValue; // "s" suffix or a bare number: seconds
}

CSE_CountdownTimer::CSE_CountdownTimer( CPanel2D *pParent, const char *pchID )
	: CPanel2D( pParent, pchID )
	, m_flTimeLeft( 0 )
	, m_flLastTickTime( 0 )
	, m_flUpdateInterval( 1.0f )
	, m_strTimeDialogVariable( "duration" )
	, m_bOutputMilliseconds( false )
	, m_bUpdateEnabled( false )
	, m_nUpdateCount( 0 )
{
	RegisterForReadyEvents( true );

	if ( !UIEngine()->BHaveEventHandlersRegisteredForType( CSE_CountdownTimer::GetPanelSymbol() ) )
	{
		RegisterEventHandlerOnPanelType( ReadyForDisplay(), &CSE_CountdownTimer::EventReadyForDisplay );
		RegisterEventHandlerOnPanelType( UpdateCountdownTimerInternal(), &CSE_CountdownTimer::EventUpdateTimer );
	}
}

CSE_CountdownTimer::~CSE_CountdownTimer()
{
}

void CSE_CountdownTimer::SetTimeLeft( double flSeconds )
{
	m_flTimeLeft = flSeconds;
	if ( m_flTimeLeft < 0.0 )
		m_flTimeLeft = 0.0;

	// A timeleft from JS (mapdraft.js) re-arms the ticking even if the attribute never came.
	m_bUpdateEnabled = true;
	m_flLastTickTime = Plat_FloatTime();

	PublishTime();
	StartTimer();
}

void CSE_CountdownTimer::SetUpdateInterval( float flUpdateInterval )
{
	if ( flUpdateInterval > 0.01f )
		m_flUpdateInterval = flUpdateInterval;
}

void CSE_CountdownTimer::SetTimeDialogVariable( const char *pszTimeDialogVariable )
{
	if ( pszTimeDialogVariable && pszTimeDialogVariable[0] )
		m_strTimeDialogVariable = pszTimeDialogVariable;
}

bool CSE_CountdownTimer::BSetProperty( CPanoramaSymbol symName, const char *pchValue )
{
	static const CPanoramaSymbol k_symTimeLeft( "timeleft" );
	static const CPanoramaSymbol k_symOutputMilliseconds( "output_milliseconds" );
	static const CPanoramaSymbol k_symUpdateInterval( "update-interval" );
	static const CPanoramaSymbol k_symTimeDialogVariable( "time-dialog-variable" );
	static const CPanoramaSymbol k_symClock( "clock" );
	static const CPanoramaSymbol k_symStartTime( "start-time" );
	static const CPanoramaSymbol k_symEndTime( "end-time" );

	if ( symName == k_symTimeLeft )
	{
		SetTimeLeft( SE_ParseCountdownTime( pchValue ) );
		return true;
	}
	else if ( symName == k_symOutputMilliseconds )
	{
		SetOutputMilliseconds( V_atoi( pchValue ) != 0 );
		return true;
	}
	else if ( symName == k_symUpdateInterval )
	{
		SetUpdateInterval( V_atof( pchValue ) );
		return true;
	}
	else if ( symName == k_symTimeDialogVariable )
	{
		SetTimeDialogVariable( pchValue );
		return true;
	}
	else if ( symName == k_symClock || symName == k_symStartTime || symName == k_symEndTime )
	{
		// SE port: accepted for compatibility.  mapdraft.xml passes clock="game-tick" and
		// end-time="0" but drives the value through .timeleft, so there is nothing to do here.
		return true;
	}

	return BaseClass::BSetProperty( symName, pchValue );
}

void CSE_CountdownTimer::SetupJavascriptObjectTemplate()
{
	BaseClass::SetupJavascriptObjectTemplate();

	RegisterJSAccessor( "timeleft", PANORAMA_DELEGATE( &CSE_CountdownTimer::GetTimeLeft ), PANORAMA_DELEGATE( &CSE_CountdownTimer::SetTimeLeft ) );
	RegisterJSAccessor( "timeLeft", PANORAMA_DELEGATE( &CSE_CountdownTimer::GetTimeLeft ), PANORAMA_DELEGATE( &CSE_CountdownTimer::SetTimeLeft ) );
}

bool CSE_CountdownTimer::EventReadyForDisplay( const CPanelPtr< IUIPanel > &panelPtr )
{
	if ( m_bUpdateEnabled )
		StartTimer();

	return true;
}

void CSE_CountdownTimer::StartTimer()
{
	if ( !m_bUpdateEnabled || m_flUpdateInterval <= 0.0f )
		return;

	if ( !BReadyForDisplay() )
		return;

	// SE port: only one async update chain per instance - the token (m_nUpdateCount) is what the
	// handler checks, and raising it here drops any chain that was armed before this call.
	++m_nUpdateCount;
	DispatchEventAsync( m_flUpdateInterval, UpdateCountdownTimerInternal(), this, m_nUpdateCount );
}

bool CSE_CountdownTimer::EventUpdateTimer( int nUpdateCount )
{
	if ( nUpdateCount != m_nUpdateCount || !m_bUpdateEnabled )
		return true; // SE port: stale chain (the timer was re-armed or disabled)

	double flNow = Plat_FloatTime();
	double flElapsed = flNow - m_flLastTickTime;
	m_flLastTickTime = flNow;

	if ( flElapsed > 0.0 )
	{
		m_flTimeLeft -= flElapsed;
		if ( m_flTimeLeft < 0.0 )
			m_flTimeLeft = 0.0;
	}

	PublishTime();

	if ( BReadyForDisplay() )
		DispatchEventAsync( m_flUpdateInterval, UpdateCountdownTimerInternal(), this, ++m_nUpdateCount );

	return true;
}

void CSE_CountdownTimer::PublishTime()
{
	int nSeconds = (int)floor( m_flTimeLeft );
	if ( nSeconds < 0 )
		nSeconds = 0;

	SetDialogVariable( m_strTimeDialogVariable, nSeconds );

	if ( m_bOutputMilliseconds )
	{
		int nMilliseconds = (int)floor( ( m_flTimeLeft - (double)nSeconds ) * 1000.0 );
		if ( nMilliseconds < 0 )
			nMilliseconds = 0;
		else if ( nMilliseconds > 999 )
			nMilliseconds = 999;

		SetDialogVariable( "milliseconds", nMilliseconds );
	}
}
