//=============================================================================//
//
// Purpose:		Generic pilot for use with NPC vehicles (i.e. npc_arbeit_helicopter)
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HELI_PILOT_H
#define NPC_HELI_PILOT_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_playercompanion.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_HeliPilot : public CNPC_PlayerCompanion
{
	DECLARE_CLASS( CNPC_HeliPilot, CNPC_PlayerCompanion );
	DECLARE_DATADESC();

public:
	CNPC_HeliPilot();

	void Spawn();
	void Precache( void );
	void SelectModel( void );

	Class_T			Classify();
	Disposition_t	IRelationType( CBaseEntity *pTarget );
	bool			CanBeAnEnemyOf( CBaseEntity *pEnemy );
	bool			HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	virtual bool	QueryHearSound( CSound *pSound );
	virtual bool	QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC = false );

	virtual void	OnLooked( int iDistance );
	virtual void	OnListened();
	virtual bool	UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );
	virtual void	GatherEnemyConditions( CBaseEntity *pEnemy );

	void			ModifyOrAppendCriteria( AI_CriteriaSet &set );
	virtual void	ModifyEmitSoundParams( EmitSound_t &params );

	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );

	virtual int		SelectSchedule( void );
	virtual void	GatherConditions( void );
	int 			TranslateSchedule( int scheduleType );

	void			StartTask( const Task_t *pTask );
	void			RunTask( const Task_t *pTask );

	Activity		NPC_TranslateActivity( Activity eNewActivity );

	//-----------------------------------------------------

	bool	IsEntityInsideHelicopter( CBaseEntity *pEntity );
	bool	IsPointInsideHelicopter( const Vector &vecOrigin );
	bool	IsPointInsideHelicopter( const Vector &vecOrigin, const QAngle &angAngles );
	bool	IsPointInsideHelicopter( const matrix3x4_t &matWorldPoint );

	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_ = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,

		SCHED_PILOT_IDLE = BaseClass::NEXT_SCHEDULE,
		SCHED_PILOT_ALERT,
		SCHED_PILOT_COMBAT,
		NEXT_SCHEDULE,

		TASK_PLAY_PILOT_SEQUENCE = BaseClass::NEXT_TASK,
		NEXT_TASK,

		//AE_ = LAST_SHARED_ANIMEVENT

	};

private:

	string_t	m_iszPilotSequence;
	string_t	m_iszPilotAttachment;

	DEFINE_CUSTOM_AI;
};

#endif
