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

#endif
