//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of the "CountdownTimer" panel type.
//
//          The newer CS:GO content this port ships uses the type in three places:
//            layout/buymenu.xml         <CountdownTimer id="Countdown" output_milliseconds="false">
//                                         <Label text="{t:d:duration}"/> ...
//            layout/mapdraft.xml        <CountdownTimer clock="game-tick" end-time="0"
//                                         update-interval="1.0" time-dialog-variable="duration">
//                                       (mapdraft.js:129 writes .timeleft = nTimeRemaining)
//            layout/controlslibrary.xml <CountdownTimer timeleft="7000s" output_milliseconds="true">
//                                         <Label text="{t:d:duration}:{s:milliseconds}"/>
//
//          This tree's framework only has CCountdown ("Countdown"), which counts down to an
//          absolute time_t and does not know "timeleft".  So what follows is written against the
//          content's contract instead:
//
//            timeleft              duration to count down from, "7000s" (seconds), "500ms"
//                                  (milliseconds) or a bare number (seconds); readable/writable
//                                  from JS ("timeleft" / "timeLeft")
//            output_milliseconds   also publish the "milliseconds" dialog variable (0..999)
//            update-interval       seconds between updates (default 1.0)
//            time-dialog-variable  dialog variable that receives the whole seconds (default
//                                  "duration")
//            clock / start-time / end-time  accepted for compatibility; the port uses the frame
//                                  clock (Plat_FloatTime) as its time source
//
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

class CSE_CountdownTimer : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CSE_CountdownTimer, CPanel2D );

public:
	CSE_CountdownTimer( panorama::CPanel2D *pParent, const char *pchID );
	virtual ~CSE_CountdownTimer();

	// seconds left; mirrors the "timeleft" attribute / JS accessor
	void SetTimeLeft( double flSeconds );
	double GetTimeLeft() const { return m_flTimeLeft; }

	void SetUpdateInterval( float flUpdateInterval );
	float GetUpdateInterval() const { return m_flUpdateInterval; }

	void SetTimeDialogVariable( const char *pszTimeDialogVariable );
	const char *GetTimeDialogVariable() const { return m_strTimeDialogVariable.Get(); }

	void SetOutputMilliseconds( bool bOutput ) { m_bOutputMilliseconds = bOutput; }
	bool GetOutputMilliseconds() const { return m_bOutputMilliseconds; }

	virtual bool BSetProperty( panorama::CPanoramaSymbol symName, const char *pchValue ) OVERRIDE;
	virtual void SetupJavascriptObjectTemplate() OVERRIDE;

private:
	bool EventReadyForDisplay( const panorama::CPanelPtr< panorama::IUIPanel > &panelPtr );
	bool EventUpdateTimer( int nUpdateCount );

	void StartTimer();
	void PublishTime();

	double m_flTimeLeft;
	double m_flLastTickTime;
	float m_flUpdateInterval;
	CUtlString m_strTimeDialogVariable;
	bool m_bOutputMilliseconds;
	bool m_bUpdateEnabled;
	int m_nUpdateCount;
};
