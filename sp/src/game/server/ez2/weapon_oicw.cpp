//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "npcevent.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#include "game.h"
#include "in_buttons.h"
#include "grenade_ar2.h"
#include "ai_memory.h"
#include "soundent.h"
#include "rumble_shared.h"
#include "gamestats.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar    sk_plr_dmg_smg1_grenade;	
#ifdef MAPBASE
extern ConVar    sk_npc_dmg_smg1_grenade;
#endif

class CWeaponOICW : public CHLSelectFireMachineGun
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CWeaponOICW, CHLSelectFireMachineGun );

	CWeaponOICW();

	DECLARE_SERVERCLASS();
	
	void	Precache( void );
	void	AddViewKick( void );
	void	SecondaryAttack( void );
	int		GetMinBurst() { return 2; }
	int		GetMaxBurst() { return 5; }

	bool	Reload( void );
	void	ItemPostFrame( void );

	float	GetFireRate( void ) { return m_bZoomed ? 0.15 : 0.1f; }
	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }
	int		WeaponRangeAttack2Condition( float flDot, float flDist );
	Activity	GetPrimaryAttackActivity( void );

	virtual const Vector& GetBulletSpread( void )
	{
		static const Vector cone = VECTOR_CONE_2DEGREES;
		static const Vector zoomedCone = VECTOR_CONE_1DEGREES;

		if (GetOwner() && GetOwner()->IsNPC())
		{
			static const Vector npcCone = VECTOR_CONE_3DEGREES;
			return npcCone;
		}

		return m_bZoomed ? zoomedCone : cone;
	}

	const WeaponProficiencyInfo_t *GetProficiencyValues();

	void FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir );
	void Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	DECLARE_ACTTABLE();

protected:

	Vector	m_vecTossVelocity;
	float	m_flNextGrenadeCheck;
	bool	m_bZoomed;
};

IMPLEMENT_SERVERCLASS_ST(CWeaponOICW, DT_WeaponOICW)
END_SEND_TABLE()

//LINK_ENTITY_TO_CLASS( weapon_ar1, CWeaponOICW );
LINK_ENTITY_TO_CLASS( weapon_oicw, CWeaponOICW );

// Don't need to precache this in every game
//PRECACHE_WEAPON_REGISTER( weapon_oicw );

BEGIN_DATADESC( CWeaponOICW )

	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_bZoomed, FIELD_BOOLEAN ),

END_DATADESC()

acttable_t	CWeaponOICW::m_acttable[] = 
{
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_AR1, true },
	
#if EXPANDED_HL2_UNUSED_WEAPON_ACTIVITIES
	// Optional new NPC activities
	// (these should fall back to AR2 animations when they don't exist on an NPC)
	{ ACT_RELOAD,					ACT_RELOAD_AR1,			true },
	{ ACT_IDLE,						ACT_IDLE_AR1,				true },
	{ ACT_IDLE_ANGRY,				ACT_IDLE_ANGRY_AR1,		true },

// Readiness activities (not aiming)
	{ ACT_IDLE_RELAXED,				ACT_IDLE_AR1_RELAXED,			false },//never aims
	{ ACT_IDLE_STIMULATED,			ACT_IDLE_AR1_STIMULATED,		false },
	{ ACT_IDLE_AGITATED,			ACT_IDLE_ANGRY_AR1,			false },//always aims

	{ ACT_WALK_RELAXED,				ACT_WALK_AR1_RELAXED,			false },//never aims
	{ ACT_WALK_STIMULATED,			ACT_WALK_AR1_STIMULATED,		false },
	{ ACT_WALK_AGITATED,			ACT_WALK_AIM_AR1,				false },//always aims

	{ ACT_RUN_RELAXED,				ACT_RUN_AR1_RELAXED,			false },//never aims
	{ ACT_RUN_STIMULATED,			ACT_RUN_AR1_STIMULATED,		false },
	{ ACT_RUN_AGITATED,				ACT_RUN_AIM_AR1,				false },//always aims

// Readiness activities (aiming)
	{ ACT_IDLE_AIM_RELAXED,			ACT_IDLE_AR1_RELAXED,			false },//never aims	
	{ ACT_IDLE_AIM_STIMULATED,		ACT_IDLE_AIM_AR1_STIMULATED,	false },
	{ ACT_IDLE_AIM_AGITATED,		ACT_IDLE_ANGRY_AR1,			false },//always aims

	{ ACT_WALK_AIM_RELAXED,			ACT_WALK_AR1_RELAXED,			false },//never aims
	{ ACT_WALK_AIM_STIMULATED,		ACT_WALK_AIM_AR1_STIMULATED,	false },
	{ ACT_WALK_AIM_AGITATED,		ACT_WALK_AIM_AR1,				false },//always aims

	{ ACT_RUN_AIM_RELAXED,			ACT_RUN_AR1_RELAXED,			false },//never aims
	{ ACT_RUN_AIM_STIMULATED,		ACT_RUN_AIM_AR1_STIMULATED,	false },
	{ ACT_RUN_AIM_AGITATED,			ACT_RUN_AIM_AR1,				false },//always aims
//End readiness activities

	{ ACT_WALK,						ACT_WALK_AR1,					true },
	{ ACT_WALK_AIM,					ACT_WALK_AIM_AR1,				true },
	{ ACT_WALK_CROUCH,				ACT_WALK_CROUCH_RIFLE,					true },
	{ ACT_WALK_CROUCH_AIM,			ACT_WALK_CROUCH_AIM_RIFLE,				true },
	{ ACT_RUN,						ACT_RUN_AR1,					true },
	{ ACT_RUN_AIM,					ACT_RUN_AIM_AR1,				true },
	{ ACT_RUN_CROUCH,				ACT_RUN_CROUCH_RIFLE,					true },
	{ ACT_RUN_CROUCH_AIM,			ACT_RUN_CROUCH_AIM_RIFLE,				true },
	{ ACT_GESTURE_RANGE_ATTACK1,	ACT_GESTURE_RANGE_ATTACK_AR1,	true },
	{ ACT_RANGE_ATTACK1_LOW,		ACT_RANGE_ATTACK_AR1_LOW,		true },
	{ ACT_COVER_LOW,				ACT_COVER_AR1_LOW,				false },
	{ ACT_RANGE_AIM_LOW,			ACT_RANGE_AIM_AR1_LOW,			false },
	{ ACT_RELOAD_LOW,				ACT_RELOAD_AR1_LOW,			false },
	{ ACT_GESTURE_RELOAD,			ACT_GESTURE_RELOAD_AR1,		true },

	{ ACT_ARM,						ACT_ARM_RIFLE,					false },
	{ ACT_DISARM,					ACT_DISARM_RIFLE,				false },

#if EXPANDED_HL2_COVER_ACTIVITIES
	{ ACT_RANGE_AIM_MED,			ACT_RANGE_AIM_AR1_MED,			false },
	{ ACT_RANGE_ATTACK1_MED,		ACT_RANGE_ATTACK_AR1_MED,		false },
#endif

#if EXPANDED_HL2DM_ACTIVITIES
	// HL2:DM activities (for third-person animations in SP)
	{ ACT_HL2MP_IDLE,                    ACT_HL2MP_IDLE_AR1,                    false },
	{ ACT_HL2MP_RUN,                    ACT_HL2MP_RUN_AR1,                    false },
	{ ACT_HL2MP_IDLE_CROUCH,            ACT_HL2MP_IDLE_CROUCH_AR1,            false },
	{ ACT_HL2MP_WALK_CROUCH,            ACT_HL2MP_WALK_CROUCH_AR1,            false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,    ACT_HL2MP_GESTURE_RANGE_ATTACK_AR1,    false },
	{ ACT_HL2MP_GESTURE_RELOAD,            ACT_HL2MP_GESTURE_RELOAD_AR1,        false },
	{ ACT_HL2MP_JUMP,                    ACT_HL2MP_JUMP_AR1,                    false },
	{ ACT_HL2MP_WALK,					ACT_HL2MP_WALK_AR1,					false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK2,	ACT_HL2MP_GESTURE_RANGE_ATTACK2_AR1,    false },
#endif
#endif
};

IMPLEMENT_ACTTABLE(CWeaponOICW);

//=========================================================
CWeaponOICW::CWeaponOICW( )
{
	m_fMinRange1		= 65; 
	m_fMaxRange1		= 2048;

	m_bAltFiresUnderwater = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::Precache( void )
{
	UTIL_PrecacheOther("grenade_ar2");

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir )
{
	WeaponSoundRealtime( SINGLE_NPC );

	CSoundEnt::InsertSound( SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE, pOperator->GetAbsOrigin(), SOUNDENT_VOLUME_MACHINEGUN, 0.2, pOperator, SOUNDENT_CHANNEL_WEAPON, pOperator->GetEnemy() );
	
	pOperator->FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_PRECALCULATED,
		MAX_TRACE_LENGTH, m_iPrimaryAmmoType, 2, entindex(), 0 );

	pOperator->DoMuzzleFlash();
	m_iClip1 = m_iClip1 - 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::Operator_ForceNPCFire( CBaseCombatCharacter *pOperator, bool bSecondary )
{
	// Ensure we have enough rounds in the clip
	m_iClip1++;

	Vector vecShootOrigin, vecShootDir;
	QAngle	angShootDir;
	GetAttachment( LookupAttachment( "muzzle" ), vecShootOrigin, angShootDir );
	AngleVectors( angShootDir, &vecShootDir );
	FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
}

#ifdef MAPBASE
float GetCurrentGravity( void );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_SMG1:
	case EVENT_WEAPON_AR1:
	case EVENT_WEAPON_AR2:
		{
			Vector vecShootOrigin, vecShootDir;
			QAngle angDiscard;

			// Support old style attachment point firing
			if ((pEvent->options == NULL) || (pEvent->options[0] == '\0') || (!pOperator->GetAttachment(pEvent->options, vecShootOrigin, angDiscard)))
			{
				vecShootOrigin = pOperator->Weapon_ShootPosition();
			}

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			ASSERT( npc != NULL );
			vecShootDir = npc->GetActualShootTrajectory( vecShootOrigin );

			FireNPCPrimaryAttack( pOperator, vecShootOrigin, vecShootDir );
		}
		break;

#ifdef MAPBASE
	case EVENT_WEAPON_AR2_ALTFIRE:
		{
			WeaponSound( WPN_DOUBLE );

			CAI_BaseNPC *npc = pOperator->MyNPCPointer();
			if (!npc)
				return;

			Vector vecShootOrigin, vecShootDir;
			vecShootOrigin = pOperator->Weapon_ShootPosition();
			vecShootDir = npc->GetShootEnemyDir( vecShootOrigin );

			Vector vecTarget = npc->GetAltFireTarget();
			Vector vecThrow;
			if (vecTarget == vec3_origin)
				AngleVectors( npc->EyeAngles(), &vecThrow ); // Not much else to do, unfortunately
			else
			{
				// Because this is happening right now, we can't "VecCheckThrow" and can only "VecDoThrow", you know what I mean?
				// ...Anyway, this borrows from that so we'll never return vec3_origin.
				//vecThrow = VecCheckThrow( this, vecShootOrigin, vecTarget, 600.0, 0.5 );

				vecThrow = (vecTarget - vecShootOrigin);

				// throw at a constant time
				float time = vecThrow.Length() / 600.0;
				vecThrow = vecThrow * (1.0 / time);

				// adjust upward toss to compensate for gravity loss
				vecThrow.z += (GetCurrentGravity() * 0.5) * time * 0.5;
			}

			CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecShootOrigin, vec3_angle, npc );
			pGrenade->SetAbsVelocity( vecThrow );
			pGrenade->SetLocalAngularVelocity( QAngle( 0, 400, 0 ) );
			pGrenade->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE ); 

			pGrenade->SetThrower( npc );

			pGrenade->SetGravity(0.5); // lower gravity since grenade is aerodynamic and engine doesn't know it.

			pGrenade->SetDamage(sk_npc_dmg_smg1_grenade.GetFloat());

			variant_t var;
			var.SetEntity(pGrenade);
			npc->FireNamedOutput("OnThrowGrenade", var, pGrenade, npc);
		}
		break;
#else
		/*//FIXME: Re-enable
		case EVENT_WEAPON_AR2_GRENADE:
		{
		CAI_BaseNPC *npc = pOperator->MyNPCPointer();

		Vector vecShootOrigin, vecShootDir;
		vecShootOrigin = pOperator->Weapon_ShootPosition();
		vecShootDir = npc->GetShootEnemyDir( vecShootOrigin );

		Vector vecThrow = m_vecTossVelocity;

		CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecShootOrigin, vec3_angle, npc );
		pGrenade->SetAbsVelocity( vecThrow );
		pGrenade->SetLocalAngularVelocity( QAngle( 0, 400, 0 ) );
		pGrenade->SetMoveType( MOVETYPE_FLYGRAVITY ); 
		pGrenade->m_hOwner			= npc;
		pGrenade->m_pMyWeaponAR2	= this;
		pGrenade->SetDamage(sk_npc_dmg_ar2_grenade.GetFloat());

		// FIXME: arrgg ,this is hard coded into the weapon???
		m_flNextGrenadeCheck = gpGlobals->curtime + 6;// wait six seconds before even looking again to see if a grenade can be thrown.

		m_iClip2--;
		}
		break;
		*/
#endif

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Activity
//-----------------------------------------------------------------------------
Activity CWeaponOICW::GetPrimaryAttackActivity( void )
{
	if ( m_nShotsFired < 2 )
		return ACT_VM_PRIMARYATTACK;

	if ( m_nShotsFired < 3 )
		return ACT_VM_RECOIL1;
	
	if ( m_nShotsFired < 4 )
		return ACT_VM_RECOIL2;

	return ACT_VM_RECOIL3;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( pOwner )
	{
		if ( pOwner->m_nButtons & IN_ZOOM )
		{
			m_bZoomed = true;
		}
		else
		{
			m_bZoomed = false;
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CWeaponOICW::Reload( void )
{
	bool fRet;
	float fCacheTime = m_flNextSecondaryAttack;

	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( fRet )
	{
		// Undo whatever the reload process has done to our secondary
		// attack timer. We allow you to interrupt reloading to fire
		// a grenade.
		m_flNextSecondaryAttack = GetOwner()->m_flNextAttack = fCacheTime;

		WeaponSound( RELOAD );
	}

	return fRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::AddViewKick( void )
{
	#define	EASY_DAMPEN			0.5f
	#define	MAX_VERTICAL_KICK	8.0f	//Degrees
	#define	SLIDE_LIMIT			1.5f	//Seconds
	
	#define	ZOOMED_EASY_DAMPEN			0.5f
	#define	ZOOMED_MAX_VERTICAL_KICK	4.0f	//Degrees
	#define	ZOOMED_SLIDE_LIMIT			3.5f	//Seconds

	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( pPlayer == NULL )
		return;

	if ( m_bZoomed )
	{
		DoMachineGunKick( pPlayer, ZOOMED_EASY_DAMPEN, ZOOMED_MAX_VERTICAL_KICK, m_fFireDuration, ZOOMED_SLIDE_LIMIT );
	}
	else
	{
		DoMachineGunKick( pPlayer, EASY_DAMPEN, MAX_VERTICAL_KICK, m_fFireDuration, SLIDE_LIMIT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponOICW::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	//Must have ammo
	if ( ( pPlayer->GetAmmoCount( m_iSecondaryAmmoType ) <= 0 ) || ( pPlayer->GetWaterLevel() == 3 ) )
	{
		SendWeaponAnim( ACT_VM_DRYFIRE );
		BaseClass::WeaponSound( EMPTY );
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5f;
		return;
	}

	if( m_bInReload )
		m_bInReload = false;

	// MUST call sound before removing a round from the clip of a CMachineGun
	BaseClass::WeaponSound( WPN_DOUBLE );

	pPlayer->RumbleEffect( RUMBLE_357, 0, RUMBLE_FLAGS_NONE );

	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector	vecThrow;
	// Don't autoaim on grenade tosses
	AngleVectors( pPlayer->EyeAngles() + pPlayer->GetPunchAngle(), &vecThrow );
	VectorScale( vecThrow, 1000.0f, vecThrow );
	
	//Create the grenade
	QAngle angles;
	VectorAngles( vecThrow, angles );
	CGrenadeAR2 *pGrenade = (CGrenadeAR2*)Create( "grenade_ar2", vecSrc, angles, pPlayer );
	pGrenade->SetAbsVelocity( vecThrow );

	pGrenade->SetLocalAngularVelocity( RandomAngle( -400, 400 ) );
	pGrenade->SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_BOUNCE ); 
	pGrenade->SetThrower( GetOwner() );
	pGrenade->SetDamage( sk_plr_dmg_smg1_grenade.GetFloat() );

	SendWeaponAnim( ACT_VM_SECONDARYATTACK );

	CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 1000, 0.2, GetOwner(), SOUNDENT_CHANNEL_WEAPON );

	// player "shoot" animation
#ifdef MAPBASE
	pPlayer->SetAnimation( PLAYER_ATTACK2 );
#else
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif

	// Decrease ammo
	pPlayer->RemoveAmmo( 1, m_iSecondaryAmmoType );

	// Can shoot again immediately
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5f;

	// Can blow up after a short delay (so have time to release mouse button)
	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;

	// Register a muzzleflash for the AI.
	pPlayer->SetMuzzleFlashTime( gpGlobals->curtime + 0.5 );	

	m_iSecondaryAttacks++;
	gamestats->Event_WeaponFired( pPlayer, false, GetClassname() );
}

#define	COMBINE_MIN_GRENADE_CLEAR_DIST 256

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CWeaponOICW::WeaponRangeAttack2Condition( float flDot, float flDist )
{
	CAI_BaseNPC *npcOwner = GetOwner()->MyNPCPointer();

	return COND_NONE;

/*
	// --------------------------------------------------------
	// Assume things haven't changed too much since last time
	// --------------------------------------------------------
	if (gpGlobals->curtime < m_flNextGrenadeCheck )
		return m_lastGrenadeCondition;
*/

	// -----------------------
	// If moving, don't check.
	// -----------------------
	if ( npcOwner->IsMoving())
		return COND_NONE;

	CBaseEntity *pEnemy = npcOwner->GetEnemy();

	if (!pEnemy)
		return COND_NONE;

	Vector vecEnemyLKP = npcOwner->GetEnemyLKP();
	if ( !( pEnemy->GetFlags() & FL_ONGROUND ) && pEnemy->GetWaterLevel() == 0 && vecEnemyLKP.z > (GetAbsOrigin().z + WorldAlignMaxs().z) )
	{
		//!!!BUGBUG - we should make this check movetype and make sure it isn't FLY? Players who jump a lot are unlikely to 
		// be grenaded.
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}
	
	// --------------------------------------
	//  Get target vector
	// --------------------------------------
	Vector vecTarget;
	if (random->RandomInt(0,1))
	{
		// magically know where they are
		vecTarget = pEnemy->WorldSpaceCenter();
	}
	else
	{
		// toss it to where you last saw them
		vecTarget = vecEnemyLKP;
	}
	// vecTarget = m_vecEnemyLKP + (pEnemy->BodyTarget( GetLocalOrigin() ) - pEnemy->GetLocalOrigin());
	// estimate position
	// vecTarget = vecTarget + pEnemy->m_vecVelocity * 2;


	if ( ( vecTarget - npcOwner->GetLocalOrigin() ).Length2D() <= COMBINE_MIN_GRENADE_CLEAR_DIST )
	{
		// crap, I don't want to blow myself up
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return (COND_NONE);
	}

	// ---------------------------------------------------------------------
	// Are any friendlies near the intended grenade impact area?
	// ---------------------------------------------------------------------
	CBaseEntity *pTarget = NULL;

	while ( ( pTarget = gEntList.FindEntityInSphere( pTarget, vecTarget, COMBINE_MIN_GRENADE_CLEAR_DIST ) ) != NULL )
	{
		//Check to see if the default relationship is hatred, and if so intensify that
		if ( npcOwner->IRelationType( pTarget ) == D_LI )
		{
			// crap, I might blow my own guy up. Don't throw a grenade and don't check again for a while.
			m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
			return (COND_WEAPON_BLOCKED_BY_FRIEND);
		}
	}

	// ---------------------------------------------------------------------
	// Check that throw is legal and clear
	// ---------------------------------------------------------------------
	// FIXME: speed is based on difficulty...

	Vector vecToss = VecCheckThrow( this, npcOwner->GetLocalOrigin() + Vector(0,0,60), vecTarget, 600.0, 0.5 );
	if ( vecToss != vec3_origin )
	{
		m_vecTossVelocity = vecToss;

		// don't check again for a while.
		// JAY: HL1 keeps checking - test?
		//m_flNextGrenadeCheck = gpGlobals->curtime;
		m_flNextGrenadeCheck = gpGlobals->curtime + 0.3; // 1/3 second.
		return COND_CAN_RANGE_ATTACK2;
	}
	else
	{
		// don't check again for a while.
		m_flNextGrenadeCheck = gpGlobals->curtime + 1; // one full second.
		return COND_WEAPON_SIGHT_OCCLUDED;
	}
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CWeaponOICW::GetProficiencyValues()
{
	static WeaponProficiencyInfo_t proficiencyTable[] =
	{
		{ 7.0,		0.75	},
		{ 5.00,		0.75	},
		{ 10.0/3.0,	0.85	},
		{ 5.0/3.0,	0.75	},
		{ 1.00,		1.0		},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(proficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return proficiencyTable;
}
