//=============================================================================//
//
// Purpose:		Overrides of stealth behaviors for CNPC_PlayerCompanion.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_BEHAVIOR_COMPANION_H
#define AI_STEALTH_BEHAVIOR_COMPANION_H

#include "ai_stealth_behavior_curious.h"
#include "ai_stealth_behavior_search.h"
#include "ai_stealth_behavior_alarm.h"

#if defined( _WIN32 )
#pragma once
#endif

class CNPC_PlayerCompanion;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BEHAVIOR_CLASS>
class CAI_CompanionStealthBehavior : public BEHAVIOR_CLASS
{
public:
	virtual bool	SpeakStealthConcept( const AIConcept_t &concept, AI_CriteriaSet *modifiers = NULL, bool bForce = false )
	{
		return GetOuter()->SpeakStealthConcept( concept, modifiers, bForce );
	}

	virtual void	SetSpeechTarget( CBaseEntity *pEntity )
	{
		return GetOuter()->SetSpeechTarget( pEntity );
	}

	DEFINE_AI_COMPONENT_OUTER( CNPC_PlayerCompanion )
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_Companion_StealthCuriousBehavior : public CAI_CompanionStealthBehavior<CAI_StealthCuriousBehavior>
{
	typedef CAI_StealthCuriousBehavior BaseClass;

public:

	virtual bool	IsInvestigatingSound();
	virtual bool	ShouldGoToSoundSource( CSound *pSound );
	virtual void	OnStartInvestigatingSound();

	virtual int		TranslateSchedule( int scheduleType );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	enum
	{
		// Schedules
		SCHED_STEALTH_THROW_GRENADE_AT_SOUND = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		// Tasks
		TASK_STEALTH_PC_FACE_TOSS_DIR = BaseClass::NEXT_TASK,
		NEXT_TASK,

		// Conditions
		//COND_STEALTH_ = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_Companion_StealthSearchBehavior : public CAI_CompanionStealthBehavior<CAI_StealthSearchBehavior>
{
	typedef CAI_StealthSearchBehavior BaseClass;

public:

	virtual void	OnLeaveSearchPoint( CAI_Hint *pHint );
	virtual void	OnArrivedAtSearchPoint( CAI_Hint *pHint );

	DEFINE_AI_COMPONENT_OUTER( CNPC_PlayerCompanion )
};

//-----------------------------------------------------------------------------

#endif
