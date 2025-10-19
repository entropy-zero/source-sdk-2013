//=============================================================================//
//
// Purpose:		Base class for AI behaviors in the stealth system.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_BEHAVIOR_H
#define AI_STEALTH_BEHAVIOR_H

#include "ai_basenpc.h"
#include "ai_behavior.h"
#include "ai_speech.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Base class for AI behaviors in the stealth system.
// Some of these are stubs are designed to be overridden by CNPC_PlayerCompanion derived behaviors.
//-----------------------------------------------------------------------------
template <class NPC_CLASS = CAI_BaseNPC, const int ID_SPACE_OFFSET = 100000>
class CAI_StealthBehavior : public CAI_Behavior<NPC_CLASS, ID_SPACE_OFFSET>
{
public:
	virtual bool	SpeakStealthConcept( const AIConcept_t &concept, AI_CriteriaSet *modifiers = NULL, bool bForce = false )
	{
		if ( GetOuter()->GetExpresser() )
		{
			if ( GetOuter()->GetExpresser()->CanSpeak() && GetOuter()->GetExpresser()->CanSpeakConcept( concept ) )
				return GetOuter()->GetExpresser()->Speak( concept, modifiers );
		}

		return false;
	}

	virtual void	SetSpeechTarget( CBaseEntity *pEntity )
	{
		// Overridden by CNPC_PlayerCompanion
	}

	CAI_StealthSenses *GetStealthSenses()
	{
		return GetOuter()->GetStealthSenses();
	}
};

//-----------------------------------------------------------------------------

#endif
