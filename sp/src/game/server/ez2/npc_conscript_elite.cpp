//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_conscript_elite.h"
#include "beam_shared.h"
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

//-----------------------------------------------------------------------------

// Since our laser material doesn't support shading out anymore, this has to be long enough that the player
// is unlikely to see it awkwardly end
#define GUN_LASER_LENGTH		5000
#define GUN_LASER_LERP_TIME		0.5

int	ACT_LASER_ENABLE;
int	ACT_LASER_DISABLE;
int	AE_CONSCRIPT_ENABLE_LASER;
int	AE_CONSCRIPT_DISABLE_LASER;

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_ConscriptElite )
	DEFINE_KEYFIELD( m_bLaserOn, FIELD_BOOLEAN, "LaserStartsOn" ),
	DEFINE_FIELD( m_bLaserTrackEnemy, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hGunLaser, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hGunLaserEnd, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hGunLaserHitTarget, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bLaserAimsAtEnemy, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLaserTargetTime, FIELD_TIME ),

	DEFINE_THINKFUNC( LaserThink ),

	DEFINE_CONSCRIPT_DATADESC()
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_conscript_elite, CNPC_ConscriptElite );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_ConscriptElite::CNPC_ConscriptElite()
{
	RemoveSpawnFlags( SF_COMBINE_COMMANDABLE );
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

	if ( GetActiveWeapon() )
	{
		AddLaserToGun( GetActiveWeapon() );
	}

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

	PrecacheMaterial( "effects/progenitor_redlaser1_elite.vmt" );

	PrecacheScriptSound( "NPC_ConscriptElite.LaserOn" );
	PrecacheScriptSound( "NPC_ConscriptElite.LaserOff" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::OnRestore()
{
	BaseClass::OnRestore();

	if ( m_hGunLaser && GetActiveWeapon() )
	{
		m_nGunLaserAttachment = GetActiveWeapon()->LookupAttachment( "laser" );
		if ( m_nGunLaserAttachment <= 0 )
		{
			m_nGunLaserAttachment = GetActiveWeapon()->LookupAttachment( "muzzle" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::AddLaserToGun( CBaseCombatWeapon *pWeapon )
{
	m_nGunLaserAttachment = pWeapon->LookupAttachment( "laser" );
	if ( m_nGunLaserAttachment <= 0 )
	{
		m_nGunLaserAttachment = pWeapon->LookupAttachment( "muzzle" );
	}
	
	//if ( m_nGunLaserAttachment <= 0 )
	//	return;

	// Muzzle Target - Where the laser ends
	m_hGunLaserEnd = CBaseEntity::CreateNoSpawn( "info_target", vec3_origin, vec3_angle, this );
	m_hGunLaserEnd->SetParent( pWeapon, m_nGunLaserAttachment );
	m_hGunLaserEnd->SetLocalOrigin( Vector( GUN_LASER_LENGTH, 0, 0 ) );
	m_hGunLaserEnd->SetLocalAngles( vec3_angle );

	m_hGunLaser = CBeam::BeamCreate( "effects/progenitor_redlaser1_elite.vmt", 2.0 );
	if ( m_hGunLaser != NULL )
	{
		m_hGunLaser->EntsInit( pWeapon, m_hGunLaserEnd );
		m_hGunLaser->SetStartAttachment( m_nGunLaserAttachment );
		m_hGunLaser->SetWidth( 2.0 );
		m_hGunLaser->SetBrightness( 150 );
		m_hGunLaser->SetColor( 255, 0, 0 );
		m_hGunLaser->RelinkBeam();
		m_hGunLaser->SetNoise( 0 );
		m_hGunLaser->SetBeamFlags( FBEAM_FADEOUT );

		m_hGunLaser->SetParent( pWeapon, m_nGunLaserAttachment );
		m_hGunLaser->SetLocalOrigin( vec3_origin );
		m_hGunLaser->SetLocalAngles( vec3_angle );

		if ( m_bLaserOn )
		{
			m_hGunLaser->RemoveEffects( EF_NODRAW );
		}
		else
		{
			m_hGunLaser->AddEffects( EF_NODRAW );
		}

		// In case this weapon supports lasers for players (e.g. deagles)
		variant_t var;
		var.SetBool( true );
		pWeapon->AcceptInput( "SetLaserEquipped", this, this, var, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::TurnOnLaser()
{
	m_bLaserOn = true;
	m_hGunLaser->RemoveEffects( EF_NODRAW );
	EmitSound( "NPC_ConscriptElite.LaserOn" );

	SetContextThink( &CNPC_ConscriptElite::LaserThink, gpGlobals->curtime, "ConscriptEliteLaserThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::TurnOffLaser()
{
	m_bLaserOn = false;
	m_hGunLaser->AddEffects( EF_NODRAW );
	EmitSound( "NPC_ConscriptElite.LaserOff" );

	SetContextThink( NULL, TICK_NEVER_THINK, "ConscriptEliteLaserThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	if ( m_bLaserOn )
	{
		set.AppendCriteria( "laser", "1" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	if ( m_hGunLaser )
	{
		m_hGunLaser->LiveForTime( 0.75f );
	}

	if ( m_hGunLaserEnd )
	{
		m_hGunLaserEnd->SetThink( &CBaseEntity::SUB_Remove );
		m_hGunLaserEnd->SetNextThink( gpGlobals->curtime + 0.75f );
	}
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
float CNPC_ConscriptElite::GetLaserDotToTarget( CBaseEntity *pTarget )
{
	Vector vecToTarget = (pTarget->GetAbsOrigin() - GetAbsOrigin());
	VectorNormalize( vecToTarget );

	Vector vecGunLaserForward;
	if ( GetActiveWeapon() )
	{
		Vector vecOrigin;
		GetActiveWeapon()->GetAttachment( m_nGunLaserAttachment, vecOrigin, &vecGunLaserForward );
	}

	float flDotLaser = DotProduct( vecToTarget, vecGunLaserForward );
	
	return flDotLaser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_ConscriptElite::GetEyeDotToTarget( CBaseEntity *pTarget )
{
	Vector vecToTarget = (pTarget->GetAbsOrigin() - GetAbsOrigin());
	VectorNormalize( vecToTarget );

	float flDotEye = DotProduct( vecToTarget, HeadDirection3D() ); // EyeDirection3D
	
	return flDotEye;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::TargetCrossingLaser( CBaseEntity *pTarget )
{
	if ( pTarget == m_hGunLaserHitTarget )
		return true;

	/*Vector vecToTarget = (pTarget->GetAbsOrigin() - GetAbsOrigin());
	Vector vecToGunLaser = (m_hGunLaserEnd->GetAbsOrigin() - GetAbsOrigin());

	// Allow some tolerance
	if (vecToTarget.LengthSqr() > (vecToGunLaser.LengthSqr() + Square(8.0)))
		return false;

	VectorNormalize( vecToTarget );
	
	float flDotEye = DotProduct( vecToTarget, HeadDirection3D() ); // EyeDirection3D

	Vector vecGunLaserForward;
	if ( GetActiveWeapon() )
	{
		Vector vecOrigin;
		GetActiveWeapon()->GetAttachment( m_nGunLaserAttachment, vecOrigin, &vecGunLaserForward );
	}

	float flDotLaser = DotProduct( vecToTarget, vecGunLaserForward );

	if (flDotLaser > 0.98 && flDotEye > 0.9)
		return true;*/
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For assassin cloaking
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType )
{
	if ( TargetCrossingLaser( pCloaker ) )
	{
		iCompromiseType = COMPROMISE_TYPE_LASER;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_ConscriptElite::ShouldAimLaserAtEnemy( CBaseEntity *pEnemy )
{
	if ( !pEnemy /*|| !m_bLaserTrackEnemy*/ )
		return false;
		
	if (GetActivity() != ACT_RANGE_ATTACK1 && GetActivity() != ACT_IDLE && GetActivity() != ACT_RUN_AIM && GetActivity() != ACT_WALK_AIM)
	{
		//printl("Not in right act: " + self.GetActivity())
		return false;
	}

	if (GetLaserDotToTarget(GetEnemy()) < 0.97)
	{
		//printl("Dot: " + GetLaserDotToTarget(self.GetEnemy()))
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::LaserThink( void )
{
	if ( !IsAlive() || !GetActiveWeapon() )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, "ConscriptEliteLaserThink" );
		return;
	}

	if ( m_bLaserOn && m_hGunLaser )
	{
		if ( ShouldAimLaserAtEnemy( GetEnemy() ) )
		{
			if (!m_bLaserAimsAtEnemy)
				m_flLaserTargetTime = gpGlobals->curtime;
			m_bLaserAimsAtEnemy = true;
		}
		else
		{
			if (m_bLaserAimsAtEnemy)
				m_flLaserTargetTime = gpGlobals->curtime;
			m_bLaserAimsAtEnemy = false;
		}
		
		Vector vecLaserEnd;
		Vector vecLaserStartOrigin;
		Vector vecLaserForward;
		
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->GetAttachment( m_nGunLaserAttachment, vecLaserStartOrigin, &vecLaserForward );
			
			if (m_bLaserAimsAtEnemy)
				vecLaserEnd = m_hGunLaserEnd->GetAbsOrigin(); // Use current
			else
				vecLaserEnd = vecLaserStartOrigin + (vecLaserForward * GUN_LASER_LENGTH);
		}

		// Lerp to where the laser should be
		if (m_flLaserTargetTime != -1)
		{
			float flLerpTime = gpGlobals->curtime - m_flLaserTargetTime;
			if (flLerpTime < GUN_LASER_LERP_TIME)
			{
				float flLerpPerc = (flLerpTime / GUN_LASER_LERP_TIME);
				if (m_bLaserAimsAtEnemy)
				{
					vecLaserEnd = vecLaserStartOrigin + (GetEnemy()->BodyTarget( vecLaserStartOrigin, true ) - vecLaserStartOrigin).Normalized() * GUN_LASER_LENGTH;
					vecLaserEnd += ((vecLaserStartOrigin + (vecLaserForward * GUN_LASER_LENGTH) - vecLaserEnd) * (1.0 - flLerpPerc));
				}
				else
					vecLaserEnd += ((m_hGunLaserEnd->GetAbsOrigin() - vecLaserEnd) * flLerpPerc);
					
				//printf("Lerping (%f)\n", lerpPerc)
				
				//debugoverlay.Cross3D(laserEnd, 3.0, 255, (255 * lerpPerc), 0, true, 4.0)
			}
			else
			{
				// Finished lerping
				m_flLaserTargetTime = -1;
				
				if (m_bLaserAimsAtEnemy)
					vecLaserEnd = vecLaserStartOrigin + (GetEnemy()->BodyTarget( vecLaserStartOrigin, true ) - vecLaserStartOrigin).Normalized() * GUN_LASER_LENGTH;
			}
		}
	
		trace_t tr;
		UTIL_TraceLine( vecLaserStartOrigin, vecLaserEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		
		//printf("Frac = %f\n", frac)
		//debugoverlay.Cross3D(laserEnd, 3.0, 255, (255 * frac), 0, true, 4.0)
		
		m_hGunLaserEnd->SetAbsOrigin( tr.endpos );
		m_hGunLaserHitTarget = tr.m_pEnt;
	}

	float flNextThink = 0.5f;
	if (m_flLaserTargetTime != -1 || IsMoving())
		flNextThink = TICK_INTERVAL;
	else if (HasCondition( COND_SEE_PLAYER ))
		flNextThink = 0.1f;
	else if (HasCondition( COND_IN_PVS ))
		flNextThink = 0.2f;

	SetContextThink( &CNPC_ConscriptElite::LaserThink, gpGlobals->curtime + flNextThink, "ConscriptEliteLaserThink" );
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
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	if ( IsCurSchedule( SCHED_COMBAT_FACE, false ) )
	{
		// Always aim while combat facing
		SetReadinessLevel( AIRL_AGITATED, false, false );
	}

	if ( !IsInAScript() )
	{
		// Determine desired laser state
		int iLaserLayer = -1;
		if ( !m_bLaserOn )
		{
			if ( GetState() != NPC_STATE_COMBAT )
			{
				if ( ( IsUsingStealthSenses() && ( HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_HEAR_PLAYER ) || HasCondition( COND_HEAR_WORLD )) )
					|| HasCondition( COND_LOST_ENEMY ) )
				{
					iLaserLayer = AddGesture( (Activity)ACT_LASER_ENABLE );
				}
			}
			else if ( !HasCondition( COND_SEE_ENEMY ) || !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
			{
				// We have an opportunity in combat
				iLaserLayer = AddGesture( (Activity)ACT_LASER_ENABLE );
			}
		}
		else
		{
			if ( GetState() != NPC_STATE_COMBAT )
			{
				if ( GetState() == NPC_STATE_IDLE && ( !IsUsingStealthSenses() || gpGlobals->curtime - GetStealthSenses()->GetLastSoundLocationUpdateTime() > 20.0f ) )
				{
					// Nobody left to look out for
					iLaserLayer = AddGesture( (Activity)ACT_LASER_DISABLE );
				}
			}
			/*
			else if ( HasCondition( COND_ENEMY_DEAD ) && GetEnemies()->NumEnemies() <= 1 )
			{
				// Turn off laser after killing the player because it looks cool
				if ( GetEnemy() && GetEnemy()->IsPlayer() )
					iLaserLayer = AddGesture( (Activity)ACT_LASER_DISABLE );
			}
			*/
		}

		if ( iLaserLayer != -1 )
		{
			GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + GetLayerDuration( iLaserLayer ) );
		}
	}
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
				}
			}
			break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_ConscriptElite::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	if ( m_bLaserOn && GetState() == NPC_STATE_ALERT )
	{
		// Make sure we're always aiming
		switch ( baseAct )
		{
			case ACT_IDLE:						baseAct = ACT_IDLE_ANGRY; break;
			case ACT_WALK:						baseAct = ACT_WALK_AIM; break;
			case ACT_RUN:						baseAct = ACT_RUN_AIM; break;
			case ACT_IDLE_RELAXED:
			case ACT_IDLE_STIMULATED:			baseAct = ACT_IDLE_AIM_STIMULATED; break;
			case ACT_WALK_RELAXED:
			case ACT_WALK_STIMULATED:			baseAct = ACT_WALK_AIM_STIMULATED; break;
			case ACT_RUN_RELAXED:
			case ACT_RUN_STIMULATED:			baseAct = ACT_RUN_AIM_STIMULATED; break;
		}
	}

	return BaseClass::Weapon_TranslateActivity( baseAct, pRequired );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_ConscriptElite::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->event == AE_CONSCRIPT_ENABLE_LASER)
	{
		TurnOnLaser();
		return;
	}
	else if (pEvent->event == AE_CONSCRIPT_DISABLE_LASER)
	{
		TurnOffLaser();
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
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

	DECLARE_ACTIVITY( ACT_LASER_ENABLE )
	DECLARE_ACTIVITY( ACT_LASER_DISABLE )
	DECLARE_ANIMEVENT( AE_CONSCRIPT_ENABLE_LASER )
	DECLARE_ANIMEVENT( AE_CONSCRIPT_DISABLE_LASER )

AI_END_CUSTOM_NPC()
