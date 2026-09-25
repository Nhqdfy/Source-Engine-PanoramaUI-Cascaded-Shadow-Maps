//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)

#include "basetypes.h"
#include "mathlib/vmatrix.h"
#include "mathlib/mathlib.h"
#include <string.h>
#include "ssemath.h"
#include "mathlib/vector4d.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning (disable : 4700) // local variable 'x' used without having been initialized

// ------------------------------------------------------------------------------------------- //
// Helper functions.
// ------------------------------------------------------------------------------------------- //


static inline void FrustumPlanesFromMatrixHelper( const VMatrix &shadowToWorld, const Vector &p1, const Vector &p2, const Vector &p3, VPlane &plane )
{
	Vector world1, world2, world3;
	Vector3DMultiplyPositionProjective( shadowToWorld, p1, world1 );
	Vector3DMultiplyPositionProjective( shadowToWorld, p2, world2 );
	Vector3DMultiplyPositionProjective( shadowToWorld, p3, world3 );

	Vector v1, v2;
	VectorSubtract( world2, world1, v1 );
	VectorSubtract( world3, world1, v2 );

	CrossProduct( v1, v2, plane.m_Normal );
	VectorNormalize( plane.m_Normal );
	plane.m_Dist = DotProduct( plane.m_Normal, world1 );	
}

void FrustumPlanesFromMatrix( const VMatrix &clipToWorld, Frustum_t &frustum )
{
	VPlane planes[6];

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 0.0f ), Vector( 1.0f, 0.0f, 0.0f ), Vector( 0.0f, 1.0f, 0.0f ), planes[FRUSTUM_NEARZ] );
	
	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 1.0f ), Vector( 0.0f, 1.0f, 1.0f ), Vector( 1.0f, 0.0f, 1.0f ), planes[FRUSTUM_FARZ] );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 0.0f, 0.0f ), Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 0.0f ), planes[FRUSTUM_RIGHT] );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 1.0f, 1.0f ), Vector( 0.0f, 0.0f, 1.0f ), planes[FRUSTUM_LEFT] );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 1.0f, 0.0f ), Vector( 1.0f, 1.0f, 1.0f ), Vector( 0.0f, 1.0f, 1.0f ), planes[FRUSTUM_TOP] );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), Vector( 1.0f, 0.0f, 1.0f ), planes[FRUSTUM_BOTTOM] );
	
	frustum.SetPlanes(planes);
}

void ExtractClipPlanesFromNonTransposedMatrix( const VMatrix &viewProjMatrix, VPlane *pPlanesOut, bool bD3DClippingRange )
{
	// Left
	Vector4D vPlane = MatrixGetRowAsVector4D( viewProjMatrix, 0 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	pPlanesOut[ FRUSTUM_LEFT ].Init( vPlane.AsVector3D(), -vPlane.w );

	// Right
	vPlane = -MatrixGetRowAsVector4D( viewProjMatrix, 0 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	pPlanesOut[ FRUSTUM_RIGHT ].Init( vPlane.AsVector3D(), -vPlane.w );

	// Bottom
	vPlane = MatrixGetRowAsVector4D( viewProjMatrix, 1 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	pPlanesOut[ FRUSTUM_BOTTOM ].Init( vPlane.AsVector3D(), -vPlane.w );

	// Top
	vPlane = -MatrixGetRowAsVector4D( viewProjMatrix, 1 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	pPlanesOut[ FRUSTUM_TOP ].Init( vPlane.AsVector3D(), -vPlane.w );

	// Near
	if ( bD3DClippingRange )
	{
		// [0,1] Z clipping range (D3D-style)
		vPlane = MatrixGetRowAsVector4D( viewProjMatrix, 2 );
	}
	else
	{
		// [-1,1] Z clipping range (OpenGL-style)
		vPlane = MatrixGetRowAsVector4D( viewProjMatrix, 2 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	}

	pPlanesOut[ FRUSTUM_NEARZ ].Init( vPlane.AsVector3D(), -vPlane.w );

	// Far
	vPlane = -MatrixGetRowAsVector4D( viewProjMatrix, 2 ) + MatrixGetRowAsVector4D( viewProjMatrix, 3 );
	pPlanesOut[ FRUSTUM_FARZ ].Init( vPlane.AsVector3D(), -vPlane.w );

	for ( uint i = 0; i < FRUSTUM_NUMPLANES; ++i )
	{
		float flLen2 = pPlanesOut[i].m_Normal.x * pPlanesOut[i].m_Normal.x + pPlanesOut[i].m_Normal.y * pPlanesOut[i].m_Normal.y + pPlanesOut[i].m_Normal.z * pPlanesOut[i].m_Normal.z;
		if ( flLen2 != 0.0f )
		{
			float flScale = 1.0f / sqrt( flLen2 );
			pPlanesOut[i].m_Normal *= flScale;
			pPlanesOut[i].m_Dist *= flScale;
		}
	}
}

#endif // !_STATIC_LINKED || _SHARED_LIB

