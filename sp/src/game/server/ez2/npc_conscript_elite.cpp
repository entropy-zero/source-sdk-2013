//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_conscript_elite.h"
#include "prop_armor.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

ConVar sk_conscript_elite_health( "sk_conscript_elite_health", "95" );
ConVar sk_conscript_elite_kick( "sk_conscript_elite_kick", "15" );
ConVar sk_conscript_elite_head( "sk_conscript_elite_head", "0.5" );
ConVar sk_conscript_elite_helmet( "sk_conscript_elite_helmet", "0.5" );
ConVar sk_conscript_elite_helmet_protection( "sk_conscript_elite_helmet_protection", "10" );

ConVar sk_conscript_elite_default_proficiency( "sk_conscript_elite_default_proficiency", "2" );
ConVar sk_conscript_elite_laser_proficiency( "sk_conscript_elite_laser_proficiency", "3" );

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_ConscriptElite, DT_NPC_ConscriptElite )
	SendPropEHandle( SENDINFO( m_hGunLaser ) ),
	SendPropVector( SENDINFO( m_vecGunLaserDir ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_ConscriptElite )

	DEFINE_THINKFUNC( LaserThink ),

	// Inputs
	DEFINE_WEAPONLASER_DATADESC()
	DEFINE_CONSCRIPT_DATADESC()
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_conscript_elite, CNPC_ConscriptElite );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_ConscriptElite::CNPC_ConscriptElite()
{
	RemoveSpawnFlags( SF_COMBINE_COMMANDABLE );

	m_bInvestigateSounds = true;

	m_bLaserOn = false;
	m_bCanUseLaserDuringAI = true;
	m_bAlwaysAddLaser = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::Spawn()
{
	BaseClass::Spawn();

	m_iHealth = sk_conscript_elite_health.GetFloat();
	SetKickDamage( sk_conscript_elite_kick.GetFloat() );

	m_fIsElite = true;
	SetAlternateCapable( true );

	CapabilitiesAdd( bits_CAP_MOVE_JUMP );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::Precache()
{
	if( !GetModelName() )
	{
		SetModelName( MAKE_STRING( "models/humans/group_conscripts/male_elite.mdl" ) );
	}

	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "weapon_frag" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_ConscriptElite::GetSquadIDPrefix()
{
	const char *pszID = BaseClass::GetSquadIDPrefix();
	if ( pszID != NULL )
		return pszID;

	return "elite";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

extern ConVar sk_plr_dmg_buckshot;
extern ConVar sk_plr_num_shotgun_pellets;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( FStrEq( info.GetAmmoName(), "AR2" ) )
		return true;

	// 357 rounds are heavy damage
	if ( FStrEq( info.GetAmmoName(), "357" ) )
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

	// Heavy damage when hit directly by shotgun flechettes
	if ( info.GetDamageType() & (DMG_DISSOLVE | DMG_NEVERGIB) && info.GetInflictor() && FClassnameIs( info.GetInflictor(), "shotgun_flechette" ) )
		return true;

	return BaseClass::IsHeavyDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_ConscriptElite::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( !BaseClass::OnTakeDamage_Alive( info ) )
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( ptr->hitgroup == HITGROUP_GEAR )
	{
		// Emulate CPropConscriptHeadwear
		CTakeDamageInfo newInfo = info;

		float flPenetrationScale = CArmorProp::GetPenetrationScale( newInfo );
		if (flPenetrationScale == 0.0f)
			flPenetrationScale = 1.0f;

		newInfo.SubtractDamage( sk_conscript_elite_helmet_protection.GetFloat() / flPenetrationScale );

		g_pEffects->Sparks( ptr->endpos );

		ptr->hitgroup = HITGROUP_HEAD;

		// We need a flag to deal damage to the head hitgroup while also indicating that this came from the gas mask helmet.
		// DMG_DIRECT is normally used in combination with DMG_BURN to indicate burn damage from fires.
		// Every other instance of it is used in combination with DMG_BURN, so this is safe to use.
		newInfo.AddDamageType( DMG_DIRECT );

		BaseClass::TraceAttack( newInfo, vecDir, ptr, pAccumulator );
		return;
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::CanBeSneakAttacked( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( info.GetDamageType() & DMG_DIRECT )
	{
		// The helmet saved us
		return false;
	}

	return BaseClass::CanBeSneakAttacked( info, vecDir, ptr );
}

extern ConVar sk_npc_head;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_ConscriptElite::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
		case HITGROUP_HEAD:
			{
				if ( info.GetDamageType() & DMG_DIRECT )
				{
					// Hit the gas mask

					// Use base head damage to emulate CPropConscriptHeadwear, but also factor in our own value
					return sk_conscript_elite_helmet.GetFloat() * BaseClass::GetHitgroupDamageMultiplier( HITGROUP_HEAD, info );
				}
				else
				{
					// Hit another part of the mask

					float flScale = sk_conscript_elite_head.GetFloat();

					// Also use helmet armor penetration
					flScale *= CArmorProp::GetPenetrationScale( info );
					if ( flScale > 1.0f )
					{
						// Completely nullified
						flScale = 1.0f;
					}

					// Multiplied by sk_npc_head
					return BaseClass::GetHitgroupDamageMultiplier( HITGROUP_HEAD, info ) * flScale;
				}
			} break;
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::StartLaserThink()
{
	SetContextThink( &CNPC_ConscriptElite::LaserThink, gpGlobals->curtime, WEAPON_LASER_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::LaserThink( void )
{
	if ( !IsAlive() || !GetActiveWeapon() )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, WEAPON_LASER_THINK_CONTEXT );
		return;
	}

	float flNextThink = DoLaserThink();

	SetContextThink( &CNPC_ConscriptElite::LaserThink, gpGlobals->curtime + flNextThink, WEAPON_LASER_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_ConscriptElite::TranslateSchedule( int scheduleType )
{
	scheduleType = BaseClass::TranslateSchedule( scheduleType );

	switch( scheduleType )
	{
		case SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE:
			{
				AI_EnemyInfo_t *pMemory = GetEnemies()->Find( GetEnemy() );
				if ( pMemory && gpGlobals->curtime - pMemory->timeLastSeen > 5.0f )
				{
					// If it's been more than 5 seconds and we can see the last known position, run to it
					if ( FVisible( pMemory->vLastKnownLocation ) )
					{
						return SCHED_COMBINE_ASSAULT;
					}
					else if ( HasStrategySlot( SQUAD_SLOT_ATTACK1 ) )
					{
						// Be the hero and wander into the unknown
						return SCHED_COMBINE_ASSAULT;
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

	return scheduleType;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
WeaponProficiency_t CNPC_ConscriptElite::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	int nProficiency = sk_conscript_elite_default_proficiency.GetInt();

	if (m_bLaserOn)
		nProficiency = sk_conscript_elite_laser_proficiency.GetInt();

	return Clamp( (WeaponProficiency_t)nProficiency, WEAPON_PROFICIENCY_POOR, WEAPON_PROFICIENCY_PERFECT );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_conscript_elite, CNPC_ConscriptElite )
AI_END_CUSTOM_NPC()
