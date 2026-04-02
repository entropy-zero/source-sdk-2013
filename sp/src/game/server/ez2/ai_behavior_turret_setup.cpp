//=============================================================================//
//
// Purpose:		AI behavior
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_behavior_turret_setup.h"
#include "ai_hint.h"
#include "scriptevent.h"
#include "npcevent.h"
#include "ai_squad.h"
#include "ai_squadslot.h"
#include "npc_turret_floor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_TURRET_DIST		2048.0f

ConVar	ai_debug_turret_setup( "ai_debug_turret_setup", "0" );

int	ACT_TURRET_CARRY_RUN;
int	ACT_TURRET_CARRY_WALK;
int	ACT_TURRET_CARRY_IDLE;
int	ACT_TURRET_DROP;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_TurretSetupBehavior )

	DEFINE_FIELD( m_hTurret, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bCarryingTurret, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vecTurretDropPos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_angTurretDropAngle, FIELD_VECTOR ),

	DEFINE_FIELD( m_iHolsterLayer, FIELD_INTEGER ),
	DEFINE_FIELD( m_bHolsteredWeapon, FIELD_BOOLEAN ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_TurretSetupBehavior::CAI_TurretSetupBehavior()
{
	m_vecTurretDropPos = vec3_invalid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_FloorTurret *CAI_TurretSetupBehavior::FindTurretToPickup( Vector &vecDropPos, QAngle &angDropAngle )
{
	CNPC_FloorTurret *pBestTurret = NULL;
	float flBestTurretDistSqr = Square( MAX_TURRET_DIST );
	Vector vecBestDropPos = vec3_invalid;
	QAngle angBestDropAngle = vec3_angle;

	for ( int i = 0; i < IFloorTurretAutoList::AutoList().Count(); i++ )
	{
		CNPC_FloorTurret *pTurret = static_cast<CNPC_FloorTurret *>( IFloorTurretAutoList::AutoList()[i] );
		if (pTurret && GetOuter()->IRelationType(pTurret) == D_LI)
		{
			float flTurretDistSqr = (GetOuter()->GetAbsOrigin() - pTurret->GetAbsOrigin()).LengthSqr();
			if ( flTurretDistSqr > flBestTurretDistSqr )
				continue;

			if ( ShouldPickupTurret( pTurret, vecBestDropPos, angBestDropAngle ) )
			{
				pBestTurret = pTurret;
				flBestTurretDistSqr = flTurretDistSqr;
			}
		}
	}

	if ( pBestTurret )
	{
		// Ensure drop pos is on ground
		trace_t tr;
		UTIL_TraceLine( vecBestDropPos, vecBestDropPos - Vector(0,0,64), MASK_NPCSOLID, pBestTurret, COLLISION_GROUP_NONE, &tr );
		vecBestDropPos = tr.endpos + Vector(0,0,1);
	}

	vecDropPos = vecBestDropPos;
	angDropAngle = angBestDropAngle;
	return pBestTurret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TurretSetupBehavior::ShouldPickupTurret( CNPC_FloorTurret *pTurret, Vector &vecDropPos, QAngle &angDropAngle )
{
	// Turret already has a carrier
	if ( pTurret->m_hNPCCarrier && pTurret->m_hNPCCarrier != GetOuter() )
		return false;

	if ( pTurret->HasSpawnFlags( SF_FLOOR_TURRET_OUT_OF_AMMO ) )
		return false;

	if ( pTurret->OnSide() && pTurret->m_flThrashTime < gpGlobals->curtime )
	{
		// For now, only pick up downed turrets that we can see or a squadmate can see
		// This is to get around turrets that are inaccessible, turrets we wouldn't know are tipped, etc.
		if ( GetOuter()->GetSquad() )
		{
			bool bVisible = false;
			AISquadIter_t iter;
			for ( CAI_BaseNPC *pSquadMember = GetOuter()->GetSquad()->GetFirstMember( &iter ); pSquadMember; pSquadMember = GetOuter()->GetSquad()->GetNextMember( &iter ) )
			{
				if ( pSquadMember->FVisible( pTurret ) )
				{
					bVisible = true;
					break;
				}
			}

			if ( !bVisible )
				return false;
		}
		else if ( !GetOuter()->FVisible( pTurret ) )
			return false;

		// Make sure we can stand on it
		trace_t tr;
		UTIL_TraceEntity( GetOuter(), pTurret->WorldSpaceCenter(), pTurret->WorldSpaceCenter(), MASK_NPCSOLID, pTurret, COLLISION_GROUP_NONE, &tr );

		if ( !tr.startsolid )
		{
			vecDropPos = pTurret->m_vecStandOrigin;
			angDropAngle = pTurret->m_angStandAngles;
			return true;
		}
		else if ( ai_debug_turret_setup.GetBool() )
			NDebugOverlay::Box( pTurret->WorldSpaceCenter(), GetOuter()->GetHullMins(), GetOuter()->GetHullMaxs(), 255, 0, 0, 128, 2.0f );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::PickupTurret( CNPC_FloorTurret *pTurret )
{
	if ( pTurret->m_iEyeState != TURRET_EYE_DEAD )
	{
		pTurret->SetThink( &CNPC_FloorTurret::InactiveThink );
		pTurret->SetNextThink( gpGlobals->curtime );
		pTurret->SetEyeState( TURRET_EYE_DEAD );
	}

	if ( pTurret->VPhysicsGetObject() )
	{
		pTurret->VPhysicsDestroyObject();
		pTurret->SetMoveType( MOVETYPE_NONE );
	}

	pTurret->SetParent( GetOuter(), GetOuter()->LookupAttachment( "anim_attachment_LH" ) );
	pTurret->SetSequence( pTurret->LookupSequence( "carry_pose" ) );

	pTurret->SetLocalOrigin( vec3_origin );
	pTurret->SetLocalAngles( vec3_angle );

	m_bCarryingTurret = true;

	pTurret->m_hNPCCarrier = GetOuter();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::DropTurret( CNPC_FloorTurret *pTurret, const Vector *vecPos, const QAngle *angAngles )
{
	pTurret->SetParent( NULL );

	if ( vecPos || angAngles )
		pTurret->Teleport( vecPos, angAngles, NULL );
	else if ( GetActivity() == (Activity)ACT_TURRET_DROP )
	{
		// At least place it in front of us
		Vector vecForward;
		GetOuter()->GetVectors( &vecForward, NULL, NULL );

		Vector vecTurretPos = GetAbsOrigin() + (vecForward * 48.0f);
	}

	if ( !pTurret->VPhysicsGetObject() )
	{
		pTurret->CreateVPhysics();
	}

	m_bCarryingTurret = false;

	pTurret->m_hNPCCarrier = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TurretSetupBehavior::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
	case HINT_TURRET_SETUP:
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
CAI_Hint *CAI_TurretSetupBehavior::FindTurretSetupHint( CBaseEntity *pEnemy )
{
	/*
	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_TURRET_SETUP );

	int iBits = bits_HINT_NODE_USE_GROUP;
	if ( ai_debug_turret_setup.GetBool() )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	hintCriteria.AddIncludePosition( GetAbsOrigin(), MAX_TURRET_DIST );

	CUtlVector<CAI_Hint *> vecHints;
	int nNumHints = CAI_HintManager::FindAllHints( GetOuter(), hintCriteria, &vecHints );
	*/

	// TODO
	return NULL;

	/*
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
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	//criteriaSet.AppendCriteria( "raising_alarm", IsRaisingAlarm() ? "1" : "0" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TurretSetupBehavior::SelectSchedule()
{
	if ( m_bCarryingTurret )
	{
		// TODO: Only interrupt if we weren't expecting an enemy
		if ( GetEnemy() )
		{
			m_vecTurretDropPos = vec3_invalid;
			return SCHED_TURRETSETUP_DROP;
		}

		if ( m_vecTurretDropPos != vec3_invalid )
			return GetNpcState() == NPC_STATE_COMBAT ? SCHED_TURRETSETUP_RUN_TO_DROP_POS : SCHED_TURRETSETUP_WALK_TO_DROP_POS;
	}
	else if ( m_hTurret )
	{
		if ( !HasCondition( COND_HEAR_COMBAT ) && !HasCondition( COND_HEAR_DANGER ) )
		{
			// Pick it up
			return GetNpcState() == NPC_STATE_COMBAT ? SCHED_TURRETSETUP_RUN_TO_PICKUP : SCHED_TURRETSETUP_WALK_TO_PICKUP;
			//return SCHED_TURRETSETUP_PICKUP;
		}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TurretSetupBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TurretSetupBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( IsCurSchedule( SCHED_TURRETSETUP_WALK_TO_DROP_POS, false ) || IsCurSchedule( SCHED_TURRETSETUP_RUN_TO_DROP_POS, false ) )
	{
		m_vecTurretDropPos = vec3_invalid;
		return SCHED_TURRETSETUP_DROP;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::GatherConditions( void )
{
	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TurretSetupBehavior::CanSelectSchedule( void )
{
	if ( m_bCarryingTurret )
		return true;

	if ( /*GetOuter()->GetSquad() ? GetOuter()->HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) :*/ GetNpcState() == NPC_STATE_COMBAT )
		return false;

	if ( GetOuter()->IsPropShieldEquipped() )
		return false;
	
	if ( m_hTurret )
		return true;

	m_hTurret = FindTurretToPickup( m_vecTurretDropPos, m_angTurretDropAngle );
	if ( m_hTurret )
	{
		m_hTurret->m_hNPCCarrier = GetOuter();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::EndScheduleSelection( void )
{
	if ( m_bCarryingTurret && m_hTurret )
	{
		// Just drop it here
		m_vecTurretDropPos = vec3_invalid;
		DropTurret( m_hTurret );
		m_hTurret = NULL;
	}

	if ( m_bHolsteredWeapon )
	{
		GetOuter()->UnholsterWeapon();
	}

	BaseClass::EndScheduleSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::Event_Killed( const CTakeDamageInfo &info )
{
	if ( m_hTurret && m_bCarryingTurret )
	{
		// Just drop it here
		m_vecTurretDropPos = vec3_invalid;
		DropTurret( m_hTurret );
		m_hTurret = NULL;
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CAI_TurretSetupBehavior::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( m_bCarryingTurret )
	{
		switch ( eNewActivity )
		{
			case ACT_WALK:
			case ACT_WALK_RELAXED:
			case ACT_WALK_STIMULATED:
				return (Activity)ACT_TURRET_CARRY_WALK;
			case ACT_RUN:
			case ACT_RUN_RELAXED:
			case ACT_RUN_STIMULATED:
				return (Activity)ACT_TURRET_CARRY_RUN;
			case ACT_IDLE:
			case ACT_IDLE_RELAXED:
			case ACT_IDLE_STIMULATED:
				return (Activity)ACT_TURRET_CARRY_IDLE;
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::HandleAnimEvent( animevent_t *pEvent )
{
	if ( m_hTurret )
	{
		if ( pEvent->event == SCRIPT_EVENT_FIREEVENT )
		{
			if ( GetActivity() == ACT_TURRET_DROP && m_bCarryingTurret && pEvent->options && *pEvent->options == '1' )
			{
				// Dropping our turret
				if ( m_vecTurretDropPos != vec3_invalid )
					DropTurret( m_hTurret, &m_vecTurretDropPos, &m_angTurretDropAngle );
				else
					DropTurret( m_hTurret );

				m_hTurret = NULL;
				return;
			}
		}
		else if ( pEvent->event == NPC_EVENT_ITEM_PICKUP )
		{
			// Picking up a turret
			PickupTurret( m_hTurret );
			return;
		}
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TURRETSETUP_GET_PATH_TO_TURRET:
		{
			if ( m_hTurret == NULL )
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else 
			{
				AI_NavGoal_t goal( static_cast<const Vector&>(m_hTurret->WorldSpaceCenter()) );
				goal.pTarget = m_hTurret;
				GetNavigator()->SetGoal( goal );
			}
		}
		break;
		
	case TASK_TURRETSETUP_PICKUP_TURRET:
		{
			if ( m_hTurret && !m_hTurret->OnSide() )
			{
				GetOuter()->SetIdealActivity( ACT_PICKUP_RACK );
			}
			else
			{
				GetOuter()->SetIdealActivity( ACT_PICKUP_GROUND );
			}
		}
		break;

	case TASK_TURRETSETUP_GET_PATH_TO_DROP_POS:
		{
			if ( m_vecTurretDropPos == vec3_invalid )
			{
				TaskFail(FAIL_NO_TARGET);
			}
			else 
			{
				// Try to get slightly behind the drop position
				Vector vecDir;
				AngleVectors( m_angTurretDropAngle, &vecDir );

				Vector vecGoalPos = m_vecTurretDropPos - ( vecDir * 48.0f );

				trace_t tr;
				UTIL_TraceEntity( GetOuter(), vecGoalPos + Vector(0,0,1), vecGoalPos + Vector(0,0,1), MASK_NPCSOLID, &tr);

				if ( !tr.startsolid && GetNavigator()->SetGoal( vecGoalPos ) )
				{
					GetNavigator()->SetArrivalDirection( vecDir );
					GetNavigator()->SetArrivalActivity( (Activity)ACT_TURRET_DROP );
					GetNavigator()->SetGoalTolerance( 2.0f );
				}
				else
				{
					// Go directly to the position with a high tolerance distance, then
					if ( GetNavigator()->SetGoal( m_vecTurretDropPos ) )
					{
						GetNavigator()->SetGoalTolerance( 32.0f );
					}
				}
			}
		}
		break;

	case TASK_TURRETSETUP_FACE_DROP_POS:
		{
			// See TASK_TURRETSETUP_GET_PATH_TO_DROP_POS
			if ( GetNavigator()->GetGoalTolerance() == 2.0f )
			{
				// Behind the position
				Vector vecDir;
				AngleVectors( m_angTurretDropAngle, &vecDir );
				GetMotor()->SetIdealYaw( vecDir );
			}
			else
			{
				// Directly at position
				GetMotor()->SetIdealYawToTarget( m_vecTurretDropPos );
			}
			GetOuter()->SetTurnActivity();
		}
		break;

	case TASK_TURRETSETUP_HOLSTER_WEAPON:
		{
			m_iHolsterLayer = GetOuter()->HolsterWeapon();
			
			if ( m_iHolsterLayer == -1 )
			{
				TaskComplete();
			}
			else
				m_bHolsteredWeapon = true;
		}
		break;

	case TASK_TURRETSETUP_UNHOLSTER_WEAPON:
		{
			GetOuter()->ResetActivity();

			if ( m_bHolsteredWeapon )
			{
				m_iHolsterLayer = GetOuter()->UnholsterWeapon();
				m_bHolsteredWeapon = false;
			}
			else
				m_iHolsterLayer = -1;
			
			//if ( m_iHolsterLayer == -1 )
			{
				TaskComplete();
			}
		}
		break;

	default:
		BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TurretSetupBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TURRETSETUP_PICKUP_TURRET:
		{
			if ( !m_hTurret )
			{
				TaskFail( FAIL_NO_TARGET );
				break;
			}

			GetMotor()->SetIdealYawToTargetAndUpdate( m_hTurret->WorldSpaceCenter() );

			if ( GetOuter()->IsActivityFinished() || m_bCarryingTurret )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_TURRETSETUP_FACE_DROP_POS:
		{
			// If the yaw is locked, this function will not act correctly
			Assert( GetMotor()->IsYawLocked() == false );

			GetMotor()->UpdateYaw();

			if ( GetOuter()->FacingIdeal() )
			{
				TaskComplete();
			}
		}
		break;

	case TASK_TURRETSETUP_HOLSTER_WEAPON:
	case TASK_TURRETSETUP_UNHOLSTER_WEAPON:
		{
			CAnimationLayer *pLayer = GetOuter()->GetAnimOverlay( m_iHolsterLayer );
			if ( !pLayer || !pLayer->IsActive() || pLayer->m_flCycle > 0.25f ) // At least a quarter through
			{
				TaskComplete();
			}

			if ( m_hTurret )
			{
				// Face the turret while we wait to holster
				GetMotor()->SetIdealYawToTargetAndUpdate( m_hTurret->WorldSpaceCenter() );
			}
		}
		break;

	default:
		BaseClass::RunTask( pTask );
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_TurretSetupBehavior )

	DECLARE_ACTIVITY( ACT_TURRET_CARRY_IDLE )
	DECLARE_ACTIVITY( ACT_TURRET_CARRY_WALK )
	DECLARE_ACTIVITY( ACT_TURRET_CARRY_RUN )
	DECLARE_ACTIVITY( ACT_TURRET_DROP )

	DECLARE_TASK( TASK_TURRETSETUP_GET_PATH_TO_TURRET )
	DECLARE_TASK( TASK_TURRETSETUP_PICKUP_TURRET )
	DECLARE_TASK( TASK_TURRETSETUP_GET_PATH_TO_DROP_POS )
	DECLARE_TASK( TASK_TURRETSETUP_FACE_DROP_POS )
	DECLARE_TASK( TASK_TURRETSETUP_HOLSTER_WEAPON )
	DECLARE_TASK( TASK_TURRETSETUP_UNHOLSTER_WEAPON )

	//---------------------------------

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_WALK_TO_PICKUP,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_TOLERANCE_DISTANCE		32"
		"		TASK_TURRETSETUP_GET_PATH_TO_TURRET	0"
		"		TASK_WALK_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TURRETSETUP_PICKUP"
		""
		"	Interrupts"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_RUN_TO_PICKUP,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_TOLERANCE_DISTANCE		32"
		"		TASK_TURRETSETUP_GET_PATH_TO_TURRET	0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TURRETSETUP_PICKUP"
		""
		"	Interrupts"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_PICKUP,

		"	Tasks"
		"		TASK_TURRETSETUP_HOLSTER_WEAPON	0"
		"		TASK_TURRETSETUP_PICKUP_TURRET	0"
		//"		TASK_WAIT						1"// Don't move before done standing up
		""
		"	Interrupts"
		"		"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_WALK_TO_DROP_POS,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		//"		TASK_SET_TOLERANCE_DISTANCE		5"
		"		TASK_TURRETSETUP_GET_PATH_TO_DROP_POS	0"
		"		TASK_WALK_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING				0"
		"		TASK_TURRETSETUP_FACE_DROP_POS	0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TURRETSETUP_DROP"
		""
		"	Interrupts"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_RUN_TO_DROP_POS,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		//"		TASK_SET_TOLERANCE_DISTANCE		5"
		"		TASK_TURRETSETUP_GET_PATH_TO_DROP_POS	0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING				0"
		"		TASK_TURRETSETUP_FACE_DROP_POS	0"
		"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_TURRETSETUP_DROP"
		""
		"	Interrupts"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TURRETSETUP_DROP,

		"	Tasks"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_TURRET_DROP"
		"		TASK_TURRETSETUP_UNHOLSTER_WEAPON	0"
		""
		"	Interrupts"
		"		"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()
