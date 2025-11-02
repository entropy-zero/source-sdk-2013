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
#include "ez2/ai_stealth_shared.h"

#if defined( _WIN32 )
#pragma once
#endif

class CAI_Squad;

//-----------------------------------------------------------------------------

// Finds a path that the enemy could have taken based on where we last saw them and the direction
// they seemed to be heading in.
bool	FindPredictedEnemyPos( CAI_BaseNPC *pNPC, CBaseEntity *pEnemy, Vector &vecOutPos );

//-----------------------------------------------------------------------------
// Purpose: Interface for entities that can compromise player or assassin cloak
// (e.g. elite or sniper with a laser sight)
//-----------------------------------------------------------------------------
class ICloakCompromisable
{
public:

	// This uses CBaseCombatCharacter because it can be either a player or a npc_assassin
	virtual bool	CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType ) = 0;
};

#endif
