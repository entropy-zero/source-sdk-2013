//=============================================================================//
//
// Purpose:		Strider husks.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_HUSK_STRIDER_H
#define NPC_HUSK_STRIDER_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_strider.h"
#include "npc_husk_base.h"

//=========================================================
//	>> CNPC_HuskStrider
//=========================================================
class CNPC_HuskStrider : public CAI_BaseHusk<CNPC_Strider>
{
	DECLARE_CLASS( CNPC_HuskStrider, CAI_BaseHusk<CNPC_Strider> );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

public:
	CNPC_HuskStrider();

	// From CAI_HuskSink
	const char	*GetBaseNPCClassname() override { return "npc_strider"; }

	void		Spawn( void );
	void		Precache( void );

	void		SuspiciousSound() { EmitSound( "NPC_HuskStrider.Suspicious" ); }
	void		StartledSound() { EmitSound( "NPC_HuskStrider.Startled" ); }

	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskAggressionLevel );
	IMPLEMENT_NETWORK_VAR_FOR_DERIVED( m_nHuskCognizanceFlags );

	//DEFINE_CUSTOM_AI;

private:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_HUSK_STRIDER_ = BaseClass::NEXT_CONDITION,

		//SCHED_HUSK_STRIDER_ = BaseClass::NEXT_SCHEDULE,

		//TASK_HUSK_STRIDER_ = BaseClass::NEXT_TASK,
	};
};

#endif // NPC_HUSK_STRIDER_H
