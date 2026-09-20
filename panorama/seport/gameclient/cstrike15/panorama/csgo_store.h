//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_store.h - the "SalesBanner" panel type
//          (the main-menu store banner; layout/store.xml's root element is <SalesBanner>).
//
//=============================================================================//
#pragma once

#include "panorama/controls/panel2d.h"

class CCSGO_SalesBanner : public panorama::CPanel2D
{
	DECLARE_PANEL2D( CCSGO_SalesBanner, panorama::CPanel2D );
public:
	CCSGO_SalesBanner( panorama::CPanel2D *pParent, const char* pchID );
	virtual ~CCSGO_SalesBanner();
};
