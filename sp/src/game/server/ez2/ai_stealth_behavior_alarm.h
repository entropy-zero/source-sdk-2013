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
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_STEALTH_ALARM_FIND_ALARM = BaseClass::NEXT_TASK,
		TASK_STEALTH_ALARM_RAISE,
		TASK_STEALTH_ALARM_FINISH,
		NEXT_TASK,
		
		// Conditions
		COND_STEALTH_ALARM_INVALID = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	//-----------------------------------------------

	bool	IsRaisingAlarm();
	bool	ShouldRaiseAlarm();
	bool	ForceRaiseAlarm();
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

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

#endif
