//=============================================================================//
//
// Purpose:		Progenitor energy shield functions
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_progenitor.h"
#include "particle_parse.h"
#include "IEffects.h"
#include "SpriteTrail.h"
#include "soundenvelope.h"
#include "ez2_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar	sk_progenitor_shield_fire_rate;
extern ConVar	sk_progenitor_shield_max_time;
extern ConVar	sk_progenitor_shield_max_cooldown;
extern ConVar	sk_progenitor_shield_throw;
extern ConVar	sk_progenitor_shield_throw_speed;
extern ConVar	sk_progenitor_shield_throw_dmg;
extern ConVar	sk_progenitor_shield_throw_radius;
extern ConVar	sk_progenitor_shield_slam_dmg;
extern ConVar	sk_progenitor_shield_slam_radius;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldActivateShield()
{
	if ( !GetEnemy() )
		return false;

	// Don't equip shield if we just used it
	if ( gpGlobals->curtime - m_flShieldDeactivateTime < sk_progenitor_shield_max_cooldown.GetFloat() )
		return false;

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
bool CNPC_Progenitor::CanThrowShield()
{
	if ( !sk_progenitor_shield_throw.GetBool() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldDeactivateShield( bool &bThrow )
{
	if ( m_flShieldDeactivateTime < gpGlobals->curtime )
		return true;

	if ( bThrow )
	{
		// If we aren't under attack and we'll be deactivating soon anyway, try throwing it
		if ( (gpGlobals->curtime - m_flLastDamageTime) > 2.0f
			&& (gpGlobals->curtime - m_flShieldDeactivateTime) < (sk_progenitor_shield_max_time.GetFloat() * 0.5f) )
			return true;

		if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_ENEMY, false ) && GetEnemy() /*&& (gpGlobals->curtime - m_flLastDamageTime) > 1.0f*/ )
		{
			// If we are about to get into cover and the enemy isn't right on us, then throw to cover our escape
			Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
			if ( vecToEnemy.LengthSqr() > Square( 200.0f ) )
				return true;
		}
	}

	// Whether or not we would be capable of throwing our shield if we were in position for it
	if ( !CanThrowShield() )
	{
		// We're trying to press on now
		if ( IsCurSchedule( SCHED_COMBINE_ASSAULT )
			|| IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK )
			|| IsCurSchedule( SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE ) )
			return true;

		// We're in cover
		if ( !FVisible( GetEnemyLKP() ) )
		{
			bThrow = false;
			return true;
		}
	}
	else if ( GetEnemy() )
	{
		// If we're not trying to press on, deactivate in cover if the enemy isn't close by
		if ( !IsCurSchedule( SCHED_COMBINE_ASSAULT )
			&& !IsCurSchedule( SCHED_COMBINE_PRESS_ATTACK )
			&& !IsCurSchedule( SCHED_COMBINE_ESTABLISH_LINE_OF_FIRE ) )
		{
			Vector vecEnemyLKP = GetEnemyLKP() + GetEnemy()->GetViewOffset();
			Vector vecToLKP = (vecEnemyLKP - GetAbsOrigin());
			if ( vecToLKP.LengthSqr() > 300.0f && !FVisible( vecEnemyLKP ) )
			{
				bThrow = false;
				return true;
			}
		}
	}

	return false;
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
		m_hShieldSprite = CSprite::SpriteCreate( PROGENITOR_SHIELD_SPRITE, GetAbsOrigin(), false );
		m_hShieldSprite->SetAttachment( this, nShieldAttach );
		m_hShieldSprite->SetTransparency( kRenderWorldGlow, 255, 192, 224, 255, kRenderFxHologram );
		m_hShieldSprite->SetBrightness( 255, 1.0f );
		m_hShieldSprite->SetScale( 0.3f, 0.5f );
		m_hShieldSprite->SetGlowProxySize( 4.0f );
		m_hShieldSprite->TurnOn();
	}

	for ( int i = 0; i < PROGENITOR_SHIELD_NUM_CORNERS; i++ )
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
void CNPC_Progenitor::RemoveShieldEffects( bool bOnShield )
{
	if ( m_pShieldSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		//controller.SoundChangePitch( m_pShieldSound, 90.0f, 0.2f );
		//controller.SoundFadeOut( m_pShieldSound, 0.25f, true );
		controller.SoundDestroy( m_pShieldSound );
		m_pShieldSound = NULL;
	}

	if ( m_hShieldSprite )
	{
		UTIL_Remove( m_hShieldSprite );
		m_hShieldSprite = NULL;
	}

	// Sometimes we want to detach the effects associated with the shield itself later
	if ( bOnShield )
	{
		if ( m_hShieldLight )
		{
			UTIL_Remove( m_hShieldLight );
			m_hShieldLight = NULL;
		}

		for ( int i = 0; i < PROGENITOR_SHIELD_NUM_CORNERS; i++ )
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

	}
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

	RemoveShieldEffects();

	// Update shot regulator for no shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();

	m_flShieldDeactivateTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnShieldSlam( CPropShield *pShield )
{
	RemoveShieldEffects();

	// Create a small shockwave
	CTakeDamageInfo expInfo( this, this, sk_progenitor_shield_slam_dmg.GetFloat(), DMG_SHOCK );
	RadiusDamage( expInfo, pShield->WorldSpaceCenter(), sk_progenitor_shield_slam_radius.GetFloat(), CLASS_NONE, this );

	DispatchParticleEffect( "temporalboss_electrical_arc_01", pShield->WorldSpaceCenter(), pShield->GetAbsAngles() );
	EmitSound( "Weapon_EnergyShield.Thrown_Explode" );

	g_pEffects->Sparks( pShield->WorldSpaceCenter(), 4, 3 );

	BaseClass::OnShieldSlam( pShield );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Progenitor::CreateShieldProjectile( CPropShield *pShield )
{
	RemoveShieldEffects( false );

	// Update shot regulator for no shield
	if ( GetActiveWeapon() )
		OnUpdateShotRegulator();

	CPropProgenitorThrownShield *pProjectile = (CPropProgenitorThrownShield*)CreateNoSpawn( "prop_progenitor_thrown_shield", pShield->GetAbsOrigin(), pShield->GetAbsAngles(), this );
	if ( !pProjectile )
	{
		if ( m_hShieldLight )
		{
			UTIL_Remove( m_hShieldLight );
			m_hShieldLight = NULL;
		}
	
		for ( int i = 0; i < PROGENITOR_SHIELD_NUM_CORNERS; i++ )
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

	pProjectile->m_flEnableGravityTime = (m_bInjured ? 1.0f : 2.0f);
	pProjectile->SetModelName( MAKE_STRING( PROGENITOR_SHIELD_THROWN_MODEL ) );
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
	}
	else
	{
		// Use our body angles if no enemy
		vecVelocity = BodyDirection3D();
	}

	QAngle angAngles;
	VectorAngles( vecVelocity, angAngles );

	// Since our angles are relative to the NPC, this doesn't resolve consistently,
	// but it's fine for this
	angAngles.x -= 90.0f;

	pProjectile->SetAbsAngles( angAngles );

	vecVelocity *= sk_progenitor_shield_throw_speed.GetFloat();

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

	for ( int i = 0; i < PROGENITOR_SHIELD_NUM_CORNERS; i++ )
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

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CPropProgenitorThrownShield )

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_THINKFUNC( EnableGravityThink ),

	DEFINE_FIELD( m_flNextDangerSoundTime, FIELD_TIME ),
	//DEFINE_FIELD( m_flEnableGravityTime, FIELD_FLOAT ), // We only use this on spawn

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
	m_flEnableGravityTime = 2.0f;
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
	SetNextThink( gpGlobals->curtime + m_flEnableGravityTime );
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
