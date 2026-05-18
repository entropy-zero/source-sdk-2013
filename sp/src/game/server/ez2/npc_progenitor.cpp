//=============================================================================//
//
// Purpose:		The first combat template. The first PCU. The father of every Combine soldier.
//				The ultimate "Adrian Shephard at home."
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_progenitor.h"
#include "ai_interactions.h"
#include "particle_parse.h"
#include "soundenvelope.h"
#include "saverestore_utlvector.h"
#include "ai_tacticalservices.h"
#include "SpriteTrail.h"
#include "explode.h"
#include "IEffects.h"
#include "ez2_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_progenitor_health( "sk_progenitor_health","1000" );
ConVar	sk_progenitor_kick( "sk_progenitor_kick", "20" );
ConVar	sk_progenitor_shield_fire_rate( "sk_progenitor_shield_fire_rate", "2.0" );
ConVar	sk_progenitor_shield_max_time( "sk_progenitor_shield_max_time", "10" );
ConVar	sk_progenitor_shield_throw_speed( "sk_progenitor_shield_throw_speed", "650" );
ConVar	sk_progenitor_shield_throw_dmg( "sk_progenitor_shield_throw_dmg", "50" );
ConVar	sk_progenitor_shield_throw_radius( "sk_progenitor_shield_throw_radius", "96" );

#define SHIELD_SPRITE			"sprites/glow02.vmt"
#define SHIELD_THROWN_MODEL		"models/weapons/w_progenitor_energy_shield_thrown.mdl"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Progenitor )

	DEFINE_FIELD( m_bThrowSatchels, FIELD_BOOLEAN ),
	DEFINE_UTLVECTOR( m_hSatchels, FIELD_EHANDLE ),

	DEFINE_FIELD( m_flNextShieldStateCheck, FIELD_TIME ),
	DEFINE_FIELD( m_flShieldDeactivateTime, FIELD_TIME ),
	DEFINE_FIELD( m_hShieldLight, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hShieldSprite, FIELD_EHANDLE ),
	DEFINE_SOUNDPATCH( m_pShieldSound ),
	DEFINE_ARRAY( m_hShieldSpriteTrails, FIELD_EHANDLE, SHIELD_NUM_CORNERS ),

	DEFINE_CONSCRIPT_DATADESC()
	DEFINE_PROPSHIELD_DATADESC()

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_progenitor, CNPC_Progenitor );

CNPC_Progenitor::CNPC_Progenitor()
{
	SetThrowXenGrenades( false );
	m_SquadName = MAKE_STRING( "prog_squad" );
	m_iSpawnsWithShield = TRS_NONE;
	m_TripminePlaceBehavior.SetTripmineCapable( true );

	m_bThrowSatchels = true;
	m_flNextShieldStateCheck = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::Spawn( void )
{
	BaseClass::Spawn();

	// Stronger, tougher.
	SetHealth( sk_progenitor_health.GetFloat() );
	SetMaxHealth( sk_progenitor_health.GetFloat() );
	SetKickDamage( sk_progenitor_kick.GetFloat() );

	// UNDONE: Custom glow color
	//AddContext( "glowcolor", "0 128 255" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Progenitor::Precache()
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/progenitor.mdl" ) );
	}

	PrecacheScriptSound( "Weapon_EnergyShield.Holster" );
	PrecacheScriptSound( "Weapon_EnergyShield.Unholster" );
	PrecacheScriptSound( "Weapon_EnergyShield.Idle" );
	PrecacheScriptSound( "Weapon_EnergyShield.Thrown_Loop" );

	PrecacheParticleSystem( "temporal_striderbuster_attach_flash" );
	PrecacheParticleSystem( "temporalboss_electrical_arc_01" );

	PrecacheModel( SHIELD_THROWN_MODEL );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Progenitor::Activate()
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	if ( m_hShieldLight )
	{
		UTIL_Remove( m_hShieldLight );
		m_hShieldLight = NULL;
	}

	if ( m_hShieldSprite )
	{
		UTIL_Remove( m_hShieldSprite );
		m_hShieldSprite = NULL;
	}
	
	for ( int i = 0; i < SHIELD_NUM_CORNERS; i++ )
	{
		if ( m_hShieldSpriteTrails[i] )
		{
			UTIL_Remove( m_hShieldSpriteTrails[i] );
			m_hShieldSpriteTrails[i] = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::StopLoopingSounds()
{
	BaseClass::StopLoopingSounds();

	if ( m_pShieldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundDestroy( m_pShieldSound );
		m_pShieldSound = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Progenitor::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	int ret = BaseClass::OnTakeDamage_Alive( info );
	if ( ret != 1 )
		return ret;

	if ( IsPropShieldEquipped() )
	{
		if ( info.GetAttacker() && info.GetAttacker() != GetEnemy() )
		{
			// If we're being attacked by one of our other enemies while we have our shield equipped, switch to that enemy
			if ( GetEnemies()->Find( info.GetAttacker() ) )
			{
				SetEnemy( info.GetAttacker(), false );
			}
		}
	}
	else
	{
		m_flNextShieldStateCheck = gpGlobals->curtime;
	}

	return ret;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Progenitor::GatherConditions()
{
	BaseClass::GatherConditions();

	if ( CanUseShieldDuringAI() && GetEnemy() )
	{
		if ( ( HasCondition( COND_NEW_ENEMY ) || HasCondition( COND_HEAVY_DAMAGE ) ) && !IsPropShieldEquipped() )
		{
			m_flNextShieldStateCheck = gpGlobals->curtime;
		}

		if ( m_flNextShieldStateCheck < gpGlobals->curtime )
		{
			m_flNextShieldStateCheck = gpGlobals->curtime + 1.0f;

			if ( !IsPropShieldEquipped() )
			{
				if ( ShouldActivateShield() )
				{
					int nLayer = AddActionGesture( (Activity)ACT_ACTIVATE_SHIELD );
					SetCondition( COND_COMBINE_SHIELD_RETREAT );
					m_flNextShieldStateCheck = gpGlobals->curtime + 5.0f;

					// Do it quickly if the enemy is really close
					if ( nLayer != -1 && ( GetEnemyLKP() - GetAbsOrigin() ).LengthSqr() < Square( 200.0f ) )
						SetLayerPlaybackRate( nLayer, 1.5f );
				}
			}
			else
			{
				bool bCanThrow = false;
				if ( HasCondition( COND_SEE_ENEMY ) || FVisible( GetEnemyLKP() ) )
				{
					// See if we can do a trace forward
					Vector vecShieldOrigin = IsCrouching() ? EyePosition() : m_hShield->GetAbsOrigin();
					trace_t tr;
					Vector bounds( 24, 24, 24 );
					Vector vecTarget = vecShieldOrigin + ( BodyDirection3D() * 64.0f );
					UTIL_TraceHull( vecShieldOrigin, vecTarget, -bounds, bounds, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
					if ( !tr.DidHit() || tr.m_pEnt == GetEnemy() )
						bCanThrow = true;
				}

				if ( ShouldDeactivateShield( bCanThrow ) )
				{
					int nLayer = -1;
					if ( HasCondition( COND_SEE_ENEMY ) )
					{
						// See if we can do a trace forward
						trace_t tr;
						Vector maxs( 24, 24, 24 );
						Vector vecTarget = m_hShield->GetAbsOrigin() + ( BodyDirection3D() * 64.0f );
						UTIL_TraceHull( m_hShield->GetAbsOrigin(), vecTarget, -maxs, maxs, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
						if ( !tr.DidHit() || tr.m_pEnt == GetEnemy() )
							nLayer = AddActionGesture( (Activity)ACT_THROW_SHIELD );
					}
				
					if ( nLayer == -1 )
					{
						AddActionGesture( (Activity)ACT_DEACTIVATE_SHIELD );
					}

					m_flNextShieldStateCheck = gpGlobals->curtime + 5.0f;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldActivateShield()
{
	switch ( GetActivity() )
	{
		case ACT_IDLE:
		case ACT_IDLE_ANGRY:
		case ACT_WALK:
		case ACT_WALK_AIM:
		case ACT_RUN:
		case ACT_RUN_AIM:
		case ACT_RANGE_ATTACK1:
		case ACT_TRANSITION:
			break;
		default:
			// Can't equip shield if we're playing a unique activity
			return false;
	}

	if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY, false )
		|| IsCurSchedule( SCHED_COMBINE_TAKE_COVER1, false )
		|| IsCurSchedule( SCHED_RUN_FROM_ENEMY, false )
		|| IsCurSchedule( SCHED_HIDE_AND_RELOAD )
		|| HasCondition( COND_NO_PRIMARY_AMMO ) )
	{
		// We're taking cover or need to take cover
		// Activate the shield if we're damaged and the enemy is facing me,
		// OR if we have more than 3 enemies and our current enemy can range attack
		if ( ( HasCondition( COND_LIGHT_DAMAGE ) && HasCondition( COND_ENEMY_FACING_ME ) )
			|| ( GetEnemies()->NumEnemies() > 3 && GetEnemy()->IsNPC()
				&& GetEnemy()->MyNPCPointer()->CapabilitiesGet() &
				(bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2
				| bits_CAP_WEAPON_RANGE_ATTACK1 | bits_CAP_WEAPON_RANGE_ATTACK2) ) )
		{
			return true;
		}
	}

	if ( (gpGlobals->curtime - m_flLastDamageTime) < 1.0 )
	{
		// If we're receiving lots of damage, try to equip a shield
		if ( IsCurSchedule( SCHED_RANGE_ATTACK1 ) && m_flSumDamage > 25.0f )
		{
			return true;
		}
		else if ( m_flSumDamage > 50.0f && ConditionInterruptsCurSchedule( COND_HEAVY_DAMAGE ) )
		{
			return true;
		}
	}
	
	if ( HasCondition( COND_ENEMY_FACING_ME ) && GetHealth() < ( GetMaxHealth() * 0.5f ) && FVisible( GetEnemyLKP() ) )
	{
		return true;
	}

	// If the enemy has a shotgun, and that enemy is close, then activate our shield
	CBaseCombatCharacter *pBCC = GetEnemy()->MyCombatCharacterPointer();
	if ( pBCC && pBCC->GetActiveWeapon() && pBCC->GetActiveWeapon()->m_iClassname == gm_iszShotgunClassname )
	{
		if ( ( GetEnemyLKP() - GetAbsOrigin() ).LengthSqr() < Square( 200.0f ) )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldDeactivateShield( bool &bThrow )
{
	// We're trying to press on now
	if ( IsCurSchedule( SCHED_COMBINE_ASSAULT )
		|| IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK )
		|| IsCurSchedule( SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE ) )
		return true;

	if ( m_flShieldDeactivateTime < gpGlobals->curtime )
		return true;

	if ( bThrow )
	{
		if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY, false ) && GetEnemy() /*&& (gpGlobals->curtime - m_flLastDamageTime) > 1.0f*/ )
		{
			// If we are about to get into cover and the enemy isn't right on us, then throw to cover our escape
			Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
			if ( vecToEnemy.LengthSqr() > Square( 200.0f ) )
				return true;
		}
	}

	// We're in cover
	if ( !FVisible( GetEnemyLKP() ) )
	{
		bThrow = false;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Progenitor::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if ( ConditionInterruptsCurSchedule( COND_HEAVY_DAMAGE ) )
	{
		SetCustomInterruptCondition( COND_COMBINE_SHIELD_RETREAT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::PrescheduleThink()
{
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Progenitor::SelectSchedule( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Progenitor::TranslateSchedule( int scheduleType )
{
	scheduleType = BaseClass::TranslateSchedule( scheduleType );

	switch (scheduleType)
	{
		case SCHED_COMBINE_RANGE_ATTACK1:
		case SCHED_COMBINE_MERCILESS_RANGE_ATTACK1:
		case SCHED_COMBINE_MERCILESS_SUPPRESS:
		case SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP:
		case SCHED_IDLE_STAND:
		case SCHED_ALERT_STAND:
		case SCHED_COMBINE_PATROL:
		case SCHED_PATROL_WALK:
			{
				// Make sure I'm not standing near one of my satchels. If so, move away from it
				FOR_EACH_VEC( m_hSatchels, i )
				{
					if ( m_hSatchels[i] )
					{
						float flDistSqr = ( m_hSatchels[i]->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
						if ( flDistSqr < Square( 200.0f ) )
						{
							SetTarget( m_hSatchels[i] );

							if ( GetState() == NPC_STATE_COMBAT )
								return SCHED_COMBINE_RUN_AWAY_FROM_TARGET;
							else
								return SCHED_COMBINE_WALK_AWAY_FROM_TARGET;
						}
					}
				}
			}
			break;
		case SCHED_COMBINE_FLANK_LINE_OF_FIRE:
		case SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE:
		case SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE:
		case SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE:
			{
				if ( IsPropShieldEquipped() )
				{
					// Prefer to take cover when we have a shield
					if ( HasMemory( bits_MEMORY_INCOVER ) && !HasCondition( COND_SEE_ENEMY ) )
						return SCHED_COMBAT_FACE;

					return SCHED_TAKE_COVER_FROM_ENEMY;
				}
				else if ( ( GetHealth() <= GetMaxHealth() * 0.5 ) || GetEnemies()->NumEnemies() > 3 )
				{
					// Be a bit more cagey when we're in a bad spot
					if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) )
						return SCHED_COMBINE_GRENADE_COVER1;

					//if ( !HasMemory( bits_MEMORY_INCOVER ) )
					//	return SCHED_TAKE_COVER_FROM_ENEMY;
				}
			}
			break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Progenitor::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_DEFER_DODGE:
			// Progenitor can dodge again sooner
			m_flNextDodgeTime = gpGlobals->curtime + (pTask->flTaskData * 0.25f);
			TaskComplete();
			break;

		/*case TASK_COMBINE_IGNORE_ATTACKS:
			{
				if ( IsPropShieldEquipped() )
				{
					// Account for shield delay
					if (m_pSquad && m_pSquad->NumMembers() > 2)
					{
						if (GetEnemy() && (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).Length() > 512.0 )
						{
							m_flNextAttack	= gpGlobals->curtime + pTask->flTaskData;
							break;
						}
					}
				}
			}
			break;*/

		case TASK_RANGE_ATTACK1:
			{
				BaseClass::StartTask( pTask );

				if ( IsPropShieldEquipped() )
				{
					// Account for shield delay
					float flDelay = GetActiveWeapon()->GetFireRate() * sk_progenitor_shield_fire_rate.GetFloat();
					SetShotDelay( flDelay );
					m_flNextAttack = gpGlobals->curtime + flDelay - 0.1;
				}
			}
			break;

		case TASK_RUN_PATH:
			{
				BaseClass::StartTask( pTask );

				// See if we should drop a satchel to cover our escape
				// (have to do this here and not in something like OnStartSchedule
				// because we don't know if we'll be moving until now)
				if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY, false )
					|| IsCurSchedule( SCHED_COMBINE_HIDE_AND_RELOAD, false ) )
				{
					if ( GetNavigator()->IsGoalSet()
						&& ( GetNavigator()->GetGoalPos() - GetAbsOrigin() ).LengthSqr() > Square( 128.0f )
						&& abs( GetAbsOrigin().z - GetEnemyLKP().z ) < 64.0f && !IsPropShieldEquipped() && ShouldThrowProximitySatchel( true ) )
					{
						AddActionGesture( ACT_GESTURE_SPECIAL_ATTACK2 );
					}
				}
			}
			break;

		/*case TASK_GET_PATH_TO_ENEMY_LOS:
		case TASK_GET_PATH_TO_ENEMY_LKP_LOS:
			{
				ChainStartTask( TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS );
			}
			break;*/

		case TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET:
			{
				if ( GetTarget() != NULL )
				{
					Vector vecBackAwayOrigin = GetTarget()->GetAbsOrigin();

					// Find a point closer to me for the AI to use as a reference
					Vector vecToTarget = ( vecBackAwayOrigin - GetAbsOrigin() );
					VectorNormalize( vecToTarget );

					vecBackAwayOrigin = GetAbsOrigin() + ( vecToTarget * 24.0f );

					Vector backPos;
					if ( !GetTacticalServices()->FindBackAwayPos( vecBackAwayOrigin, &backPos ) )
					{
						// no place to backaway
						TaskFail(FAIL_NO_BACKAWAY_NODE);
					}
					else 
					{
						if (GetNavigator()->SetGoal( AI_NavGoal_t( backPos, ACT_RUN ) ) )
						{
							TaskComplete();
						}
						else
						{
							// no place to backaway
							TaskFail(FAIL_NO_ROUTE);
						}
					}
				}
				else
				{
					TaskFail(FAIL_NO_ENEMY);
				}
			}
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if (interactionType == g_interactionBadCopKickWarn || interactionType == g_interactionZombieMeleeWarning || interactionType == g_interactionGenericMeleeWarning)
	{
		// Dodge away if I'm running a schedule that will be interrupted if I'm hit.
		if (!IsInAScript() && IsInterruptable() && (ConditionInterruptsCurSchedule(COND_LIGHT_DAMAGE) || ConditionInterruptsCurSchedule(COND_HEAVY_DAMAGE) || IsCurSchedule(SCHED_MELEE_ATTACK1)) && IsAllowedToDodge())
		{
			if (sourceEnt /*&& FInViewCone(sourceEnt)*/)
			{
				UpdateEnemyMemory( sourceEnt, sourceEnt->GetAbsOrigin(), this );
				SetTarget( sourceEnt );
				SetSchedule( SCHED_COMBINE_DODGE );
			}
		}

		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldDodgeProjectile( CBaseEntity *pProjectile )
{
	// Disregard base class viewcone check
	// The Progenitor has super reflexes
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	bool bResult = BaseClass::MovementCost( moveType, vecStart, vecEnd, pCost );

	if ( moveType == bits_CAP_MOVE_GROUND )
	{
		// Stay away from my satchels and tripmines
		const float MAX_SATCHEL_DIST_SQR = Square( 250.0f );

		FOR_EACH_VEC( m_hSatchels, i )
		{
			if ( m_hSatchels[i] )
			{
				float flSatchelDistToLinkSqr = CalcDistanceSqrToLine( m_hSatchels[i]->GetAbsOrigin(), vecStart, vecEnd );
				if ( flSatchelDistToLinkSqr < MAX_SATCHEL_DIST_SQR )
				{
					bResult = true;

					if ( GetEnemy() && flSatchelDistToLinkSqr < Square( 150.0f ) )
					{
						// Avoid this node as much as possible
						*pCost *= 100.0f;

						// UNDONE: Avoid this node entirely, unless we are inside of that radius
						//if ( ( GetAbsOrigin() - m_hSatchels[i]->GetAbsOrigin() ).LengthSqr() > MAX_SATCHEL_DIST_SQR ) 
						//	*pCost = FLT_MAX;
					}
					else
					{
						// Increase cost the closer it gets to the center
						*pCost *= RemapValClamped( flSatchelDistToLinkSqr, 0.0f, MAX_SATCHEL_DIST_SQR, 10.0f, 1.0f );
					}

					//float flDebugPerc = flSatchelDistToLinkSqr / MAX_SATCHEL_DIST_SQR;
					//NDebugOverlay::HorzArrow( vecStart, vecEnd, 32.0f, 255.0f * (1.0f - flDebugPerc), 255.0f * flDebugPerc, 0, 255, true, 3.0f );
				}
			}
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CNPC_Progenitor::NPC_TranslateActivity( Activity eNewActivity )
{
	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::CanAltFireEnemy( bool bUseFreeKnowledge )
{
	if ( !BaseClass::CanAltFireEnemy( bUseFreeKnowledge ) )
		return false;

	if ( IsPropShieldEquipped() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::CanGrenadeEnemy( bool bUseFreeKnowledge )
{
	if ( !BaseClass::CanGrenadeEnemy( bUseFreeKnowledge ) )
		return false;

	if ( IsPropShieldEquipped() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::IsProximitySatchelCapable()
{
	if ( !m_bThrowSatchels )
		return false;

	// Take opportunity to clean up invalid satchels
	int nNumSatchels = 0;
	FOR_EACH_VEC_BACK( m_hSatchels, i )
	{
		if ( !m_hSatchels[i] )
			m_hSatchels.Remove( i );

		nNumSatchels++;
	}

	//if ( m_hSatchels.Count() > 8 )
	if ( nNumSatchels > 8 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldThrowProximitySatchel( bool bDrop )
{
	if ( !IsProximitySatchelCapable() )
		return false;

	// Assume that, if we're satchel capable and in a script, then proximity satchels are always desired
	if ( GetState() == NPC_STATE_SCRIPT || IsInAScript() )
		return true;

	// Non-dropping conditions
	if ( bDrop )
	{
		// Make sure there aren't already any satchels around me
		FOR_EACH_VEC( m_hSatchels, i )
		{
			if ( m_hSatchels[i] )
			{
				if ( (m_hSatchels[i]->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < Square( 175.0f ) )
					return false;
			}
		}
	}
	else
	{
		// No need if we can see our enemy
		if ( HasCondition( COND_SEE_ENEMY ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnThrowProximitySatchel( CBaseEntity *pGrenade )
{
	m_hSatchels.AddToTail( pGrenade );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnShieldSpawn( CPropShield *pShield )
{
	BaseClass::OnShieldSpawn( pShield );

	EmitSound( "Weapon_EnergyShield.Unholster" );

	int nShieldAttach = LookupAttachment( "shield_projector" );
	DispatchParticleEffect( "temporal_striderbuster_attach_flash", PATTACH_POINT_FOLLOW, this, nShieldAttach );

	g_pEffects->Sparks( pShield->GetAbsOrigin(), 1, 1 );

	if ( !m_pShieldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		CPASAttenuationFilter filter( this );
		m_pShieldSound = controller.SoundCreate( filter, entindex(), "Weapon_EnergyShield.Idle" );

		controller.Play( m_pShieldSound, 0.0, 90.0f );
		controller.SoundChangeVolume( m_pShieldSound, 1.0f, 0.4f );
		controller.SoundChangePitch( m_pShieldSound, 100.0f, 0.5f );
	}

	if ( !m_hShieldLight )
	{
		// Yes, it's a dlight that could be done on the client with more work. Don't sue me
		m_hShieldLight = CreateNoSpawn( "light_dynamic", m_hShield->GetAbsOrigin(), m_hShield->GetAbsAngles(), this );
		m_hShieldLight->KeyValue( "_light", "0 192 255 200" );
		m_hShieldLight->KeyValue( "_cone", "0" );
		m_hShieldLight->KeyValue( "_inner_cone", "0" );
		m_hShieldLight->KeyValue( "brightness", "5" );
		m_hShieldLight->KeyValue( "distance", "150" );

		m_hShieldLight->SetParent( this, nShieldAttach );
		DispatchSpawn( m_hShieldLight );
	}

	if ( !m_hShieldSprite )
	{
		m_hShieldSprite = CSprite::SpriteCreate( SHIELD_SPRITE, GetAbsOrigin(), false );
		m_hShieldSprite->SetAttachment( this, nShieldAttach );
		m_hShieldSprite->SetTransparency( kRenderWorldGlow, 255, 192, 224, 255, kRenderFxHologram );
		m_hShieldSprite->SetBrightness( 255, 1.0f );
		m_hShieldSprite->SetScale( 0.3f, 0.5f );
		m_hShieldSprite->SetGlowProxySize( 4.0f );
		m_hShieldSprite->TurnOn();
	}

	for ( int i = 0; i < SHIELD_NUM_CORNERS; i++ )
	{
		if ( !m_hShieldSpriteTrails[i] )
		{
			m_hShieldSpriteTrails[i] = CSpriteTrail::SpriteTrailCreate( "sprites/bluelaser1.vmt", GetLocalOrigin(), false );

			m_hShieldSpriteTrails[i]->SetAttachment( pShield, pShield->LookupAttachment( UTIL_VarArgs( "shield_corner%i", i ) ) );
			m_hShieldSpriteTrails[i]->SetTransparency( kRenderTransAdd, 0, 255, 255, 200, kRenderFxNone );

			m_hShieldSpriteTrails[i]->SetStartWidth( 8.0f );
			m_hShieldSpriteTrails[i]->SetLifeTime( 1.0f );
		}
	}

	// Update shot regulator for shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();

	m_flShieldDeactivateTime = gpGlobals->curtime + sk_progenitor_shield_max_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnShieldRemove( CPropShield *pShield )
{
	BaseClass::OnShieldRemove( pShield );

	EmitSound( "Weapon_EnergyShield.Holster" );

	int nShieldAttach = LookupAttachment( "shield_projector" );
	DispatchParticleEffect( "temporalboss_electrical_arc_01", PATTACH_POINT_FOLLOW, this, nShieldAttach );

	g_pEffects->Sparks( pShield->GetAbsOrigin(), 1, 1 );

	if ( m_pShieldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangePitch( m_pShieldSound, 90.0f, 0.2f );
		controller.SoundFadeOut( m_pShieldSound, 0.25f, true );
		m_pShieldSound = NULL;
	}

	if ( m_hShieldLight )
	{
		UTIL_Remove( m_hShieldLight );
		m_hShieldLight = NULL;
	}

	if ( m_hShieldSprite )
	{
		UTIL_Remove( m_hShieldSprite );
		m_hShieldSprite = NULL;
	}
	
	for ( int i = 0; i < SHIELD_NUM_CORNERS; i++ )
	{
		if ( m_hShieldSpriteTrails[i] )
		{
			m_hShieldSpriteTrails[i]->SetParent( NULL );
			//m_hShieldSpriteTrails[i]->FadeAndDie( 1.0f );
			m_hShieldSpriteTrails[i]->SetThink( &CBaseEntity::SUB_Remove );
			m_hShieldSpriteTrails[i]->SetNextThink( gpGlobals->curtime + 2.0f );
			m_hShieldSpriteTrails[i] = NULL;
		}
	}

	// Update shot regulator for no shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Progenitor::CreateShieldProjectile( CPropShield *pShield )
{
	if ( m_pShieldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangePitch( m_pShieldSound, 90.0f, 0.2f );
		controller.SoundFadeOut( m_pShieldSound, 0.25f, true );
		m_pShieldSound = NULL;
	}

	if ( m_hShieldSprite )
	{
		UTIL_Remove( m_hShieldSprite );
		m_hShieldSprite = NULL;
	}

	// Update shot regulator for no shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();

	CPhysicsProp *pProjectile = (CPhysicsProp*)CreateNoSpawn( "prop_progenitor_thrown_shield", pShield->GetAbsOrigin(), pShield->GetAbsAngles(), this );
	if ( !pProjectile )
	{
		if ( m_hShieldLight )
		{
			UTIL_Remove( m_hShieldLight );
			m_hShieldLight = NULL;
		}
	
		for ( int i = 0; i < SHIELD_NUM_CORNERS; i++ )
		{
			if ( m_hShieldSpriteTrails[i] )
			{
				UTIL_Remove( m_hShieldSpriteTrails[i] );
				m_hShieldSpriteTrails[i] = NULL;
			}
		}

		UTIL_Remove( pShield );
		return NULL;
	}

	g_pEffects->Sparks( pShield->GetAbsOrigin(), 2, 2 );

	pProjectile->SetModelName( MAKE_STRING( SHIELD_THROWN_MODEL ) );
	DispatchSpawn( pProjectile );

	Vector vecVelocity;
	if ( GetEnemy() && sk_progenitor_shield_throw_speed.GetFloat() != 0.0f )
	{
		extern float GetCurrentGravity( void );

		// Get where we think the enemy's head is at
		Vector vecEnemyLKP = GetEnemyLKP();
		Vector vecEnemyOffset = GetEnemy()->HeadTarget( pProjectile->GetAbsOrigin() ) - GetEnemy()->GetAbsOrigin();
		Vector vecEnemyPos = vecEnemyOffset + vecEnemyLKP;

		// Now get a lead offset
		Vector vecLead = GetEnemy()->GetSmoothedVelocity();
		vecVelocity = ( vecEnemyPos - pProjectile->GetAbsOrigin() );
		float flLeadTime = ( ( vecVelocity.Length() - vecLead.Length() ) / sk_progenitor_shield_throw_speed.GetFloat() );
		vecLead *= flLeadTime;

		// Account for gravity if in air
		// TODO: Check how close enemy is to ground?
		if ( !GetEnemy()->GetGroundEntity() && (GetEnemy()->GetMoveType() == MOVETYPE_WALK || GetEnemy()->GetMoveType() == MOVETYPE_STEP) )
			vecLead.z -= ((GetCurrentGravity() * 0.5) * flLeadTime);

		// Clamp the lead velocity
		vecLead.x = clamp( vecLead.x, -750.0f, 750.0f );
		vecLead.y = clamp( vecLead.y, -750.0f, 750.0f );
		vecLead.z = clamp( vecLead.z, -750.0f, 750.0f );

		// If the enemy is above me, and the lead puts them below me, then just make it level with me
		// Quick fix for throwing the shield into the ground
		if ( vecEnemyPos.z > GetAbsOrigin().z && ( vecEnemyPos.z + vecLead.z ) < GetAbsOrigin().z )
		{
			vecLead.z = ( pProjectile->GetAbsOrigin().z - vecEnemyPos.z );
		}

		vecEnemyPos += vecLead;

		vecVelocity = ( vecEnemyPos - pProjectile->GetAbsOrigin() );
		VectorNormalize( vecVelocity );

		QAngle angAngles;
		VectorAngles( vecVelocity, angAngles );

		// Since our angles are relative to the NPC, this doesn't resolve consistently,
		// but it's fine for this
		angAngles.x -= 90.0f;

		pProjectile->SetAbsAngles( angAngles );

		vecVelocity *= sk_progenitor_shield_throw_speed.GetFloat();
	}

	IPhysicsObject *pPhys = pProjectile->VPhysicsGetObject();
	if ( pPhys )
	{
		AngularImpulse angVel( 1250.0, 0, 0 );
		pPhys->SetVelocity( &vecVelocity, &angVel );

		// Disable drag and gravity
		pPhys->EnableDrag( false );
		pPhys->EnableGravity( false );
	}

	pProjectile->OnThrownByNPC( this );
	pProjectile->EmitSound( "Weapon_EnergyShield.Thrown_Loop" );

	if ( m_hShieldLight )
	{
		m_hShieldLight->SetParent( pProjectile );
		m_hShieldLight->SetLocalOrigin( vec3_origin );
		m_hShieldLight->SetLocalAngles( vec3_angle );
		m_hShieldLight = NULL;
	}

	for ( int i = 0; i < SHIELD_NUM_CORNERS; i++ )
	{
		if ( m_hShieldSpriteTrails[i] )
		{
			m_hShieldSpriteTrails[i]->SetAttachment( pProjectile, pProjectile->LookupAttachment( UTIL_VarArgs( "shield_corner%i", i ) ) );
			m_hShieldSpriteTrails[i] = NULL;
		}
	}

	UTIL_Remove( pShield );

	return pProjectile;
}

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CPropProgenitorThrownShield )

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( EnableGravityThink ),

	DEFINE_FIELD( m_flNextDangerSoundTime, FIELD_TIME ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_progenitor_thrown_shield, CPropProgenitorThrownShield );

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CPropProgenitorThrownShield::CPropProgenitorThrownShield()
{
	m_explodeDamage = sk_progenitor_shield_throw_dmg.GetFloat();
	m_explodeRadius = sk_progenitor_shield_throw_radius.GetFloat();

	m_flNextDangerSoundTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "Weapon_EnergyShield.Thrown_Explode" );
	PrecacheParticleSystem( "temporal_striderbuster_attach_flash" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::Spawn()
{
	BaseClass::Spawn();

	RemoveFlag( FL_STATICPROP );
	SetPlaybackRate( 1.0f );

	SetContextThink( &CPropProgenitorThrownShield::AnimateThink, gpGlobals->curtime + 0.1f, "AnimateThink" );

	SetThink( &CPropProgenitorThrownShield::EnableGravityThink );
	SetNextThink( gpGlobals->curtime + 2.0f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::AnimateThink()
{
	if ( GetCycle() == 1.0f )
		SetCycle( 0 );

	StudioFrameAdvance();

	if ( m_flNextDangerSoundTime < gpGlobals->curtime )
	{
		Vector vecSoundOrigin = GetAbsOrigin();
		if ( VPhysicsGetObject() )
		{
			Vector velocity;
			VPhysicsGetObject()->GetVelocity( &velocity, NULL );
			vecSoundOrigin += (velocity * 0.5f); // Where we'll be in one half-second
		}

		CSoundEnt::InsertSound( SOUND_DANGER, vecSoundOrigin, sk_progenitor_shield_throw_radius.GetFloat() * 2.0f, 1.0f, GetOwnerEntity(), SOUNDENT_CHANNEL_REPEATING );

		m_flNextDangerSoundTime = gpGlobals->curtime + 0.25f;
	}

	SetNextThink( gpGlobals->curtime + 0.1f, "AnimateThink" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::EnableGravityThink()
{
	if ( VPhysicsGetObject() )
	{
		VPhysicsGetObject()->EnableGravity( true );
		VPhysicsGetObject()->EnableDrag( true );

		// Only enable angular drag
		float drag = 0.0f;
		VPhysicsGetObject()->SetDragCoefficient( &drag, NULL );
	}

	// Eventually just break entirely
	SetThink( &CBreakableProp::BreakThink );
	SetNextThink( gpGlobals->curtime + 3.0f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CPropProgenitorThrownShield::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( IsDissolving() )
		return 0;

	int nBase = BaseClass::OnTakeDamage( info );

	if ( nBase != 1 )
		return nBase;

	/*if ( VPhysicsGetObject() && !VPhysicsGetObject()->IsGravityEnabled() && !IsDissolving() )
	{
		// Enable gravity early
		EnableGravityThink();
	}*/

	return nBase;
}

extern int g_interactionBadCopKick;

//-----------------------------------------------------------------------------
// Purpose:  Uses the new CBaseEntity interaction implementation
// Input  :  The type of interaction, extra info pointer, and who started it
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CPropProgenitorThrownShield::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
	if ( interactionType == g_interactionBadCopKick && sourceEnt )
	{
		// Go in the opposite direction
		IPhysicsObject *pPhys = VPhysicsGetObject();
		if ( pPhys && !IsDissolving() )
		{
			//ApplyAbsVelocityImpulse( sourceEnt->EyeDirection3D() * 256.0f );

			Vector velocity;
			AngularImpulse angVelocity;
			pPhys->GetVelocity( &velocity, &angVelocity );

			velocity *= -1.0f;
			velocity += ( sourceEnt->EyeDirection3D() * 750.0f );
			
			// Add angular impulse
			KickInfo_t * info = static_cast< KickInfo_t *>( data );
			Vector vecForce;
			AngularImpulse vecTorque;
			pPhys->CalculateForceOffset( velocity, info->tr->endpos, &vecForce, &vecTorque );
			angVelocity += vecTorque;

			pPhys->SetVelocity( &velocity, &angVelocity );

			SetOwnerEntity( sourceEnt );

			if ( !pPhys->IsGravityEnabled() )
			{
				// Reset gravity timer
				SetThink( &CPropProgenitorThrownShield::EnableGravityThink );
				SetNextThink( gpGlobals->curtime + 2.0f );
			}
		}
		return true;
	}

	return BaseClass::HandleInteraction(interactionType, data, sourceEnt);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::Event_Killed( const CTakeDamageInfo &info )
{
	// Based on CBreakableProp::Event_Killed()
	// We have to override it because the base class calls UTIL_Remove, which we don't want

	if (ScriptDeathHook( const_cast<CTakeDamageInfo *>(&info) ) == false)
		return;

	IPhysicsObject *pPhysics = VPhysicsGetObject();
	if ( pPhysics && !pPhysics->IsMoveable() )
	{
		pPhysics->EnableMotion( true );
		VPhysicsTakeDamage( info );
	}
	Break( info.GetInflictor(), info );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPropProgenitorThrownShield::Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;
	m_OnBreak.FireOutput( pBreaker, this );

	Vector velocity;
	AngularImpulse angVelocity;
	IPhysicsObject *pPhysics = GetRootPhysicsObjectForBreak();

	Vector origin;
	QAngle angles;
	//AddSolidFlags( FSOLID_NOT_SOLID );
	if ( pPhysics )
	{
		pPhysics->GetVelocity( &velocity, &angVelocity );
		pPhysics->GetPosition( &origin, &angles );
		pPhysics->RecheckCollisionFilter();

		pPhysics->EnableGravity( false );
	}
	else
	{
		velocity = GetAbsVelocity();
		QAngleToAngularImpulse( GetLocalAngularVelocity(), angVelocity );
		origin = GetAbsOrigin();
		angles = GetAbsAngles();
	}

	CBaseEntity *pAttacker = GetLastAttacker();
	if ( !pAttacker )
		pAttacker = GetOwnerEntity();

	//ExplosionCreate( origin, angles, GetOwnerEntity(), m_explodeDamage, m_explodeRadius,
	//	SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE | SF_ENVEXPLOSION_NOFIREBALL | SF_ENVEXPLOSION_NOPARTICLES | SF_ENVEXPLOSION_NOSOUND,
	//	0.0f, this, DMG_SHOCK /*| DMG_BLAST*/ );

	CTakeDamageInfo expInfo( pAttacker, pAttacker, m_explodeDamage, DMG_SHOCK );
	RadiusDamage( expInfo, origin, m_explodeRadius, CLASS_NONE, pAttacker );

	DispatchParticleEffect( "temporal_striderbuster_attach_flash", origin, angles );
	EmitSound( "Weapon_EnergyShield.Thrown_Explode" );

	Vector vecSparkDir = velocity;
	VectorNormalize( vecSparkDir );
	g_pEffects->Sparks( origin, 3, 2 );
	g_pEffects->Sparks( origin, 2, 4, &vecSparkDir );

	// Detach all sprite trails
	string_t iszSpriteTrail = FindPooledString( "env_spritetrail" );
	CBaseEntity *pChild = FirstMoveChild();
	while ( pChild )
	{
		if ( pChild->m_iClassname == iszSpriteTrail )
		{
			// Need to get the move peer here because we unparent the child
			CBaseEntity *pNextChild = pChild->NextMovePeer();
			pChild->StopFollowingEntity();

			// Make sure they stop following it too
			((CSpriteTrail *)pChild)->m_hAttachedToEntity = NULL;
			((CSpriteTrail *)pChild)->m_nAttachment = 0;

			//pChild->FadeAndDie( 1.0f );
			pChild->SetThink( &CBaseEntity::SUB_Remove );
			pChild->SetNextThink( gpGlobals->curtime + 2.0f );

			pChild = pNextChild;
		}
		else
			pChild = pChild->NextMovePeer();
	}

	//UTIL_Remove( this );
	Dissolve( "", gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL ); // ENTITY_DISSOLVE_ELECTRICAL

	// Don't enable gravity anymore
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );

	m_flNextDangerSoundTime = FLT_MAX;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnUpdateShotRegulator()
{
	BaseClass::OnUpdateShotRegulator();

	if ( m_hShield ) // IsPropShieldEquipped()
	{
		// Shield decreases fire rate
		float flMinBurstInterval, flMaxBurstInterval;
		GetShotRegulator()->GetBurstInterval( &flMinBurstInterval, &flMaxBurstInterval );

		flMinBurstInterval *= sk_progenitor_shield_fire_rate.GetFloat();
		flMaxBurstInterval *= sk_progenitor_shield_fire_rate.GetFloat();

		GetShotRegulator()->SetBurstInterval( flMinBurstInterval, flMaxBurstInterval );

		// Update shot delay if we're already firing
		if ( gpGlobals->curtime - GetLastAttackTime() < 1.0f )
		{
			float flDelay = GetActiveWeapon()->GetFireRate() * sk_progenitor_shield_fire_rate.GetFloat();
			SetShotDelay( flDelay );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_Progenitor::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	if ( pWeapon->ClassMatches( "weapon_ar2_progenitor" ) )
	{
		return WEAPON_PROFICIENCY_VERY_GOOD;
	}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.r1 = 16;
	params.g1 = 192;
	params.b1 = 64;

	return true;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_progenitor, CNPC_Progenitor )

	DECLARE_TASK( TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET )

	DECLARE_CONDITION( COND_COMBINE_SHIELD_RETREAT )

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_RUN_AWAY_FROM_TARGET,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_SET_TOLERANCE_DISTANCE				48"
		"		TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET	0"
		"		TASK_COMBINE_SET_STANDING		1"
		"		TASK_RUN_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_WALK_AWAY_FROM_TARGET,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE					SCHEDULE:SCHED_RUN_RANDOM"
		"		TASK_SET_TOLERANCE_DISTANCE				48"
		"		TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET	0"
		"		TASK_COMBINE_SET_STANDING		1"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
	)

AI_END_CUSTOM_NPC()
