//=============================================================================//
//
// Purpose:		An experimental pulse MG carried by the Progenitor.
//				(the prototype AR2 coughs awkwardly from the audience)
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "basecombatweapon.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "player.h"
#include "weapon_ar2_progenitor.h"
#include "gamestats.h"
#include "rumble_shared.h"
#include "world.h"
#include "te_effect_dispatch.h"
#include "beam_flags.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_plr_dmg_ar2_progenitor( "sk_plr_dmg_ar2_progenitor", "16", FCVAR_REPLICATED );
ConVar	sk_npc_dmg_ar2_progenitor( "sk_npc_dmg_ar2_progenitor", "9", FCVAR_REPLICATED );

ConVar	sk_weapon_ar2_progenitor_alt_fire_duration( "sk_weapon_ar2_progenitor_alt_fire_duration", "1.5" );
ConVar	sk_weapon_ar2_progenitor_alt_fire_speed( "sk_weapon_ar2_progenitor_alt_fire_speed", "750" );

BEGIN_DATADESC( CWeaponAR2Progenitor )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CWeaponAR2Progenitor, DT_WeaponAR2Progenitor )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_ar2_progenitor, CWeaponAR2Progenitor );
PRECACHE_WEAPON_REGISTER( weapon_ar2_progenitor );

CWeaponAR2Progenitor::CWeaponAR2Progenitor()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::Precache()
{
	BaseClass::Precache();

	PrecacheParticleSystem( "progenitor_ar2_secondaryflash" );
}

//-----------------------------------------------------------------------------
// Purpose: BREADMAN --- This overrides the primaryfire function to suit the mod - Breadman
// Input  : &info - 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::PrimaryAttack( void )
{
	if (CBasePlayer *pPlayer = ToBasePlayer(GetOwner()))
	{
		SendWeaponAnim( GetPrimaryAttackActivity() );
		WeaponSound( SINGLE );
		
		m_nShotsFired++;

		// Fire the bullets
		//if ( sk_ez2_super_proto_ar2.GetBool() )
		{
			FireBulletsInfo_t info;
			info.m_iShots = 1;
			info.m_vecSrc = pPlayer->Weapon_ShootPosition();
			info.m_vecDirShooting = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
			info.m_vecSpread = pPlayer->GetAttackSpread( this );
			info.m_flDistance = MAX_TRACE_LENGTH;
			info.m_iAmmoType = m_iPrimaryAmmoType;
			info.m_iTracerFreq = 2;

			info.m_flDamage = sk_plr_dmg_ar2_progenitor.GetFloat();
			info.m_iPlayerDamage = (int)info.m_flDamage;

			pPlayer->FireBullets( info );
		}
		
		pPlayer->DoMuzzleFlash();

		// Time we wait before allowing to throw another
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.09f;

		m_iPrimaryAttacks++;
		gamestats->Event_WeaponFired(pPlayer, false, GetClassname());

		m_iClip1 = m_iClip1 - 1;

		AddViewKick();

		BaseClass::ItemPostFrame();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::DelayedAttack( void )
{
	m_bShotDelayed = false;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

	// Deplete the clip completely
	SendWeaponAnim( ACT_VM_SECONDARYATTACK );
	m_flNextSecondaryAttack = pOwner->m_flNextAttack = gpGlobals->curtime + SequenceDuration();

	// Register a muzzleflash for the AI
	pOwner->DoMuzzleFlash();
	pOwner->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );
	
	WeaponSound( WPN_DOUBLE );

	pOwner->RumbleEffect(RUMBLE_SHOTGUN_DOUBLE, 0, RUMBLE_FLAG_RESTART );

	// Fire the bullets
	Vector vecSrc	 = pOwner->Weapon_ShootPosition( );
	Vector vecAiming = pOwner->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	Vector impactPoint = vecSrc + ( vecAiming * MAX_TRACE_LENGTH );
	
	for ( int i = 0; i < 3; i++ )
	{
		// For all but the first ball, simulate inaccuracy from player recoil
		Vector vecTempAiming = vecAiming;

		if ( i != 0 )
		{
			#define PROTO_AR2_CONE_MAX	0.13053f // 10 degrees
			#define PROTO_AR2_CONE_MIN	0.04362f // 5 degrees

			Vector vecRandom;
			vecRandom.Random( -PROTO_AR2_CONE_MAX, PROTO_AR2_CONE_MAX );

			for ( int i = 0; i < 3; i++ )
			{
				// Wrap values above min
				if ( vecRandom[i] > 0.0f && vecRandom[i] < PROTO_AR2_CONE_MIN)
				{
					vecRandom[i] = PROTO_AR2_CONE_MAX - vecRandom[i];
				}
				else if ( vecRandom[i] < 0.0f && vecRandom[i] > -PROTO_AR2_CONE_MIN)
				{
					vecRandom[i] = -PROTO_AR2_CONE_MAX - vecRandom[i];
				}
			}

			vecTempAiming += vecRandom;
		}

		// Fire the bullets
		Vector vecVelocity = vecTempAiming * sk_weapon_ar2_progenitor_alt_fire_speed.GetFloat();

		// Fire the combine ball
		QAngle angAiming;
		VectorAngles( vecTempAiming, angAiming );
		CGrenadeProgenitorEnergy::Shoot( pOwner, vecSrc, vecVelocity, angAiming,
			gpGlobals->curtime + sk_weapon_ar2_progenitor_alt_fire_duration.GetFloat() + RandomFloat(-0.125f,0.125f) );
	}

	// View effects
	color32 white = {255, 255, 255, 64};
	UTIL_ScreenFade( pOwner, white, 0.1, 0, FFADE_IN  );
	
	//Disorient the player
	QAngle angles = pOwner->GetLocalAngles();

	angles.x += random->RandomInt( -4, 4 );
	angles.y += random->RandomInt( -4, 4 );
	angles.z = 0;

	pOwner->SnapEyeAngles( angles );
	
	pOwner->ViewPunch( QAngle( random->RandomInt( -8, -12 ), random->RandomInt( 1, 2 ), 0 ) );

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 1.0f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: ENERGY BALL ATTACK
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::SecondaryAttack( void )
{
	if ( m_bShotDelayed )
		return;

	// Cannot fire underwater
	if ( GetOwner() && GetOwner()->GetWaterLevel() == 3 )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}

	m_bShotDelayed = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flDelayedFire = gpGlobals->curtime + 0.5f;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if( pPlayer )
	{
		pPlayer->RumbleEffect(RUMBLE_AR2_ALT_FIRE, 0, RUMBLE_FLAG_RESTART );
	}

	SendWeaponAnim( ACT_VM_FIDGET );
	WeaponSound( SPECIAL1 );

	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	// Decrease ammo - trying this down here.
	//Msg("\n DEDUCTING AR2 ORB \n");
	pOwner->RemoveAmmo(1, m_iSecondaryAmmoType);

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pOperator - 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	Vector vecShootOrigin, vecShootDir;

	CAI_BaseNPC *npc = pOperator->MyNPCPointer();
	ASSERT( npc != NULL );

	if ( bUseWeaponAngles )
	{
		QAngle	angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
		AngleVectors( angShootDir, &vecShootDir );
	}
	else 
	{
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );
	}

	WeaponSoundRealtime( SINGLE_NPC );

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );

	// Fire the bullets
	FireBulletsInfo_t info;
	info.m_iShots = 1;
	info.m_vecSrc = vecShootOrigin;
	info.m_vecDirShooting = vecShootDir;
	info.m_vecSpread = VECTOR_CONE_6DEGREES;
	info.m_flDistance = MAX_TRACE_LENGTH;
	info.m_iAmmoType = m_iPrimaryAmmoType;
	info.m_iTracerFreq = 2;

	info.m_flDamage = sk_npc_dmg_ar2_progenitor.GetFloat();
	info.m_iPlayerDamage = (int)info.m_flDamage;

	pOperator->FireBullets( info );

	// NOTENOTE: This is overriden on the client-side
	// pOperator->DoMuzzleFlash();

	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles )
{
	WeaponSound( WPN_DOUBLE );

	if ( !GetOwner() )
		return;
		
	CAI_BaseNPC *pNPC = GetOwner()->MyNPCPointer();
	if ( !pNPC )
		return;
	
	// Fire!
	Vector vecSrc;
	Vector vecAiming;

	if ( bUseWeaponAngles )
	{
		QAngle	angShootDir;
		GetAttachment( LookupAttachment( "muzzle" ), vecSrc, angShootDir );
		AngleVectors( angShootDir, &vecAiming );

		// Add some randomness to simulate player rumble
		//vecSrc += Vector( RandomFloat(-4, 4), RandomFloat(-4, 4), RandomFloat(-4, 4) );
	}
	else 
	{
		// Direct attachment is needed due to clearer trails
		//vecSrc = pNPC->Weapon_ShootPosition( );
		GetAttachment( LookupAttachment( "muzzle" ), vecSrc );

		// Add some randomness to simulate player rumble
		//vecSrc += Vector( RandomFloat(-4, 4), RandomFloat(-4, 4), RandomFloat(-4, 4) );
		
		Vector vecTarget;

		// It's shared across all NPCs now that it's available on more than just soldiers on more than just the AR2.
		vecTarget = pNPC->GetAltFireTarget();

		vecAiming = vecTarget - vecSrc;
		VectorNormalize( vecAiming );
	}

	// Need to do this here instead of the model since the worldmodel doesn't have an animation for this
	QAngle angAiming;
	VectorAngles( vecAiming, angAiming );
	DispatchParticleEffect( "progenitor_ar2_secondaryflash", vecSrc, angAiming, this );

	for ( int i = 0; i < 3; i++ )
	{
		// For all but the first ball, simulate inaccuracy from player recoil
		Vector vecTempAiming = vecAiming;

		if ( i != 0 )
		{
			#define PROTO_AR2_NPC_CONE_MAX	0.13053f // 10 degrees
			#define PROTO_AR2_NPC_CONE_MIN	0.04362f // 5 degrees

			Vector vecRandom;
			vecRandom.Random( -PROTO_AR2_NPC_CONE_MAX, PROTO_AR2_NPC_CONE_MAX );

			for ( int i = 0; i < 3; i++ )
			{
				// Wrap values above min
				if ( vecRandom[i] > 0.0f && vecRandom[i] < PROTO_AR2_NPC_CONE_MIN)
				{
					vecRandom[i] = PROTO_AR2_NPC_CONE_MAX - vecRandom[i];
				}
				else if ( vecRandom[i] < 0.0f && vecRandom[i] > -PROTO_AR2_NPC_CONE_MIN )
				{
					vecRandom[i] = -PROTO_AR2_NPC_CONE_MAX - vecRandom[i];
				}
			}

			vecTempAiming += vecRandom;
		}

		// Fire the bullets
		Vector vecVelocity = vecTempAiming * sk_weapon_ar2_progenitor_alt_fire_speed.GetFloat();

		// Fire the combine ball
		VectorAngles( vecTempAiming, angAiming );
		CBaseEntity *pBall = CGrenadeProgenitorEnergy::Shoot( pNPC, vecSrc, vecVelocity, angAiming, gpGlobals->curtime + sk_weapon_ar2_progenitor_alt_fire_duration.GetFloat() + RandomFloat(-0.125f,0.125f) );

		variant_t var;
		var.SetEntity(pBall);
		pNPC->FireNamedOutput("OnThrowGrenade", var, pBall, pNPC);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponAR2Progenitor::AddViewKick( void )
{
	#define	PROTO_EASY_DAMPEN			0.5f
	#define	PROTO_MAX_VERTICAL_KICK	10.0f	//Degrees - was 9.0
	#define	PROTO_SLIDE_LIMIT			2.0f	//Seconds - was 5.0
	
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if (!pPlayer)
		return;

	float flDuration = m_fFireDuration;

	if( g_pGameRules->GetAutoAimMode() == AUTOAIM_ON_CONSOLE )
	{
		// On the 360 (or in any configuration using the 360 aiming scheme), don't let the
		// AR2 progressive into the late, highly inaccurate stages of its kick. Just
		// spoof the time to make it look (to the kicking code) like we haven't been
		// firing for very long.
		flDuration = MIN( flDuration, 0.75f );
	}

	DoMachineGunKick( pPlayer, PROTO_EASY_DAMPEN, PROTO_MAX_VERTICAL_KICK, flDuration, PROTO_SLIDE_LIMIT );
}

//=============================================================================
// Progenitor Energy Grenade (tesla balls, displacer cannon-like)
//=============================================================================
ConVar    sk_dmg_energy_grenade_progenitor		( "sk_dmg_energy_grenade_progenitor","40");
ConVar    sk_dmg_energy_grenade_progenitor_shock	( "sk_dmg_energy_grenade_progenitor_shock","20");
ConVar	  sk_energy_grenade_progenitor_radius	( "sk_energy_grenade_progenitor_radius","225");

#define ENERGY_GRENADE_MAX_DANGER_RADIUS	300

BEGIN_DATADESC( CGrenadeProgenitorEnergy )

	DEFINE_FIELD( m_flNextShockTime, FIELD_TIME ),

	DEFINE_THINKFUNC( GrenadeEnergyThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( grenade_energy_progenitor, CGrenadeProgenitorEnergy );

void CGrenadeProgenitorEnergy::Spawn( void )
{
	BaseClass::Spawn();

	m_flDamage			= sk_dmg_energy_grenade_progenitor.GetFloat();
	m_DmgRadius		= sk_energy_grenade_progenitor_radius.GetFloat();

	// No gravity
	SetMoveType( MOVETYPE_FLY );

	m_flNextShockTime = gpGlobals->curtime + 0.25f;
}

void CGrenadeProgenitorEnergy::Precache( void )
{
	BaseClass::Precache();

	m_nBeamModelIndex = PrecacheModel( "sprites/laserbeam.vmt" );

	PrecacheScriptSound( "Weapon_AR2_Progenitor.GrenadeSpark" );
	PrecacheScriptSound( "Weapon_AR2_Progenitor.GrenadeExplode" );
	PrecacheScriptSound( "Weapon_AR2_Progenitor.GrenadeExplodeImpact" );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
CGrenadeProgenitorEnergy *CGrenadeProgenitorEnergy::Shoot( CBaseEntity* pOwner, const Vector &vStart, const Vector &vVelocity, QAngle &vShootAng, float flDetonateTime )
{
	CGrenadeProgenitorEnergy *pEnergy = (CGrenadeProgenitorEnergy *)CreateEntityByName( "grenade_energy_progenitor" );
	pEnergy->Spawn();
	
	UTIL_SetOrigin( pEnergy, vStart );
	pEnergy->SetAbsVelocity( vVelocity );
	pEnergy->SetLocalAngles( vShootAng );
	pEnergy->SetOwnerEntity( pOwner );

	pEnergy->SetThink ( &CGrenadeProgenitorEnergy::GrenadeEnergyThink );
	pEnergy->SetNextThink( gpGlobals->curtime + 0.1f );

	pEnergy->m_nRenderMode = kRenderTransAdd;
	pEnergy->SetRenderColor( 70, 130, 245, 200 ); // 155, 70, 245
	pEnergy->m_nRenderFX = kRenderFxNone;

	pEnergy->m_flDetonateTime = flDetonateTime;

	return pEnergy;
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CGrenadeProgenitorEnergy::Detonate(void)
{
	m_takedamage	= DAMAGE_NO;

	UTIL_Remove( m_pFragSprite );
	m_pFragSprite = NULL;

	UTIL_Remove( m_pFragTrail );
	m_pFragTrail = NULL;

	Vector vecForward = GetAbsVelocity();
	VectorNormalize(vecForward);

	trace_t		tr;
	UTIL_TraceLine ( GetAbsOrigin(), GetAbsOrigin() + 60 * vecForward, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	// Larger radius and more damage if we detonated immediately
	float flRadius = m_DmgRadius;
	float flDamage = m_flDamage;

	float flInvProgress = RemapValClamped( gpGlobals->curtime, m_flDetonateTime - sk_weapon_ar2_progenitor_alt_fire_duration.GetFloat(), m_flDetonateTime,
		1.0f, 0.0f );

	if ( flInvProgress > 0.0f )
	{
		flRadius *= ( 1.0f + ( 0.2f * flInvProgress ) );
		flDamage *= ( 1.0f + ( 0.5f * flInvProgress ) );
	}

	CEffectData data;

	data.m_vOrigin = GetAbsOrigin();
	data.m_flRadius = flRadius * 0.25f;
	data.m_vNormal = tr.plane.normal;

	if ( tr.fraction != 1.0f )
	{
		if ( ( tr.m_pEnt != GetWorldEntity() ) || ( tr.hitbox != 0 ) )
		{
			// non-world needs smaller decals
			if( tr.m_pEnt && !tr.m_pEnt->IsNPC() )
				UTIL_DecalTrace( &tr, "SmallScorch" );
		}
		else
			UTIL_DecalTrace( &tr, "Scorch" );
		
		DispatchEffect( "AR2Explosion", data );

		EmitSound( "Weapon_AR2_Progenitor.GrenadeExplodeImpact" );
	}
	else
		EmitSound( "Weapon_AR2_Progenitor.GrenadeExplode" );

	//DispatchEffect( "cball_explode", data );
	
	//Shockring
	extern int s_nExplosionTexture;
	CRecipientFilter filter;
	filter.AddRecipientsByPVS( GetAbsOrigin() );
	te->BeamRingPoint( filter, 0, GetAbsOrigin(),	//origin
		128,	//start radius
		384,		//end radius
		s_nExplosionTexture, //texture
		0,			//halo index
		0,			//start frame
		2,			//framerate
		0.25f,		//life
		48,			//width
		0,			//spread
		0,			//amplitude
		255,	//r
		255,	//g
		225,	//b
		64,		//a
		0,		//speed
		FBEAM_FADEOUT
		);

	UTIL_ScreenShake( GetAbsOrigin(), 25.0, 150.0, 1.0, 750, SHAKE_START );

	RadiusDamage ( CTakeDamageInfo( this, GetThrower(), flDamage, DMG_BLAST | DMG_DISSOLVE ), GetAbsOrigin(), flRadius, CLASS_NONE, NULL );

	CSoundEnt::InsertSound( SOUND_COMBAT | SOUND_CONTEXT_EXPLOSION, WorldSpaceCenter(), 100.0f * ( 1.0f + ( 0.5f * flInvProgress ) ), 0.25, this );

	UTIL_Remove( this );
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CGrenadeProgenitorEnergy::GrenadeEnergyThink()
{
	// If I just went solid and my velocity is zero, it means I'm resting on
	// the floor already when I went solid so blow up
	if (GetAbsVelocity().Length() == 0.0 ||
			GetGroundEntity() != NULL )
		{
			Detonate();
			return;
		}

	// Detonate if we've reached the time limit
	if ( m_flDetonateTime != 0.0f && m_flDetonateTime < gpGlobals->curtime )
	{
		Detonate();
		return;
	}

	// The old way of making danger sounds would scare the crap out of EVERYONE between you and where the grenade
	// was going to hit. The radius of the danger sound now 'blossoms' over the grenade's lifetime, making it seem
	// dangerous to a larger area downrange than it does from where it was fired.
	if( m_fDangerRadius <= ENERGY_GRENADE_MAX_DANGER_RADIUS )
	{
		m_fDangerRadius += ( ENERGY_GRENADE_MAX_DANGER_RADIUS * 0.1 );
	}

	CSoundEnt::InsertSound( SOUND_DANGER, GetAbsOrigin() + GetAbsVelocity() * 0.5, m_fDangerRadius, 0.2, this, SOUNDENT_CHANNEL_REPEATED_DANGER );

	if ( m_flNextShockTime < gpGlobals->curtime )
	{
		// Find anyone in the radius and shock them
		CTakeDamageInfo info( this, GetThrower(), sk_dmg_energy_grenade_progenitor_shock.GetFloat(), DMG_SHOCK );
		//RadiusDamage( info, GetAbsOrigin(), m_fDangerRadius, GetThrower() ? GetThrower()->Classify() : CLASS_NONE, this );

		float flDamageRadius = m_fDangerRadius * 0.75f;
		Vector vecDamageOrigin = GetAbsOrigin() + GetAbsVelocity() * 0.375;

		CRecipientFilter filter;
		filter.AddRecipientsByPVS( GetAbsOrigin() );

		CBaseEntity *pEntity = gEntList.FindEntityInSphere( NULL, vecDamageOrigin, flDamageRadius );
		while ( pEntity )
		{
			if ( pEntity->IsViewable() && pEntity != GetThrower() && pEntity->PassesDamageFilter( info ) && !( pEntity->GetFlags() & FL_NOTARGET ) )
			{
				// Trace to the entity's center
				trace_t tr;
				UTIL_TraceLine( GetAbsOrigin(), pEntity->WorldSpaceCenter(), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
				if ( tr.m_pEnt != pEntity )
				{
					// Try the eye position
					UTIL_TraceLine( GetAbsOrigin(), pEntity->EyePosition(), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
					if ( tr.m_pEnt != pEntity )
					{
						// Try just above the origin
						UTIL_TraceLine( GetAbsOrigin(), pEntity->GetAbsOrigin() + Vector(0,0,4), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
					}
				}

				if ( tr.m_pEnt == pEntity )
				{
					// We have a hit
					// Perform falloff from standard RadiusDamage
					CTakeDamageInfo adjustedInfo = info;

					Vector dir = tr.endpos - tr.startpos;
					float dist = VectorNormalize( dir );

					CalculateExplosiveDamageForce( &adjustedInfo, dir, tr.startpos );

					adjustedInfo.ScaleDamage( 1.0f - (dist / (ENERGY_GRENADE_MAX_DANGER_RADIUS * 0.75f)) );

					if ( adjustedInfo.GetDamage() > 1.0f )
					{
						ClearMultiDamage();
						pEntity->DispatchTraceAttack( adjustedInfo, dir, &tr );
						ApplyMultiDamage();

						pEntity->EmitSound( "Weapon_AR2_Progenitor.GrenadeSpark" );

						// Add a beam effect
						color32 clr = GetRenderColor();
						float flAmplitude = RemapValClamped( gpGlobals->curtime, m_flDetonateTime - sk_weapon_ar2_progenitor_alt_fire_duration.GetFloat(), m_flDetonateTime,
							1.0f, 8.0f );

						extern short g_sModelIndexLaser;
						te->BeamEntPoint( filter, 0.0f, entindex(), &GetAbsOrigin(), pEntity->entindex(), &tr.endpos, m_nBeamModelIndex,
							0, 0, 10, 0.2f, 1.0f, 1.0f, 0, flAmplitude, clr.r, clr.g, clr.b, clr.a, 4 );
					}
				}
			}

			pEntity = gEntList.FindEntityInSphere( pEntity, vecDamageOrigin, flDamageRadius );
		}

		m_flNextShockTime = gpGlobals->curtime + 0.2f;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}
