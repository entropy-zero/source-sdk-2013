//=============================================================================//
//
// Purpose:		Utility functions for stealth AI.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_UTILS_H
#define AI_STEALTH_UTILS_H

#include "ai_basenpc.h"

#if defined( _WIN32 )
#pragma once
#endif

class CAI_Squad;

//-----------------------------------------------------------------------------

// Finds a path that the enemy could have taken based on where we last saw them and the direction
// they seemed to be heading in.
bool	FindPredictedEnemyPos( CAI_BaseNPC *pNPC, CBaseEntity *pEnemy, Vector &vecOutPos );

//-----------------------------------------------------------------------------

#endif
