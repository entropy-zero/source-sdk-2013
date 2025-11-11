//=============================================================================//
//
// Purpose:		Synth husks.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_husk_synths.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Husks generally use 90% of the original NPC's health

ConVar	sk_husk_crabsynth_health( "sk_husk_crabsynth_health", "810" ); // From 900
ConVar	sk_husk_mortarsynth_health( "sk_husk_mortarsynth_health", "225" ); // From 250

//-----------------------------------------------------------------------------

//=============================================================================
// CRAB SYNTH
//=============================================================================

LINK_ENTITY_TO_CLASS( npc_husk_crabsynth, CNPC_HuskCrabSynth );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HuskCrabSynth )

	DEFINE_USEFUNC( HuskUse ),

	DEFINE_BASE_HUSK_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_HuskCrabSynth, DT_NPC_HuskCrabSynth )

	DEFINE_BASE_HUSK_SENDPROPS()

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HuskCrabSynth::CNPC_HuskCrabSynth()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskCrabSynth::Spawn( void )
{
	BaseClass::Spawn();

	m_flFieldOfView = 0.5f;

	SetHealth( sk_husk_crabsynth_health.GetInt() );
	SetMaxHealth( GetHealth() );

	SetUse( &CNPC_HuskCrabSynth::HuskUse );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HuskCrabSynth::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
			case EZ_VARIANT_ASH:
				SetModelName( MAKE_STRING( "models/husks/ash/husk_crabsynth.mdl" ) );
				break;

			default:
				SetModelName( MAKE_STRING( "models/husks/husk_crabsynth.mdl" ) );
				break;
		}
	}

	PrecacheScriptSound( "NPC_HuskCrabSynth.Suspicious" );
	PrecacheScriptSound( "NPC_HuskCrabSynth.Startled" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HuskCrabSynth::HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Disposition_t relation = IRelationType( pActivator );
	if ( (relation == D_HT || relation == D_FR) && !FInViewCone( pActivator ) )
	{
		// Don't like being used without warning
		MakeAngry( pActivator );
		UpdateEnemyMemory( pActivator, pActivator->GetAbsOrigin(), pActivator );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CNPC_HuskCrabSynth::SelectCombatSchedule( void )
{
	if ( IsSuspicious() )
	{
		// Only face the enemy
		return SCHED_COMBAT_FACE;
	}

	return BaseClass::SelectCombatSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Vector CNPC_HuskCrabSynth::GetActualShootPosition( const Vector &shootOrigin )
{
	if ( IsSuspicious() && GetEnemy() && GetEnemy()->IsPlayer() )
	{
		// Do not aim at the player's eyes while suspicious, it looks weird
		Vector vecEnemyLKP = GetEnemyLKP();
		Vector vecEnemyOffset = (GetEnemy()->EyePosition() - GetEnemy()->GetAbsOrigin()) * 0.5f;
		Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

		// lead for some fraction of a second.
		extern ConVar ai_lead_time;
		return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
	}

	return BaseClass::GetActualShootPosition( shootOrigin );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_husk_crabsynth, CNPC_HuskCrabSynth )

AI_END_CUSTOM_NPC()

//=============================================================================
// MORTAR SYNTH
//=============================================================================

LINK_ENTITY_TO_CLASS( npc_husk_mortarsynth, CNPC_HuskMortarSynth );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HuskMortarSynth )

	DEFINE_USEFUNC( HuskUse ),

	DEFINE_BASE_HUSK_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_HuskMortarSynth, DT_NPC_HuskMortarSynth )

	DEFINE_BASE_HUSK_SENDPROPS()

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HuskMortarSynth::CNPC_HuskMortarSynth()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskMortarSynth::Spawn( void )
{
	BaseClass::Spawn();

	m_flFieldOfView = 0.0f;

	SetHealth( sk_husk_mortarsynth_health.GetInt() );
	SetMaxHealth( GetHealth() );

	SetUse( &CNPC_HuskMortarSynth::HuskUse );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HuskMortarSynth::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		switch (m_tEzVariant)
		{
			case EZ_VARIANT_ASH:
				SetModelName( MAKE_STRING( "models/husks/ash/husk_mortarsynth.mdl" ) );
				break;

			default:
				SetModelName( MAKE_STRING( "models/husks/husk_mortarsynth.mdl" ) );
				break;
		}
	}

	PrecacheScriptSound( "NPC_HuskMortarSynth.Suspicious" );
	PrecacheScriptSound( "NPC_HuskMortarSynth.Startled" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_HuskMortarSynth::HuskUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Disposition_t relation = IRelationType( pActivator );
	if ( (relation == D_HT || relation == D_FR) && !FInViewCone( pActivator ) )
	{
		// Don't like being used without warning
		MakeAngry( pActivator );
		UpdateEnemyMemory( pActivator, pActivator->GetAbsOrigin(), pActivator );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CNPC_HuskMortarSynth::SelectSchedule( void )
{
	// For now, just force target
	if ( IsSuspicious() && GetEnemy() )
	{
		m_flAttackNearDist = SCANNER_ATTACK_NEAR_DIST;
		m_flAttackFarDist = SCANNER_ATTACK_FAR_DIST;

		SetTarget( GetEnemy() );
	}
	else if ( m_flAttackFarDist != 500.0f )
	{
		extern ConVar sk_energy_grenade_radius;
		m_flAttackNearDist = sk_energy_grenade_radius.GetFloat() + 50.0f;
		m_flAttackFarDist = 500.0f;

		SetTarget( NULL );
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_husk_mortarsynth, CNPC_HuskMortarSynth )

AI_END_CUSTOM_NPC()
