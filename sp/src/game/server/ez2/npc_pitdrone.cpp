//=============================================================================//
//
// Purpose: Biological soldiers. Predators that serve as grunts for Race X
//		alongside Shock Troopers.
//
//		They have similar behavior to other predators such as bullsquids,
//		but have a limited "clip" of spines that they must reload.
// Author: 1upD
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "npc_pitdrone.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_pitdrone_health( "sk_pitdrone_health", "100" );
ConVar sk_pitdrone_dmg_spit( "sk_pitdrone_dmg_spit", "15" );
ConVar sk_pitdrone_dmg_slash( "sk_pitdrone_dmg_slash", "15" );
ConVar sk_pitdrone_spit_speed( "sk_pitdrone_spit_speed", "512" );

LINK_ENTITY_TO_CLASS( npc_pitdrone, CNPC_PitDrone );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_PitDrone )

	DEFINE_KEYFIELD( m_iClip, FIELD_INTEGER, "clip" ),
	DEFINE_KEYFIELD( m_iAmmo, FIELD_INTEGER, "ammo" )

END_DATADESC()

#define MAX_PITDRONE_CLIP 6

//=========================================================
// Spawn
//=========================================================
void CNPC_PitDrone::Spawn()
{
	Precache( );

	SetModel( STRING( GetModelName() ) );

	SetHullType( HULL_MEDIUM );
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_GREEN );
	}
	
	SetRenderColor( 255, 255, 255, 255 );
	
	m_iMaxHealth		= sk_pitdrone_health.GetFloat();
	m_iHealth			= m_iMaxHealth;

	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	// For the purposes of melee attack conditions triggering attacks, we are treating the flying predator as though it has two melee attacks like the bullsquid.
	// In reality, the melee attack schedules will be translated to SCHED_RANGE_ATTACK_1.
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 | bits_CAP_SQUAD );
	CapabilitiesAdd( bits_CAP_MOVE_JUMP ); // THEY FLY NOW

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	BaseClass::Spawn();

	NPCInit();

	// If pit drones have the "long range" spawnflag set, let them snipe from any distance
	if (HasSpawnFlags( SF_NPC_LONG_RANGE ))
	{
		m_flDistTooFar = 1e9f;
	}
	else
	{
		m_flDistTooFar = 784;
	}

	UpdateAmmoBodyGroups();
}

CNPC_PitDrone::CNPC_PitDrone()
{
	m_iClip = MAX_PITDRONE_CLIP;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_PitDrone::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/pit_drone.mdl" ) );

		// Pit drones use skins instead of of models
		if (m_tEzVariant == EZ_VARIANT_XEN)
		{
			m_nSkin = 0;
		}
		else
		{
			m_nSkin = 1;
		}
	}

	PrecacheModel( STRING( GetModelName() ) );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}
	else
	{
		PrecacheParticleSystem( "blood_impact_yellow_01" );
	}

	PrecacheScriptSound( "NPC_PitDrone.Idle" );
	PrecacheScriptSound( "NPC_PitDrone.Pain" );
	PrecacheScriptSound( "NPC_PitDrone.Alert" );
	PrecacheScriptSound( "NPC_PitDrone.Death" );
	PrecacheScriptSound( "NPC_PitDrone.Attack1" );
	PrecacheScriptSound( "NPC_PitDrone.FoundEnemy" );
	PrecacheScriptSound( "NPC_PitDrone.Growl" );
	PrecacheScriptSound( "NPC_PitDrone.TailWhip");
	PrecacheScriptSound( "NPC_PitDrone.Bite" );
	PrecacheScriptSound( "NPC_PitDrone.Eat" );
	PrecacheScriptSound( "NPC_PitDrone.Explode" );

	UTIL_PrecacheOther( "crossbow_bolt" );
	PrecacheModel( "models/pitdrone_projectile.mdl" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_PitDrone::Classify( void )
{
	return CLASS_RACE_X; 
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CNPC_PitDrone::MaxYawSpeed( void )
{
	float flYS = 0;

	switch ( GetActivity() )
	{
	case	ACT_WALK:			flYS = 90;	break;
	case	ACT_RUN:			flYS = 90;	break;
	case	ACT_IDLE:			flYS = 90;	break;
	case	ACT_RANGE_ATTACK1:	flYS = 90;	break;
	default:
		flYS = 90;
		break;
	}

	return flYS;
}

//=========================================================
// Should this predator eat while enemies are present?
// Overridden by Pit Drones to check for ammo
//=========================================================
bool CNPC_PitDrone::ShouldEatInCombat()
{
	return ( m_iClip <= 0 || m_iMaxHealth > m_iHealth ) && HasCondition( COND_PREDATOR_SMELL_FOOD ) && ( !IsInSquad() || OccupyStrategySlot( SQUAD_SLOT_FEED ) );
}

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CNPC_PitDrone::RangeAttack1Conditions( float flDot, float flDist )
{
	// Do we have enough ammo?
	if ( m_iClip <= 0 )
	{
		return COND_NONE;
	}

	// Be sure not to use ranged attacks against potential food
	if ( IsPrey( GetEnemy() ))
	{
		// Don't spit at prey - monch it!
		return ( COND_NONE );
	}

	return(BaseClass::RangeAttack1Conditions( flDot, flDist ));
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_PitDrone::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case PREDATOR_AE_SPIT:
		{
			if ( GetEnemy() )
			{
				Vector vSpitPos;
				GetAttachment( "head", vSpitPos );
				
				Vector vecShootDir = GetShootEnemyDir( vSpitPos, false );
				
				QAngle angAiming;
				VectorAngles( vecShootDir, angAiming );

				CBaseCombatCharacter *pBolt = (CBaseCombatCharacter *)CreateEntityByName( "crossbow_bolt" );
				UTIL_SetOrigin( pBolt, vSpitPos );
				pBolt->SetAbsAngles( angAiming );
				pBolt->KeyValue( "ImpactEffect", "DroneBoltImpact" );
				pBolt->Spawn();
				pBolt->SetOwnerEntity( this );
				pBolt->SetDamage( GetProjectileDamge() );
				pBolt->SetModel( "models/pitdrone_projectile.mdl" );

				if ( this->GetWaterLevel() == 3 )
				{
					pBolt->SetAbsVelocity( vecShootDir * sk_pitdrone_spit_speed.GetFloat() * 1/2 );
				}
				else
				{
					pBolt->SetAbsVelocity( vecShootDir * sk_pitdrone_spit_speed.GetFloat() );
				}

				AttackSound();

				m_iClip--;

				// I'm hungry again!
				m_flHungryTime = gpGlobals->curtime;

				UpdateAmmoBodyGroups();
			}
		}
		break;
		case PREDATOR_AE_BITE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), GetBiteDamage(), DMG_SLASH | DMG_ALWAYSGIB );
			if (pHurt)
			{
				BiteSound(); // Only play the bite sound if we have a target

				Vector forward, up;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );
				pHurt->ApplyAbsVelocityImpulse( 100 * (up-forward) * GetModelScale() );
				pHurt->SetGroundEntity( NULL );
			}
		}
		break;

		case PREDATOR_AE_WHIP_SND:
		{
			EmitSound( "NPC_PitDrone.TailWhip" );
			break;
		}

		case PREDATOR_AE_TAILWHIP:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, Vector( -16, -16, -16 ), Vector( 16, 16, 16 ), GetWhipDamage(), DMG_SLASH | DMG_ALWAYSGIB );
			if (pHurt)
			{
				Vector right, up;
				AngleVectors( GetAbsAngles(), NULL, &right, &up );

				if (pHurt->GetFlags() & (FL_NPC | FL_CLIENT))
					pHurt->ViewPunch( QAngle( 20, 0, -20 ) );

				pHurt->ApplyAbsVelocityImpulse( 100 * (up+2*right) * GetModelScale() );
			}
		}
		break;

		case PREDATOR_AE_BLINK:
		{
			// Close eye. 
			// Even skins are eyes open, odd are closed
			m_nSkin = ((m_nSkin / 2) * 2) + 1;
		}
		break;

		case PREDATOR_AE_HOP:
		{
			float flGravity = GetCurrentGravity();

			// throw the squid up into the air on this frame.
			if (GetFlags() & FL_ONGROUND)
			{
				SetGroundEntity( NULL );
			}

			// jump 40 inches into the air
			Vector vecVel = GetAbsVelocity();
			vecVel.z += sqrt( flGravity * 2.0 * 40 );
			SetAbsVelocity( vecVel );
		}
		break;

		case AE_WPN_INCREMENTAMMO:
		{
			// If the clip is already full, don't do anything
			if ( m_iClip >= MAX_PITDRONE_CLIP )
			{
				break;
			}

			if ( m_iAmmo >= MAX_PITDRONE_CLIP )
			{
				m_iClip += MAX_PITDRONE_CLIP;
				m_iAmmo -= MAX_PITDRONE_CLIP;
			}
			else if ( m_iAmmo >= 0 )
			{
				m_iClip += m_iAmmo;
				m_iAmmo = 0;
			}
			UpdateAmmoBodyGroups();
		}
		break;
		default:
			BaseClass::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Overridden to handle reloading
//=========================================================
int CNPC_PitDrone::TranslateSchedule( int scheduleType )
{
	switch (scheduleType)
	{
	// If standing in place or running from the enemy, check if we should reload instead
	case SCHED_IDLE_STAND:
	case SCHED_ALERT_STAND:
		if ( m_iClip < MAX_PITDRONE_CLIP && m_iAmmo > 0 )
		{
			return SCHED_HIDE_AND_RELOAD;
		}
		break;
	case SCHED_CHASE_ENEMY_FAILED:
	case SCHED_CHASE_ENEMY:
		if ( m_iClip <= 0 && m_iAmmo > 0 )
		{
			return SCHED_HIDE_AND_RELOAD;
		}
		else if ( m_iClip > 0 && scheduleType == SCHED_CHASE_ENEMY )
		{
			return SCHED_ESTABLISH_LINE_OF_FIRE;
		}
		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//=========================================================
// Damage for projectile attack
//=========================================================
float CNPC_PitDrone::GetProjectileDamge()
{
	return sk_pitdrone_dmg_spit.GetFloat();
}

//=========================================================
// Damage for claw slash attack
//=========================================================
float CNPC_PitDrone::GetBiteDamage( void )
{
	return sk_pitdrone_dmg_slash.GetFloat();
}

//=========================================================
// Damage for "whip" claw attack
//=========================================================
float CNPC_PitDrone::GetWhipDamage( void )
{
	return sk_pitdrone_dmg_slash.GetFloat();
}

//=========================================================
// Method called when the pit drone feeds
// Overridden to add a clip to ammo
//=========================================================
void CNPC_PitDrone::OnFed()
{
	m_iAmmo += MAX_PITDRONE_CLIP;
	BaseClass::OnFed();
}

//=========================================================
// Change the body groups for the spines on the pit drone
//=========================================================
void CNPC_PitDrone::UpdateAmmoBodyGroups( void )
{
	m_nBody = MAX( MIN( 1 + MAX_PITDRONE_CLIP - m_iClip, 1 + MAX_PITDRONE_CLIP), 0 );
}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( NPC_PitDrone, CNPC_PitDrone )

AI_END_CUSTOM_NPC()
