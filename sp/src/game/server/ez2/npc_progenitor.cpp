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
#include "rope.h"
#include "ai_route.h"
#include "animation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_progenitor_health( "sk_progenitor_health","1000" );
ConVar	sk_progenitor_kick( "sk_progenitor_kick", "20" );
ConVar	sk_progenitor_slide_decel_dist( "sk_progenitor_slide_decel_dist", "100" );
ConVar	sk_progenitor_slide_always( "sk_progenitor_slide_always", "0" );
ConVar	sk_progenitor_shield_health( "sk_progenitor_shield_health", "500" );
ConVar	sk_progenitor_shield_fire_rate( "sk_progenitor_shield_fire_rate", "2.0" );
ConVar	sk_progenitor_shield_max_time( "sk_progenitor_shield_max_time", "10" );
ConVar	sk_progenitor_shield_max_cooldown( "sk_progenitor_shield_max_cooldown", "5" );
ConVar	sk_progenitor_shield_throw( "sk_progenitor_shield_throw", "1" );
ConVar	sk_progenitor_shield_throw_speed( "sk_progenitor_shield_throw_speed", "650" );
ConVar	sk_progenitor_shield_throw_dmg( "sk_progenitor_shield_throw_dmg", "50" );
ConVar	sk_progenitor_shield_throw_radius( "sk_progenitor_shield_throw_radius", "96" );
ConVar	sk_progenitor_shield_slam_dmg( "sk_progenitor_shield_slam_dmg", "40" );
ConVar	sk_progenitor_shield_slam_radius( "sk_progenitor_shield_slam_radius", "128" );
ConVar	sk_progenitor_grapple_speed( "sk_progenitor_grapple_speed", "600" );
ConVar	sk_progenitor_grapple_decel_dist( "sk_progenitor_grapple_decel_dist", "150" );
ConVar	sk_progenitor_grapple_accel_dist( "sk_progenitor_grapple_accel_dist", "250" );
ConVar	sk_progenitor_grapple_hook_speed( "sk_progenitor_grapple_hook_speed", "1000" );
ConVar	sk_progenitor_grapple_hook_dmg( "sk_progenitor_grapple_hook_dmg", "5" );
ConVar	sk_progenitor_grapple_min_dist( "sk_progenitor_grapple_min_dist", "200" );
ConVar	sk_progenitor_grapple_max_dist( "sk_progenitor_grapple_max_dist", "3000" );
ConVar	sk_progenitor_grapple_allow_shoot( "sk_progenitor_grapple_allow_shoot", "1" );
ConVar	sk_progenitor_jump_shoot_allow( "sk_progenitor_jump_shoot_allow", "1" );

// Sort of forced to do this, since the enum is in npc_combine.cpp
#define TACTICAL_VARIANT_PRESSURE_ENEMY	1

//---------------------------------------------------------

#define GRAPPLING_HOOK_MODEL	"models/weapons/w_progenitor_grappling_hook.mdl"
#define GRAPPLING_PROJ_MODEL	"models/weapons/w_progenitor_grappling_proj.mdl"

int COMBINE_AE_GRAPPLE_UNHOLSTER;
int COMBINE_AE_GRAPPLE_SHOOT;
int COMBINE_AE_GRAPPLE_PULL;
int COMBINE_AE_SIDEARM_UNHOLSTER;
int COMBINE_AE_SIDEARM_HOLSTER;
int COMBINE_AE_SIDEARM_SHOOT;
int COMBINE_AE_SIDEARM_LASER_ON;
int COMBINE_AE_SIDEARM_LASER_OFF;

Activity ACT_IDLE_ANGRY_GRAPPLE;
Activity ACT_RANGE_ATTACK_GRAPPLE;
Activity ACT_RANGE_ATTACK_GRAPPLE_PULL;
Activity ACT_GESTURE_RANGE_ATTACK_GRAPPLE;
Activity ACT_GESTURE_RANGE_ATTACK_GRAPPLE_PULL;
Activity ACT_GESTURE_RANGE_ATTACK_GRAPPLE_ANGRY;
Activity ACT_GRAPPLE_FLY;
Activity ACT_IDLE_ANGRY_SIDEARM;
Activity ACT_RANGE_ATTACK_SIDEARM;
Activity ACT_UNHOLSTER_SIDEARM;
Activity ACT_HOLSTER_SIDEARM;

extern Activity ACT_GESTURE_DODGE_HOP;
extern Activity ACT_GESTURE_DODGE_STEP;
extern Activity ACT_GESTURE_DODGE_ROLL;

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_Progenitor, DT_NPC_Progenitor )
	SendPropEHandle( SENDINFO( m_hGunLaser ) ),
	SendPropEHandle( SENDINFO( m_hLeftHandGun ) ),
	SendPropVector( SENDINFO( m_vecGunLaserDir ) ),
END_SEND_TABLE()

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Progenitor )

	DEFINE_INPUT( m_bThrowSatchels, FIELD_BOOLEAN, "SetThrowSatchels" ),
	DEFINE_UTLVECTOR( m_hSatchels, FIELD_EHANDLE ),

	DEFINE_INPUT( m_bInjured, FIELD_BOOLEAN, "SetInjured" ),

	DEFINE_FIELD( m_flNextShieldStateCheck, FIELD_TIME ),
	DEFINE_FIELD( m_flShieldDeactivateTime, FIELD_TIME ),
	DEFINE_FIELD( m_hShieldLight, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hShieldSprite, FIELD_EHANDLE ),
	DEFINE_SOUNDPATCH( m_pShieldSound ),
	DEFINE_ARRAY( m_hShieldSpriteTrails, FIELD_EHANDLE, PROGENITOR_SHIELD_NUM_CORNERS ),

	DEFINE_FIELD( m_hGrappleDest, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecGrappleDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGrappleAngle, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecGrappleDropDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecGrappleLastOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flGrappleStartDistSqr, FIELD_FLOAT ),
	DEFINE_FIELD( m_hGrapplingHook, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hGrapplingHookProjectile, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hGrapplingHookCable, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iGrapplePhase, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGrappleType, FIELD_INTEGER ),
	DEFINE_FIELD( m_nGrappleLayer, FIELD_INTEGER ),
	DEFINE_INPUT( m_bGrappleAllowed, FIELD_BOOLEAN, "SetGrappleAllowed" ),
	DEFINE_INPUT( m_flGrappleWeight, FIELD_FLOAT, "SetGrappleWeight" ),
	DEFINE_FIELD( m_bNavEvaluatedJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bNavTrueJump, FIELD_BOOLEAN ),

	DEFINE_INPUT( m_bUseSidearm, FIELD_BOOLEAN, "SetUseSidearm" ),
	DEFINE_FIELD( m_flNextSidearmUseTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextSidearmFireTime, FIELD_TIME ),
	DEFINE_FIELD( m_hLeftHandGun, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLeftHandWeapon, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hForcedSidearmTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flSidearmWaitTime, FIELD_FLOAT ),

	DEFINE_THINKFUNC( LaserThink ),

	DEFINE_INPUTFUNC( FIELD_STRING, "GrappleToTargetForced", InputGrappleToTargetForced ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "GrappleToTarget", InputGrappleToTarget ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "GrapplePullTarget", InputGrapplePullTarget ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SidearmFireAtTarget", InputSidearmFireAtTarget ),

	DEFINE_CONSCRIPT_DATADESC()
	DEFINE_PROPSHIELD_DATADESC()
	DEFINE_WEAPONLASER_DATADESC()

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_progenitor, CNPC_Progenitor );

CNPC_Progenitor::CNPC_Progenitor()
{
	SetThrowXenGrenades( false );
	m_SquadName = MAKE_STRING( "prog_squad" );
	m_iSpawnsWithShield = TRS_NONE;
	m_TripminePlaceBehavior.SetTripmineCapable( true );
	SetUseAvoidantFlanking( true );

	m_bThrowSatchels = true;
	m_flNextShieldStateCheck = 0.0f;

	m_vecGrappleDest = vec3_invalid;
	m_vecGrappleDropDest = vec3_invalid;
	m_bGrappleAllowed = true;
	m_nGrappleLayer = -1;

	// By default, try not to grapple unless really necessary
	m_flGrappleWeight = 0.0f;

	m_bUseSidearm = true;
	m_flNextSidearmUseTime = 0.0f;
	m_flNextSidearmFireTime = 0.0f;
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

	PrecacheModel( PROGENITOR_SHIELD_THROWN_MODEL );

	PrecacheModel( GRAPPLING_HOOK_MODEL );
	PrecacheModel( GRAPPLING_PROJ_MODEL );

	PrecacheScriptSound( "Weapon_GrapplingHook.Shoot" );
	PrecacheScriptSound( "Weapon_GrapplingHook.Wire" );
	PrecacheScriptSound( "Weapon_GrapplingHook.Hook" );
	PrecacheScriptSound( "NPC_Combine.Zipline_Mid" );
	PrecacheScriptSound( "NPC_Combine.Zipline_End" );

	PrecacheParticleSystem( "progenitor_grapple_muzzleflash" );
	PrecacheParticleSystem( "progenitor_grapple_hook" );

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
void CNPC_Progenitor::PopulatePoseParameters( void )
{
	BaseClass::PopulatePoseParameters();

	m_poseAim2_Pitch	= LookupPoseParameter( "aim2_pitch" );
	m_poseAim2_Yaw		= LookupPoseParameter( "aim2_yaw" );
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
	
	for ( int i = 0; i < PROGENITOR_SHIELD_NUM_CORNERS; i++ )
	{
		if ( m_hShieldSpriteTrails[i] )
		{
			UTIL_Remove( m_hShieldSpriteTrails[i] );
			m_hShieldSpriteTrails[i] = NULL;
		}
	}

	RemoveGrapplingEntities();

	if ( m_hLeftHandGun )
	{
		UTIL_Remove( m_hLeftHandGun );
		m_hLeftHandGun = NULL;
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

	if ( IsGrappling() )
	{
		// Melee attacks break us out of grappling
		if ( info.GetDamageType() & DMG_CLUB )
			StopGrappling();
	}

	return ret;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Progenitor::GatherConditions()
{
	BaseClass::GatherConditions();

	if ( m_iGrapplePhase != GRAPPLE_PHASE_NONE )
	{
		// Can only attack while grappling under specific circumstances
		if ( ( !sk_progenitor_grapple_allow_shoot.GetBool() || m_iGrapplePhase >= GRAPPLE_PHASE_GRAPPLING )
			&& ( !ShouldUseJumpGesture() || m_iGrapplePhase != GRAPPLE_PHASE_LANDING ) )
		{
			ClearCondition( COND_CAN_RANGE_ATTACK1 );
		}
		else if ( IsValidLayer( m_nGrappleLayer ) && GetLayerActivity( m_nGrappleLayer ) == ACT_GESTURE_RANGE_ATTACK_GRAPPLE
			&& m_iGrapplePhase < GRAPPLE_PHASE_GRAPPLING && sk_progenitor_grapple_allow_shoot.GetBool() )
		{
			// Change gesture layer if we can attack
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				CAnimationLayer *pLayer = GetAnimOverlay( m_nGrappleLayer );
				pLayer->m_nActivity = ACT_GESTURE_RANGE_ATTACK_GRAPPLE_ANGRY;
				pLayer->m_nSequence = SelectWeightedSequence( ACT_GESTURE_RANGE_ATTACK_GRAPPLE_ANGRY );
			}
		}

		ClearCondition( COND_CAN_RANGE_ATTACK2 );
		ClearCondition( COND_CAN_MELEE_ATTACK1 );
		ClearCondition( COND_CAN_MELEE_ATTACK2 );

		if ( !IsPullObjGrapple() )
		{
			// Ignore these conditions if we're about to rope away
			ClearCondition( COND_HEAR_DANGER );
			ClearCondition( COND_LIGHT_DAMAGE );
			ClearCondition( COND_HEAVY_DAMAGE );
			ClearCondition( COND_NEW_ENEMY );
		}
	}
	/*else*/ if ( CanUseShieldDuringAI() )
	{
		if ( IsPropShieldEquipped() ?
			( HasCondition( COND_HEAR_DANGER ) )
			: ( HasCondition( COND_NEW_ENEMY ) || HasCondition( COND_HEAVY_DAMAGE ) ) )
		{
			m_flNextShieldStateCheck = gpGlobals->curtime;
		}

		if ( m_flNextShieldStateCheck <= gpGlobals->curtime )
		{
			switch ( GetState() )
			{
				case NPC_STATE_IDLE:
				case NPC_STATE_ALERT:
				default:
					m_flNextShieldStateCheck = gpGlobals->curtime + 1.0f;
					break;
				case NPC_STATE_COMBAT:
					m_flNextShieldStateCheck = gpGlobals->curtime + 0.5f;
					break;
			}

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
				int nLayer = -1;
				if ( HasCondition( COND_HEAR_DANGER ) )
				{
					// If it's a grenade that's nearby, then slam our shield down
					CSound *pSound = GetBestSound( SOUND_DANGER );
					if ( pSound && pSound->m_hOwner && (GetAbsOrigin() - pSound->GetSoundReactOrigin()).LengthSqr() < Square( sk_progenitor_shield_slam_radius.GetFloat() ) )
					{
						CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade *>(pSound->m_hOwner.Get());
						if ( pGrenade && pGrenade->GetThrower() && IRelationType( pGrenade->GetThrower() ) != D_LI )
						{
							nLayer = AddActionGesture( (Activity)ACT_SLAM_SHIELD );
							m_flNextShieldStateCheck = gpGlobals->curtime + 5.0f;
						}
					}
				}

				if ( nLayer == -1 )
				{
					bool bCanThrow = false;
					if ( CanThrowShield() )
					{
						if ( HasCondition( COND_SEE_ENEMY ) || ( GetEnemy() && FVisible( GetEnemyLKP() ) ) )
						{
							if ( IsUsingTacticalVariant( TACTICAL_VARIANT_PRESSURE_ENEMY ) )
							{
								// Always throw when angry
								bCanThrow = true;
							}
							else
							{
								// See if we can do a trace forward
								Vector vecShieldOrigin = EyePosition() + (BodyDirection3D() * 24.0f) + (GetSmoothedVelocity() * 0.2f); //IsCrouching() ? EyePosition() : m_hShield->GetAbsOrigin();
								trace_t tr;
								Vector bounds( 24, 24, 24 );
								Vector vecTarget = vecShieldOrigin + ( GetShootEnemyDir( vecShieldOrigin, false ) * 48.0f );
								UTIL_TraceHull( vecShieldOrigin, vecTarget, -bounds, bounds, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
								if ( !tr.DidHit() || tr.m_pEnt == GetEnemy() )
									bCanThrow = true;

								//if ( !bCanThrow )
								//	NDebugOverlay::Box( tr.endpos, bounds, -bounds, 255, 0, 0, 64, 4.0f );
							}
						}
					}

					if ( ShouldDeactivateShield( bCanThrow ) )
					{
						if ( HasCondition( COND_SEE_ENEMY ) && GetEnemy() && EnemyDistance( GetEnemy() ) < ( bCanThrow ? 64.0f : 128.0f ) )
						{
							// Slam down
							AddActionGesture( (Activity)ACT_SLAM_SHIELD );
						}
						else
						{
							AddActionGesture( bCanThrow ? (Activity)ACT_THROW_SHIELD : (Activity)ACT_DEACTIVATE_SHIELD );
						}
						m_flNextShieldStateCheck = gpGlobals->curtime + 5.0f;
					}
				}
			}
		}
	}

	if ( !IsNavGrapple() && m_vecGrappleDest != vec3_invalid )
	{
		Vector vecToDest = (m_vecGrappleDest - EyePosition());
		float flDistSqr = vecToDest.LengthSqr();
		if ( flDistSqr < Square( sk_progenitor_grapple_max_dist.GetFloat() ) && flDistSqr > Square( sk_progenitor_grapple_min_dist.GetFloat() ) )
		{
			if ( FVisible( m_vecGrappleDest, MASK_NPCSOLID ) )
			{
				SetCondition( COND_COMBINE_CAN_GRAPPLE );
			}
		}
	}

	CBaseEntity *pGestureDodgeTarget = m_hDodgeTarget;
	CBaseEntity *pEnemy = GetEnemy();

	// TODO: Consider simplifying this...
	if ( HasCondition( COND_ENEMY_FACING_ME )
		&& pEnemy && ( pEnemy->IsPlayer() || ( pEnemy->IsNPC() && pEnemy->MyNPCPointer()->HasCondition( COND_CAN_RANGE_ATTACK1 ) ) )
		&& !HasCondition( COND_COMBINE_INCOMING_PROJECTILE ) && IsAllowedToDodge() && gpGlobals->curtime - m_flNextDodgeTime > 5.0f && m_iGrapplePhase == GRAPPLE_PHASE_NONE )
	{
		// Are we not in a good position to just keep shooting?
		if ( ( ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) || HasCondition( COND_LOW_PRIMARY_AMMO ) ) && !IsCurSchedule( SCHED_COMBINE_SIDEARM_AIM, false ) )
			|| HasCondition( COND_HEAVY_DAMAGE ) )
		{
			// See if our enemy is aiming directly at us
			Vector vecEnemyToMyHead = (HeadTarget( pEnemy->EyePosition() ) - pEnemy->EyePosition());
			float flDist = VectorNormalize( vecEnemyToMyHead );
			bool bEnemyIsPlayer = pEnemy->IsPlayer();

			if ( flDist < 2000.0f && flDist > ( bEnemyIsPlayer ? 100.0f : 200.0f ) && DotProduct( vecEnemyToMyHead, pEnemy->MyCombatCharacterPointer()->EyeDirection3D() ) > DOT_3DEGREE )
			{
				// If we have full attention as well, then try to dodge their attack
				if ( DotProduct( -vecEnemyToMyHead, HeadDirection3D() ) > ( bEnemyIsPlayer ? DOT_30DEGREE : DOT_15DEGREE ) )
				{
					SetCondition( COND_COMBINE_INCOMING_PROJECTILE );
					SetTarget( pEnemy );
					pGestureDodgeTarget = pEnemy;
				}
			}
		}
	}

	if ( HasCondition( COND_COMBINE_INCOMING_PROJECTILE ) && pGestureDodgeTarget && GetMotor()->GetCurSpeed() > 100.0f && m_iGrapplePhase == GRAPPLE_PHASE_NONE )
	{
		if ( IsSliding() )
		{
			// Not in a position to dodge
			m_hDodgeTarget = NULL;
			ClearCondition( COND_COMBINE_INCOMING_PROJECTILE );
		}
		else
		{
			// Consider doing a gesture dodge instead, if our center will be out of the way
			Vector vecMyPos = WorldSpaceCenter() + (GetSmoothedVelocity() * 0.5f);
			Vector vecTheirPos;

			if ( pGestureDodgeTarget == m_hDodgeTarget )
			{
				vecTheirPos = pGestureDodgeTarget->WorldSpaceCenter() + (GetSmoothedVelocity() * 0.5f);
			}
			else
			{
				// Just their direct vector
				pGestureDodgeTarget->GetVectors( &vecTheirPos, NULL, NULL );
				vecTheirPos += pGestureDodgeTarget->WorldSpaceCenter();
			}

			if ( CalcDistanceSqrToLine( vecMyPos, pGestureDodgeTarget->EyePosition(), vecTheirPos ) > Square( pGestureDodgeTarget->BoundingRadius() * 1.5f ) )
			{
				Activity actDodgeGesture = ACT_INVALID;

				float flPathDist = GetNavigator()->GetPathDistanceToGoal();
				if ( flPathDist > 150.0f )
				{
					actDodgeGesture = ACT_GESTURE_DODGE_ROLL;

					// Check dead zones before committing
					CUtlVector<interval_t> vecDeadZones;
					GetDodgeActivity( 0, &vecDeadZones );

					float flMoveYaw = GetPoseParameter( LookupPoseMoveYaw() );
					FOR_EACH_VEC( vecDeadZones, i )
					{
						float flDist = abs( flMoveYaw - vecDeadZones[i].start );
						if ( flDist < vecDeadZones[i].range )
						{
							actDodgeGesture = ACT_GESTURE_DODGE_HOP;
							break;
						}
					}
				}
				else if ( flPathDist > 72.0f )
				{
					actDodgeGesture = ACT_GESTURE_DODGE_HOP;
				}

				if ( actDodgeGesture != ACT_INVALID )
				{
					// Replace the condition with an action gesture
					m_hDodgeTarget = NULL;
					m_flNextDodgeTime = gpGlobals->curtime + 5.0f;
					ClearCondition( COND_COMBINE_INCOMING_PROJECTILE );
					AddActionGesture( actDodgeGesture );
				}
			}
		}
	}
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

	if ( m_hGrappleDest )
	{
		if ( m_iGrapplePhase < GRAPPLE_PHASE_SHOT || IsToObjGrapple() )
			GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );

		if ( ( ( IsPullObjGrapple() && IsGrappleHooked() ) || ( IsToObjGrapple() && IsGrappling() ) ) && m_hGrappleDest->VPhysicsGetObject() )
		{
			// If this is a physical entity, slowly pull it towards us
			Vector vecImpulse;
			AngularImpulse vecAngImpulse;
			CalculateGrappleImpulse( m_hGrappleDest->VPhysicsGetObject(), 48.0f, vecImpulse, vecAngImpulse );
			vecImpulse.z += 4.0f;

			if ( m_hGrappleDest->IsPlayer() )
			{
				vecImpulse *= 2.5f;
			}

			ApplyGrappleImpulse( m_hGrappleDest, vecImpulse, vecAngImpulse );
		}
	}

	// See how close the projectile is to the destination
	if ( m_iGrapplePhase == GRAPPLE_PHASE_SHOT && m_hGrapplingHookProjectile )
	{
		GrappleHookMove();
	}
	else if ( IsNavGrapple() )
	{
		if ( !GetNavigator()->GetPath()->GetCurWaypoint() )
		{
			// We lost our path while grappling. Typically happens on schedule interrupt.
			// It's kind of botched to keep grappling in this state, but the main reason this is a problem
			// is because grappling is done through MoveJump, which doesn't run without a path.
			// Since all of the grappling vars are kept independently from the path, there technically isn't
			// anything stopping us from running it here instead if we're already mid-grapple.
			// So instead of canceling entirely, just keep moving until we're supposed to stop.
			if ( GrappleMove() )
			{
				StopGrappling();
			}

			//DevWarning( "%s lost path while nav grappling\n", GetDebugName() );
			//StopGrappling();
		}
		else if ( IsGrapplePreShot() && ( GetIdealActivity() != ACT_RANGE_ATTACK_GRAPPLE && m_nGrappleLayer == -1 ) )
		{
			// We're not in the grapple activity anymore
			StopGrappling();
		}
	}

	if ( m_iGrapplePhase == GRAPPLE_PHASE_LANDING && GetMoveType() == MOVETYPE_STEP && GetIdealActivity() != ACT_GLIDE && !GetAcrobaticMotor()->IsUsingGlideLayer() )
	{
		// Stuck landing when we're already on the ground
		// This can happen sometimes when grapple isn't interrupted properly
		// Should have warnings there instead of here...
		Warning( "%s had to break out of grapple land\n", GetDebugName() );
		m_iGrapplePhase = GRAPPLE_PHASE_NONE;
	}

	if ( m_bInjured )
	{
		if ( m_pShieldSound )
		{
			// Shield goes crazy
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			controller.SoundChangePitch( m_pShieldSound, RandomFloat( 80.0f, 115.0f ), 0.5f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	if ( m_iGrapplePhase != GRAPPLE_PHASE_NONE && IsInterruptable() )
	{
		StopGrappling();
	}

	if ( m_hLeftHandGun )
	{
		RemoveLaserFromGun( m_hLeftHandGun );

		if ( GetActiveWeapon() )
			AddLaserToGun( GetActiveWeapon() );

		UTIL_Remove( m_hLeftHandGun );
		m_hLeftHandGun = NULL;

		// Don't try again for a bit if we were interrupted
		if ( m_flNextSidearmUseTime < gpGlobals->curtime )
			m_flNextSidearmUseTime = gpGlobals->curtime + 5.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Progenitor::SelectSchedule( void )
{
	if ( m_vecGrappleDest != vec3_invalid )
	{
		if ( !IsNavGrapple() )
		{
			if ( !HasCondition( COND_COMBINE_CAN_GRAPPLE ) )
			{
				return SCHED_COMBINE_MOVE_TO_GRAPPLE_LOS;
			}

			return SCHED_COMBINE_GRAPPLE_SHOOT;
		}
	}

	if ( m_hForcedSidearmTarget )
	{
		if ( !FVisible( m_hForcedSidearmTarget ) )
		{
			return SCHED_COMBINE_MOVE_TO_FORCED_SIDEARM_LOS;
		}

		return SCHED_COMBINE_SIDEARM_FORCED_SHOOT;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Progenitor::SelectStealthCounterSchedule( int nInSchedule )
{
	// Aim laser at last seen position
	if ( CanUseSidearm() && m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] < 0.5f )
		return SCHED_COMBINE_SIDEARM_AIM;

	return BaseClass::SelectStealthCounterSchedule( nInSchedule );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Progenitor::TranslateSchedule( int scheduleType )
{
	int translatedType = BaseClass::TranslateSchedule( scheduleType );

	switch (translatedType)
	{
		case SCHED_COMBINE_RANGE_ATTACK1:
		case SCHED_COMBINE_MERCILESS_RANGE_ATTACK1:
		case SCHED_COMBINE_MERCILESS_SUPPRESS:
		case SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP:
			{
				if ( HasCondition( COND_SEE_ENEMY ) && GetEnemy() )
				{
					if ( CanUseGrapple() && !IsGrappling() )
					{
						CBaseEntity *pEnemy = GetEnemy();
						if ( pEnemy->IsNPC() && pEnemy->GetGroundEntity() ) // Flying enemies don't catch well
						{
							if ( pEnemy->MyNPCPointer()->GetHullType() == HULL_TINY || pEnemy->MyNPCPointer()->GetHullType() == HULL_TINY_CENTERED )
							{
								// Pull towards us?
								if ( GrappleTraceTest( pEnemy, Square( 1500.0f ) ) && (GetAbsOrigin() - pEnemy->GetAbsOrigin()).z < 64.0f )
								{
									m_hGrappleDest = pEnemy;
									GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );

									m_iGrappleType = GRAPPLE_TYPE_PULL;
									return SCHED_COMBINE_GRAPPLE_SHOOT;
								}
							}
						}

						if ( IsUsingTacticalVariant( TACTICAL_VARIANT_PRESSURE_ENEMY ) && ( GetHealth() > GetMaxHealth() * 0.3 ) && RandomFloat() < m_flGrappleWeight )
						{
							// Make sure grappling to this enemy doesn't put me in a bad position
							AIEnemiesIter_t iter;
							bool bFailed = false;
							for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
							{
								if ( pEMemory->hEnemy != pEnemy && pEMemory->hEnemy->IsAlive() && pEMemory->hEnemy->Classify() != CLASS_BULLSEYE )
								{
									if ( ( pEMemory->vLastKnownLocation - pEnemy->GetAbsOrigin() ).LengthSqr() < Square( 200.0f ) && pEnemy->FVisible( pEMemory->hEnemy ) )
									{
										bFailed = true;
										break;
									}
								}
							}

							// When pressuring, pull self towards enemies
							if ( !bFailed && GrappleTraceTest( pEnemy, Square( 2500.0f ) ) )
							{
								m_hGrappleDest = pEnemy;
								GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );

								m_iGrappleType = GRAPPLE_TYPE_PULL_TO;
								return SCHED_COMBINE_GRAPPLE_SHOOT;
							}
						}
					}

					if ( CanUseSidearm() && m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] < 0.25f && gpGlobals->curtime - m_flLastDamageTime > 2.0f )
					{
						// If the enemy is far away, jumping, and/or really small, consider using sidearm
						CBaseEntity *pEnemy = GetEnemy();
						float flDistSqr = ( GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();
						float flMinDistSqr = Square( 500.0f );

						if ( pEnemy->IsNPC() && pEnemy->MyNPCPointer()->GetHullHeight() < 32.0f )
							flMinDistSqr = Square( 300.0f );
						else if ( m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] > 0.0f )
							flMinDistSqr -= Square( m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] * 300.0f );

						if ( flDistSqr > flMinDistSqr )
						{
							return SCHED_COMBINE_SIDEARM_AIM;
						}
					}
				}
			}
			// Fall through
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
					// Just try to keep distance
					return SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE;

					// Prefer to take cover when we have a shield
					/*
					if ( HasMemory( bits_MEMORY_INCOVER ) && !HasCondition( COND_SEE_ENEMY ) )
						return SCHED_COMBAT_FACE;

					return SCHED_TAKE_COVER_FROM_ENEMY;
					*/
				}
				else if ( ( GetHealth() <= GetMaxHealth() * 0.5 ) || GetEnemies()->NumEnemies() > 3 )
				{
					// Be a bit more cagey when we're in a bad spot
					if ( HasCondition( COND_CAN_RANGE_ATTACK2 ) )
						return SCHED_COMBINE_GRENADE_COVER1;

					//if ( !HasMemory( bits_MEMORY_INCOVER ) )
					//	return SCHED_TAKE_COVER_FROM_ENEMY;
				}
				else if ( m_bCounterTacticsAllowed && ( scheduleType == SCHED_RANGE_ATTACK1 || scheduleType == SCHED_COMBINE_RANGE_ATTACK1 ) )
				{
					// We're flanking as a counter-tactic (see CNPC_CloneCop::TranslateSchedule)
					// See if we should consider pulling out a sidearm instead
					if ( CanUseSidearm() && gpGlobals->curtime - m_flLastDamageTime > 4.0f )
					{
						// If the enemy is far away and/or jumping, consider using sidearm
						CBaseEntity *pEnemy = GetEnemy();
						float flDistSqr = ( GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();

						if ( flDistSqr > Square( 1000.0f - ( m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] * 800.0f ) ) )
						{
							return SCHED_COMBINE_SIDEARM_AIM;
						}
					}
				}
			}
			break;
		case SCHED_GET_HEALTHKIT:
			{
				if ( CanUseGrapple() && GetTarget() && !IsGrappling() )
				{
					Vector vecDelta = (GetAbsOrigin() - GetTarget()->GetAbsOrigin());
					float flDistToTargetSqr = vecDelta.LengthSqr();
					if ( flDistToTargetSqr > Square( 128.0f ) && vecDelta.z < 64.0f && FVisible( GetTarget(), MASK_SHOT_HULL ) )
					{
						m_hGrappleDest = GetTarget();
						GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );

						m_iGrappleType = GRAPPLE_TYPE_PULL;
						return SCHED_COMBINE_GRAPPLE_SHOOT;
					}
				}
			}
			break;

		case SCHED_RELOAD:
		case SCHED_HIDE_AND_RELOAD:
		case SCHED_COMBINE_HIDE_AND_RELOAD:
			{
				// See if we could use our sidearm
				if ( CanUseSidearm() && gpGlobals->curtime - m_flLastDamageTime > 4.0f )
				{
					float flDistSqr = ( GetAbsOrigin() - GetEnemy()->GetAbsOrigin() ).LengthSqr();
					if ( flDistSqr > Square( 200.0f ) )
					{
						return SCHED_COMBINE_SIDEARM_AIM;
					}
				}
			}
			break;

		case SCHED_COMBINE_WAIT_IN_COVER:
			{
				if ( m_bLaserOn && !HasCondition( COND_SEE_ENEMY ) )
				{
					// Ensure we retain LOS
					return SCHED_COMBAT_FACE;
				}
			}
			break;
	}

	return translatedType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == COMBINE_AE_GRAPPLE_UNHOLSTER )
		{
			// Make the grappling hook
			if ( !m_hGrapplingHook )
			{
				m_hGrapplingHook = (CBaseAnimating*)CreateNoSpawn( "prop_dynamic_ornament", GetAbsOrigin(), GetAbsAngles(), this );
				m_hGrapplingHook->SetModelName( MAKE_STRING( GRAPPLING_HOOK_MODEL ) );
				DispatchSpawn( m_hGrapplingHook );
				m_hGrapplingHook->FollowEntity( this );
				m_hGrapplingHook->RemoveEffects( EF_NODRAW );
			}

			if ( !m_hGrapplingHookProjectile )
			{
				m_hGrapplingHookProjectile = (CBaseAnimating*)CreateNoSpawn( "prop_dynamic", GetAbsOrigin(), GetAbsAngles(), this );
				m_hGrapplingHookProjectile->SetModelName( MAKE_STRING( GRAPPLING_PROJ_MODEL ) );
				m_hGrapplingHookProjectile->KeyValue( "solid", "0" );
				DispatchSpawn( m_hGrapplingHookProjectile );
				m_hGrapplingHookProjectile->SetParent( m_hGrapplingHook, m_hGrapplingHook->LookupAttachment( "rope_end" ) );
				m_hGrapplingHookProjectile->SetMoveType( MOVETYPE_NONE );
				m_hGrapplingHookProjectile->AddSolidFlags( FSOLID_NOT_SOLID );
				m_hGrapplingHookProjectile->SetLocalOrigin( vec3_origin );
				m_hGrapplingHookProjectile->SetLocalAngles( vec3_angle );
			}

			return;
		}
		else if ( pEvent->event == COMBINE_AE_GRAPPLE_SHOOT )
		{
			if ( m_hGrapplingHookProjectile )
			{
				if ( m_hGrappleDest )
				{
					// Update the position before we shoot
					GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );
				}

				// Launch the projectile in the direction of the destination
				m_hGrapplingHookProjectile->SetParent( NULL );
				m_hGrapplingHookProjectile->SetMoveType( MOVETYPE_NOCLIP ); // MOVETYPE_FLY

				Vector vecOrigin = m_hGrapplingHookProjectile->GetAbsOrigin();
				int nMuzzleAttach = m_hGrapplingHook->LookupAttachment( "muzzle" );
				if ( nMuzzleAttach > 0 )
				{
					m_hGrapplingHook->GetAttachment( nMuzzleAttach, vecOrigin );
					m_hGrapplingHookProjectile->SetAbsOrigin( vecOrigin );
				}

				Vector vecVelocity = (m_vecGrappleDest - vecOrigin);
				VectorNormalize( vecVelocity );
				QAngle angles;
				VectorAngles( vecVelocity, angles );

				vecVelocity *= sk_progenitor_grapple_hook_speed.GetFloat();
				m_hGrapplingHookProjectile->SetAbsVelocity( vecVelocity );

				m_hGrapplingHookProjectile->SetAbsAngles( angles );

				// Make the rope
				if ( !m_hGrapplingHookCable )
				{
					m_hGrapplingHookCable = CRopeKeyframe::Create( m_hGrapplingHook, m_hGrapplingHookProjectile,
						m_hGrapplingHook->LookupAttachment( "rope_start" ), m_hGrapplingHookProjectile->LookupAttachment( "rope_locator" ),
						1, "cable/cable.vmt", 4, "keyframe_rope" );
				}

				DispatchParticleEffect( "progenitor_grapple_muzzleflash", PATTACH_POINT_FOLLOW, m_hGrapplingHook, m_hGrapplingHook->LookupAttachment( "muzzle" ) );
				DispatchParticleEffect( "progenitor_grapple_hook", PATTACH_POINT_FOLLOW, m_hGrapplingHookProjectile, m_hGrapplingHookProjectile->LookupAttachment( "front" ) );

				EmitSound( "Weapon_GrapplingHook.Shoot" );
				m_hGrapplingHookProjectile->EmitSound( "Weapon_GrapplingHook.Wire" );

				m_iGrapplePhase = GRAPPLE_PHASE_SHOT;
			}
			return;
		}
		else if ( pEvent->event == COMBINE_AE_GRAPPLE_PULL )
		{
			if ( m_hGrappleDest && m_hGrappleDest->VPhysicsGetObject() )
			{
				// Yank it towards us
				Vector vecImpulse;
				AngularImpulse vecAngImpulse;
				CalculateGrappleImpulse( m_hGrappleDest->VPhysicsGetObject(), 300.0f, vecImpulse, vecAngImpulse );
				vecImpulse.z += 150.0f;
				ApplyGrappleImpulse( m_hGrappleDest, vecImpulse, vecAngImpulse );

				EmitSound( "NPC_Combine.Zipline_Mid" );

				if ( m_hGrapplingHookCable )
				{
					// Technically shouldn't be any slack, but add just enough to shake the rope
					m_hGrapplingHookCable->m_Slack = 3;
				}
			}
			return;
		}
		else if ( pEvent->event == COMBINE_AE_SIDEARM_UNHOLSTER )
		{
			// Find sidearm
			m_hLeftHandWeapon = NULL;
			for (int i=0;i<MAX_WEAPONS;i++)
			{
				if ( m_hMyWeapons[i].Get() && m_hMyWeapons[i]->WeaponClassify() == WEPCLASS_HANDGUN && GetActiveWeapon() != m_hMyWeapons[i] )
				{
					m_hLeftHandWeapon = dynamic_cast<CBaseHLCombatWeapon *>( m_hMyWeapons[i].Get() );
					break;
				}
			}

			if ( !m_hLeftHandWeapon )
			{
				// Create sidearm if we don't have one
				m_hLeftHandWeapon = dynamic_cast<CBaseHLCombatWeapon *>( GiveWeaponHolstered( MAKE_STRING( GetBackupWeaponClass() ) ) );
			}

			if ( m_hLeftHandWeapon && !m_hLeftHandGun )
			{
				Assert( !m_hLeftHandWeapon->GetLeftHandGun() );

				m_hLeftHandGun = m_hLeftHandWeapon->CreateLeftHandGun();
				m_hLeftHandWeapon->SetLeftHandGun( NULL );	// Don't let the weapon remember it, since its handle is used for dual wielding

				m_hLeftHandWeapon->m_iClip1 = m_hLeftHandWeapon->GetMaxClip1();
			}

			if ( m_hLeftHandGun )
			{
				if ( GetActiveWeapon() )
					RemoveLaserFromGun( GetActiveWeapon() );
				AddLaserToGun( m_hLeftHandGun );
			}
			return;
		}
		else if ( pEvent->event == COMBINE_AE_SIDEARM_HOLSTER )
		{
			m_hLeftHandWeapon = NULL;

			if ( m_hLeftHandGun )
			{
				RemoveLaserFromGun( m_hLeftHandGun );

				if ( GetActiveWeapon() )
					AddLaserToGun( GetActiveWeapon() );

				UTIL_Remove( m_hLeftHandGun );
				m_hLeftHandGun = NULL;
			}
			return;
		}
		else if ( pEvent->event == COMBINE_AE_SIDEARM_SHOOT )
		{
			if (m_hLeftHandGun)
			{
				int sequence = m_hLeftHandGun->SelectWeightedSequence( ACT_RANGE_ATTACK_PISTOL );
				if (sequence != ACTIVITY_NOT_AVAILABLE)
				{
					m_hLeftHandGun->SetSequence( sequence );
					m_hLeftHandGun->SetCycle( 0 );
					m_hLeftHandGun->ResetSequenceInfo();
				}

				if ( m_hLeftHandWeapon )
				{
					Vector vecShootOrigin, vecShootDir;
					m_hLeftHandGun->GetAttachment( m_hLeftHandGun->LookupAttachment( "muzzle" ), vecShootOrigin );

					CBaseCombatWeapon *pTrueActiveWeapon = GetActiveWeapon();
					SetActiveWeapon( m_hLeftHandWeapon );

					if ( m_hForcedSidearmTarget )
					{
						vecShootDir = GetLaserTargetPos( m_hForcedSidearmTarget, vecShootOrigin ) - vecShootOrigin;
					}
					else
					{
						vecShootDir = GetLaserTargetPos( GetEnemy(), vecShootOrigin ) - vecShootOrigin;
					}

					VectorNormalize( vecShootDir );

					m_hLeftHandWeapon->SetFiringLeft( true );
					m_hLeftHandWeapon->FireNPCPrimaryAttack( this, vecShootOrigin, vecShootDir );
					m_hLeftHandWeapon->SetFiringLeft( false );

					SetActiveWeapon( pTrueActiveWeapon );

					// A bit of a hack to take advantage of existing assassin dual wield code
					//extern int AE_PISTOL_FIRE_LEFT;

					//animevent_t proxyEvent;
					//memcpy( &proxyEvent, pEvent, sizeof( animevent_t ) );
					//proxyEvent.event = AE_PISTOL_FIRE_LEFT;
					//m_hLeftHandWeapon->Operator_HandleAnimEvent( &proxyEvent, this );
				}
			}
			return;
		}
		else if ( pEvent->event == COMBINE_AE_SIDEARM_LASER_ON )
		{
			TurnOnLaser();
			return;
		}
		else if ( pEvent->event == COMBINE_AE_SIDEARM_LASER_OFF )
		{
			TurnOffLaser();
			return;
		}
	}

	BaseClass::HandleAnimEvent( pEvent );
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
			m_flNextDodgeTime = gpGlobals->curtime + (pTask->flTaskData * 0.5f);
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
					Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
					if ( GetNavigator()->IsGoalSet()
						&& ( GetNavigator()->GetGoalPos() - GetAbsOrigin() ).LengthSqr() > Square( 128.0f )
						&& abs( vecToEnemy.z ) < 64.0f && vecToEnemy.LengthSqr() > Square( 200.0f )
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

		case TASK_COMBINE_GET_PATH_TO_GRAPPLE_LOS:
			{
				float flMaxRange = sk_progenitor_grapple_max_dist.GetFloat();
				float flMinRange = sk_progenitor_grapple_min_dist.GetFloat();

				Vector posLos;
				bool found = false;

				Vector vecDest = m_vecGrappleDest;

				NDebugOverlay::Cross3D( vecDest, 4.0f, 255, 0, 0, true, 1.0f );

				if ( GetTacticalServices()->FindLateralLos( vecDest, &posLos ) )
				{
					float dist = ( posLos - m_vecGrappleDest ).Length();
					if ( dist < flMaxRange && dist > flMinRange )
						found = true;
				}

				if ( !found && GetTacticalServices()->FindLos( vecDest, vecDest, flMinRange, flMaxRange, 1.0, &posLos ) )
				{
					found = true;
				}

				if ( !found )
				{
					TaskFail( FAIL_NO_SHOOT );
				}
				else
				{
					// else drop into run task to offer an interrupt
					m_vInterruptSavePosition = posLos;
				}
			}
			break;

		case TASK_COMBINE_SET_GRAPPLE_SCHEDULE:
			{
				switch ( m_iGrappleType )
				{
					case GRAPPLE_TYPE_PULL:
						SetSchedule( SCHED_COMBINE_GRAPPLE_PULL );
						break;
					default:
						SetSchedule( SCHED_COMBINE_GRAPPLE );
						break;
				}
			}
			break;

		case TASK_COMBINE_GRAPPLE_SHOOT:
			{
				SetIdealActivity( ACT_RANGE_ATTACK_GRAPPLE );
				m_iGrapplePhase = GRAPPLE_PHASE_PRESHOT;
				ClearWait();
			}
			break;

		case TASK_COMBINE_GRAPPLE_MOVE:
			{
				// See how long this grapple should take
				Vector vecToDest = (m_vecGrappleDest - GetAbsOrigin());

				float flTimeToReach = 0.0f;
				if ( sk_progenitor_grapple_speed.GetFloat() > 0.0f )
					flTimeToReach = ( vecToDest.LengthSqr() / Square( sk_progenitor_grapple_speed.GetFloat() ) );

				// Wait for that time plus 4 seconds
				SetWait( flTimeToReach + 4.0f );

				StartGrappling( m_iGrappleType ); // GRAPPLE_TYPE_FORCED

				SetIdealActivity( ACT_GRAPPLE_FLY );
				//SetActivity( ACT_GRAPPLE_FLY );
			}
			break;

		case TASK_COMBINE_GRAPPLE_PULL_OBJ:
			{
				SetIdealActivity( ACT_IDLE_ANGRY_GRAPPLE );
				AddActionGesture( ACT_GESTURE_RANGE_ATTACK_GRAPPLE_PULL );

				// See how long this grapple should take
				Vector vecToDest = (m_vecGrappleDest - GetAbsOrigin());

				float flTimeToReach = 0.0f;
				if ( sk_progenitor_grapple_speed.GetFloat() > 0.0f )
					flTimeToReach = ( vecToDest.LengthSqr() / Square( sk_progenitor_grapple_speed.GetFloat() ) );

				// Wait for that time plus 2 seconds
				SetWait( flTimeToReach + 2.0f );
			}
			break;

		case TASK_COMBINE_SIDEARM_AIM:
			{
				SetWait( 10.0f, 15.0f );
				SetIdealActivity( ACT_IDLE_ANGRY_SIDEARM );

				float flDistanceSqr = 0.0f;
				if ( GetEnemy() )
					flDistanceSqr = ((GetEnemyLKP() + GetEnemy()->GetSmoothedVelocity()) - GetAbsOrigin()).LengthSqr();

				m_flNextSidearmFireTime = gpGlobals->curtime + MIN((flDistanceSqr / Square(625.0f)) * 1.25f, 1.25f);

				// HACKHACK: Sometimes the laser think doesn't start
				StartLaserThink();
			}
			break;

		case TASK_COMBINE_SIDEARM_AIM_FORCED:
			{
				SetWait( m_flSidearmWaitTime );
				SetIdealActivity( ACT_IDLE_ANGRY_SIDEARM );

				// HACKHACK: Sometimes the laser think doesn't start
				StartLaserThink();
			}
			break;

		case TASK_COMBINE_GET_PATH_TO_SIDEARM_TARGET:
			{
				SetTarget( m_hForcedSidearmTarget );

				float flMaxRange = 2000.0f;
				float flMinRange = 0.0f;

				Vector vecEnemy = m_hForcedSidearmTarget->GetAbsOrigin();
				Vector vecEnemyEye = vecEnemy + m_hForcedSidearmTarget->GetViewOffset();

				Vector posLos;
				bool found = false;

				if ( GetTacticalServices()->FindLateralLos( vecEnemyEye, &posLos ) )
				{
					float dist = ( posLos - vecEnemyEye ).Length();
					if ( dist < flMaxRange && dist > flMinRange )
						found = true;
				}

				if ( !found && GetTacticalServices()->FindLos( vecEnemy, vecEnemyEye, flMinRange, flMaxRange, 1.0, &posLos ) )
				{
					found = true;
				}

				if ( !found )
				{
					TaskFail( FAIL_NO_SHOOT );
				}
				else
				{
					// else drop into run task to offer an interrupt
					m_vInterruptSavePosition = posLos;
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
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Progenitor::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_COMBINE_GRAPPLE_SHOOT:
			{
				if ( IsGrappleHooked() )
				{
					TaskComplete();
					break;
				}

				if ( IsWaitSet() && IsWaitFinished() )
				{
					TaskFail( FAIL_BAD_PATH_GOAL );
					break;
				}

				if ( IsActivityFinished() && GetActivity() == ACT_RANGE_ATTACK_GRAPPLE )
				{
					SetActivity( ACT_IDLE_ANGRY_GRAPPLE );
					SetWait( 5.0f );
				}

				GetMotor()->SetIdealYawToTargetAndUpdate( m_vecGrappleDest );
			}
			break;

		case TASK_COMBINE_GRAPPLE_MOVE:
			{
				if ( IsWaitFinished() )
				{
					StopGrappling();
					TaskFail( FAIL_BAD_POSITION );
					break;
				}

				if ( !IsGrappling() )
				{
					// It was canceled somehow
					TaskComplete();
					break;
				}

				Vector vecYawDir = GetAbsVelocity();
				float flSpeed = VectorNormalize( vecYawDir );

				if ( GetEnemy() )
				{
					// Face enemy if they are in front of us
					Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
					float flDist = VectorNormalize( vecToEnemy );

					if ( DotProduct( vecYawDir, vecToEnemy ) > 0.0f )
						vecYawDir = vecToEnemy;

					// If we are grappling towards our enemy, kick them when we get close enough
					if ( GetEnemy() == m_hGrappleDest && flDist < ( flSpeed * 0.5f ) && !IsPlayingActionGesture() )
					{
						AddActionGesture( ACT_GESTURE_MELEE_ATTACK2 );
					}
				}

				GetMotor()->SetIdealYawAndUpdate( UTIL_VecToYaw( vecYawDir ) );
				AimGun();

				if ( GrappleMove() )
				{
					StopGrappling();
					TaskComplete();
				}
			}
			break;

		case TASK_COMBINE_GET_PATH_TO_GRAPPLE_LOS:
			{
				if ( m_vecGrappleDest == vec3_invalid )
				{
					TaskFail(FAIL_NO_ENEMY);
					return;
				}

				if ( GetTaskInterrupt() > 0 )
				{
					ClearTaskInterrupt();

					AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );

					GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET );
					GetNavigator()->SetArrivalDirection( m_vecGrappleDest - goal.dest );
				}
				else
				{
					TaskInterrupt();
				}
			}
			break;

		case TASK_COMBINE_GRAPPLE_PULL_OBJ:
			{
				if ( !m_hGrappleDest || !m_hGrapplingHookProjectile )
				{
					TaskFail( FAIL_NO_TARGET );
					break;
				}

				if ( IsWaitFinished() )
				{
					// Make sure we can still see the target
					trace_t tr;
					UTIL_TraceLine( EyePosition(), m_hGrappleDest->WorldSpaceCenter(), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
					if ( !tr.DidHit() || tr.m_pEnt == m_hGrappleDest )
					{
						// Do another pull
						AddActionGesture( ACT_GESTURE_RANGE_ATTACK_GRAPPLE_PULL );
						SetWait( 4.0f, 6.0f );
					}
					else
					{
						// Assume we can't get it
						TaskFail( FAIL_BAD_POSITION );
						break;
					}
				}

				GetMotor()->SetIdealYawToTargetAndUpdate( m_hGrappleDest->GetAbsOrigin() );
				AimGun();

				Vector vecToTarget = (m_hGrappleDest->GetAbsOrigin() - WorldSpaceCenter());
				float flDistSqr = ( vecToTarget.LengthSqr() - Square( m_hGrappleDest->BoundingRadius() ) );
				if ( flDistSqr < Square( 48.0f ) )
				{
					if ( m_hGrappleDest->IsCombatItem() )
					{
						PickupItem( m_hGrappleDest );
					}
					else if ( m_hGrappleDest->IsBaseCombatWeapon() )
					{
						Weapon_Equip( m_hGrappleDest->MyCombatWeaponPointer() );
					}

					RemoveGrapplingEntities();
					m_hGrappleDest = NULL;
					m_vecGrappleDest = vec3_invalid;

					m_iGrappleType = GRAPPLE_TYPE_NONE;
					m_iGrapplePhase = GRAPPLE_PHASE_NONE;

					TaskComplete();
				}

				if ( GetEnemy() )
				{
					// If we are pulling our enemy, kick them when they get close enough
					if ( GetEnemy() == m_hGrappleDest && flDistSqr < ( m_hGrappleDest->GetAbsVelocity().LengthSqr() * 0.25f ) && !IsPlayingActionGesture() )
					{
						AddActionGesture( ACT_GESTURE_MELEE_ATTACK2 );
					}
				}
			}
			break;

		case TASK_COMBINE_SIDEARM_AIM:
			{
				if ( !GetEnemy() || HasCondition( COND_ENEMY_DEAD ) )
				{
					// Find a new enemy, if possible
					CBaseEntity *pNewEnemy = BestEnemy();
					if ( pNewEnemy && pNewEnemy != GetEnemy() && pNewEnemy->IsAlive() && FVisible( pNewEnemy ) )
					{
						SetEnemy( pNewEnemy );
						ClearCondition( COND_ENEMY_DEAD );
					}
					else
					{
						//TaskFail( FAIL_NO_TARGET );
						TaskComplete();
						m_flNextSidearmUseTime = gpGlobals->curtime + 15.0f;
						break;
					}
				}

				if ( !m_hLeftHandGun )
				{
					Warning( "%s unexpectedly lost sidearm\n", GetDebugName() );
					TaskFail( FAIL_ITEM_NO_FIND );
					break;
				}

				if ( IsWaitFinished() )
				{
					TaskComplete();
					m_flNextSidearmUseTime = gpGlobals->curtime + 15.0f;
					break;
				}

				GetMotor()->SetIdealYawToTargetAndUpdate( GetEnemy()->GetAbsOrigin() );
				AimGun();

				if ( GetActivity() == ACT_RANGE_ATTACK_SIDEARM )
				{
					if ( IsActivityFinished() )
					{
						SetActivity( ACT_IDLE_ANGRY_SIDEARM );
						m_flNextSidearmFireTime = gpGlobals->curtime + 0.75f;
					}
				}
				else
				{
					if ( !RunSidearmAim() )
					{
						TaskComplete();
						m_flNextSidearmUseTime = gpGlobals->curtime + 15.0f;
					}
				}
			}
			break;

		case TASK_COMBINE_GET_PATH_TO_SIDEARM_TARGET:
			{
				if ( !m_hForcedSidearmTarget )
				{
					TaskFail(FAIL_NO_ENEMY);
					return;
				}

				if ( GetTaskInterrupt() > 0 )
				{
					ClearTaskInterrupt();

					Vector vecEnemy = m_hForcedSidearmTarget->GetAbsOrigin();
					AI_NavGoal_t goal( m_vInterruptSavePosition, ACT_RUN, AIN_HULL_TOLERANCE );

					GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET );
					GetNavigator()->SetArrivalDirection( vecEnemy - goal.dest );
				}
				else
				{
					TaskInterrupt();
				}
			}
			break;

		case TASK_COMBINE_SIDEARM_AIM_FORCED:
			{
				if ( !m_hForcedSidearmTarget )
				{
					//TaskFail( FAIL_NO_TARGET );
					TaskComplete();
					break;
				}

				AimGun();

				// If the target is behind us, turn until it's at least to our side
				Vector2D vecToTarget2D = (m_hForcedSidearmTarget->GetAbsOrigin().AsVector2D() - GetAbsOrigin().AsVector2D());
				Vector2DNormalize( vecToTarget2D );
				if ( DotProduct2D( BodyDirection3D().AsVector2D(), vecToTarget2D ) < 0.0f )
				{
					GetMotor()->SetIdealYawToTargetAndUpdate( m_hForcedSidearmTarget->GetAbsOrigin() );
					break;
				}

				// Wait until we're ready
				if ( !IsWaitFinished() )
					break;

				if ( GetActivity() == ACT_RANGE_ATTACK_SIDEARM )
				{
					if ( IsActivityFinished() )
					{
						TaskComplete();
						m_hForcedSidearmTarget = NULL;
					}
				}
				else
				{
					SetActivity( ACT_RANGE_ATTACK_SIDEARM );
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
bool CNPC_Progenitor::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
		case HINT_GRAPPLE_POINT:
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
	else if ( moveType == bits_CAP_MOVE_JUMP )
	{
		if ( !IsTrueJumpLegal( vecStart, vecEnd, vecEnd ) && CanUseGrapple() )
		{
			// Prefer not to grapple if our enemy is close to the start or end
			if ( GetEnemy() )
			{
				const float GRAPPLE_ENEMY_DANGER_DIST = 400.0f;

				float flEnemyDistSqr = (GetEnemyLKP() - vecStart).LengthSqr();
				if ( flEnemyDistSqr < Square( GRAPPLE_ENEMY_DANGER_DIST ) )
				{
					float flDistFrac = (sqrtf( flEnemyDistSqr ) / GRAPPLE_ENEMY_DANGER_DIST);
					if ( flDistFrac > 0.01f )
						*pCost *= (1.0f / flDistFrac);
					else
						*pCost *= 100.0f;
				}

				flEnemyDistSqr = (GetEnemyLKP() - vecEnd).LengthSqr();
				if ( flEnemyDistSqr < Square( GRAPPLE_ENEMY_DANGER_DIST ) )
				{
					float flDistFrac = (sqrtf( flEnemyDistSqr ) / GRAPPLE_ENEMY_DANGER_DIST);
					if ( flDistFrac > 0.01f )
						*pCost *= (1.0f / flDistFrac);
					else
						*pCost *= 100.0f;
				}
			}

			if ( m_flGrappleWeight == 0.0f )
			{
				*pCost *= 100.0f;
			}
			else
			{
				*pCost *= (1.0f / m_flGrappleWeight);
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
	if ( IsGrappling() )
	{
		// Some schedules may try to break the activity
		return ACT_GRAPPLE_FLY;
	}

	if ( m_nGrappleLayer != -1 && !IsSliding() )
	{
		switch ( eNewActivity )
		{
			case ACT_IDLE:		eNewActivity = ACT_IDLE_ANGRY; break;
			case ACT_WALK:		eNewActivity = ACT_WALK_AIM; break;
			case ACT_RUN:		eNewActivity = ACT_RUN_AIM; break;
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
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
		// UNDONE: No need if we can see our enemy (unless they're moving a lot)
		//if ( HasCondition( COND_SEE_ENEMY ) && m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] < 0.5f )
		//	return false;
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
bool CNPC_Progenitor::ShouldSlideToGoal( AILocalMoveGoal_t *pMoveGoal )
{
	if ( sk_progenitor_slide_always.GetBool() )
		return true;

	if ( !BaseClass::ShouldSlideToGoal( pMoveGoal ) )
		return false;

	// Too busy
	if ( IsPropShieldEquipped() || m_bInjured )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldUseJumpGesture( void )
{
	if ( !GetEnemy() )
		return false;

	if ( !sk_progenitor_jump_shoot_allow.GetBool() )
		return false;

	if ( !m_MoveAndShootOverlay.CanAimAtEnemy() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldStopJumpGesture( void )
{
	if ( !BaseClass::ShouldStopJumpGesture() )
		return false;

	// Wait until we've landed
	if ( m_iGrapplePhase == GRAPPLE_PHASE_LANDING )
		return false;

	return true;
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
bool CNPC_Progenitor::GunSupportsLaser( CBaseAnimating *pWeapon, int &iAttachment, bool bForce )
{
	// TEMPTEMP
	if ( pWeapon->ClassMatches( "weapon_css_scout" ) )
		bForce = true;

	return BaseClass::GunSupportsLaser( pWeapon, iAttachment, bForce );
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ )
{
	if ( !BaseClass::Weapon_Switch( pWeapon, viewmodelindex ) )
		return false;

	if ( pWeapon && pWeapon->ClassMatches( "weapon_css_scout" ) && m_hGunLaser )
	{
		// Sniper laser starts on
		TurnOnLaser();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::StartLaserThink()
{
	SetContextThink( &CNPC_Progenitor::LaserThink, gpGlobals->curtime, WEAPON_LASER_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::LaserThink( void )
{
	if ( !IsAlive() || !GetActiveWeapon() )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, WEAPON_LASER_THINK_CONTEXT );
		return;
	}

	float flNextThink = DoLaserThink();

	SetContextThink( &CNPC_Progenitor::LaserThink, gpGlobals->curtime + flNextThink, WEAPON_LASER_THINK_CONTEXT );
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
	DECLARE_TASK( TASK_COMBINE_GET_PATH_TO_GRAPPLE_LOS )
	DECLARE_TASK( TASK_COMBINE_SET_GRAPPLE_SCHEDULE )
	DECLARE_TASK( TASK_COMBINE_GRAPPLE_SHOOT )
	DECLARE_TASK( TASK_COMBINE_GRAPPLE_MOVE )
	DECLARE_TASK( TASK_COMBINE_GRAPPLE_PULL_OBJ )
	DECLARE_TASK( TASK_COMBINE_SIDEARM_AIM )
	DECLARE_TASK( TASK_COMBINE_GET_PATH_TO_SIDEARM_TARGET )
	DECLARE_TASK( TASK_COMBINE_SIDEARM_AIM_FORCED )

	DECLARE_CONDITION( COND_COMBINE_SHIELD_RETREAT )
	DECLARE_CONDITION( COND_COMBINE_CAN_GRAPPLE )
	DECLARE_CONDITION( COND_COMBINE_GRAPPLE_FAILED )

	DECLARE_ANIMEVENT( COMBINE_AE_GRAPPLE_UNHOLSTER )
	DECLARE_ANIMEVENT( COMBINE_AE_GRAPPLE_SHOOT )
	DECLARE_ANIMEVENT( COMBINE_AE_GRAPPLE_PULL )
	DECLARE_ANIMEVENT( COMBINE_AE_SIDEARM_UNHOLSTER )
	DECLARE_ANIMEVENT( COMBINE_AE_SIDEARM_HOLSTER )
	DECLARE_ANIMEVENT( COMBINE_AE_SIDEARM_SHOOT )
	DECLARE_ANIMEVENT( COMBINE_AE_SIDEARM_LASER_ON )
	DECLARE_ANIMEVENT( COMBINE_AE_SIDEARM_LASER_OFF )

	DECLARE_ACTIVITY( ACT_IDLE_ANGRY_GRAPPLE )
	DECLARE_ACTIVITY( ACT_RANGE_ATTACK_GRAPPLE )
	DECLARE_ACTIVITY( ACT_RANGE_ATTACK_GRAPPLE_PULL )
	DECLARE_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_GRAPPLE )
	DECLARE_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_GRAPPLE_PULL )
	DECLARE_ACTIVITY( ACT_GESTURE_RANGE_ATTACK_GRAPPLE_ANGRY )
	DECLARE_ACTIVITY( ACT_GRAPPLE_FLY )
	DECLARE_ACTIVITY( ACT_IDLE_ANGRY_SIDEARM )
	DECLARE_ACTIVITY( ACT_RANGE_ATTACK_SIDEARM )
	DECLARE_ACTIVITY( ACT_UNHOLSTER_SIDEARM )
	DECLARE_ACTIVITY( ACT_HOLSTER_SIDEARM )

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

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_MOVE_TO_GRAPPLE_LOS,
	
		"	Tasks "
		"		TASK_SET_TOLERANCE_DISTANCE					48"
		"		TASK_COMBINE_GET_PATH_TO_GRAPPLE_LOS		0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		"	"
		"	Interrupts "
		//"		COND_NEW_ENEMY"
		//"		COND_ENEMY_DEAD"
		//"		COND_CAN_MELEE_ATTACK1"
		//"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		//"		COND_HEAR_MOVE_AWAY"
		//"		COND_HEAVY_DAMAGE"
		//"		COND_COMBINE_CAN_GRAPPLE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_GRAPPLE_SHOOT,
	
		"	Tasks"
		"		TASK_COMBINE_GRAPPLE_SHOOT			0"
		"		TASK_COMBINE_SET_GRAPPLE_SCHEDULE	0"
		""
		"	Interrupts"
		"		COND_COMBINE_GRAPPLE_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_GRAPPLE,
	
		"	Tasks"
		"		TASK_COMBINE_GRAPPLE_MOVE				0"
		""
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_GRAPPLE_PULL,
	
		"	Tasks"
		"		TASK_COMBINE_GRAPPLE_PULL_OBJ			0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
		"		COND_COMBINE_GRAPPLE_FAILED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_SIDEARM_AIM,
	
		"	Tasks"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_UNHOLSTER_SIDEARM"
		"		TASK_COMBINE_SIDEARM_AIM			0"
		"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_HOLSTER_SIDEARM"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_MOVE_AWAY"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_MOVE_TO_FORCED_SIDEARM_LOS,
	
		"	Tasks"
		"		TASK_SET_TOLERANCE_DISTANCE					48"
		"		TASK_COMBINE_GET_PATH_TO_SIDEARM_TARGET		0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
	)

	DEFINE_SCHEDULE
	(
		SCHED_COMBINE_SIDEARM_FORCED_SHOOT,
	
		"	Tasks"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_UNHOLSTER_SIDEARM" // TASK_PLAY_SEQUENCE_FACE_TARGET
		"		TASK_COMBINE_SIDEARM_AIM_FORCED		0"
		"		TASK_PLAY_SEQUENCE					ACTIVITY:ACT_HOLSTER_SIDEARM"
		""
		"	Interrupts"
		"		COND_HEAR_DANGER"
	)

AI_END_CUSTOM_NPC()
