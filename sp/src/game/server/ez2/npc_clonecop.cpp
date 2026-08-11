//=============================================================================//
//
// Purpose:		Clone Cop, a former human bent on killing anyone who stands in his way.
//				He was trapped under Arbeit 1 for a long time (from his perspective),
//				but now he's back and he's bigger, badder, and possibly even more deranged than ever before.
//				I mean, you could just see the brains of this guy.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_clonecop.h"
#include "gameweaponmanager.h"
#include "ammodef.h"
#include "npc_manhack.h"
#include "particle_parse.h"
#include "prop_combine_ball.h"
#include "ez2_player.h"
#include "ai_interactions.h"
#include "items.h"
#include "ai_network.h"
#include "saverestore_utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_clonecop_health( "sk_clonecop_health","1000" );
ConVar	sk_clonecop_kick( "sk_clonecop_kick", "40" );

ConVar	npc_clonecop_suppress_min_occlude_dist( "npc_clonecop_suppress_min_occlude_dist", "128" );
ConVar	npc_clonecop_suppress_max_enemy_dist_from_seen( "npc_clonecop_suppress_max_enemy_dist_from_seen", "72" );

ConVar	npc_clonecop_use_new_weapon_switching( "npc_clonecop_use_new_weapon_switching", "1" );
ConVar	npc_clonecop_always_use_avoidant_flanking( "npc_clonecop_always_use_avoidant_flanking", "0" );
ConVar	npc_clonecop_throw_manhacks( "npc_clonecop_throw_manhacks", "1" );
ConVar	npc_clonecop_throw_manhack_speed( "npc_clonecop_throw_manhack_speed", "800" );
ConVar	npc_clonecop_moving_pickup( "npc_clonecop_moving_pickup", "1" );
ConVar	npc_clonecop_moving_melee( "npc_clonecop_moving_melee", "1" );
ConVar	npc_clonecop_moving_altfire( "npc_clonecop_moving_altfire", "1" );
ConVar	npc_clonecop_moving_grenades( "npc_clonecop_moving_grenades", "1" );
ConVar	npc_clonecop_moving_manhacks( "npc_clonecop_moving_manhacks", "1" );
ConVar	npc_clonecop_moving_tripmines( "npc_clonecop_moving_tripmines", "1" );

extern ConVar sk_plr_dmg_buckshot;
extern ConVar sk_plr_num_shotgun_pellets;

// Think contexts
static const char *CC_BLEED_THINK = "CCBleed";

int CNPC_CloneCop::gm_nBloodAttachment = -1;
float CNPC_CloneCop::gm_flBodyRadius = 10.0f;

#define COMBINE_AE_GREN_DROP		( 9 )
#define COMBINE_AE_KICK				( 3 )

Activity	ACT_GESTURE_PICKUP_GROUND;
Activity	ACT_GESTURE_PICKUP_RACK;
Activity	ACT_METROPOLICE_THROW_MANHACK;
Activity	ACT_GESTURE_THROW_MANHACK;

int	AE_PICKUP_NEAREST_ITEM;
int	AE_METROPOLICE_THROW_DEPLOY;
int	AE_METROPOLICE_THROW_MANHACK;
extern int AE_SLAM_TRIPMINE_PLACE;

extern int ACT_METROPOLICE_DEPLOY_MANHACK;
extern Activity ACT_GESTURE_DEPLOY_MANHACK;

//---------------------------------------------------------

CNPC_CloneCop::SwitchableWeaponData_t CNPC_CloneCop::g_SwitchableWeaponData[] = {
	// Classname			Min Pref. Range		Max Pref. Range		Deploy Sound
	{ "weapon*shotgun",		0.0f,				650.0f,				SPECIAL1 },
	{ "weapon_smg1",		100.0f,				1000.0f,			SPECIAL2 },
	{ "weapon_ar2*",		100.0f,				2000.0f,			RELOAD_NPC },
	{ "weapon_crossbow",	1000.0f,			4000.0f,			RELOAD_NPC },

	{ "weapon_oicw",		100.0f,				1500.0f,			SPECIAL2 },
	{ "weapon_css_m249",	100.0f,				1500.0f,			RELOAD_NPC },
	{ "weapon_css_scout",	1000.0f,			4000.0f,			RELOAD_NPC },
};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_CloneCop )

	DEFINE_INPUT( m_ArmorValue, FIELD_INTEGER, "SetArmor" ),
	DEFINE_FIELD( m_bIsBleeding, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_flNextWeaponSwitchTime, FIELD_TIME ),

	DEFINE_INPUT( m_bUseAvoidantFlanking, FIELD_BOOLEAN, "SetUseAvoidantFlanking" ),
	DEFINE_INPUT( m_bUseGestureAltFire, FIELD_BOOLEAN, "SetUseGestureAltFire" ),

	DEFINE_KEYFIELD( m_bCounterTacticsAllowed, FIELD_BOOLEAN, "CounterTacticsAllowed" ),
	DEFINE_ARRAY( m_flCounterTacticWeights, FIELD_FLOAT, CNPC_CloneCop::COUNTER_TACTIC_COUNT ),

	DEFINE_FIELD( m_hClosestItem, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_hIgnoreItems, FIELD_EHANDLE ),

	DEFINE_FIELD( m_nActionGesture, FIELD_INTEGER ),
	DEFINE_FIELD( m_flActionGestureEndTime, FIELD_TIME ),

	// Function Pointers
	DEFINE_THINKFUNC( BleedThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_clonecop, CNPC_CloneCop );

CNPC_CloneCop::CNPC_CloneCop()
{
	// KV will override this if the NPC was spawned by Hammer
	AddGrenades( 99 );
	SetAlternateCapable( true );
	AddSpawnFlags( SF_COMBINE_REGENERATE );
	RemoveSpawnFlags( SF_COMBINE_COMMANDABLE );
	m_tEzVariant = EZ_VARIANT_RAD;
	m_ArmorValue = 200;
	m_bThrowXenGrenades = true;
	m_bLookForItems = true;
	SetDisplacementImpossible( true );
	m_SquadName = MAKE_STRING( "cc_squad" );

	// TODO - See comment in npc_combine.cpp
	// Clone Cop probably shouldn't order surrender by default
	m_iCanOrderSurrender = TRS_FALSE;

	SetDefLessFunc( m_SwitchableWeaponCache );

	m_nActionGesture = -1;
	m_flActionGestureEndTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	AimGun();

	// Stronger, tougher.
	SetHealth( sk_clonecop_health.GetFloat() );
	SetMaxHealth( sk_clonecop_health.GetFloat() );
	SetKickDamage( sk_clonecop_kick.GetFloat() );

	AddEFlags( EFL_NO_DISSOLVE );

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	BaseClass::Spawn();

	if (m_tEzVariant == EZ_VARIANT_RAD || m_tEzVariant == EZ_VARIANT_TEMPORAL)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Precache()
{
	// For now, CC is always an elite
	m_fIsElite = true;

	if( !GetModelName() )
	{
		if (IsBadCop())
		{
			SetModelName( MAKE_STRING( "models/bad_cop.mdl" ) );
		}
		else
		{
			SetModelName( MAKE_STRING( "models/clone_cop.mdl" ) );
		}
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_ammo_ar2_altfire" );

	PrecacheParticleSystem( "blood_spurt_synth_01" );
	PrecacheParticleSystem( "blood_drip_synth_01" );

	if (m_tEzVariant == EZ_VARIANT_RAD || m_tEzVariant == EZ_VARIANT_TEMPORAL)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}

	if (m_bThrowXenGrenades)
	{
		VerifyXenRecipeManager( GetClassname() );
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Activate()
{
	gm_nBloodAttachment = LookupAttachment( "body_blood_loc" );

	if ( IsBleeding() )
	{
		StartBleeding();
	}

	BaseClass::Activate();
}


void CNPC_CloneCop::DeathSound( const CTakeDamageInfo &info )
{
	AI_CriteriaSet set;
	ModifyOrAppendDamageCriteria(set, info);
	SpeakIfAllowed( TLK_CMB_DIE, set, SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CloneCop::ClearAttackConditions()
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check the innate weapon LOS for an owner at an arbitrary position
//			If bSetConditions is true, LOS related conditions will also be set
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions )
{
	bool bBase = BaseClass::WeaponLOSCondition( ownerPos, targetPos, bSetConditions );

	if (HasCondition( COND_WEAPON_SIGHT_OCCLUDED ) && bSetConditions)
	{
		// Re-do the trace, but this time from our eye position
		trace_t tr;
		AI_TraceLine( EyePosition(), targetPos, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr);

		// If the end pos is within a certain number of units (or the start pos is solid), set the custom condition
		if (tr.startsolid || (EyePosition() - tr.endpos).LengthSqr() <= Square( npc_clonecop_suppress_min_occlude_dist.GetFloat() ))
		{
			SetCondition( COND_COMBINE_WEAPON_SIGHT_OCCLUDED );
		}
	}

	return bBase;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::GatherConditions()
{
	BaseClass::GatherConditions();

	if ( IsPlayingActionGesture() )
	{
		SetCondition( COND_COMBINE_PLAYING_ACTION_GESTURE );

		if ( GetLayerActivity( m_nActionGesture ) == ACT_GESTURE_MELEE_ATTACK1 || GetLayerActivity( m_nActionGesture ) == ACT_GESTURE_MELEE_ATTACK2 )
		{
			// Don't melee attack while playing this gesture
			ClearCondition( COND_CAN_MELEE_ATTACK1 );
		}
	}
	else
	{
		ClearCondition( COND_COMBINE_PLAYING_ACTION_GESTURE );

		if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) && npc_clonecop_moving_melee.GetBool() )
		{
			// If we're moving and meleeing can interrupt this schedule, then don't interrupt and use the gesture instead
			if ( IsMoving() && ConditionInterruptsCurSchedule( COND_CAN_MELEE_ATTACK1 ) )
			{
				// Clearing the condition is needed to prevent interrupt, so we use a different condition
				// to check later on
				ClearCondition( COND_CAN_MELEE_ATTACK1 );
				SetCondition( COND_COMBINE_CAN_MELEE_GESTURE );
			}
		}
		else
			ClearCondition( COND_COMBINE_CAN_MELEE_GESTURE );
	}

	if ( IsCurSchedule( SCHED_COMBINE_MERCILESS_SUPPRESS, false ) || IsCurSchedule( SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP, false ) )
	{
		if ( !HasCondition( COND_NO_PRIMARY_AMMO ) )
		{
			// Make sure they haven't moved and we know it
			AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy(), true );
			if ( pMemory )
			{
				Vector vecLastSeen = pMemory->vLastSeenLocation;
				Vector vecLastKnown = pMemory->vLastKnownLocation;

				if ((vecLastSeen - vecLastKnown).LengthSqr() < Square( npc_clonecop_suppress_max_enemy_dist_from_seen.GetFloat() )
					&& !CBaseCombatCharacter::FVisible( vecLastKnown ))
				{
					SetCondition( COND_CAN_RANGE_ATTACK1 );
				}
				else
				{
					// Interrupt
					SetCondition( COND_COMBINE_WEAPON_SIGHT_OCCLUDED );
				}
			}
		}
	}

	if ( !IsInAScript() )
	{
		if ( npc_clonecop_use_new_weapon_switching.GetBool() )
		{
			if ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) || HasCondition( COND_LOW_PRIMARY_AMMO ) )
			{
				if ( GetActivity() == ACT_IDLE
					|| GetActivity() == ACT_WALK
					|| GetActivity() == ACT_RUN
					|| GetActivity() == ACT_RANGE_ATTACK1 )
				{
					// If we don't need to attack right now or we're low on ammo, consider switching
					SetCondition( COND_COMBINE_DESIRE_WEAPON_SWITCH );
				}
			}
		}

		if ( npc_clonecop_moving_pickup.GetBool() )
		{
			if ( HasCondition( COND_HEALTH_ITEM_AVAILABLE ) )
			{
				// This bounces off of the implementation in CNPC_Combine::GatherConditions(), which looks for a nearby health item,
				// sets this condition, and then sets m_flNextHealthSearchTime. However, it doesn't store the item, nor does it keep
				// track of the closest one, so we have to do that here

				// See if an item's near our path and store it
				Vector vecOrigin = GetAbsOrigin() + (GetSmoothedVelocity() * 0.5f);

				CBaseEntity *pClosestItem = FindNearestHealthItem( vecOrigin, 240.0f );
				if ( pClosestItem )
				{
					m_hClosestItem = pClosestItem;
				}
			}
		}
	}

	if ( m_bCounterTacticsAllowed )
	{
		for ( int i = 0; i < COUNTER_TACTIC_COUNT; i++ )
		{
			float flTacticWeightAdd = -0.002f;

			if ( GetEnemy() )
			{
				switch ( i )
				{
					case COUNTER_TACTIC_BRUTE:
						{
							if ( HasCondition( COND_SEE_ENEMY ) )
							{
								// If they have a shotgun or deagle, steadily increase
								CBaseCombatCharacter *pBCC = GetEnemy()->MyCombatCharacterPointer();
								if ( pBCC && pBCC->GetActiveWeapon()
									&& ( pBCC->GetActiveWeapon()->ClassMatches( "weapon*shotgun" ) || pBCC->GetActiveWeapon()->ClassMatches( "weapon_css_deagle*" ) ) )
								{
									flTacticWeightAdd += 0.005f;

									// If the enemy is rapidly approaching me with a shotgun, assume they're going in for a blast
									// Try to dodge it
									if ( m_flCounterTacticWeights[i] > 0.3f && GetEnemy()->IsPlayer() && IsAllowedToDodge() )
									{
										Vector vecEnemyToMe = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin());
										Vector vecEnemyVel = GetEnemy()->GetSmoothedVelocity();

										float flDist = VectorNormalize( vecEnemyToMe );
										float flSpeed = VectorNormalize( vecEnemyVel );

										if ( flDist < 300.0f && flSpeed > 200.0f && DotProduct( vecEnemyToMe, vecEnemyVel ) > DOT_30DEGREE )
										{
											SetCondition( COND_COMBINE_INCOMING_PROJECTILE );
											SetTarget( GetEnemy() );
										}
									}
								}

								// Get a shotgun to match
								if ( m_flCounterTacticWeights[i] > 0.7f && GetActiveWeapon() && !GetActiveWeapon()->ClassMatches( "weapon*shotgun" ) )
									SetCondition( COND_COMBINE_DESIRE_WEAPON_SWITCH );
							}
						}
						break;

					case COUNTER_TACTIC_SNIPER:
						{
							// Steadily increase or decrease based on distance
							float flEnemyDistSqr = (GetEnemyLKP() - GetAbsOrigin()).LengthSqr();

							if ( flEnemyDistSqr > Square( 350.0f ) )
								flTacticWeightAdd += RemapValClamped( flEnemyDistSqr, Square( 350.0f ), Square( 4000.0f ), 0.002f, 0.03f );
							else
								flTacticWeightAdd += RemapValClamped( flEnemyDistSqr, 0.0f, Square( 350.0f ), -0.05f, 0.002f );
						}
						break;

					case COUNTER_TACTIC_MOBILE:
						{
							if ( HasCondition( COND_SEE_ENEMY ) )
							{
								// Steadily increase if enemy is moving
								Vector vecEnemyVel = GetEnemy()->GetSmoothedVelocity();

								float flRatio = Bias( Clamp( vecEnemyVel.LengthSqr() / Square( 500.0f ), 0.0f, 1.0f ), 0.25f );
								flTacticWeightAdd += (0.03f * flRatio);
							}
						}
						break;
				}

				// More influence if the enemy just hurt me (they are actively using, or not using, this tactic against me)
				if ( gpGlobals->curtime - GetLastDamageTime() < 1.0f )
					flTacticWeightAdd *= 2.0f;
			}

			m_flCounterTacticWeights[i] = Clamp( m_flCounterTacticWeights[i] + flTacticWeightAdd, 0.0f, 1.0f );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();
	
	// Health opportunities can interrupt alert schedules
	if ( GetState() == NPC_STATE_IDLE || IsCurSchedule( SCHED_ALERT_STAND ) || IsCurSchedule( SCHED_COMBINE_PATROL ) || IsCurSchedule( SCHED_INVESTIGATE_SOUND ) )
	{
		SetCustomInterruptCondition( COND_HEALTH_ITEM_AVAILABLE );
	}

	if ( IsCurSchedule( SCHED_RUN_FROM_ENEMY, true ) && IsCurSchedule( SCHED_PC_MELEE_AND_MOVE_AWAY, false ) )
	{
		// HACKHACK: CNPC_PlayerCompanion's melee code thinks the schedule is still SCHED_RUN_FROM_ENEMY, so don't let it interrupt and get stuck in a loop
		ClearCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
	}

	if ( IsCurSchedule( SCHED_GET_HEALTHKIT, false ) && npc_clonecop_moving_pickup.GetBool() )
	{
		// Need to be able to interrupt when we pick up while moving
		SetCustomInterruptCondition( COND_PROVOKED );

		if ( GetState() == NPC_STATE_COMBAT )
		{
			// Add combat conditions if we're doing this in a combat situation
			SetCustomInterruptCondition( COND_NEW_ENEMY );
			SetCustomInterruptCondition( COND_HEAR_DANGER );
			SetCustomInterruptCondition( COND_HEAR_MOVE_AWAY );
			SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK1 );
			SetCustomInterruptCondition( COND_CAN_MELEE_ATTACK2 );
		}
	}

	if ( IsCurSchedule( SCHED_RANGE_ATTACK1 ) || IsCurSchedule( SCHED_COMBINE_RANGE_ATTACK1 ) )
	{
		// Make sure we stop shooting when playing the gesture
		SetCustomInterruptCondition( COND_COMBINE_PLAYING_ACTION_GESTURE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::PrescheduleThink()
{
	BaseClass::PrescheduleThink();

	if ( !IsInAScript() && GetState() != NPC_STATE_SCRIPT )
	{
		// See if we want to play an action gesture
		if ( !HasCondition( COND_COMBINE_PLAYING_ACTION_GESTURE ) )
		{
			PrescheduleSelectActionGesture();
		}

		if ( !npc_clonecop_use_new_weapon_switching.GetBool() )
		{
			// TODO: Consider caching this
			int iNumWeapons = 0;
			int iMyWeapon = WEAPONSWITCH_COUNT;
			CHandle<CBaseCombatWeapon> pWeapons[WEAPONSWITCH_COUNT];
			for (int i=0;i<MAX_WEAPONS;i++)
			{
				if ( m_hMyWeapons[i].Get() )
				{
					iNumWeapons++;

					if (EntIsClass( m_hMyWeapons[i], gm_isz_class_Shotgun ))
					{
						pWeapons[WEAPONSWITCH_SHOTGUN] = m_hMyWeapons[i];
						if (GetActiveWeapon() == m_hMyWeapons[i].Get())
							iMyWeapon = WEAPONSWITCH_SHOTGUN;
					}
					else if (EntIsClass( m_hMyWeapons[i], gm_isz_class_AR2 ) || FClassnameIs( m_hMyWeapons[i], "weapon_ar2_proto" ))
					{
						pWeapons[WEAPONSWITCH_AR2] = m_hMyWeapons[i];
						if (GetActiveWeapon() == m_hMyWeapons[i].Get())
							iMyWeapon = WEAPONSWITCH_AR2;
					}
					else if (FClassnameIs( m_hMyWeapons[i], "weapon_crossbow" ))
					{
						pWeapons[WEAPONSWITCH_CROSSBOW] = m_hMyWeapons[i];
						if (GetActiveWeapon() == m_hMyWeapons[i].Get())
							iMyWeapon = WEAPONSWITCH_CROSSBOW;
					}
				}
			}

			// Behavior for when CC has >1 weapons
			if (iNumWeapons > 1 && iMyWeapon < WEAPONSWITCH_COUNT)
			{
				int iSwitchTo = iMyWeapon;

				// Check if enemy is too far
				if (HasCondition( COND_TOO_FAR_TO_ATTACK ))
					iSwitchTo = clamp( iSwitchTo + 1, WEAPONSWITCH_SHOTGUN, WEAPONSWITCH_CROSSBOW );
				else if (HasCondition( COND_TOO_CLOSE_TO_ATTACK ))
					iSwitchTo = clamp( iSwitchTo - 1, WEAPONSWITCH_SHOTGUN, WEAPONSWITCH_CROSSBOW );

				// Check if we have no ammo
				if ( iSwitchTo == iMyWeapon && HasCondition( COND_NO_PRIMARY_AMMO ))
					iSwitchTo = RandomInt(0, WEAPONSWITCH_COUNT-1);

				if (iSwitchTo != iMyWeapon && pWeapons[iSwitchTo].Get() != NULL)
				{
					inputdata_t inputdata;
					inputdata.value.SetString( pWeapons[iSwitchTo]->m_iClassname );
					InputChangeWeapon( inputdata );
				}
			}
		}

		if ( GetState() != NPC_STATE_COMBAT )
		{
			// Clear our ignore items if we still have any
			if ( m_hIgnoreItems.Count() > 0 )
				m_hIgnoreItems.RemoveAll();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CloneCop::PrescheduleSelectActionGesture()
{
	// Don't play an action gesture while reloading
	if ( FindGestureLayer( TranslateActivity( ACT_GESTURE_RELOAD ) ) != -1 )
		return -1;

	//
	// Item pickup
	//
	if ( m_hClosestItem && GetState() != NPC_STATE_IDLE
		&& ( GetActivity() == ACT_IDLE || GetActivity() == ACT_WALK || GetActivity() == ACT_RUN ) )
	{
		// Check if we're close enough an item it to pick it up
		Vector vecOrigin = WorldSpaceCenter() + ( GetSmoothedVelocity() * 0.5f );
		Vector vecToItem = (vecOrigin - m_hClosestItem->GetAbsOrigin());
		vecToItem.z *= 0.5f; // Reduce vertical distance, but don't completely ignore it

		float flDistSqr = vecToItem.LengthSqr();
		if ( flDistSqr < Square( 56.0f ) )
		{
			if (fabs( vecToItem.z ) >= (12.0f * 0.5f)) // Account for reduced verticality
			{
				return AddActionGesture( ACT_GESTURE_PICKUP_RACK, m_hClosestItem->GetAbsOrigin(), 0.5f );
			}
			else
			{
				return AddActionGesture( ACT_GESTURE_PICKUP_GROUND, m_hClosestItem->GetAbsOrigin(), 0.5f );
			}
		}
		else if ( flDistSqr < Square( 150.0f ) )
		{
			// We're approaching closely enough that we shouldn't pick another gesture
			return -1;
		}
		else if ( flDistSqr > Square( 300.0f ) && m_hClosestItem != GetTarget() )
		{
			// We're probably not getting this item for now if it's gotten this far away
			m_hClosestItem = NULL;
		}
	}

	if ( GetEnemy() )
	{
		//
		// Melee attack
		//
		if ( HasCondition( COND_COMBINE_CAN_MELEE_GESTURE ) )
		{
			// If we're heading towards or away from our enemy, then kick
			// If not, then bash
			Vector vecVelocity = GetSmoothedVelocity();
			VectorNormalize( vecVelocity );
			Vector vecToEnemy = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
			VectorNormalize( vecToEnemy );

			float flDot = DotProduct( vecVelocity, vecToEnemy );
			if ( flDot > DOT_45DEGREE || flDot < -DOT_45DEGREE )
			{
				return AddActionGesture( ACT_GESTURE_MELEE_ATTACK2 );
			}
			else
			{
				return AddActionGesture( ACT_GESTURE_MELEE_ATTACK1 );
			}
		}

		if ( IsMoving() )
		{
			//
			// Grenades and alt-fire
			//
			if ( HasCondition( COND_SEE_ENEMY ) && ( npc_clonecop_moving_altfire.GetBool() || npc_clonecop_moving_grenades.GetBool() ) )
			{
				// When we're fighting on hard difficulty and things are hectic, consider these actions while moving
				// TODO: Keyvalue for this? It'd be interesting if you could base this on boss phase
				if ( ( GetHealth() < ( GetMaxHealth() * 0.5f ) || GetEnemies()->NumEnemies() > 4 ) && g_pGameRules->IsSkillLevel( SKILL_HARD ) )
				{
					// Only if we aren't going directly towards our enemy, but are facing them enough
					Vector vecEnemyLKP = GetEnemyLKP();
					Vector vecVelocity = GetSmoothedVelocity();
					Vector vecToEnemy = ( vecEnemyLKP - GetAbsOrigin() );
					float flEnemyLKPDist = VectorNormalize( vecToEnemy );

					Vector vecForward;
					GetVectors( &vecForward, NULL, NULL );

					if ( DotProduct( vecVelocity.Normalized(), vecToEnemy ) < DOT_45DEGREE && DotProduct2D( vecForward.AsVector2D(), vecToEnemy.AsVector2D() ) > DOT_45DEGREE )
					{
						if ( npc_clonecop_moving_altfire.GetBool() && m_bUseGestureAltFire && CanAltFireEnemy( false ) )
						{
							// Now we need to check whether we can still fire by the time the animation reaches that point
							// This is a copy of CNPC_Combine::CanAltFireEnemy() and only checks if the trace still goes at least 50% of the way there
							trace_t tr;

							Vector mins( -12, -12, -12 );
							Vector maxs( 12, 12, 12 );

							Vector vShootPosition = EyePosition();

							if ( GetActiveWeapon() )
							{
								GetActiveWeapon()->GetAttachment( "muzzle", vShootPosition );
							}

							// Lead according to our velocity times the animation duration
							float flDuration = SequenceDuration( SelectWeightedSequence( ACT_GESTURE_COMBINE_AR2_ALTFIRE ) );
							vShootPosition += ( vecVelocity * flDuration );

							UTIL_TraceHull( vShootPosition, m_vecAltFireTarget, mins, maxs, MASK_COMBINE_BALL_LOS, this, COLLISION_GROUP_NONE, &tr );
							if ( tr.fraction >= 0.5f && OccupyStrategySlot( SQUAD_SLOT_SPECIAL_ATTACK ) )
							{
								// Target is valid
								return AddActionGesture( ACT_GESTURE_COMBINE_AR2_ALTFIRE, m_vecAltFireTarget, 1.0f );
							}
						}

						// We couldn't alt-fire, but could we throw a grenade?
						if ( npc_clonecop_moving_grenades.GetBool() )
						{
							// HACKHACK: Needed because the base grenade code checks for this.
							// A virtual function can be added to it if this becomes more widespread
							float flGroundSpeed = m_flGroundSpeed;
							m_flGroundSpeed = 0.0f;

							if ( CanGrenadeEnemy() )
							{
								// Now we need to check whether we can still throw by the time the animation reaches that point
								// For now, just check if we would still have LOS
								float flDuration = SequenceDuration( SelectWeightedSequence( ACT_GESTURE_COMBINE_THROW_GRENADE ) );
								Vector vecEyePos = EyePosition() + ( vecVelocity * flDuration );

								trace_t tr;
								CTraceFilterLOS traceFilter( this, COLLISION_GROUP_NONE, GetEnemy() );
								UTIL_TraceLine( vecEyePos, GetEnemy()->EyePosition(), MASK_BLOCKLOS_AND_NPCS, &traceFilter, &tr );
								if ( tr.fraction == 1.0 || tr.m_pEnt == GetEnemy() )
								{
									// Target is valid
									m_flGroundSpeed = flGroundSpeed;
									return AddActionGesture( ACT_GESTURE_COMBINE_THROW_GRENADE, GetAbsOrigin() + (m_vecTossVelocity * flEnemyLKPDist), 1.0f );
								}
							}

							m_flGroundSpeed = flGroundSpeed;
						}
					}
				}
			}

			//
			// Manhacks
			//
			if ( CanDeployManhack() && !HasCondition( COND_HEAR_DANGER ) && npc_clonecop_moving_manhacks.GetBool() && OccupyStrategySlot( SQUAD_SLOT_SPECIAL_ATTACK ) )
			{
				Activity nActivity = TranslateActivity( ACT_GESTURE_DEPLOY_MANHACK );

				if ( nActivity == ACT_GESTURE_THROW_MANHACK )
				{
					// Check if we'll still have LOS by the time we throw it
					Vector vecVelocity = GetSmoothedVelocity();
					float flDuration = SequenceDuration( SelectWeightedSequence( nActivity ) );
					Vector vecEyePos = EyePosition() + ( vecVelocity * flDuration );

					Vector mins( -12, -12, -12 );
					Vector maxs( 12, 12, 12 );
					trace_t tr;
					UTIL_TraceHull( vecEyePos, GetEnemy()->EyePosition(), mins, maxs, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
					if ( tr.fraction != 1.0 && ( !tr.m_pEnt || tr.m_pEnt != GetEnemy() ) )
					{
						// Do a regular deploy
						nActivity = ACT_GESTURE_DEPLOY_MANHACK;
					}
				}

				return AddActionGesture( nActivity );
			}

			//
			// Tripmines
			//
			if ( m_TripminePlaceBehavior.IsTripmineCapable() && !HasCondition( COND_HEAR_DANGER ) && !HasCondition( COND_CAN_RANGE_ATTACK1 )
				&& IsInterruptable() && npc_clonecop_moving_tripmines.GetBool())
			{
				// Don't do this if our goal is close by
				if ( (GetNavigator()->GetGoalPos() - GetAbsOrigin()).LengthSqr() > Square( 150.0f ) )
				{
					// Determine our origin and velocity
					Vector2D vec2DToEnemy = (GetEnemyLKP() - GetAbsOrigin()).AsVector2D();
					Vector2DNormalize( vec2DToEnemy );
					Vector vecVelocity = GetSmoothedVelocity();
					float flSpeed = VectorNormalize( vecVelocity );

					// Where we'll be in one second
					Vector vecPlaceOrigin = GetAbsOrigin() + ( vecVelocity * flSpeed );

					if ( m_TripminePlaceBehavior.GetTripmineContext() == TRIPMINE_CONTEXT_MOVING )
					{
						// See if we've moved far enough away from our last origin
						Vector vecToRef = m_TripminePlaceBehavior.GetTripmineContextData().vecOrigin - vecPlaceOrigin;
						if ( vecToRef.LengthSqr() > Square( 64.0f ) )
							m_TripminePlaceBehavior.ClearTripmineCandidates();
					}

					if ( m_TripminePlaceBehavior.GetTripmineContext() != TRIPMINE_CONTEXT_MOVING || m_TripminePlaceBehavior.GetTripmineCandidates().Count() == 0 )
					{
						// Place tripmines while moving away from enemy and unable to fire (taking cover, etc.)
						// Ignore direction if our enemy is moving a lot
						if ( DotProduct2D( vec2DToEnemy, vecVelocity.AsVector2D() ) < DOT_45DEGREE || m_flCounterTacticWeights[COUNTER_TACTIC_MOBILE] > 0.6f )
						{
							m_TripminePlaceBehavior.TryFindTripmineSurfaces( vecPlaceOrigin, 128.0f, TRIPMINE_CONTEXT_MOVING );
						}
					}
				
					if ( m_TripminePlaceBehavior.GetTripmineCandidates().Count() > 0 )
					{
						// See if we're close enough to use any of the candidates
						const CUtlVector<TripmineCandidate_t> &vecTripmineCandidates = m_TripminePlaceBehavior.GetTripmineCandidates();
						FOR_EACH_VEC( vecTripmineCandidates, i )
						{
							Vector2D vec2DToCandidate = (vecTripmineCandidates[i].vecOrigin.AsVector2D() - vecPlaceOrigin.AsVector2D());
							if ( vec2DToCandidate.LengthSqr() < Square( 64.0f ) )
							{
								// Make sure we're not moving away from it too sharply either
								Vector2DNormalize( vec2DToCandidate );
								if ( DotProduct2D( vec2DToCandidate, vecVelocity.AsVector2D() ) > -0.5f && OccupyStrategySlot( GetEngineerSlot() ) )
								{
									// Make sure it isn't floating in the air now
									const Vector vecTestMaxs = Vector( 4.0f, 4.0f, 4.0f );
									trace_t tr;
									UTIL_TraceHull( vecTripmineCandidates[i].vecOrigin, vecTripmineCandidates[i].vecOrigin, -vecTestMaxs, vecTestMaxs, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
									if ( tr.startsolid && OccupyStrategySlot( GetEngineerSlot() ) )
									{
										// Place it there
										m_TripminePlaceBehavior.MoveCandidateToFront( i );
										return AddActionGesture( ACT_GESTURE_RANGE_ATTACK_TRIPWIRE, vecTripmineCandidates[i].vecOrigin, 1.0f );
									}
								}
							}
						}
					}
				}
			}
		}

		//
		// Weapon switching
		//
		if ( npc_clonecop_use_new_weapon_switching.GetBool() )
		{
			// Do we want to switch?
			if ( m_flNextWeaponSwitchTime < gpGlobals->curtime && !IsPlayingActionGesture() && !IsPropShieldEquipped() )
			{
				if ( HasCondition( COND_COMBINE_DESIRE_WEAPON_SWITCH )
						|| HasCondition( COND_TOO_FAR_TO_ATTACK )
						|| HasCondition( COND_TOO_CLOSE_TO_ATTACK ) )
				{
					// Collect any weapons we might want under these conditions
					CUtlVector< int > vecSelectableWeapons;

					float flDistToEnemy = ( GetEnemyLKP() - GetAbsOrigin() ).Length();

					for (int i=0;i<MAX_WEAPONS;i++)
					{
						// See if this weapon has any switching data
						int j = -1;
						if ( m_hMyWeapons[i] )
							j = GetSwitchableWeaponIdx( m_hMyWeapons[i] );

						if ( j == -1 )
							continue;

						// Cutoff if it's too far for the weapon
						if ( flDistToEnemy > m_hMyWeapons[i]->m_fMaxRange1 || flDistToEnemy < m_hMyWeapons[i]->m_fMinRange1 )
							continue;

						// Now check if it's in our preferred range
						const SwitchableWeaponData_t &data = g_SwitchableWeaponData[j];
						if ( flDistToEnemy > data.flMaxRange || flDistToEnemy < data.flMinRange )
							continue;

						// TODO: Other conditions? Num enemies, size of enemy, health of enemy?

						// We want to consider this weapon
						vecSelectableWeapons.AddToTail( j );
					}

					if ( vecSelectableWeapons.Count() > 0 )
					{
						bool bSorted = false;

						//------------------------------------------
						// COUNTER TACTICS
						//------------------------------------------
						if ( m_bCounterTacticsAllowed )
						{
							if ( m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] > 0.7f )
							{
								// If the enemy is aggressive, pick the lowest ranged weapon we can use
								vecSelectableWeapons.Sort( []( const int *a, const int *b )
									{
										if (g_SwitchableWeaponData[*a].flMaxRange < g_SwitchableWeaponData[*b].flMaxRange)
											return -1;
										return 1;
									} );

								bSorted = true;
							}
							else if ( m_flCounterTacticWeights[COUNTER_TACTIC_SNIPER] > 0.7f )
							{
								// If the enemy snipes a lot, pick the longest ranged weapon we can use
								vecSelectableWeapons.Sort( []( const int *a, const int *b )
									{
										if (g_SwitchableWeaponData[*a].flMaxRange > g_SwitchableWeaponData[*b].flMaxRange)
											return -1;
										return 1;
									} );

								bSorted = true;
							}
						}

						CBaseCombatWeapon *pSelectedWeapon = NULL;
						if ( bSorted )
						{
							// Just use the head weapon
							pSelectedWeapon = Weapon_OwnsThisType( g_SwitchableWeaponData[vecSelectableWeapons[0]].pszClassname );
						}
						else
						{
							// Select a random weapon to switch to
							pSelectedWeapon = Weapon_OwnsThisType( g_SwitchableWeaponData[vecSelectableWeapons[RandomInt(0, vecSelectableWeapons.Count()-1)]].pszClassname );
						}

						// Try again in a while
						m_flNextWeaponSwitchTime = gpGlobals->curtime + 15.0f;

						// If it's not our active one, swap to it
						if ( pSelectedWeapon != GetActiveWeapon() )
						{
							inputdata_t inputdata;
							inputdata.value.SetString( pSelectedWeapon->m_iClassname );
							InputChangeWeapon( inputdata );

							// TODO: We should really have our own function for this instead of working around InputChangeWeapon
							int nLayer = FindGestureLayer( TranslateActivity( ACT_DISARM ) );
							if ( nLayer != -1 )
							{
								m_nActionGesture = nLayer;
								m_flActionGestureEndTime = gpGlobals->curtime + ( GetLayerDuration( nLayer ) * 1.5f ); // Account for both holster and unholster
								return nLayer;
							}
						}
					}
					else
					{
						// No weapons are valid right now, wait a bit
						m_flNextWeaponSwitchTime = gpGlobals->curtime + 2.0f;
					}
				}
				else
				{
					// Try again in a bit
					m_flNextWeaponSwitchTime = gpGlobals->curtime + 2.0f;
				}
			}
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CloneCop::SelectSchedule( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CloneCop::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( failedSchedule == SCHED_GET_HEALTHKIT && GetState() == NPC_STATE_COMBAT )
	{
		// Stop looking for this item
		if ( GetTarget() )
			m_hIgnoreItems.AddToTail( GetTarget() );
		return SCHED_RUN_RANDOM;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CloneCop::TranslateSchedule( int scheduleType )
{
	scheduleType = BaseClass::TranslateSchedule( scheduleType );

	switch (scheduleType)
	{
		case SCHED_COMBINE_ASSAULT:
		case SCHED_COMBINE_PRESS_ATTACK:
		case SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE:
		{
			if (!HasCondition(COND_COMBINE_CAN_ORDER_SURRENDER) && GetEnemy())
			{
				// See if we should find a health item
				if ( npc_clonecop_moving_pickup.GetBool() && ShouldLookForHealthItem( false ) )
				{
					CBaseEntity *pItem = m_hClosestItem;
					if ( !pItem )
					{
						float flRadius = 150.0f;

						// Search farther if I can't attack
						if ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
							flRadius = 300.0f;

						// Find one in a larger radius if we're avoidant
						else if ( ShouldUseAvoidantFlanking() )
							flRadius = 1000.0f;

						pItem = FindNearestHealthItem( GetAbsOrigin(), flRadius, true );
					}

					if ( pItem )
					{
						// See if we could opportunistically get this item
						Vector vecEnemyLKP = GetEnemyLKP();
						Vector vecMeToEnemy = (vecEnemyLKP - GetAbsOrigin());
						Vector vecMeToItem = (pItem->GetAbsOrigin() - GetAbsOrigin());

						bool bGoForItem = true;

						// Check if this item is in the direction of the enemy
						float flDot = DotProduct( vecMeToItem.Normalized(), vecMeToEnemy.Normalized() );
						if ( flDot > 0.0f )
						{
							// Make sure it's not farther away than the enemy
							if ( vecMeToItem.LengthSqr() > vecMeToEnemy.LengthSqr() )
								bGoForItem = false;

							// Check if we should be more careful
							else if ( GetHealth() < (GetMaxHealth() * 0.5f) || GetEnemies()->NumEnemies() > 3 )
							{
								// Don't go for it if they're close or can see it
								float flItemDistToEnemySqr = (pItem->GetAbsOrigin() - vecEnemyLKP).LengthSqr();
								if ( flItemDistToEnemySqr < Square( 200.0f )
									|| ( pItem->FVisible( vecEnemyLKP ) && flItemDistToEnemySqr < Square( 500.0f ) ) )
									bGoForItem = false;
							}
						}

						if ( bGoForItem )
						{
							SetTarget( pItem );
							return SCHED_GET_HEALTHKIT;
						}
					}
				}

				if ( !HasCondition( COND_NO_PRIMARY_AMMO ) && !HasCondition( COND_SEE_ENEMY )
					&& !IsPropShieldEquipped() && gpGlobals->curtime - GetLastDamageTime() < 30.0f )
				{
					// Just suppress if we've been damaged recently and we see our enemy's last seen position
					AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy(), true );
					if ( pMemory )
					{
						Vector vecLastSeen = pMemory->vLastSeenLocation;
						Vector vecLastKnown = pMemory->vLastKnownLocation;

						// Make sure they haven't moved away from it and we know it
						if ((vecLastSeen - vecLastKnown).LengthSqr() < Square( npc_clonecop_suppress_max_enemy_dist_from_seen.GetFloat() )
							&& CBaseCombatCharacter::FVisible( vecLastSeen ) && !CBaseCombatCharacter::FVisible( vecLastKnown ))
						{
							// If we're around the same level, creep towards the position
							if ( fabs(GetAbsOrigin().z - vecLastSeen.z) < 48.0f )
								return SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP;

							return SCHED_COMBINE_MERCILESS_SUPPRESS;
						}
					}
				}

				// Clone Cop attempts to flank unless he hasn't seen his enemy in a bit
				float flTimeLastSeen = GetEnemies()->LastTimeSeen(GetEnemy());
				if ( flTimeLastSeen != AI_INVALID_TIME && gpGlobals->curtime - flTimeLastSeen < 5.0f )
				{
					bool bHasShotgun = false;
					if ( GetActiveWeapon() && GetActiveWeapon()->ClassMatches( "weapon*shotgun" ) )
						bHasShotgun = true;

					if ( bHasShotgun && m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] > 0.75f )
					{
						// Be more defensive with the shotgun
						bHasShotgun = false;
					}

					if ( ShouldUseAvoidantFlanking() )
					{
						// Go behind our enemy if we're fighting a particularly dangerous opponent
						if ( ( GetEnemy()->IsPlayer() || GetEnemy()->GetHealth() > 50 ) && GetEnemies()->NumEnemies() < 3 )
							return SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE;

						// If we're getting low on health or have lots of enemies, then keep distance if we don't have a shotgun
						if ( ( GetHealth() < ( GetMaxHealth() * 0.5f ) || GetEnemies()->NumEnemies() > 3 ) && !bHasShotgun )
							return SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE;
					}

					// If we have a shotgun and we want to use one of the pressing schedules, use them
					// Otherwise, flank
					if ( !bHasShotgun || scheduleType == SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE )
						return SCHED_COMBINE_FLANK_LINE_OF_FIRE;
				}
			}
		} break;

		case SCHED_RANGE_ATTACK1:
		case SCHED_COMBINE_RANGE_ATTACK1:
		{
			if ( m_bCounterTacticsAllowed )
			{
				// Instead of standing still, use flanking schedules depending on what we would expect our opponent to do

				if ( m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] > 0.5f )
					return SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE;

				if ( m_flCounterTacticWeights[COUNTER_TACTIC_SNIPER] > 0.3f )
					return SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE;
			}

			// Only do this for weapons that are meant to be fired continuously (prototype AR2, M249, other LMG-like weapons)
			if ( GetActiveWeapon() && GetActiveWeapon()->GetMaxBurst() >= 8 )
			{
				// Don't stop firing
				return SCHED_COMBINE_MERCILESS_RANGE_ATTACK1;
			}
		} break;

		case SCHED_COMBINE_SUPPRESS:
		case SCHED_COMBINE_SIGNAL_SUPPRESS:
		{
			// Merciless
			return SCHED_COMBINE_MERCILESS_SUPPRESS;
		} break;

		case SCHED_COMBINE_DEPLOY_MANHACK:
		{
			if ( HasCondition( COND_SEE_ENEMY ) && npc_clonecop_throw_manhacks.GetBool() )
			{
				// See if a manhack would go through
				Vector mins( -12, -12, -12 );
				Vector maxs( 12, 12, 12 );
				trace_t tr;
				UTIL_TraceHull( EyePosition(), GetEnemy()->EyePosition(), mins, maxs, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
				if ( tr.fraction == 1.0f || tr.m_pEnt == GetEnemy() )
				{
					// whatever. Go my manhack
					return SCHED_COMBINE_THROW_MANHACK;
				}
			}
		} break;

		case SCHED_RUN_FROM_ENEMY:
		{
			if (HasCondition( COND_CAN_MELEE_ATTACK1 ))
			{
				return SCHED_PC_MELEE_AND_MOVE_AWAY;
			}
		} break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CloneCop::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ITEM_PICKUP:
		//case TASK_WEAPON_PICKUP:
			{
				if ( npc_clonecop_moving_pickup.GetBool() && GetState() != NPC_STATE_IDLE && ( !GetTarget() || m_hClosestItem == GetTarget() ) )
				{
					// Do nothing, the gesture should play automatically if it hasn't already
					TaskComplete();
				}
				else
				{
					BaseClass::StartTask( pTask );
				}
			}
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//-----------------------------------------------------------------------------
// Continuous movement tasks
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::IsCurTaskContinuousMove()
{
	// Needed to allow shooting while moving, as this can now be done in combat
	const Task_t* pTask = GetTask();
	if ( pTask && (pTask->iTask == TASK_ITEM_RUN_PATH) && npc_clonecop_moving_pickup.GetBool() )
		return true;

	return BaseClass::IsCurTaskContinuousMove();
}

//-----------------------------------------------------------------------------
// Purpose: Variant of CNPC_Combine::FindHealthItem() adapted for mobile use
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_CloneCop::FindNearestHealthItem( const Vector &vecOrigin, float flRadius, bool bComplex )
{
	CBaseEntity *pClosestEnt = NULL;
	float flClosestDistSqr = FLT_MAX;

	CBaseEntity *pEntity = gEntList.FindEntityInSphere( NULL, vecOrigin, flRadius );
	for ( ; pEntity; pEntity = gEntList.FindEntityInSphere( pEntity, vecOrigin, flRadius ) )
	{
		// Check if the classname at least begins with 'i' before evaluating
		if ( STRING( pEntity->m_iClassname )[0] != 'i' )
			continue;

		CItem *pItem = dynamic_cast<CItem *>(pEntity);
		if( pItem )
		{
			if (pItem->HasSpawnFlags(SF_ITEM_NO_NPC_PICKUP))
				continue;

			// Healthkits, healthvials, and batteries
			if ( pItem->ClassMatches( "item_health*" ) || pItem->ClassMatches( "item_battery" ) )
			{
				float flDistSqr = (GetAbsOrigin() - pItem->GetAbsOrigin()).Length2DSqr();
				if ( flDistSqr < flClosestDistSqr )
				{
					if ( bComplex )
					{
						// Don't use items we're ignoring in complex calls
						EHANDLE hItem = pItem;
						if ( m_hIgnoreItems.Find( hItem ) != m_hIgnoreItems.InvalidIndex() )
							continue;

						// UNDONE: Make sure we can fit here
						/*Vector vecOrigin = pItem->WorldSpaceCenter();
						vecOrigin.z += 8.0f;
						trace_t tr;
						AI_TraceHull( vecOrigin, vecOrigin, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, pItem, COLLISION_GROUP_NONE, &tr );

						if ( tr.startsolid && tr.m_pEnt != this )
							continue;*/

						// Make sure there's a node nearby that we can use to get this item.
						// Copied directly from TASK_GET_PATH_TO_TARGET_WEAPON since that doesn't run until the schedule is already running
						const float XY_LENIENCY = 64.0;
						const float Z_LENIENCY = 16.0; // 72.0

						int node = GetNavigator()->GetNetwork()->NearestNodeToPoint( this, pItem->GetAbsOrigin(), false );
						CAI_Node *pNode = GetNavigator()->GetNetwork()->GetNode( node );

						Vector vecNodePos = pNode->GetPosition( GetHullType() );

						float flDistZ = fabs( vecNodePos.z - pItem->GetAbsOrigin().z );
						if ( flDistZ > Z_LENIENCY )
							continue;
						
						float flDistXY = ( vecNodePos - pItem->GetAbsOrigin() ).Length2D();
						if( flDistXY > XY_LENIENCY )
							continue;
					}

					pClosestEnt = pEntity;
					flClosestDistSqr = flDistSqr;
				}
			}
		}
	}

	return pClosestEnt;
}

extern ConVar sk_healthkit;
extern ConVar sk_healthvial;

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CNPC_CloneCop::PickupItem( CBaseEntity *pItem )
{
	// Overrides base class for suit sounds

	// Must cache number of elements in case any fire only once (therefore being removed after being fired)
	int iNumElements = m_OnItemPickup.NumberOfElements();
	if (iNumElements > 0)
		m_OnItemPickup.Set( pItem, pItem, this );

	Assert( pItem != NULL );
	if( FClassnameIs( pItem, "item_healthkit" ) )
	{
		if ( TakeHealth( sk_healthkit.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
			EmitSound( "HealthKit.Touch" ); // TODO: E:Z variant distinction
		}
	}
	else if( FClassnameIs( pItem, "item_healthvial" ) )
	{
		if ( TakeHealth( sk_healthvial.GetFloat(), DMG_GENERIC ) )
		{
			RemoveAllDecals();
			UTIL_Remove( pItem );
			EmitSound( "HealthKit.Touch" ); // TODO: E:Z variant distinction
		}
	}
	else if (FClassnameIs( pItem, "item_battery" ))
	{
		// Add batteries to armor value
		m_ArmorValue += 25;
		UTIL_Remove( pItem );
		EmitSound( "ItemBattery.Touch" );
	}
	else if ( FClassnameIs(pItem, "item_ammo_ar2_altfire") || FClassnameIs(pItem, "item_ammo_smg1_grenade") ||
		FClassnameIs(pItem, "weapon_frag") )
	{
		AddGrenades( 1 );
		UTIL_Remove( pItem );
	}

	// Only warn if we didn't have any elements of OnItemPickup
	else if (iNumElements <= 0)
	{
		DevMsg("%s doesn't know how to pick up %s!\n", GetClassname(), pItem->GetClassname() );
	}
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
bool CNPC_CloneCop::IsPlayingActionGesture()
{
	return m_flActionGestureEndTime > gpGlobals->curtime && m_nActionGesture != -1;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
bool CNPC_CloneCop::IsPlayingActionGesture( Activity activity )
{
	if ( !IsPlayingActionGesture() )
		return false;

	return GetLayerActivity( m_nActionGesture ) == activity;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
int CNPC_CloneCop::AddActionGesture( Activity activity )
{
	if ( IsPlayingActionGesture() )
	{
		// Continue playing the one in progress if it's the same activity
		if ( GetLayerActivity( m_nActionGesture ) == activity )
			return m_nActionGesture;

		// Cancel the existing one if not
		RemoveLayer( m_nActionGesture );
	}

	m_nActionGesture = AddGesture( activity );

	if ( m_nActionGesture != -1 )
	{
		m_flActionGestureEndTime = gpGlobals->curtime + GetLayerDuration( m_nActionGesture );

		// Stop firing while playing action gestures
		GetShotRegulator()->FireNoEarlierThan( m_flActionGestureEndTime );
	}

	return m_nActionGesture;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
int CNPC_CloneCop::AddActionGesture( Activity activity, const Vector &vecPosition, float flImportance )
{
	int nLayer = AddActionGesture( activity );
	if ( nLayer == -1 )
		return nLayer;

	// Face the target
	float flDuration = GetLayerDuration( nLayer );
	AddFacingTarget( vecPosition, flImportance, flDuration );

	if ( GetEnemy() )
	{
		// Overwrite facing target given by move shoot
		// (shot regulator in default action gesture portion should prevent shooting during this gesture)
		float flInvImportance = 1.0f - flImportance; //  MAX( 0.1f, 1.0f - flImportance )
		if ( flInvImportance > 0.0f )
		{
			AddFacingTarget( GetEnemy(), GetEnemyLKP(), flInvImportance, flDuration );
		}
		else
			AddFacingTarget( GetEnemy(), GetEnemyLKP(), 0.1f, 0.1f );

		// NOTE: This could get overwritten by CNPC_Combine::PrescheduleThink() if we're close enough to our goal
		m_MoveAndShootOverlay.SuspendMoveAndShoot( flDuration );
	}

	return nLayer;
}

extern ConVar sk_npc_dmg_combineball;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_CloneCop::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// Take less damage from NPC balls
	if ( info.GetInflictor() && FClassnameIs(info.GetInflictor(), "prop_combine_ball") &&
			static_cast<CPropCombineBall*>(info.GetInflictor())->WasFiredByNPC() )
	{
		if (info.GetDamage() == GetMaxHealth())
			info.SetDamage( sk_npc_dmg_combineball.GetFloat() );
	}

	// Clone Cop has his own armor, similar to the player's. 
	if (info.GetDamage() && m_ArmorValue && !(info.GetDamageType() & (DMG_FALL | DMG_DROWN | DMG_POISON | DMG_RADIATION)) )// armor doesn't protect against fall or drown damage!
	{
		float flRatio = 0.2f;

		float flNew = info.GetDamage() * flRatio;

		float flArmor;

		flArmor = (info.GetDamage() - flNew);

		if( flArmor < 1.0 )
		{
			flArmor = 1.0;
		}

		// Does this use more armor than we have?
		if (flArmor > m_ArmorValue)
		{
			flArmor = m_ArmorValue;
			flNew = info.GetDamage() - flArmor;
			m_ArmorValue = 0;
		}
		else
		{
			m_ArmorValue -= flArmor;
		}
		
		info.SetDamage( flNew );
	}

	if (m_lifeState == LIFE_ALIVE)
	{
		// Spark at 30% health.
		if ( !IsBleeding() && ( GetHealth() <= GetMaxHealth() * 0.3 ) )
		{
			StartBleeding();
		}

		if ( m_bCounterTacticsAllowed )
		{
			if ( info.GetDamageType() & (DMG_BUCKSHOT | DMG_CLUB) )
			{
				// Increase brute tactic based on damage amount
				m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] += Clamp( info.GetDamage() / 150.0f, 0.0f, 1.0f ) * 0.5f;

				// Make sure we consider getting a shotgun now
				if ( GetActiveWeapon() && !GetActiveWeapon()->ClassMatches( "weapon*shotgun" ) )
					m_flNextWeaponSwitchTime = gpGlobals->curtime;
			}
		}
	}

	return BaseClass::OnTakeDamage( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_CloneCop::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
	case HITGROUP_HEAD:
		{
			// Soldiers take double headshot damage
			return 2.0f;
		}
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
Vector CNPC_CloneCop::GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy )
{
	CBaseEntity *pEnemy = GetEnemy();

	if ( pEnemy )
	{
		Vector vecEnemyLKP = GetEnemyLKP();
		Vector vecEnemyOffset;

		if (GetEnemy()->IsNPC())
		{
			float flDist = EnemyDistance( GetEnemy() );
			if (flDist < 768.0f && flDist > 32.0f)
			{
				// Aim for the head, like players do
				vecEnemyOffset = pEnemy->HeadTarget( shootOrigin ) - pEnemy->GetAbsOrigin();
			}
		}
		else
		{
			vecEnemyOffset = pEnemy->BodyTarget( shootOrigin, bNoisy ) - pEnemy->GetAbsOrigin();
		}

		Vector retval = vecEnemyOffset + vecEnemyLKP - shootOrigin;
		VectorNormalize( retval );
		return retval;
	}
	else
	{
		Vector forward;
		AngleVectors( GetLocalAngles(), &forward );
		return forward;
	}
}

extern ConVar ai_lead_time;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Vector CNPC_CloneCop::GetActualShootPosition( const Vector &shootOrigin )
{
	if ( GetEnemy() && GetEnemy()->IsNPC() )
	{
		float flDist = EnemyDistance( GetEnemy() );
		if (flDist < 768.0f && flDist > 32.0f)
		{
			// Aim for the head, like players do
			Vector vecEnemyLKP = GetEnemyLKP();
			Vector vecEnemyOffset = GetEnemy()->HeadTarget( shootOrigin ) - GetEnemy()->GetAbsOrigin();

			// Scale down towards the torso the closer the target is
			if (flDist < 192.0f)
			{
				vecEnemyOffset *= ( ((flDist / 192.0f) * 0.5f) + 0.5f );
			}

			Vector vecTargetPosition = vecEnemyOffset + vecEnemyLKP;

			// lead for some fraction of a second.
			return (vecTargetPosition + ( GetEnemy()->GetSmoothedVelocity() * ai_lead_time.GetFloat() ));
		}
	}

	return BaseClass::GetActualShootPosition( shootOrigin );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::HandleAnimEvent( animevent_t *pEvent )
{
	bool handledEvent = false;

	CBaseEntity *pHurt;
	CBaseCombatCharacter* pBCC;;
	trace_t tr;

	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if ( pEvent->event == AE_PICKUP_NEAREST_ITEM )
		{
			CBaseEntity *pItem = m_hClosestItem;
			if ( !pItem )
			{
				pItem = GetTarget();
				if ( !dynamic_cast<CItem *>(pItem) )
				{
					// No item stored. Try finding one
					pItem = FindHealthItem( GetAbsOrigin(), Vector( 48.0f, 48.0f, 24.0f ) );
				}
			}

			if (pItem)
			{
				PickupItem( pItem );
				m_hClosestItem = NULL;

				// Tell the schedule that we got the item
				if ( IsCurSchedule( SCHED_GET_HEALTHKIT, false ) )
					SetCondition( COND_PROVOKED );

				// Now that we've picked up this item, tell the NPC to look again in case there's another one nearby
				m_flNextHealthSearchTime = gpGlobals->curtime + 0.25f;
			}
		}
		else if ( pEvent->event == AE_METROPOLICE_THROW_DEPLOY )
		{
			OnAnimEventStartDeployManhack();

			// Activate it early
			m_hManhack->RemoveSpawnFlags( SF_MANHACK_CARRIED );
			m_hManhack->RemoveSpawnFlags( SF_NPC_WAIT_FOR_SCRIPT );
			m_hManhack->ClearSchedule( "Manhack released by metropolice" );
		}
		else if ( pEvent->event == AE_METROPOLICE_THROW_MANHACK )
		{
			OnAnimEventDeployManhack( pEvent );

			if ( m_hManhack && m_hManhack->VPhysicsGetObject() )
			{
				// Throw it in the direction of our enemy
				Vector vecThrowDir;
				if ( GetEnemy() )
				{
					Vector vecTargetPos = GetEnemies()->LastSeenPosition( GetEnemy() );
					//vecTargetPos += (GetEnemy()->GetViewOffset() * 0.75f);

					vecThrowDir = (vecTargetPos - m_hManhack->GetAbsOrigin());
					VectorNormalize( vecThrowDir );
				}
				else
				{
					GetVectors( &vecThrowDir, NULL, NULL );
				}

				vecThrowDir *= npc_clonecop_throw_manhack_speed.GetFloat();

				Vector	forceAng = vec3_origin;

				// Set the velocity to override the pre-existing one
				m_hManhack->VPhysicsGetObject()->SetVelocity( &vecThrowDir, &forceAng );
			}
		}
		else if ( pEvent->event == AE_SLAM_TRIPMINE_PLACE )
		{
			// Since we can run this without the behavior
			m_TripminePlaceBehavior.HandleAnimEvent( pEvent );
		}
		else
			BaseClass::HandleAnimEvent( pEvent );
	}
	else
	{
		switch( pEvent->event )
		{
		case COMBINE_AE_KICK:
			if ( m_hOpeningDoor && !IsCurSchedule( SCHED_MELEE_ATTACK1 ) )
			{
				if ( KickDoor( m_hOpeningDoor, sk_clonecop_kick.GetInt() ) )
				{
					handledEvent = true;
					break;
				}
			}

			// Try to dispatch a kick interaction
			pHurt = CheckTraceHullAttack( 70, -Vector( 16, 16, 18 ), Vector( 16, 16, 18 ), 0, DMG_CLUB );
			pBCC = ToBaseCombatCharacter( pHurt );
			
			if (pBCC)
			{
				CTakeDamageInfo dmgInfo( this, this, sk_clonecop_kick.GetFloat(), DMG_CLUB );

				UTIL_TraceLine( WorldSpaceCenter(), pHurt->WorldSpaceCenter(), MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
				KickInfo_t kickInfo( &tr, &dmgInfo );
				if (pBCC && pBCC->DispatchInteraction( g_interactionBadCopKick, &kickInfo, this ))
				{
					// If the interaction returns true, exit
					handledEvent = true;
					break;
				}
			}
			// Fall through if the kick was unhandled
		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
		}
	}

	if( handledEvent )
	{
		m_iLastAnimEventHandled = pEvent->event;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::CanAltFireEnemy( bool bUseFreeKnowledge )
{
	if ( !BaseClass::CanAltFireEnemy( bUseFreeKnowledge ) )
		return false;

	// Not while picking things up, etc.
	if ( IsPlayingActionGesture() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::CanGrenadeEnemy( bool bUseFreeKnowledge )
{
	if ( !BaseClass::CanGrenadeEnemy( bUseFreeKnowledge ) )
		return false;
	
	// Not while picking things up, etc.
	if ( IsPlayingActionGesture() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::CanRunAScriptedNPCInteraction( bool bForced )
{
	if ( !BaseClass::CanRunAScriptedNPCInteraction( bForced ) )
		return false;
	
	// Not while picking things up, etc.
	if ( IsPlayingActionGesture() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_CloneCop::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	CBaseEntity *pEnemy = GetEnemy();
	if (pEnemy)
	{
		set.AppendCriteria("enemy_visible", (FInViewCone(pEnemy) && FVisible(pEnemy)) ? "1" : "0");
	}
}

//-----------------------------------------------------------------------------
// Our health is low. Show damage effects.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::BleedThink()
{
	if (GetHealth() > GetMaxHealth() * 0.3)
	{
		// We don't need to bleed anymore
		StopBleeding();
		SetNextThink( TICK_NEVER_THINK, CC_BLEED_THINK );
		return;
	}

	// Spurt blood from random points on the hunter's head.
	Vector vecOrigin;
	QAngle angDir;
	GetAttachment( gm_nBloodAttachment, vecOrigin, angDir );
	
	Vector vecDir = RandomVector( -1, 1 );
	VectorNormalize( vecDir );
	VectorAngles( vecDir, Vector( 0, 0, 1 ), angDir );

	vecDir *= gm_flBodyRadius;
	DispatchParticleEffect( "blood_spurt_synth_01", vecOrigin + vecDir, angDir );

	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.7, 1.9 ), CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::StartBleeding()
{
	// Do this even if we're already bleeding (see OnRestore).
	m_bIsBleeding = true;

	// Start gushing blood from our... anus or something.
	DispatchParticleEffect( "blood_drip_synth_01", PATTACH_POINT_FOLLOW, this, gm_nBloodAttachment );

	// Emit spurts of our blood
	SetContextThink( &CNPC_CloneCop::BleedThink, gpGlobals->curtime + 0.1, CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::StopBleeding()
{
	m_bIsBleeding = false;

	// Not really any other way to stop the bleeding
	StopParticleEffects( this );

	SetContextThink( &CNPC_CloneCop::BleedThink, TICK_NEVER_THINK, CC_BLEED_THINK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::ShouldUseAvoidantFlanking()
{
	return ( m_bUseAvoidantFlanking || npc_clonecop_always_use_avoidant_flanking.GetBool() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::CanDeployManhack()
{
	if ( !BaseClass::CanDeployManhack() )
		return false;

	// Not while picking things up, etc.
	if ( IsPlayingActionGesture() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::HandleManhackSpawn( CAI_BaseNPC *pNPC )
{
	CNPC_Manhack *pManhack = static_cast<CNPC_Manhack*>(pNPC);

	pManhack->TurnIntoNemesis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Event_Killed( const CTakeDamageInfo &info )
{
	if( IsElite() )
	{
		if ( HasSpawnFlags( SF_COMBINE_NO_AR2DROP ) == false )
		{
			CBaseEntity *pItem;
			if (GetActiveWeapon() && FClassnameIs(GetActiveWeapon(), "weapon_smg1"))
				pItem = DropItem( "item_ammo_smg1_grenade", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			else
				pItem = DropItem( "item_ammo_ar2_altfire", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

			if ( pItem )
			{
				IPhysicsObject *pObj = pItem->VPhysicsGetObject();

				if ( pObj )
				{
					Vector			vel		= RandomVector( -64.0f, 64.0f );
					AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

					vel[2] = 0.0f;
					pObj->AddVelocity( &vel, &angImp );
				}

				if( info.GetDamageType() & DMG_DISSOLVE )
				{
					CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

					if( pAnimating )
					{
						pAnimating->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
					}
				}
				else
				{
					WeaponManager_AddManaged( pItem );
				}
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::Event_KilledOther(pVictim, info);

	// Actually shoot at the Combine's aim target instead of just aiming
	// Helps mask some of the other stuff ported from Alyx.
	if ( GetEnemies()->NumEnemies() == 1 && GetAimTarget() )
	{
		// Don't do this against dissolve or blast damage
		if( !HasShotgun() && !(info.GetDamageType() & (DMG_DISSOLVE | DMG_BLAST)) )
		{
			CAI_BaseNPC *pTarget = GetAimTarget()->MyNPCPointer();
			if (pTarget)
			{
				AddEntityRelationship( pTarget, IRelationType(pVictim), IRelationPriority(pVictim) );

				GetEnemies()->UpdateMemory( GetNavigator()->GetNetwork(), pTarget, pTarget->GetAbsOrigin(), 0.0f, true );
				AI_EnemyInfo_t *pMemory = GetEnemies()->Find( pTarget );

				if( pMemory )
				{
					// Pretend we've known about this target longer than we really have.
					pMemory->timeFirstSeen = gpGlobals->curtime - 10.0f;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clone Cop only uses gesture flinches.
//-----------------------------------------------------------------------------
Activity CNPC_CloneCop::GetFlinchActivity( bool bHeavyDamage, bool bGesture )
{
	if (!bGesture)
		return ACT_RESET;

	return BaseClass::GetFlinchActivity( bHeavyDamage, bGesture );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;
	
	if ( info.GetAmmoType() == GetAmmoDef()->Index("556mm") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	return BaseClass::IsHeavyDamage( info );
}

Activity CNPC_CloneCop::NPC_TranslateActivity( Activity eNewActivity )
{
	// If there's any reason to treat this as an actual melee attack 2, do that instead. For now I see no reason not to just translate the activity
	if ( eNewActivity == ACT_MELEE_ATTACK1 )
	{
		return ACT_MELEE_ATTACK2;
	}

	if ( eNewActivity == ACT_GESTURE_DEPLOY_MANHACK )
	{
		// Throw the manhack if we see our enemy
		// (the main activity is already handled in TranslateSchedule)
		if ( HasCondition( COND_SEE_ENEMY ) && npc_clonecop_throw_manhacks.GetBool() )
			return ACT_GESTURE_THROW_MANHACK;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: Translate base class activities
//-----------------------------------------------------------------------------
Activity CNPC_CloneCop::Weapon_TranslateActivity( Activity eNewActivity, bool *pRequired )
{
	return BaseClass::Weapon_TranslateActivity( eNewActivity, pRequired );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Weapon_HandleEquip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_HandleEquip( pWeapon );

	// Since the new weapon switching doesn't directly rely on the weapon's own range values,
	// make it more difficult to rush at Clone Cop with a shotgun when he hasn't switched to one
	/*if ( npc_clonecop_use_new_weapon_switching.GetBool() && FClassnameIs( pWeapon, "weapon_ar2*" ) )
	{
		pWeapon->m_fMinRange1 = 0.0f;
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::Weapon_SetActivity( Activity newActivity, float duration )
{
	if ( newActivity == ACT_RANGE_ATTACK_SHOTGUN )
	{
		// Allow double barrel shot if our enemy is being equally aggressive
		if ( m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] > 0.5f && GetEnemy() && EnemyDistance( GetEnemy() ) < 250.0f && GetActiveWeapon() && GetActiveWeapon()->m_iClip1 > 1 )
		{
			animevent_t animEvent;
			animEvent.event = EVENT_WEAPON_AR2_GRENADE;
			GetActiveWeapon()->Operator_HandleAnimEvent( &animEvent, this );

			// Twice as much delay
			SetNextAttack( gpGlobals->curtime + ( ( GetNextAttack() - gpGlobals->curtime ) * 2.0f ) );


			return;
		}
	}

	BaseClass::Weapon_SetActivity( newActivity, duration );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	bool bResult = BaseClass::MovementCost( moveType, vecStart, vecEnd, pCost );

	if ( GetEnemy() == NULL )
		return bResult;

	if ( IsCurSchedule( SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE, false )
		|| IsCurSchedule( SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE, false ) )
	{
		// If this node is visible to the enemy and it isn't near the goal, try not to take it
		// This is needed because the flanking tasks may select a node that is on the other side of the enemy,
		// and they need to avoid cutting straight through.
		Vector vecToGoal = ( vecEnd - GetNavigator()->GetGoalPos() );
		Vector vecToEnemy = ( vecEnd - GetEnemy()->GetAbsOrigin() );
		if ( vecToEnemy.LengthSqr() < vecToGoal.LengthSqr() )
		{
			bResult = true;

			if ( GetEnemy()->FVisible( vecEnd ) )
			{
				*pCost *= 25.0f;
			}
			else
			{
				// Not as expensive, but we should still prefer nodes closer to the goal
				*pCost *= 10.0f;
			}
		}
	}
	else if ( m_flCounterTacticWeights[COUNTER_TACTIC_SNIPER] > 0.1f )
	{
		// Same as above, but for when we're countering sniper tactics
		Vector vecToGoal = ( vecEnd - GetNavigator()->GetGoalPos() );
		Vector vecToEnemy = ( vecEnd - GetEnemy()->GetAbsOrigin() );
		if ( vecToEnemy.LengthSqr() < vecToGoal.LengthSqr() )
		{
			if ( GetEnemy()->FVisible( vecEnd ) )
			{
				*pCost *= MAX( (20.0f * m_flCounterTacticWeights[COUNTER_TACTIC_SNIPER]), 1.0f );
			}
			else
			{
				// Not as expensive, but we should still prefer nodes closer to the goal
				*pCost *= MAX( (10.0f * m_flCounterTacticWeights[COUNTER_TACTIC_SNIPER]), 1.0f );
			}
		}
	}

	if ( ShouldUseAvoidantFlanking() )
	{
		// Avoid nodes that put me below my enemy, prefer nodes that put me above
		Vector vecEnemyOrigin = GetEnemyLKP();

		if ( vecEnd.z > vecEnemyOrigin.z )
		{
			*pCost *= RemapValClamped( vecEnd.z - vecEnemyOrigin.z, 0.0f, 200.0f, 1.0f, 0.5f );
		}
		else if ( vecEnd.z < vecEnemyOrigin.z )
		{
			*pCost *= RemapValClamped( vecEnemyOrigin.z - vecEnd.z, 0.0f, 300.0f, 1.0f, 2.0f );
		}
	}

	// UNDONE: Reduce movement cost around health items
	//if ( HasCondition( COND_HEALTH_ITEM_AVAILABLE ) )
	/*{
		// If there are any items near this node, then make it more valuable
		const float MAX_ITEM_DIST_SQR = 48.0f;
	
		const Vector vecLink = (vecEnd - vecStart);
		const Vector vecMidpoint = vecStart + (vecLink * 0.5f);
		const float flLinkDist = vecLink.Length();

		NDebugOverlay::Cross3D( vecMidpoint, 5.0f, 255, 0, 0, true, 3.0f );

		CBaseEntity *pItem = FindHealthItem( vecMidpoint, Vector( flLinkDist, flLinkDist, flLinkDist * 0.5f ) );
		if ( pItem )
		{
			// See if it's on the way there
			float flItemDistToLinkSqr = CalcDistanceSqrToLine( pItem->GetAbsOrigin(), vecStart, vecEnd );
			if ( flItemDistToLinkSqr < MAX_ITEM_DIST_SQR )
			{
				// Reduce cost based on how far it is
				*pCost = RemapVal( flItemDistToLinkSqr, 0.0f, MAX_ITEM_DIST_SQR, 0.25f, 0.5f );
				bResult = true;

				NDebugOverlay::HorzArrow( vecStart, vecEnd, 48.0f, 0, 255, 0, 255, true, 3.0f );
			}

			NDebugOverlay::HorzArrow( vecStart, vecEnd, 48.0f, 255, 128, 0, 255, true, 3.0f );
		}
		else
			NDebugOverlay::HorzArrow( vecStart, vecEnd, 48.0f, 255, 0, 0, 255, true, 3.0f );
	}*/

	return bResult;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WeaponProficiency_t CNPC_CloneCop::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	// TODO: Different proficiency?
	//if( pWeapon->ClassMatches( gm_isz_class_AR2 ) )
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}
	//else if( pWeapon->ClassMatches( gm_isz_class_Shotgun ) )
	//{
	//	return WEAPON_PROFICIENCY_PERFECT;
	//}
	//else if( pWeapon->ClassMatches( gm_isz_class_SMG1 ) )
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}
	//else if (FClassnameIs( pWeapon, "weapon_smg2" ))
	//{
	//	return WEAPON_PROFICIENCY_VERY_GOOD;
	//}

	return BaseClass::CalcWeaponProficiency( pWeapon );
}

//-----------------------------------------------------------------------------
// Purpose: Allows NPC to holster from more than just the animation event
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::DoHolster( void )
{
	if (BaseClass::DoHolster())
	{
		EmitSound( "NPC_Combine.Zipline_MidClothing" );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows NPC to unholster from more than just the animation event
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::DoUnholster( void )
{
	if (BaseClass::DoUnholster())
	{
		PlayDeploySound( GetActiveWeapon() );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ )
{
	if (BaseClass::Weapon_Switch(pWeapon, viewmodelindex))
	{
		EmitSound( "NPC_Combine.Zipline_MidClothing" );
		PlayDeploySound( pWeapon );

		// New weapon switching uses preferred ranges, rather than modifying hard range limits
		if ( !npc_clonecop_use_new_weapon_switching.GetBool() )
		{
			if (EntIsClass( pWeapon, gm_isz_class_Shotgun ))
			{
				pWeapon->m_fMaxRange1 = MIN( 512, pWeapon->m_fMaxRange1 );
			}
			else if (EntIsClass( pWeapon, gm_isz_class_AR2 ) || FClassnameIs( pWeapon, "weapon_ar2_proto" ))
			{
				pWeapon->m_fMinRange1 = MAX( 256, pWeapon->m_fMinRange1 );
				pWeapon->m_fMaxRange1 = MIN( 1024, pWeapon->m_fMaxRange1 );
			}
			else if (FClassnameIs( pWeapon, "weapon_crossbow" ))
			{
				pWeapon->m_fMinRange1 = MAX( 768, pWeapon->m_fMinRange1 );
			}
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CloneCop::PlayDeploySound( CBaseCombatWeapon *pWeapon )
{
	// Don't play sound if not in combat
	if (GetState() != NPC_STATE_COMBAT)
		return;

	int i = GetSwitchableWeaponIdx( pWeapon );
	if ( i == -1 )
	{
		// Fall back to RELOAD_NPC
		pWeapon->WeaponSound( RELOAD_NPC );
	}
	else
	{
		const SwitchableWeaponData_t &data = g_SwitchableWeaponData[i];
		pWeapon->WeaponSound( data.nDeploySound );
	}

	/*if (EntIsClass( pWeapon, gm_isz_class_Shotgun ))
	{
		pWeapon->WeaponSound( SPECIAL1 );
	}
	else if (EntIsClass( pWeapon, gm_isz_class_SMG1 ))
	{
		pWeapon->WeaponSound( SPECIAL2 );
	}
	else
	{
		pWeapon->WeaponSound( RELOAD_NPC );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE = 96.0f; // How high CC can jump; Default 64
	const float MAX_JUMP_DISTANCE = 384.0f; // How far CC can jump; Default 384
	const float MAX_JUMP_DROP = 384.0f; // How far CC can fall; Default 160

	return BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_CloneCop::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.r1 = 253;
	params.g1 = 162;
	params.b1 = 2;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CloneCop::DrawDebugTextOverlays( void )
{
	int nOffset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT) 
	{
		if ( m_bCounterTacticsAllowed )
		{
			char tempstr[512];

			Q_strncpy( tempstr, "Counter Tactic Weights:", sizeof( tempstr ) );
			EntityText( nOffset, tempstr, 0 );
			nOffset++;

			static const char *g_pszCounterTacticNames[COUNTER_TACTIC_COUNT] = {
				"Brute",
				"Sniper",
				"Mobile",
				//"Distraction",
			};

			for ( int i = 0; i < COUNTER_TACTIC_COUNT; i++ )
			{
				Q_snprintf( tempstr, sizeof( tempstr ), "- %s: %.3f", g_pszCounterTacticNames[i], m_flCounterTacticWeights[i] );

				int nClr = 255 * (1.0f - m_flCounterTacticWeights[i]);
				EntityText( nOffset, tempstr, 0, 255, nClr, nClr );
				nOffset++;
			}
		}
	}

	return nOffset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_CloneCop::GetSwitchableWeaponIdx( CBaseCombatWeapon *pWeapon )
{
	unsigned short nCachedIdx = m_SwitchableWeaponCache.Find( pWeapon->m_iClassname );
	if ( nCachedIdx != m_SwitchableWeaponCache.InvalidIndex() )
		return m_SwitchableWeaponCache[nCachedIdx];

	// Find one that matches the classname search
	int i = 0;
	for ( ; i < ARRAYSIZE( g_SwitchableWeaponData ); i++ )
	{
		if ( pWeapon->ClassMatches( g_SwitchableWeaponData[i].pszClassname ) )
			break;
	}

	if ( i == ARRAYSIZE( g_SwitchableWeaponData ) )
	{
		// No data
		i = -1;
	}

	// Add it to the cache so that we remember later
	m_SwitchableWeaponCache.Insert( pWeapon->m_iClassname, i );

	return i;
}

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
LINK_ENTITY_TO_CLASS( npc_badcop, CNPC_BadCop );

CNPC_BadCop::CNPC_BadCop()
{
	m_SquadName = MAKE_STRING( "bc_squad" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_BadCop::Spawn( void )
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_BadCop::Precache()
{
	BaseClass::Precache();

	// Pretend we're the player
	AddContext( "classname", "player" );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_BadCop::Activate()
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( npc_clonecop, CNPC_CloneCop )

DECLARE_CONDITION( COND_COMBINE_WEAPON_SIGHT_OCCLUDED )
DECLARE_CONDITION( COND_COMBINE_DESIRE_WEAPON_SWITCH )
DECLARE_CONDITION( COND_COMBINE_PLAYING_ACTION_GESTURE )
DECLARE_CONDITION( COND_COMBINE_CAN_MELEE_GESTURE )

DECLARE_ACTIVITY( ACT_GESTURE_PICKUP_GROUND )
DECLARE_ACTIVITY( ACT_GESTURE_PICKUP_RACK )
DECLARE_ACTIVITY( ACT_METROPOLICE_THROW_MANHACK )
DECLARE_ACTIVITY( ACT_GESTURE_THROW_MANHACK )

DECLARE_ANIMEVENT( AE_PICKUP_NEAREST_ITEM )
DECLARE_ANIMEVENT( AE_METROPOLICE_THROW_DEPLOY )
DECLARE_ANIMEVENT( AE_METROPOLICE_THROW_MANHACK )

 DEFINE_SCHEDULE 
 (
	 // note 4/10/2026: This schedule doesn't really use the flanking task correctly.
	 //					TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS is supposed to take a minimum angle difference for valid nodes,
	 //					but since this schedule inputs zero, it theoretically just accepts any node without doing anything different.
	 //					However, the flanking tasks still skip the lateral LOS check (i.e. stepping a little to the side instead of
	 //					finding a new node), so this schedule wouldn't necessarily act the same as SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE
	 //					either.
	 //
	 //					The "correct" way to resolve this while retaining its behavior would probably be to add a new task (probably named
	 //					something like TASK_GET_PATH_TO_ENEMY_LOS_NODE) that skips the lateral LOS check without running flanking algorithms.
	 //					However, the current behavior doesn't cause any serious problems (except running a bunch of unnecessary calculations)
	 //					and I don't know for sure if my theory holds up, so I'll keep this the same for now.
	 SCHED_COMBINE_FLANK_LINE_OF_FIRE,

	 "	Tasks "
	 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE"
	 "		TASK_SET_TOLERANCE_DISTANCE		48"
	 "		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	0"
	 "		TASK_COMBINE_SET_STANDING		1"
	 "		TASK_SPEAK_SENTENCE				1"
	 "		TASK_RUN_PATH					0"
	 "		TASK_WAIT_FOR_MOVEMENT			0"
	 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
	 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	 "	"
	 "	Interrupts "
	 "		COND_NEW_ENEMY"
	 "		COND_ENEMY_DEAD"
	 //"		COND_CAN_RANGE_ATTACK1"
	 //"		COND_CAN_RANGE_ATTACK2"
	 "		COND_CAN_MELEE_ATTACK1"
	 "		COND_CAN_MELEE_ATTACK2"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_HEAVY_DAMAGE"
 )

 DEFINE_SCHEDULE 
 (
	 SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE,

	 "	Tasks "
	 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_FLANK_LINE_OF_FIRE"
	 "		TASK_SET_TOLERANCE_DISTANCE		48"
	 "		TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION	0"
	 "		TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS		300"
	 "		TASK_COMBINE_SET_STANDING		1"
	 "		TASK_SPEAK_SENTENCE				1"
	 "		TASK_RUN_PATH					0"
	 "		TASK_WAIT_FOR_MOVEMENT			0"
	 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
	 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	 "	"
	 "	Interrupts "
	 "		COND_NEW_ENEMY"
	 "		COND_ENEMY_DEAD"
	 //"		COND_CAN_RANGE_ATTACK1"
	 //"		COND_CAN_RANGE_ATTACK2"
	 "		COND_CAN_MELEE_ATTACK1"
	 "		COND_CAN_MELEE_ATTACK2"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_HEAVY_DAMAGE"
 )

 DEFINE_SCHEDULE 
 (
	 SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE,

	 "	Tasks "
	 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_FLANK_LINE_OF_FIRE"
	 "		TASK_SET_TOLERANCE_DISTANCE		48"
	 "		TASK_STORE_POSITION_IN_SAVEPOSITION		0"
	 "		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	90"
	 "		TASK_COMBINE_SET_STANDING		1"
	 "		TASK_SPEAK_SENTENCE				1"
	 "		TASK_RUN_PATH					0"
	 "		TASK_WAIT_FOR_MOVEMENT			0"
	 "		TASK_COMBINE_IGNORE_ATTACKS		0.0"
	 "		TASK_SET_SCHEDULE				SCHEDULE:SCHED_COMBAT_FACE"
	 "	"
	 "	Interrupts "
	 "		COND_NEW_ENEMY"
	 "		COND_ENEMY_DEAD"
	 //"		COND_CAN_RANGE_ATTACK1"
	 //"		COND_CAN_RANGE_ATTACK2"
	 "		COND_CAN_MELEE_ATTACK1"
	 "		COND_CAN_MELEE_ATTACK2"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_HEAVY_DAMAGE"
 )

 //===============================================
 //	> RangeAttack1
 //===============================================
 DEFINE_SCHEDULE
 (
 	SCHED_COMBINE_MERCILESS_RANGE_ATTACK1,
 
 	"	Tasks"
 	"		TASK_STOP_MOVING		0"
 	"		TASK_FACE_ENEMY			0"
 	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
 	"		TASK_RANGE_ATTACK1		0"
 	""
 	"	Interrupts"
	"		COND_NEW_ENEMY"
 	"		COND_ENEMY_WENT_NULL"
 	"		COND_HEAVY_DAMAGE"
 	"		COND_ENEMY_OCCLUDED"
 	"		COND_NO_PRIMARY_AMMO"
 	"		COND_HEAR_DANGER"
	"		COND_HEAR_MOVE_AWAY"
	"		COND_COMBINE_NO_FIRE"
 	"		COND_WEAPON_BLOCKED_BY_FRIEND"
 	"		COND_WEAPON_SIGHT_OCCLUDED"
 )

 DEFINE_SCHEDULE
 (
	SCHED_COMBINE_MERCILESS_SUPPRESS,

	 "	Tasks"
	 "		TASK_STOP_MOVING			0"
	 "		TASK_FACE_ENEMY				0"
	 //"		TASK_COMBINE_SET_STANDING	0"
	 "		TASK_RANGE_ATTACK1			0"
	 ""
	 "	Interrupts"
	 "		COND_SEE_ENEMY"
	 "		COND_ENEMY_WENT_NULL"
	 "		COND_HEAVY_DAMAGE"
	 "		COND_NO_PRIMARY_AMMO"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_COMBINE_NO_FIRE"
	 "		COND_WEAPON_BLOCKED_BY_FRIEND"
	 "		COND_COMBINE_WEAPON_SIGHT_OCCLUDED"
 	"		COND_COMBINE_PLAYING_ACTION_GESTURE"
 )

 DEFINE_SCHEDULE
 (
	SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP,

	 "	Tasks"
	 "		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_COMBINE_MERCILESS_SUPPRESS"
	 "		TASK_SET_TOLERANCE_DISTANCE		48"
	 "		TASK_GET_PATH_TO_ENEMY_LKP_LOS	0"
	 "		TASK_COMBINE_SET_STANDING		1"
	 "		TASK_SPEAK_SENTENCE				1"
	 "		TASK_WALK_PATH					0"
	 "		TASK_WAIT_FOR_MOVEMENT			0"
	 "		TASK_FACE_ENEMY				0" // TODO: Change to face last seen pos
	 //"		TASK_COMBINE_SET_STANDING	0"
	 "		TASK_RANGE_ATTACK1			0"
	 ""
	 "	Interrupts"
	 "		COND_SEE_ENEMY"
	 "		COND_ENEMY_WENT_NULL"
	 "		COND_HEAVY_DAMAGE"
	 "		COND_NO_PRIMARY_AMMO"
	 "		COND_HEAR_DANGER"
	 "		COND_HEAR_MOVE_AWAY"
	 "		COND_COMBINE_NO_FIRE"
	 "		COND_WEAPON_BLOCKED_BY_FRIEND"
	 "		COND_COMBINE_WEAPON_SIGHT_OCCLUDED"
 )

 DEFINE_SCHEDULE
 (
 	SCHED_COMBINE_THROW_MANHACK,
 
 	"	Tasks"
 	"		TASK_SPEAK_SENTENCE					5"	// METROPOLICE_SENTENCE_DEPLOY_MANHACK
 	"		TASK_PLAY_SEQUENCE_FACE_ENEMY		ACTIVITY:ACT_METROPOLICE_THROW_MANHACK"
 	"	"
 	"	Interrupts"
 	"		COND_RECEIVED_ORDERS"
 )

 AI_END_CUSTOM_NPC()
