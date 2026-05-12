//=============================================================================//
//
// Purpose:		Shared stealth definitions.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_SHARED_H
#define AI_STEALTH_SHARED_H

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------

enum AlertSourceType_t
{
	ALERT_SOURCE_TYPE_NONE,
	ALERT_SOURCE_TYPE_HELICOPTER,
	ALERT_SOURCE_TYPE_EXTRA,	// Generic extreme danger
};

//-----------------------------------------------------------------------------

enum CompromiseType_t
{
	COMPROMISE_TYPE_SIGHT,		// Spotted generically
	COMPROMISE_TYPE_LASER,		// Seen through laser
	COMPROMISE_TYPE_TOUCH,		// Rubbing against someone
	COMPROMISE_TYPE_MYLASER,	// Laser coming from my weapon
};

//-----------------------------------------------------------------------------

//=============================================================================
// >> ENEMYMARKDATA_T
// Used to track enemies when using enemy marking mode.
//=============================================================================
struct EnemyMarkData_t
{
#ifndef CLIENT_DLL
	DECLARE_SIMPLE_DATADESC();
#endif

	EHANDLE							hEnemy;
	float							flLastTimeSeen;
#ifdef CLIENT_DLL
	Vector4D						clrOutline;
	Vector4D						clrLastOutline;
	float							flOutlineChangeTime;
#else
	Color							clrOutline;
	bool							bScripted;
#endif
};

#endif
