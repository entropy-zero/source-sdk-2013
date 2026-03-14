//=============================================================================//
//
// Purpose:		AI behavior for advanced curiosity features.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_BEHAVIOR_CURIOUS_H
#define AI_STEALTH_BEHAVIOR_CURIOUS_H

#include "ai_stealth_behavior.h"
#include "ai_stealth_senses_curious.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_StealthCuriousBehavior : public CAI_StealthBehavior<>
{
	DECLARE_CLASS( CAI_StealthCuriousBehavior, CAI_StealthBehavior<> );
public:
	DECLARE_DATADESC();
	CAI_StealthCuriousBehavior();

	enum
	{
		// Schedules
		SCHED_STEALTH_INVESTIGATE_SOUND = BaseClass::NEXT_SCHEDULE,
		SCHED_STEALTH_INVESTIGATE_SOUND_STAY,
		SCHED_STEALTH_WANDER,	// Fallback for if SCHED_PATROL_WALK doesn't work
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_STEALTH_GET_PATH_TO_BESTSOUND = BaseClass::NEXT_TASK,
		TASK_STEALTH_BESTSOUND_PAUSE,
		TASK_STEALTH_MOVE_TO_BESTSOUND,
		NEXT_TASK,
		
		// Conditions
		COND_STEALTH_SOUND_UNREACHABLE = BaseClass::NEXT_CONDITION,
		COND_STEALTH_NEW_SOUND,
		NEXT_CONDITION,
	};

	//-----------------------------------------------

	virtual const char *GetName() { return "Stealth Curious"; }

	CAI_CuriousStealthSenses	*GetStealthSenses();

	//-----------------------------------------------

	virtual bool	IsInvestigatingSound();
	bool			ShouldStayAtSound( CSound *pSound );
	virtual bool	ShouldGoToSoundSource( CSound *pSound );
	virtual void	OnStartInvestigatingSound() {}
	virtual void	OnHearNewSound( CSound *pSound );

	virtual void	MarkAsSeen( CBaseEntity *pEntity );
	virtual void	OnSeeEntity( CBaseEntity *pEntity );
	virtual void	OnSeeRagdoll( CBaseEntity *pEntity );
	virtual void	OnSeeDoor( CBaseEntity *pEntity );
	virtual void	OnSeeProp( CBaseEntity *pEntity, bool bPickup = false );
	virtual void	OnSeeLaserDot( CBaseEntity *pEntity );
	virtual void	HandleAnimEvent( animevent_t *pEvent );

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &criteriaSet );

	int		TranslateSchedule( int scheduleType );
	int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void	BuildScheduleTestBits( void );
	void	PrescheduleThink( void );
	bool	CanSelectSchedule( void );
	void	EndScheduleSelection( void );
	void	OnScheduleChange( void );
	void	OnStartSchedule( int scheduleType );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

private:

	bool	m_bCalmSound;
	int		m_nSoundChannel;
	float	m_flSoundExpireTime;

	float	m_flLastTimeHeardSound;
	int		m_nNumTimesInvestigatedSound;

	EHANDLE		m_hSuspiciousTarget;

public:
	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

#endif
