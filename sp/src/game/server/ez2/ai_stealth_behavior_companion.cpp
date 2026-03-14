//=============================================================================//
//
// Purpose:		Overrides of stealth behaviors for CNPC_PlayerCompanion.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_behavior_companion.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_senses.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_tacticalservices.h"
#include "npc_playercompanion.h"
#include "npc_citizen17.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_Companion_StealthCuriousBehavior::IsInvestigatingSound()
{
	if ( BaseClass::IsInvestigatingSound() )
		return true;

	if ( IsCurSchedule( SCHED_STEALTH_THROW_GRENADE_AT_SOUND ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_Companion_StealthCuriousBehavior::ShouldGoToSoundSource( CSound *pSound )
{
	// TODO: More elaborate suspicion and checking to throw a grenade?

	return BaseClass::ShouldGoToSoundSource( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_Companion_StealthCuriousBehavior::OnStartInvestigatingSound()
{
	// We will pick a new aim target near the sound
	GetOuter()->StopAiming( "Investigating sound" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_Companion_StealthCuriousBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	switch ( nBase )
	{
		case SCHED_ALERT_FACE_BESTSOUND:
		case SCHED_INVESTIGATE_SOUND:
		case SCHED_STEALTH_INVESTIGATE_SOUND:
		case SCHED_STEALTH_INVESTIGATE_SOUND_STAY:
			{
				if ( g_hStealthManager && g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_TENSE, STEALTH_LEVEL_LOUD ) )
				{
					// See if we should throw a grenade instead of investigating
					// But only if we have a surplus
					if (GetOuter()->IsGrenadeCapable() && GetOuter()->m_iNumGrenades > 2)
					{
						CSound *pSound = GetOuter()->GetBestSound();
						if ( pSound &&
							( ( !CAI_StealthSenses::IsStealthSound( pSound ) && pSound->IsSoundType( SOUND_COMBAT ) )
								|| CAI_StealthSenses::IsPotentialEnemyStealthSound( pSound->SoundChannel() ) ) )
						{
							// Only when we saw something moving
							if ( pSound->SoundChannel() == SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS
								&& ( pSound->m_hOwner == NULL || !pSound->m_hOwner->IsAlive() ) )
								break;

							Vector vecTarget = pSound->GetSoundReactOrigin() + Vector(0,0,32);
							if ( ( GetAbsOrigin() - vecTarget ).LengthSqr() < Square( 128.0f ) )
								break;

							// Find LOS instead of going directly to the sound
							if ( !GetOuter()->FVisible( vecTarget ) )
							{
								Vector vecPosLOS = vec3_origin;
								if ( /*GetOuter()->GetTacticalServices()->FindLateralLos( goal.dest, &vecPosLOS ) ||*/
									GetTacticalServices()->FindLos( vecTarget, vecTarget + Vector(0,0,32), 0.0f, pSound->Volume(), 1.0f, &vecPosLOS ))
								{
									if ( GetOuter()->FVisible( vecPosLOS ) && ( GetAbsOrigin() - vecTarget ).LengthSqr() > Square( 96.0f ) )
										vecTarget = vecPosLOS;
								}
							}

							if ( GetOuter()->CanThrowGrenade( vecTarget ) )
							{
								GetOuter()->StopAiming();
								GetOuter()->DelayGrenadeCheck( 6 );
								return SCHED_STEALTH_THROW_GRENADE_AT_SOUND;
							}
						}
					}
				}
			}
			break;
	}

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_Companion_StealthCuriousBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_PC_FACE_TOSS_DIR:
			{
				// Need to do this since the behavior leaf classes can't access new NPC classes by default
				ChainStartTask( CNPC_PlayerCompanion::TASK_PC_FACE_TOSS_DIR );
			}
			break;

		default:
			BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_Companion_StealthCuriousBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_PC_FACE_TOSS_DIR:
			{
				// Need to do this since the behavior leaf classes can't access new NPC classes by default
				ChainRunTask( CNPC_PlayerCompanion::TASK_PC_FACE_TOSS_DIR );
			}
			break;

		default:
			BaseClass::RunTask( pTask );
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_Companion_StealthCuriousBehavior )

	DECLARE_TASK( TASK_STEALTH_PC_FACE_TOSS_DIR )

	//---------------------------------
	
	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_THROW_GRENADE_AT_SOUND,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_STEALTH_PC_FACE_TOSS_DIR		0"
		"		TASK_WAIT							3"	// TASK_STEALTH_BESTSOUND_PAUSE
		"		TASK_ANNOUNCE_ATTACK				2"	// 2 = grenade
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_RANGE_ATTACK2"
		"		TASK_WAIT							5" // Wait for the grenade to go off
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()


//-------------------------------------


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_Companion_StealthSearchBehavior::OnLeaveSearchPoint( CAI_Hint *pHint )
{
	BaseClass::OnLeaveSearchPoint( pHint );
	GetOuter()->StopAiming( "Leaving search point" );
}

void CAI_Companion_StealthSearchBehavior::OnArrivedAtSearchPoint( CAI_Hint *pHint )
{
	BaseClass::OnArrivedAtSearchPoint( pHint );

	// Try to find a new target near this hint
	GetOuter()->FindNewAimTarget();
}
