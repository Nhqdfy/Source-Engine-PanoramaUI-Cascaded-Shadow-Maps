//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: SE port of game/client/cstrike15/panorama/csgo_store.cpp - the "SalesBanner" panel
//          type.  CS:GO's version is 29 lines: it remembers itself in a global (the game client
//          reaches the banner through it) and loads layout/store.xml into itself.
//
//          Port note: CS:GO asserts that only one banner exists.  The store page can be reloaded
//          (and this port currently drives pages through the JS layer), so a second instance is
//          tolerated with a warning instead of an assert.
//
//=============================================================================//

#include "panorama/se_gameclient_common.h"
#include "panorama/csgo_store.h"

REGISTER_PANEL2D_FACTORY( CCSGO_SalesBanner, SalesBanner );

CCSGO_SalesBanner* g_pCSGOSalesBanner = NULL;

using namespace panorama;

CCSGO_SalesBanner::CCSGO_SalesBanner( CPanel2D *pParent, const char *pchID ) : CPanel2D( pParent, pchID )
{
	// SE port: CS:GO's Assert( !g_pCSGOSalesBanner ) fires while a page is being reloaded.
	if ( g_pCSGOSalesBanner != NULL && g_pCSGOSalesBanner != this )
	{
		Msg( "[SE port] SalesBanner: replacing previous instance %p with %p\n", (void*)g_pCSGOSalesBanner, (void*)this );
	}
	g_pCSGOSalesBanner = this;

	DbgVerify( BLoadLayout( "file://{resources}/layout/store.xml" ) );
	SetAcceptsInput( true );
}

CCSGO_SalesBanner::~CCSGO_SalesBanner()
{
	if ( g_pCSGOSalesBanner == this )
		g_pCSGOSalesBanner = NULL;
}
