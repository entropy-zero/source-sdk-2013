//=============================================================================//
//
// Purpose:		Synth husks.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_SYNTHS_H
#define NPC_HUSK_SYNTHS_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_crabsynth.h"
#include "npc_mortarsynth.h"
#include "npc_scanner.h"
#include "npc_husk_base.h"

//=========================================================
//	>> CNPC_HuskCrabSynth
//=========================================================
class CNPC_HuskCrabSynth : public CAI_BaseHusk<CNPC_CrabSynth>
{
	DECLARE_CLASS( CNPC_HuskCrabSynth, CAI_BaseHusk<CNPC_CrabSynth> );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CNPC_HuskCrabSynth();

	// From CAI_HuskSink
	const char	*GetBaseNPCClassname() override { return "npc_crabsynth"; }

	void		Spawn( void );
	void		Precache( void );

	void		HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void		SuspiciousSound() { EmitSound( "NPC_HuskCrabSynth.Suspicious" ); }
	void		StartledSound() { EmitSound( "NPC_HuskCrabSynth.Startled" ); }

	int			SelectCombatSchedule( void );

	Vector		GetActualShootPosition( const Vector &shootOrigin );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskAggressionLevel );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskCognizanceFlags );

	DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_HUSK_CRABSYNTH_ = BaseClass::NEXT_CONDITION,

		//SCHED_HUSK_CRABSYNTH_ = BaseClass::NEXT_SCHEDULE,

		//TASK_HUSK_CRABSYNTH_ = BaseClass::NEXT_TASK,
	};
};

//=========================================================
//	>> CNPC_HuskMortarSynth
//=========================================================
class CNPC_HuskMortarSynth : public CAI_BaseHusk<CNPC_Mortarsynth>
{
	DECLARE_CLASS( CNPC_HuskMortarSynth, CAI_BaseHusk<CNPC_Mortarsynth> );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CNPC_HuskMortarSynth();

	// From CAI_HuskSink
	const char	*GetBaseNPCClassname() override { return "npc_mortarsynth"; }

	void		Spawn( void );
	void		Precache( void );

	void		HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void		SuspiciousSound() { EmitSound( "NPC_HuskMortarSynth.Suspicious" ); }
	void		StartledSound() { EmitSound( "NPC_HuskMortarSynth.Startled" ); }

	int			SelectSchedule( void );

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskAggressionLevel );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskCognizanceFlags );

	DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_HUSK_CRABSYNTH_ = BaseClass::NEXT_CONDITION,

		//SCHED_HUSK_CRABSYNTH_ = BaseClass::NEXT_SCHEDULE,

		//TASK_HUSK_CRABSYNTH_ = BaseClass::NEXT_TASK,
	};
};

#endif // NPC_HUSK_SYNTHS_H
