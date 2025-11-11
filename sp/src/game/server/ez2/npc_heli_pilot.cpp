//=============================================================================//
//
// Purpose:		Generic pilot for use with NPC vehicles (i.e. npc_arbeit_helicopter)
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_heli_pilot.h"
#include "cbasehelicopter.h"
#include "eventqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

ConVar sk_heli_pilot_health( "sk_heli_pilot_health", "50" );
ConVar npc_heli_pilot_cheap_interior_tests( "npc_heli_pilot_cheap_interior_tests", "1" );

int	ACT_PILOT_CONTROL;

#define PILOT_MAX_HELI_INTERIOR_DIST	200.0f
#define SOUND_PLAY_ON_PLAYER_DIST_SQR	Square( 2500.0f )

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_HeliPilot )

	DEFINE_KEYFIELD( m_iszPilotSequence, FIELD_STRING, "PilotSequence" ),
	DEFINE_KEYFIELD( m_iszPilotAttachment, FIELD_STRING, "PilotAttachment" ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_heli_pilot, CNPC_HeliPilot );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HeliPilot::CNPC_HeliPilot()
{
	m_iszPilotAttachment = MAKE_STRING( "pilot_point" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::Spawn()
{
	BaseClass::Spawn();

	if ( !GetMoveParent() || !dynamic_cast<CBaseHelicopter*>( GetMoveParent() ) )
	{
		Warning( "%s not parented to a valid helicopter, cannot spawn\n", GetDebugName() );
		UTIL_Remove( this );
		return;
	}

	if ( !GetParentAttachment() && m_iszPilotAttachment != NULL_STRING )
	{
		// Lookup the attachment
		int iAttachment = GetMoveParent()->GetBaseAnimating()->LookupAttachment( STRING( m_iszPilotAttachment ) );
		if ( iAttachment > 0 )
		{
			SetParent( GetParent(), iAttachment );
		}
	}

	CBaseHelicopter *pHeli = assert_cast<CBaseHelicopter *>(GetMoveParent());
	pHeli->SetPilot( this );

	m_iHealth = sk_heli_pilot_health.GetFloat();

	SetMoveType( MOVETYPE_NONE );
	CapabilitiesClear();

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD );
	CapabilitiesAdd( bits_CAP_FRIENDLY_DMG_IMMUNE );

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::Precache()
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::SelectModel()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( MAKE_STRING( "models/humans/group_conscripts/male_gasmasked.mdl" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Class_T CNPC_HeliPilot::Classify()
{
	if ( GetMoveParent() )
		return GetMoveParent()->Classify();

	return CLASS_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CNPC_HeliPilot::IRelationType( CBaseEntity *pTarget )
{
	if ( GetMoveParent() && GetMoveParent()->IsNPC() )
		return GetMoveParent()->MyNPCPointer()->IRelationType( pTarget );

	return BaseClass::IRelationType( pTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::CanBeAnEnemyOf( CBaseEntity *pEnemy )
{
	//if ( !pEnemy->IsPlayer() )
	if ( !IsEntityInsideHelicopter( pEnemy ) )
		return false;

	return BaseClass::CanBeAnEnemyOf( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::QueryHearSound( CSound *pSound )
{
	if ( !IsPointInsideHelicopter( pSound->GetSoundOrigin() ) )
		return false;

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	if ( pEntity != GetMoveParent()->GetEnemy() && !IsEntityInsideHelicopter( pEntity ) )
		return false;

	return BaseClass::QuerySeeEntity( pEntity, bOnlyHateOrFearIfNPC );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::OnLooked( int iDistance )
{
	BaseClass::OnLooked( iDistance );

	if ( GetMoveParent() && GetMoveParent()->IsNPC() )
	{
		// Borrow the helicopter's look conditions
		static int conditionsToSet[] =
		{
			COND_SEE_HATE,
			COND_SEE_DISLIKE,
			COND_SEE_ENEMY,
			COND_SEE_FEAR,
			COND_SEE_NEMESIS,
			COND_SEE_PLAYER,
			COND_LOST_PLAYER,
			COND_ENEMY_WENT_NULL,
		};

		CAI_BaseNPC *pHeli = GetMoveParent()->MyNPCPointer();
		for ( int i = 0; i < ARRAYSIZE( conditionsToSet ); i++ )
		{
			if ( pHeli->HasCondition( conditionsToSet[i] ) )
				SetCondition( conditionsToSet[i] );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::OnListened()
{
	BaseClass::OnListened();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if ( GetMoveParent() && GetMoveParent()->IsNPC() )
	{
		// Update the heli as if the heli saw the enemy
		GetMoveParent()->MyNPCPointer()->UpdateEnemyMemory( pEnemy, position, GetMoveParent() );
	}

	return BaseClass::UpdateEnemyMemory( pEnemy, position, pInformer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	BaseClass::GatherEnemyConditions( pEnemy );

	if ( GetMoveParent() && GetMoveParent()->IsNPC() )
	{
		// Borrow the helicopter's enemy conditions
		static int conditionsToSet[] =
		{
			COND_ENEMY_OCCLUDED,
			COND_HAVE_ENEMY_LOS,
			COND_SEE_ENEMY,
			COND_ENEMY_TOO_FAR,
			COND_ENEMY_UNREACHABLE,
		};

		CAI_BaseNPC *pHeli = GetMoveParent()->MyNPCPointer();
		for ( int i = 0; i < ARRAYSIZE( conditionsToSet ); i++ )
		{
			if ( pHeli->HasCondition( conditionsToSet[i] ) )
				SetCondition( conditionsToSet[i] );
		}

		// Normally TLK_STARTCOMBAT is said using our own COND_SEE_ENEMY
		// We need to do it here in case our heli sees the enemy instead
		if ( GetLastEnemyTime() == 0 || gpGlobals->curtime - GetLastEnemyTime() > 30 )
		{
			if ( pHeli->HasCondition( COND_SEE_ENEMY ) && !IsSpeaking() )
			{
				SetSpeechTarget( pEnemy );
				SpeakIfAllowed( TLK_STARTCOMBAT );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::ModifyEmitSoundParams( EmitSound_t &params )
{
	BaseClass::ModifyEmitSoundParams( params );

	if ( params.m_nChannel == CHAN_VOICE )
	{
		params.m_nSpecialDSP = 57;	// LOUDSPEAKER
		//params.m_SoundLevel = SNDLVL_NONE;

		// Emit on nearby players to amplify the sound
		// TODO: Better place for this
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() < SOUND_PLAY_ON_PLAYER_DIST_SQR )
			{
				CSingleUserRecipientFilter filter( pPlayer );
				EmitSound( filter, i, params );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::Event_Killed( const CTakeDamageInfo &info )
{
	if ( GetMoveParent() && GetMoveParent()->GetHealth() > 0 )
	{
		// Shoot it down
		CBaseHelicopter *pHeli = assert_cast<CBaseHelicopter *>(GetMoveParent());
		pHeli->OnPilotKilled( this );
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_HeliPilot::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( !BaseClass::OnTakeDamage_Alive( info ) )
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_HeliPilot::SelectSchedule( void )
{
	switch ( GetState() )
	{
		case NPC_STATE_IDLE:
			return SCHED_PILOT_IDLE;
		case NPC_STATE_ALERT:
			return SCHED_PILOT_ALERT;
		case NPC_STATE_COMBAT:
			return SCHED_PILOT_COMBAT;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::GatherConditions( void )
{
	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_HeliPilot::TranslateSchedule( int scheduleType )
{
	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_PLAY_PILOT_SEQUENCE:
		{
			if ( m_iszPilotSequence != NULL_STRING )
			{
				SetSequenceByName( STRING( m_iszPilotSequence ) );
				SetIdealActivity( ACT_DO_NOT_DISTURB );
			}
			else
			{
				SetIdealActivity( ACT_IDLE );
			}
		}
		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_HeliPilot::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_PLAY_PILOT_SEQUENCE:
		{
			if ( GetEnemy() )
			{
				AddLookTarget( GetEnemy()->EyePosition(), 1.0f, 1.0f );
			}
			else if ( HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_HEAR_PLAYER ) || HasCondition( COND_HEAR_WORLD ) || HasCondition( COND_HEAR_BULLET_IMPACT ) )
			{
				CSound *pSound = GetBestSound( SOUND_COMBAT | SOUND_PLAYER | SOUND_WORLD | SOUND_BULLET_IMPACT );
				if ( pSound )
					AddLookTarget( pSound->GetSoundReactOrigin() + GetViewOffset(), 1.0f, 1.0f );
			}
		}
		break;

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_HeliPilot::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_IDLE && HaveSequenceForActivity( (Activity)ACT_PILOT_CONTROL ) )
	{
		return (Activity)ACT_PILOT_CONTROL;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::IsEntityInsideHelicopter( CBaseEntity *pEntity )
{
	if ( npc_heli_pilot_cheap_interior_tests.GetBool() )
		return GetMoveParent() && ( GetMoveParent()->CollisionProp()->CalcDistanceFromPoint( pEntity->GetAbsOrigin() ) < PILOT_MAX_HELI_INTERIOR_DIST );

	return IsPointInsideHelicopter( pEntity->EntityToWorldTransform() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::IsPointInsideHelicopter( const Vector &vecOrigin )
{
	if ( npc_heli_pilot_cheap_interior_tests.GetBool() )
		return GetMoveParent() && ( GetMoveParent()->CollisionProp()->CalcDistanceFromPoint( vecOrigin ) < PILOT_MAX_HELI_INTERIOR_DIST );

	matrix3x4_t mat;
	AngleIMatrix( vec3_angle, vecOrigin, mat );
	return IsPointInsideHelicopter( mat );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::IsPointInsideHelicopter( const Vector &vecOrigin, const QAngle &angAngles )
{
	if ( npc_heli_pilot_cheap_interior_tests.GetBool() )
		return GetMoveParent() && ( GetMoveParent()->CollisionProp()->CalcDistanceFromPoint( vecOrigin ) < PILOT_MAX_HELI_INTERIOR_DIST );

	matrix3x4_t mat;
	AngleIMatrix( angAngles, vecOrigin, mat );
	return IsPointInsideHelicopter( mat );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_HeliPilot::IsPointInsideHelicopter( const matrix3x4_t &matWorldPoint )
{
	if ( !GetMoveParent() )
		return false;

	CBaseHelicopter *pHeli = assert_cast<CBaseHelicopter *>(GetMoveParent());

	// Move it to the helicopter's local space
	matrix3x4_t matLocalPoint;
	ConcatTransforms( matWorldPoint, pHeli->EntityToWorldTransform(), matLocalPoint );

	// Get the approximate center of the interior
	Vector vecInteriorCenter;
	if ( GetParentAttachment() > 0 )
	{
		QAngle angAngles;
		pHeli->GetAttachmentLocal( GetParentAttachment(), vecInteriorCenter, angAngles );

		vecInteriorCenter.x = 0;
		vecInteriorCenter.y = 0;
	}

	// Add half our height
	vecInteriorCenter.z += ( GetHullHeight() * 0.5f );

	// Get the entity's origin
	Vector vecTestPosition;
	MatrixGetColumn( matLocalPoint, 3, vecTestPosition );

	Vector vecDelta = ( vecTestPosition - vecInteriorCenter );
	vecDelta.z *= 2.0f; // Inflate Z axis so that it's a spheroid

	return vecDelta.LengthSqr() < Square( PILOT_MAX_HELI_INTERIOR_DIST );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_heli_pilot, CNPC_HeliPilot )

	DECLARE_TASK( TASK_PLAY_PILOT_SEQUENCE )

	DECLARE_ACTIVITY( ACT_PILOT_CONTROL )

	DEFINE_SCHEDULE
	(
		SCHED_PILOT_IDLE,

		"	Tasks"
		"		TASK_PLAY_PILOT_SEQUENCE	0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_PILOT_ALERT,

		"	Tasks"
		"		TASK_PLAY_PILOT_SEQUENCE	0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_PILOT_COMBAT,

		"	Tasks"
		"		TASK_PLAY_PILOT_SEQUENCE	0"
		""
		"	Interrupts"
	)

AI_END_CUSTOM_NPC()
