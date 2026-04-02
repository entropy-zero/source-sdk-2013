//=============================================================================//
//
// Purpose:		AI behavior
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_BEHAVIOR_TURRET_SETUP_H
#define AI_BEHAVIOR_TURRET_SETUP_H

#include "ai_behavior.h"

#if defined( _WIN32 )
#pragma once
#endif

class CNPC_FloorTurret;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_TurretSetupBehavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_TurretSetupBehavior, CAI_SimpleBehavior );
public:
	DECLARE_DATADESC();
	CAI_TurretSetupBehavior();

	enum
	{
		// Schedules
		SCHED_TURRETSETUP_WALK_TO_PICKUP = BaseClass::NEXT_SCHEDULE,
		SCHED_TURRETSETUP_RUN_TO_PICKUP,
		SCHED_TURRETSETUP_PICKUP,
		SCHED_TURRETSETUP_WALK_TO_DROP_POS,
		SCHED_TURRETSETUP_RUN_TO_DROP_POS,
		SCHED_TURRETSETUP_DROP,
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_TURRETSETUP_GET_PATH_TO_TURRET = BaseClass::NEXT_TASK,
		TASK_TURRETSETUP_PICKUP_TURRET,
		TASK_TURRETSETUP_GET_PATH_TO_DROP_POS,
		TASK_TURRETSETUP_FACE_DROP_POS,
		TASK_TURRETSETUP_HOLSTER_WEAPON,
		TASK_TURRETSETUP_UNHOLSTER_WEAPON,
		NEXT_TASK,
		
		// Conditions
		//COND_STEALTH_ALARM_INVALID = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,
	};

	//-----------------------------------------------

	CNPC_FloorTurret	*GetTurret() { return m_hTurret; }

	CNPC_FloorTurret	*FindTurretToPickup( Vector &vecDropPos, QAngle &angDropAngle );
	bool				ShouldPickupTurret( CNPC_FloorTurret *pTurret, Vector &vecDropPos, QAngle &angDropAngle );

	void		PickupTurret( CNPC_FloorTurret *pTurret );
	void		DropTurret( CNPC_FloorTurret *pTurret, const Vector *vecPos = NULL, const QAngle *angAngles = NULL );

	bool		FValidateHintType( CAI_Hint *pHint );
	CAI_Hint	*FindTurretSetupHint( CBaseEntity *pEnemy );

	//-----------------------------------------------

	virtual const char *GetName() { return "Turret Setup"; }

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &criteriaSet );

	int		SelectSchedule();
	int		TranslateSchedule( int scheduleType );
	int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void	GatherConditions( void );
	void	BuildScheduleTestBits( void );
	bool	CanSelectSchedule( void );
	void	EndScheduleSelection( void );
	void	OnScheduleChange( void );

	void		Event_Killed( const CTakeDamageInfo &info );
	Activity	NPC_TranslateActivity( Activity eNewActivity );
	void		HandleAnimEvent( animevent_t *pEvent );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

private:

	CHandle<CNPC_FloorTurret>	m_hTurret;

	bool	m_bCarryingTurret;
	Vector	m_vecTurretDropPos;
	QAngle	m_angTurretDropAngle;

	int		m_iHolsterLayer;
	bool	m_bHolsteredWeapon;	// Whether or not we holstered the NPC's weapon before picking up a turret

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

#endif
