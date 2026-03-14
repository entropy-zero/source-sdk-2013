//=============================================================================//
//
// Purpose:		AI behavior for pulling an alarm to call for backup.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_BEHAVIOR_ALARM_H
#define AI_STEALTH_BEHAVIOR_ALARM_H

#include "ai_stealth_behavior.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_StealthAlarmBehavior : public CAI_StealthBehavior<>
{
	DECLARE_CLASS( CAI_StealthAlarmBehavior, CAI_StealthBehavior<> );
public:
	DECLARE_DATADESC();
	CAI_StealthAlarmBehavior();

	enum
	{
		// Schedules
		SCHED_STEALTH_RAISE_ALARM = BaseClass::NEXT_SCHEDULE,
		SCHED_STEALTH_RAISE_ALARM_WALK,
		SCHED_STEALTH_PATROL_ALARM,
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_STEALTH_ALARM_FIND_ALARM = BaseClass::NEXT_TASK,
		TASK_STEALTH_ALARM_RAISE,
		TASK_STEALTH_ALARM_FINISH,
		TASK_STEALTH_GET_NODE_NEAR_ALARM,
		NEXT_TASK,
		
		// Conditions
		COND_STEALTH_ALARM_INVALID = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	//-----------------------------------------------

	bool	IsRaisingAlarm();
	bool	ShouldRaiseAlarm( bool bOrder = false );
	bool	ForceRaiseAlarm();
	void	SetNextAlarm( CAI_Hint *pHint, bool bEscort = false );
	void	OnFinishRaisingAlarm();

	bool		FValidateHintType( CAI_Hint *pHint );
	CAI_Hint	*FindAlarmHint( CBaseEntity *pEnemy );

	//-----------------------------------------------

	virtual const char *GetName() { return "Stealth Alarm"; }

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &criteriaSet );

	int		SelectSchedule();
	int		TranslateSchedule( int scheduleType );
	int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void	GatherConditions( void );
	void	BuildScheduleTestBits( void );
	bool	CanSelectSchedule( void );
	void	EndScheduleSelection( void );
	void	OnScheduleChange( void );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

private:

	bool	m_bForceRaiseAlarm;
	bool	m_bRaisingAlarm;
	float	m_flNextAlarmRaiseTime;

	// Used to force a next alarm when we can't set our hint node right now (e.g. squad regroups)
	CHandle<CAI_Hint>	m_hNextAlarm;
	bool	m_bEscort;	// Escorting someone who's already raising the alarm

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

#endif
