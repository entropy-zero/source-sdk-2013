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

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_progenitor_health( "sk_progenitor_health","1000" );
ConVar	sk_progenitor_kick( "sk_progenitor_kick", "20" );
ConVar	sk_progenitor_shield_fire_rate( "sk_progenitor_shield_fire_rate", "2.0" );

#define SHIELD_SPRITE	"sprites/glow02.vmt"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Progenitor )

	DEFINE_FIELD( m_bThrowSatchels, FIELD_BOOLEAN ),
	DEFINE_UTLVECTOR( m_hSatchels, FIELD_EHANDLE ),

	DEFINE_FIELD( m_flNextShieldStateCheck, FIELD_TIME ),
	DEFINE_FIELD( m_hShieldLight, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hShieldSprite, FIELD_EHANDLE ),
	DEFINE_SOUNDPATCH( m_pShieldSound ),

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

	PrecacheParticleSystem( "temporal_striderbuster_attach_flash" );
	PrecacheParticleSystem( "temporalboss_electrical_arc_01" );
	
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
void CNPC_Progenitor::GatherConditions()
{
	BaseClass::GatherConditions();

	if ( CanUseShieldDuringAI() && GetEnemy() )
	{
		if ( HasCondition( COND_NEW_ENEMY ) && !IsPropShieldEquipped() )
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
			else if ( ShouldDeactivateShield() )
			{
				AddActionGesture( (Activity)ACT_DEACTIVATE_SHIELD );
				m_flNextShieldStateCheck = gpGlobals->curtime + 5.0f;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldActivateShield()
{
	if ( GetActivity() != ACT_IDLE
		&& GetActivity() != ACT_WALK
		&& GetActivity() != ACT_RUN
		&& GetActivity() != ACT_RANGE_ATTACK1 )
	{
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
bool CNPC_Progenitor::ShouldDeactivateShield()
{
	// We're trying to press on now
	if ( IsCurSchedule( SCHED_COMBINE_ASSAULT )
		|| IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK )
		|| IsCurSchedule( SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE ) )
		return true;

	// We're in cover
	if ( !FVisible( GetEnemyLKP() ) )
		return true;

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
						&& !IsPropShieldEquipped() && ShouldThrowProximitySatchel( true ) )
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

	// Update shot regulator for shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();
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

	// Update shot regulator for no shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();
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
