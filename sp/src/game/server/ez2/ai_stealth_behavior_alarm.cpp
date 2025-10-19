//=============================================================================//
//
// Purpose:		AI behavior for pulling an alarm to call for backup.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_behavior_alarm.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_senses.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_playerally.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	ai_stealth_alarm_max_dist( "ai_stealth_alarm_max_dist", "2048" );

ConVar	g_debug_stealth_alarm( "g_debug_stealth_alarm", "0" );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_StealthAlarmBehavior )

	DEFINE_FIELD( m_bRaisingAlarm, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextAlarmRaiseTime, FIELD_TIME ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_StealthAlarmBehavior::CAI_StealthAlarmBehavior()
{
	m_bRaisingAlarm = false;
	m_flNextAlarmRaiseTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthAlarmBehavior::IsRaisingAlarm( void )
{
	//return IsCurSchedule( SCHED_STEALTH_RAISE_ALARM, false );
	return m_bRaisingAlarm;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthAlarmBehavior::ShouldRaiseAlarm( void )
{
	if ( !g_hStealthManager || !g_hStealthManager->GetAlarmsEnabled() || g_hStealthManager->IsAlarmRaised() )
		return false;

	if ( m_bForceRaiseAlarm )
		return true;

	if ( m_flNextAlarmRaiseTime > gpGlobals->curtime )
		return false;

	// If anyone in our squad is already raising the alarm, don't
	if (GetOuter()->GetSquad())
	{
		AISquadIter_t iter;
		CAI_Squad *pSquad = GetOuter()->GetSquad();
		for ( CAI_BaseNPC *pSquadmate = pSquad->GetFirstMember(&iter); pSquadmate; pSquadmate = pSquad->GetNextMember(&iter) )
		{
			if (pSquadmate == GetOuter() || !pSquadmate->IsAlive())
				continue;

			CAI_StealthAlarmBehavior *pBehavior;
			if ( GetOuter()->GetBehavior( &pBehavior ) )
			{
				if ( pBehavior->IsRaisingAlarm() )
					return false;
			}
		}
	}

	// Alarming body count
	if (GetStealthSenses()->GetNumBodiesFound() >= g_hStealthManager->GetNumBodiesToRaiseAlarm())
		return true;
		
	if (GetEnemy() && GetEnemy()->IsPlayer())
	{
		// Last of squad
		if (!GetOuter()->GetSquad() || g_hStealthManager->GetKnownLivingSquadMembers( GetStealthSenses()->GetStealthSquadInfo() ) <= 1)
			return true;
		
		// Low health
		if (GetOuter()->GetHealth() < (GetOuter()->GetMaxHealth() * 0.5))
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthAlarmBehavior::ForceRaiseAlarm( void )
{
	if ( !g_hStealthManager )
		return false;

	m_bForceRaiseAlarm = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::OnFinishRaisingAlarm( void )
{
	if ( !g_hStealthManager )
		return;

	g_hStealthManager->NPCStoppedRaisingAlarm( GetOuter() );

	if ( GetHintNode() )
	{
		GetHintNode()->Unlock();
		SetHintNode( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthAlarmBehavior::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
	case HINT_STEALTH_ALARM:
		return true;
		break;

	default:
		break;
	}

	return BaseClass::FValidateHintType( pHint );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Hint *CAI_StealthAlarmBehavior::FindAlarmHint( CBaseEntity *pEnemy )
{
	if ( !g_hStealthManager )
		return NULL;

	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_STEALTH_ALARM );

	int iBits = bits_HINT_NODE_USE_GROUP;
	if ( g_debug_stealth_alarm.GetBool() )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	hintCriteria.AddIncludePosition( GetAbsOrigin(), ai_stealth_alarm_max_dist.GetFloat() );

	CUtlVector<CAI_Hint *> vecHints;
	int nNumHints = CAI_HintManager::FindAllHints( GetOuter(), hintCriteria, &vecHints );
	
	// Normally, we try to find an alarm that isn't near our enemy, as it would presumably
	// be safer for us. pBestHint is the closest alarm which is closer to us than our enemy.
	// pBestHintNearEnemy is the closest alarm regardless of its distance to the enemy.
	// If we can't find an alarm that's far away from the player, then we just go for the
	// actual closest one
	CAI_Hint *pBestHint = NULL;
	float flBestDistSqr = Square( ai_stealth_alarm_max_dist.GetFloat() );
	CAI_Hint *pBestHintNearEnemy = NULL;
	float flBestDistNearEnemySqr = Square( ai_stealth_alarm_max_dist.GetFloat() );
	
	for (int i = 0; i < nNumHints; i++)
	{
		float flAlarmDistSqr = (GetAbsOrigin() - vecHints[i]->GetAbsOrigin()).LengthSqr();
		if (flAlarmDistSqr < flBestDistSqr)
		{
			if (pEnemy != NULL && pEnemy->IsAlive())
			{
				float flAlarmDistToEnemySqr = (pEnemy->GetAbsOrigin() - vecHints[i]->GetAbsOrigin()).LengthSqr();
				
				// Artificially increase distance if the enemy can't see this alarm
				if (!pEnemy->FVisible(vecHints[i]))
					flAlarmDistToEnemySqr *= 2.0;
				
				if (flAlarmDistToEnemySqr < flAlarmDistSqr)
				{
					if (flAlarmDistSqr < flBestDistNearEnemySqr)
					{
						// This alarm is closer to our enemy than it is to us. Track it as a fallback
						pBestHintNearEnemy = vecHints[i];
						flBestDistNearEnemySqr = flAlarmDistSqr;
					}
					continue;
				}
			}
			
			pBestHint = vecHints[i];
			flBestDistSqr = flAlarmDistSqr;
		}
	}
	
	if (pBestHint == NULL)
		pBestHint = pBestHintNearEnemy;

	return pBestHint;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	criteriaSet.AppendCriteria( "raising_alarm", IsRaisingAlarm() ? "1" : "0" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthAlarmBehavior::SelectSchedule()
{
	if ( ShouldRaiseAlarm() )
		return SCHED_STEALTH_RAISE_ALARM;

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthAlarmBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	switch ( nBase )
	{
		// Raise alarm randomly in combat when we have an opportunity
		case SCHED_INVESTIGATE_SOUND:
		case SCHED_CHASE_ENEMY:
		case SCHED_ESTABLISH_LINE_OF_FIRE:
		case SCHED_MOVE_TO_WEAPON_RANGE:
		case SCHED_RUN_FROM_ENEMY:
		case SCHED_ALERT_STAND:
		{
			if (ShouldRaiseAlarm())
				return SCHED_STEALTH_RAISE_ALARM;
			break;
		}
	}

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthAlarmBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( IsCurSchedule( SCHED_STEALTH_RAISE_ALARM, false ) )
	{
		// Wait before trying again
		m_flNextAlarmRaiseTime = gpGlobals->curtime + 5.0f;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::GatherConditions( void )
{
	BaseClass::GatherConditions();

	if ( m_bRaisingAlarm )
	{
		// Stop raising alarm if it's no longer valid
		if ( !GetHintNode() || GetHintNode()->IsDisabled() || !g_hStealthManager->GetAlarmsEnabled() || g_hStealthManager->IsAlarmRaised() )
			SetCondition( COND_STEALTH_ALARM_INVALID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthAlarmBehavior::CanSelectSchedule( void )
{
	if ( !ShouldRaiseAlarm() )
		return false;

	return BaseClass::CanSelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::OnScheduleChange( void )
{
	if ( m_bRaisingAlarm )
	{
		OnFinishRaisingAlarm();
		m_bRaisingAlarm = false;

		if ( GetHintNode() )
		{
			Msg( "have hint node\n" );
		}
	}

	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::EndScheduleSelection( void )
{
	if ( m_bRaisingAlarm )
	{
		OnFinishRaisingAlarm();
		m_bRaisingAlarm = false;
	}

	BaseClass::EndScheduleSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_STEALTH_ALARM_FIND_ALARM:
		{
			CAI_Hint *pHint = FindAlarmHint( GetEnemy() );
			if (pHint)
			{
				SetHintNode( pHint );
			}

			ChainStartTask( TASK_GET_PATH_TO_HINTNODE );

			if ( !HasCondition(COND_TASK_FAILED) && pHint )
			{
				if (GetNavigator()->IsGoalSet())
				{
					pHint->Lock( GetOuter() );
					pHint->NPCHandleStartNav( GetOuter(), true );

					m_bRaisingAlarm = true;

					SpeakStealthConcept( TLK_ALARM_RAISE );

					if ( g_hStealthManager )
						g_hStealthManager->NPCRaisingAlarm( GetOuter() );

					TaskComplete();
					break;
				}
			}

			TaskFail( FAIL_NO_ROUTE );
		} break;

	case TASK_STEALTH_ALARM_RAISE:
		{
			if ( GetHintNode() && GetHintNode()->HintActivityName() != NULL_STRING )
			{
				ChainStartTask( TASK_PLAY_HINT_ACTIVITY );
				break;
			}

			// No activity specified, just raise it now
			TaskComplete();
		} break;

	case TASK_STEALTH_ALARM_FINISH:
		{
			m_flNextAlarmRaiseTime = gpGlobals->curtime + 15.0f;

			if ( g_hStealthManager )
			{
				g_hStealthManager->RaiseAlarm( GetOuter(), GetHintNode() );

				if ( GetHintNode() )
				{
					// The OnUser outputs are for anything specific to individual alarms.
					// Bit of a hack since there's no dedicated output on the hint itself
					variant_t var;
					if ( g_hStealthManager->IsAlarmDisabled( GetHintNode() ) )
					{
						SpeakStealthConcept( TLK_ALARM_DISABLED );

						GetHintNode()->FireNamedOutput( "OnUser3", var, GetOuter(), GetHintNode() );
					}
					else
					{
						GetHintNode()->FireNamedOutput( "OnUser4", var, GetOuter(), GetHintNode() );
					}
				}
			}

			TaskComplete();
		} break;

	default:
		BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthAlarmBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_STEALTH_ALARM_RAISE:
		{
			ChainRunTask( TASK_PLAY_HINT_ACTIVITY );
		} break;

	default:
		BaseClass::RunTask( pTask );
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_StealthAlarmBehavior )

	DECLARE_CONDITION( COND_STEALTH_ALARM_INVALID )

	DECLARE_TASK( TASK_STEALTH_ALARM_FIND_ALARM )
	DECLARE_TASK( TASK_STEALTH_ALARM_RAISE )
	DECLARE_TASK( TASK_STEALTH_ALARM_FINISH )

	//---------------------------------

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_RAISE_ALARM,

		"	Tasks"
		"		TASK_STEALTH_ALARM_FIND_ALARM		0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING		1"
		"		TASK_STEALTH_ALARM_RAISE		0"
		"		TASK_STEALTH_ALARM_FINISH		0"
		""
		"	Interrupts"
		"		COND_STEALTH_ALARM_INVALID"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()
