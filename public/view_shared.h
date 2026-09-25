//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VIEW_SHARED_H
#define VIEW_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "mathlib/vector.h"
#include "materialsystem/MaterialSystemUtil.h"

#include "mathlib/camera.h"

class CVolumeCuller;


//-----------------------------------------------------------------------------
// Flags passed in with view setup
//-----------------------------------------------------------------------------
enum ClearFlags_t
{
	VIEW_CLEAR_COLOR = 0x1,
	VIEW_CLEAR_DEPTH = 0x2,
	VIEW_CLEAR_FULL_TARGET = 0x4,
	VIEW_NO_DRAW = 0x8,
	VIEW_CLEAR_OBEY_STENCIL = 0x10, // Draws a quad allowing stencil test to clear through portals
	VIEW_CLEAR_STENCIL = 0x20,
};

//-----------------------------------------------------------------------------
// Motion blur modes (CS:GO view_shared.h)
//-----------------------------------------------------------------------------
enum MotionBlurMode_t
{
	MOTION_BLUR_DISABLE = 1,
	MOTION_BLUR_GAME = 2,
	MOTION_BLUR_SFM = 3
};

enum StereoEye_t
{
	STEREO_EYE_MONO = 0,
	STEREO_EYE_LEFT = 1,
	STEREO_EYE_RIGHT = 2,
	STEREO_EYE_MAX = 3,
};


//-----------------------------------------------------------------------------
// Purpose: Renderer setup data.  
//-----------------------------------------------------------------------------
class CViewSetup
{
public:
	CViewSetup()
	{
		m_flAspectRatio = 0.0f;
		m_bRenderToSubrectOfLargerScreen = false;
		m_bDoBloomAndToneMapping = true;
		m_bOrtho = false;
		m_bOffCenter = false;
		m_bCacheFullSceneState = false;
//		m_bUseExplicitViewVector = false;
        m_bViewToProjectionOverride = false;
		m_bCustomViewMatrix = false;
		m_bCustomProjMatrix = false;
		m_nMotionBlurMode = MOTION_BLUR_GAME;
		m_bCSMView = false;
		m_pCSMVolumeCuller = NULL;
		m_eStereoEye = STEREO_EYE_MONO;
	}

// shared by 2D & 3D views

	// left side of view window
	int			x;					
	int			m_nUnscaledX;
	// top side of view window
	int			y;					
	int			m_nUnscaledY;
	// width of view window
	int			width;				
	int			m_nUnscaledWidth;
	// height of view window
	int			height;				
	// which eye are we rendering?
	StereoEye_t m_eStereoEye;
	int			m_nUnscaledHeight;

// the rest are only used by 3D views

	// Orthographic projection?
	bool		m_bOrtho;			
	// View-space rectangle for ortho projection.
	float		m_OrthoLeft;		
	float		m_OrthoTop;
	float		m_OrthoRight;
	float		m_OrthoBottom;

	// horizontal FOV in degrees
	float		fov;				
	// horizontal FOV in degrees for in-view model
	float		fovViewmodel;		

	// 3D origin of camera
	Vector		origin;					

	// heading of camera (pitch, yaw, roll)
	QAngle		angles;				
	// local Z coordinate of near plane of camera
	float		zNear;			
		// local Z coordinate of far plane of camera
	float		zFar;			

	// local Z coordinate of near plane of camera ( when rendering view model )
	float		zNearViewmodel;		
	// local Z coordinate of far plane of camera ( when rendering view model )
	float		zFarViewmodel;		

	// set to true if this is to draw into a subrect of the larger screen
	// this really is a hack, but no more than the rest of the way this class is used
	bool		m_bRenderToSubrectOfLargerScreen;

	// The aspect ratio to use for computing the perspective projection matrix
	// (0.0f means use the viewport)
	float		m_flAspectRatio;

	// Controls for off-center projection (needed for poster rendering)
	bool		m_bOffCenter;
	float		m_flOffCenterTop;
	float		m_flOffCenterBottom;
	float		m_flOffCenterLeft;
	float		m_flOffCenterRight;

	// Control that the SFM needs to tell the engine not to do certain post-processing steps
	bool		m_bDoBloomAndToneMapping;

	// Cached mode for certain full-scene per-frame varying state such as sun entity coverage
	bool		m_bCacheFullSceneState;

    // If using VR, the headset calibration will feed you a projection matrix per-eye.
	// This does NOT override the Z range - that will be set up as normal (i.e. the values in this matrix will be ignored).
    bool        m_bViewToProjectionOverride;
    VMatrix     m_ViewToProjection;

	// CS:GO additions used by the CSM code
	bool		m_bCustomViewMatrix;
	matrix3x4_t	m_matCustomViewMatrix;

	bool		m_bCustomProjMatrix;
	VMatrix		m_matCustomProjMatrix;

	const CVolumeCuller *m_pCSMVolumeCuller;

	int			m_nMotionBlurMode;


	// True if this is a CSM depth view. The CSM world->view matrix doesn't have an XY translation (that's moved into the CSM ortho view->projection
	// matrix to address continuity issues), so the usual assumptions made about camera/view space do not necessarily apply.
	bool		m_bCSMView:1;
	float ComputeViewMatrices( VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection ) const;
};




inline float CViewSetup::ComputeViewMatrices( VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection ) const
{
	float flAspectRatio = m_flAspectRatio;
	if ( flAspectRatio == 0.0f )
	{
		flAspectRatio = (height != 0) ? ( (float)width / (float)height ) : 1.0f;
	}

	if( !m_bCustomViewMatrix )
	{
		ComputeViewMatrix( pWorldToView, origin, angles );
	}
	else
	{
		ComputeViewMatrix( pWorldToView, m_matCustomViewMatrix );
	}

	if ( m_bCustomProjMatrix )
	{
		*pViewToProjection = m_matCustomProjMatrix;
	}
	else if ( m_bOrtho )
	{
		MatrixBuildOrtho( *pViewToProjection, m_OrthoLeft, m_OrthoTop, 
			m_OrthoRight, m_OrthoBottom, zNear, zFar );
	}
	else if ( m_bOffCenter )
	{
		MatrixBuildPerspectiveOffCenterX( *pViewToProjection, fov, flAspectRatio, 
			zNear, zFar, m_flOffCenterBottom, m_flOffCenterTop,
			m_flOffCenterLeft, m_flOffCenterRight );
	}
	else
	{
		MatrixBuildPerspectiveX( *pViewToProjection, fov, flAspectRatio, zNear, zFar );
	}

	MatrixMultiply( *pViewToProjection, *pWorldToView, *pWorldToProjection );

	return flAspectRatio;
}

#endif // VIEW_SHARED_H
