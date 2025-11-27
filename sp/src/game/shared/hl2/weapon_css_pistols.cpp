//=============================================================================//
//
// Purpose: CS:S weapons recreated from scratch in Source SDK 2013 for usage in a Half-Life 2 setting.
//
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "weapon_css_base.h"
#include "gamerules.h"
#include "in_buttons.h"
#include "gamestats.h"
#ifndef CLIENT_DLL
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "player.h"
#ifdef EZ2
#include "weapon_rpg.h"
#endif
#else
#ifdef EZ2
#include "iviewrender_beams.h"
#include "beamdraw.h"
#endif
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar pistol_use_new_accuracy;

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Glock18
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Glock18 : public CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Pistol>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Glock18, CBase_CSS_HL2_BurstableWeapon<CBase_CSS_HL2_Pistol> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Glock18(void);

	void	FinishBurst( void );

	Activity	GetPrimaryAttackActivity( void );

	virtual float GetViewKickBase() { return InBurst() ? 0.15f : 0.0f; }

	virtual float GetShotPenaltyTime() { return InBurst() ? 0.1f : 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}
	
	virtual int	GetMinBurst() { return InBurst() ? 3 : 1; }
	virtual int	GetMaxBurst() { return 3; }

	virtual float GetFireRate( void ) { return InBurst() ? 0.075f : 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Glock18, DT_Weapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_glock, CWeapon_CSS_HL2_Glock18 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_glock );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Glock18 )

	DEFINE_CSS_WEAPON_BURSTABLE_PREDICTDESC()

END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Glock18::CWeapon_CSS_HL2_Glock18( void )
{
	m_bCanUseBurstMode	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_Glock18::FinishBurst( void )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration(); // TODO: Proper cooldown?
	m_flSoonestPrimaryAttack = gpGlobals->curtime + SequenceDuration(); // TODO: Proper cooldown?
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeapon_CSS_HL2_Glock18::GetPrimaryAttackActivity( void )
{
	if (m_bInBurstMode)
		return ACT_VM_SECONDARYATTACK;

	return ACT_VM_PRIMARYATTACK;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_USP
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_USP : public CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Pistol>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_USP, CBase_CSS_HL2_SilencedWeapon<CBase_CSS_HL2_Pistol> );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_USP(void);

	virtual float GetViewKickBase() { return m_bSilenced ? 0.05f : 0.0f; }

	virtual float GetShotPenaltyTime() { return 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }

	// CS:S damage boost
	// Player damage: 7 -> 7.7 (8)
	// 
	// Silencer damage adjustment
	// Player damage: 7 -> 6.3 (6)
	// 
	// NPC damage: 3 -> 3
	virtual float GetDamageMultiplier() const { return IsSilenced() ? 0.9f : 1.1f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_USP, DT_Weapon_CSS_HL2_USP )

	DEFINE_CSS_WEAPON_SILENCED_NETWORK_TABLE()

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_usp, CWeapon_CSS_HL2_USP );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_usp );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_USP )

	DEFINE_CSS_WEAPON_SILENCED_DATADESC()

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_USP )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_USP::CWeapon_CSS_HL2_USP( void )
{
	m_bCanToggleSilencer = true;
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_P228
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_P228 : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_P228, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_P228(void);

	virtual float GetViewKickBase() { return 1.25f; }

	virtual float GetShotPenaltyTime() { return 0.3f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f;	}
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_P228, DT_Weapon_CSS_HL2_P228 )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_p228, CWeapon_CSS_HL2_P228 );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_p228 );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_P228 )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_P228 )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_P228::CWeapon_CSS_HL2_P228( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Deagle
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Deagle : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Deagle, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Deagle(void);

	virtual Activity ActivityOverride( Activity baseAct, bool *pRequired )
	{
		Activity translatedAct = BaseClass::ActivityOverride( baseAct, pRequired );

#if MAPBASE_VER_INT >= 7000
		switch (translatedAct)
		{
			case ACT_RANGE_ATTACK_PISTOL:
				return ACT_RANGE_ATTACK_REVOLVER;
			case ACT_RANGE_ATTACK_PISTOL_LOW:
				return ACT_RANGE_ATTACK_REVOLVER_LOW;
			case ACT_RANGE_ATTACK_PISTOL_MED:
				return ACT_RANGE_ATTACK_REVOLVER_MED;
		}
#endif

		return translatedAct;
	}

	virtual float GetViewKickBase() { return 5.5f; }

	virtual float GetShotPenaltyTime() { return 0.4f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_2DEGREES, VECTOR_CONE_8DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.225f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }

	// Slightly weaker than the .357
	// Player damage: 40 -> 36
	// NPC damage: 30 -> 27
	virtual float GetDamageMultiplier() const { return 0.9f; }
	virtual float GetNPCDamageMultiplier() const { return 0.9f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Deagle, DT_Weapon_CSS_HL2_Deagle )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_deagle, CWeapon_CSS_HL2_Deagle );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_deagle );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Deagle )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Deagle )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Deagle::CWeapon_CSS_HL2_Deagle( void )
{
}

#ifdef EZ2
#define	LASER_BEAM_SPRITE			"effects/laser1.vmt"

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_LaserDeagle
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_LaserDeagle : public CWeapon_CSS_HL2_Deagle
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_LaserDeagle, CWeapon_CSS_HL2_Deagle );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_LaserDeagle( void );

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped( GetAccuracyPenalty(), 0.0f, GetMaxAccuracyPenalty(), 0.0f, 1.0f );

		// More accurate with laser, less accurate without
		if ( m_bLaserPrimed )
		{
			VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_5DEGREES, ramp, cone );
		}
		else
		{
			VectorLerp( VECTOR_CONE_3DEGREES, VECTOR_CONE_10DEGREES, ramp, cone );
		}

		return cone;
	}

#ifndef CLIENT_DLL
	virtual float GetFireRate( void ) { return m_bLaserOn ? 1.0f : 0.5f; }
	virtual float GetRefireRate() { return m_bLaserOn ? 0.7f : 0.225f; }
#else
	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.225f; }
#endif
	virtual float GetDryRefireRate( void ) { return 0.25f; }

	// Slightly weaker than the .357
	// Player damage: 40 -> 36
	// NPC damage: 30 -> 27
	virtual float GetDamageMultiplier() const { return 0.9f; }
	virtual float GetNPCDamageMultiplier() const { return 0.9f; }

	void	UpdateOnRemove( void );
	void	Precache();
#ifdef CLIENT_DLL
	void	OnDataChanged( DataUpdateType_t updateType );
	void	TurnLaserBeamOn();
	void	TurnLaserBeamOff();

	Beam_t				*m_pLaserBeam;
	bool				m_bOldLaserPrimed;
#else
	void	ItemPostFrame( void );
	bool	Deploy( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void	Drop( const Vector &vecVelocity );
	void	SecondaryAttack( void );

	bool	IsLaserOn() const { return m_bLaserOn; }
	void	TurnLaserOn();
	void	TurnLaserOff();
	bool	CanToggleLaser() const { return m_bCanToggleLaser; }
	void	InputSetLaserEquipped( inputdata_t &inputdata );

	float	GetAccuracyPenaltyAmt() const;
	float	GetMaxAccuracyPenalty() const;
	float	GetAccuracyPenaltyViewkick();

	bool	ShouldDisplayAltFireHUDHint()
	{
		return BaseClass::ShouldDisplayAltFireHUDHint() && CanToggleLaser();
	}

	bool				m_bCanToggleLaser;
	bool				m_bLaserOn;
	EHANDLE				m_hLaserDot;
#endif
	CNetworkVar( bool,	m_bLaserPrimed );		// Whether or not the dot is actually drawing (turns off during reload, etc.)
#endif
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_LaserDeagle, DT_Weapon_CSS_HL2_LaserDeagle )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bLaserPrimed ) ),
#else
	SendPropBool( SENDINFO( m_bLaserPrimed ) ),
#endif

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_deagle_laser, CWeapon_CSS_HL2_LaserDeagle );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_deagle_laser );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_LaserDeagle )

#ifndef CLIENT_DLL
	DEFINE_KEYFIELD( m_bCanToggleLaser, FIELD_BOOLEAN, "CanToggleLaser" ),
	DEFINE_FIELD( m_bLaserOn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hLaserDot, FIELD_EHANDLE ),
	//DEFINE_FIELD( m_bLaserPrimed, FIELD_BOOLEAN ),	// Not necessary

	//DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetLaserEquipped", InputSetLaserEquipped ),
#endif

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_LaserDeagle )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_LaserDeagle::CWeapon_CSS_HL2_LaserDeagle( void )
{
#ifdef CLIENT_DLL
	m_pLaserBeam = NULL;
#else
	m_bCanToggleLaser = true;
	m_bLaserOn = false;
	m_bLaserPrimed = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::UpdateOnRemove( void )
{
#ifdef CLIENT_DLL
	if ( m_pLaserBeam )
	{
		m_pLaserBeam->flags = 0;
		m_pLaserBeam->die = gpGlobals->curtime - 1;
		m_pLaserBeam = NULL;
	}
#else
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#endif
	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::Precache()
{
	BaseClass::Precache();

	PrecacheModel( LASER_BEAM_SPRITE );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	
	if ( m_bLaserPrimed != m_bOldLaserPrimed )
	{
		if ( m_bLaserPrimed )
		{
			TurnLaserBeamOn();
		}
		else
		{
			TurnLaserBeamOff();
		}

		m_bOldLaserPrimed = m_bLaserPrimed;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::TurnLaserBeamOn( void )
{
	if ( !m_pLaserBeam )
	{
		if ( !GetOwner() || !GetOwner()->IsPlayer() )
			return;

		C_BaseViewModel *pVM = ToBasePlayer( GetOwner() )->GetViewModel( m_nViewModelIndex, false );
		if ( !pVM )
			return;

		SetViewModel();

		BeamInfo_t beamInfo;

		beamInfo.m_pStartEnt = pVM;
		beamInfo.m_nStartAttachment = pVM->LookupAttachment( "laser" );
		beamInfo.m_pEndEnt = pVM;
		beamInfo.m_nEndAttachment = pVM->LookupAttachment( "laser_end" );
		beamInfo.m_nType = TE_BEAMPOINTS;
		beamInfo.m_vecStart = vec3_origin;
		beamInfo.m_vecEnd = vec3_origin;

		beamInfo.m_pszModelName = LASER_BEAM_SPRITE;

		beamInfo.m_flHaloScale = 0.0f;
		beamInfo.m_flLife = 0.0f;
		beamInfo.m_flWidth = 1.0f;
		beamInfo.m_flEndWidth = 1.0f;

		beamInfo.m_flFadeLength = 0.0f;
		beamInfo.m_flAmplitude = 0.0f;
		beamInfo.m_flBrightness = 160.0;
		beamInfo.m_flSpeed = 0.0f;
		beamInfo.m_nStartFrame = 0.0;
		beamInfo.m_flFrameRate = 30.0;
		beamInfo.m_flRed = 255.0;
		beamInfo.m_flGreen = 0.0;
		beamInfo.m_flBlue = 0.0;
		beamInfo.m_nSegments = 8;
		beamInfo.m_bRenderable = true;
		beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_SHADEOUT;

		m_pLaserBeam = beams->CreateBeamEntPoint( beamInfo );
	}

	if ( m_pLaserBeam )
	{
		m_pLaserBeam->brightness = 160.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::TurnLaserBeamOff( void )
{
	if ( m_pLaserBeam )
	{
		m_pLaserBeam->brightness = 0.0f;
	}
}
#else
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();
	
	if ( m_bLaserOn && m_hLaserDot && GetOwner() && GetOwner()->IsPlayer() )
	{
		// Turn off the laser when in a non-idle activity
		bool bOldLaserPrimed = m_bLaserPrimed;
		switch ( GetActivity() )
		{
			case ACT_VM_IDLE:
			case ACT_VM_PRIMARYATTACK:
			case ACT_VM_RECOIL1:
			case ACT_VM_RECOIL2:
			case ACT_VM_RECOIL3:
			case ACT_VM_DRAW:
				m_bLaserPrimed = true;
				break;
			default:
				m_bLaserPrimed = false;
				break;
		}

		if ( m_bLaserPrimed != bOldLaserPrimed )
		{
			EnableLaserDot( m_hLaserDot, m_bLaserPrimed );

			if ( m_bLaserPrimed )
				WeaponSound( SPECIAL2 );
		}

		if ( m_bLaserPrimed )
		{
			CBaseCombatCharacter *pOwner = GetOwner();
			trace_t tr;

			Vector vecForward;
			ToBasePlayer( pOwner )->EyeVectors( &vecForward );

			if ( GetAccuracyPenalty() > 0.0f )
			{
				CBaseViewModel *pVM = ToBasePlayer( GetOwner() )->GetViewModel( m_nViewModelIndex, false );
				if ( pVM )
				{
					SetViewModel();

					Vector vecOrigin;
					int nLaserAttach = pVM->LookupAttachment( "laser" );
					pVM->GetAttachment( nLaserAttach, vecOrigin, &vecForward );
				}
				else
				{
					// Old way
					// Start with CalcView() so that we have the view punch
					Vector vecOrigin;
					QAngle angAngles;
					float zNear, zFar, fov;
					ToBasePlayer( pOwner )->CalcView( vecOrigin, angAngles, zNear, zFar, fov );
					AngleVectors( angAngles, &vecForward );
				}
				
				// Lerping from VECTOR_CONE_1DEGREES to VECTOR_CONE_10DEGREES
				// Mimics GetBulletSpread()
				float ramp = RemapValClamped( GetAccuracyPenalty(), 0.0f, GetMaxAccuracyPenalty(), 0.0f, 1.0f );
				float noise = Lerp( ramp, 0.00873, 0.08716 ) * 0.5f;

				vecForward.x += RandomGaussianFloat( 0.0f, noise );
				vecForward.y += RandomGaussianFloat( 0.0f, noise );
				vecForward.z += RandomGaussianFloat( 0.0f, noise );

				noise *= 1.0f - Clamp( gpGlobals->curtime - GetLastAttackTime(), 0.0f, 1.0f );

				if ( !pVM )
				{
					float viewKick = (cos(GetViewKickBase()) * RemapValClamped( noise, 0.00873, 0.08716, 0.0f, 1.0f ));
					vecForward.z += viewKick;
					vecForward.y += (viewKick * 0.15f);
				}

				// Also account for the previous position
				Vector vecCurDir = m_hLaserDot->GetAbsOrigin() - pOwner->Weapon_ShootPosition();
				VectorNormalize( vecCurDir );
				vecForward = VectorLerp( vecCurDir, vecForward, 0.5f );
			}

			UTIL_TraceLine( pOwner->Weapon_ShootPosition(), pOwner->Weapon_ShootPosition() + (vecForward * MAX_TRACE_LENGTH), MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );

			m_hLaserDot->SetAbsOrigin( tr.endpos );

			if ( tr.DidHitNonWorldEntity() && tr.m_pEnt && tr.m_pEnt->m_takedamage )
			{
				SetLaserDotTarget( m_hLaserDot, tr.m_pEnt );
			}
			else
			{
				SetLaserDotTarget( m_hLaserDot, NULL );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeapon_CSS_HL2_LaserDeagle::Deploy( void )
{
	if ( !BaseClass::Deploy() )
		return false;

	if ( m_bLaserOn )
	{
		// Create the laser and have it ready to be primed in ItemPostFrame()
		if ( !m_hLaserDot )
		{
			m_hLaserDot = CreateLaserDot( GetAbsOrigin(), GetOwner(), true );
			EnableLaserDot( m_hLaserDot, false );
			m_bLaserPrimed = false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CWeapon_CSS_HL2_LaserDeagle::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !BaseClass::Holster( pSwitchingTo ) )
		return false;
	
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}

	m_bLaserPrimed = false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::Drop( const Vector &vecVelocity )
{
	if ( m_hLaserDot )
	{
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}

	m_bLaserPrimed = false;

	BaseClass::Drop( vecVelocity );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CWeapon_CSS_HL2_LaserDeagle::GetAccuracyPenaltyAmt( void ) const
{
	return m_bLaserOn ? 0.5f : 1.5f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CWeapon_CSS_HL2_LaserDeagle::GetMaxAccuracyPenalty( void ) const
{
	return m_bLaserOn ? 1.75f : 3.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CWeapon_CSS_HL2_LaserDeagle::GetAccuracyPenaltyViewkick( void )
{
	// Make viewkick less dramatic
	return RemapVal( GetAccuracyPenalty(), 0.0f, 3.0f, 0.0f, 2.0f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::SecondaryAttack( void )
{
	BaseClass::SecondaryAttack();

	if ( CanToggleLaser() && m_flNextSecondaryAttack <= gpGlobals->curtime )
	{
		m_bLaserOn ? TurnLaserOff() : TurnLaserOn();
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.75f;

		if ( m_flSoonestPrimaryAttack < gpGlobals->curtime + 0.1f )
			m_flSoonestPrimaryAttack = gpGlobals->curtime + 0.1f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::TurnLaserOn( void )
{
	if ( !GetOwner() || !GetOwner()->IsPlayer() )
		return;

	if ( !m_hLaserDot )
	{
		m_hLaserDot = CreateLaserDot( GetAbsOrigin(), GetOwner(), true );
	}

	if ( m_hLaserDot )
	{
		//EnableLaserDot( m_hLaserDot, true );
		m_bLaserOn = true;
		WeaponSound( SPECIAL2 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_LaserDeagle::TurnLaserOff( void )
{
	if ( m_hLaserDot )
	{
		EnableLaserDot( m_hLaserDot, false );
		WeaponSound( SPECIAL3 );
	}

	m_bLaserOn = false;
	m_bLaserPrimed = false;
}
#endif

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_FiveSeveN
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_FiveSeveN : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_FiveSeveN, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_FiveSeveN(void);

	virtual float GetViewKickBase() { return 1.5f; }

	virtual float GetShotPenaltyTime() { return 0.4f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_3DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_5DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_FiveSeveN, DT_Weapon_CSS_HL2_FiveSeveN )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_fiveseven, CWeapon_CSS_HL2_FiveSeveN );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_fiveseven );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_FiveSeveN )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_FiveSeveN )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_FiveSeveN::CWeapon_CSS_HL2_FiveSeveN( void )
{
}

#define DUAL_BERETTAS_DROPPED_MODEL "models/weapons/w_pist_elite_dropped.mdl"

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_DualBerettas
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_DualBerettas : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_DualBerettas, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_DualBerettas(void);

	void	PrimaryAttack();
	Activity	GetPrimaryAttackActivity( void );

	virtual float GetViewKickBase() { return 0.5f; }

	virtual float GetShotPenaltyTime() { return 0.2f; }

	virtual const Vector& GetBulletSpread( void )
	{		
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if ( GetOwner() && GetOwner()->IsNPC() )
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped(	GetAccuracyPenalty(), 
										0.0f, 
										1.5f, 
										0.0f, 
										1.0f ); 

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_2DEGREES, VECTOR_CONE_7DEGREES, ramp, cone );

		return cone;
	}

#if MAPBASE_VER_INT < 7000
#ifndef CLIENT_DLL
	// HACKHACK: The dropped model needs to be set as the main model when the weapon drops (and when it spawns), so use this unsaved boolean
	// with FallInit() to make it use the dropped model instead
	bool m_bInFallInit;

	void FallInit()
	{
		m_bInFallInit = true;
		BaseClass::FallInit();
		m_bInFallInit = false;
	}
#endif

	void Precache()
	{
		BaseClass::Precache();

		m_iDroppedModelIndex = CBaseEntity::PrecacheModel( DUAL_BERETTAS_DROPPED_MODEL );
	}

	const char *GetWorldModel( void ) const
	{
#ifdef CLIENT_DLL
		return GetOwner() == NULL ? DUAL_BERETTAS_DROPPED_MODEL : BaseClass::GetWorldModel();
#else
		return (GetOwner() == NULL || m_bInFallInit) ? DUAL_BERETTAS_DROPPED_MODEL : BaseClass::GetWorldModel();
#endif
	}

#ifdef CLIENT_DLL
	int GetWorldModelIndex( void )
	{
		return GetOwner() == NULL ? m_iDroppedModelIndex : BaseClass::GetWorldModelIndex();
	}
#endif
#endif

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.12f; }
	virtual float GetDryRefireRate( void ) { return 0.25f; }

	// Tries to replicate CS:S's boosted damage without going crazy
	// Player damage: 5 -> 7
	// NPC damage: 3 -> 3
	virtual float GetDamageMultiplier() const { return 1.4f; }
	virtual float GetNPCDamageMultiplier() const { return 1.0f; }

private:
	CNetworkVar( bool, m_bGunMode );
#if MAPBASE_VER_INT < 7000
	CNetworkVar( int, m_iDroppedModelIndex ); // Not saved
#endif
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_DualBerettas, DT_Weapon_CSS_HL2_DualBerettas )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bGunMode ) ),
#if MAPBASE_VER_INT < 7000
	RecvPropInt( RECVINFO( m_iDroppedModelIndex ) ),
#endif
#else
	SendPropBool( SENDINFO( m_bGunMode ) ),
#if MAPBASE_VER_INT < 7000
	SendPropModelIndex( SENDINFO( m_iDroppedModelIndex ) ),
#endif
#endif

END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_elite, CWeapon_CSS_HL2_DualBerettas );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_elite );
#endif

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_DualBerettas )

	DEFINE_PRED_FIELD( m_bGunMode, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_DualBerettas )

	DEFINE_FIELD( m_bGunMode, FIELD_BOOLEAN ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_DualBerettas::CWeapon_CSS_HL2_DualBerettas( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_DualBerettas::PrimaryAttack( void )
{
	BaseClass::PrimaryAttack();
	m_bGunMode = !m_bGunMode;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
Activity CWeapon_CSS_HL2_DualBerettas::GetPrimaryAttackActivity( void )
{
	if (m_bGunMode)
		return ACT_VM_SECONDARYATTACK;

	return ACT_VM_PRIMARYATTACK;
}

#ifdef EZ
//-----------------------------------------------------------------------------
// CWeapon_Arbeit_Pistol
//-----------------------------------------------------------------------------
class CWeapon_Arbeit_Pistol : public CBase_CSS_HL2_Pistol
{
public:
	DECLARE_CLASS( CWeapon_Arbeit_Pistol, CBase_CSS_HL2_Pistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_Arbeit_Pistol( void );

	virtual float GetViewKickBase() { return 1.25f; }

	virtual float GetShotPenaltyTime() { return 0.3f; }

	virtual const Vector& GetBulletSpread( void )
	{
		// Handle NPCs first
		static Vector npcCone = VECTOR_CONE_5DEGREES;
		if (GetOwner() && GetOwner()->IsNPC())
			return npcCone;

		static Vector cone;

		float ramp = RemapValClamped( GetAccuracyPenalty(),
			0.0f,
			1.5f,
			0.0f,
			1.0f );

		// We lerp from very accurate to inaccurate over time
		VectorLerp( VECTOR_CONE_1DEGREES, VECTOR_CONE_6DEGREES, ramp, cone );

		return cone;
	}

	virtual float GetFireRate( void ) { return 0.5f; }
	virtual float GetRefireRate( void ) { return 0.15f; }
	virtual float GetDryRefireRate( void ) { return 0.2f; }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_Arbeit_Pistol, DT_Weapon_Arbeit_Pistol )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_arbeit_pistol, CWeapon_Arbeit_Pistol );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_arbeit_pistol );
#endif

BEGIN_DATADESC( CWeapon_Arbeit_Pistol )
END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_Arbeit_Pistol )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_Arbeit_Pistol::CWeapon_Arbeit_Pistol( void )
{
}

//-----------------------------------------------------------------------------
// CWeapon_CSS_HL2_Glock18_Silenced
//-----------------------------------------------------------------------------
class CWeapon_CSS_HL2_Glock18_Silenced : public CWeapon_CSS_HL2_Glock18 // CBase_CSS_HL2_SilencedWeapon<>
{
public:
	DECLARE_CLASS( CWeapon_CSS_HL2_Glock18_Silenced, CWeapon_CSS_HL2_Glock18 );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_DATADESC();

	CWeapon_CSS_HL2_Glock18_Silenced(void);

	bool IsSilenced() const { return true; }

	virtual const Vector& GetBulletSpread( void )
	{
		if ( IsDualWielding() && ( GetOwner() && !GetOwner()->IsNPC() ) )
		{
			static Vector cone;
			cone = BaseClass::GetBulletSpread() * 2.0f;
			return cone;
		}

		return BaseClass::GetBulletSpread();
	}

	virtual float GetViewKickBase() { return InBurst() ? 0.175f : 0.05f; }

	virtual float GetRefireRate() { return IsDualWielding() ? 0.05f : 0.1f; }
	virtual float GetFireRate( void )
	{
		if ( IsDualWielding() && (GetOwner() && GetOwner()->IsNPC()) )
			return 0.25f;

		return InBurst() ? 0.1f : 0.5f;
	}

	WeaponClass_t	WeaponClassify() { return WEPCLASS_HANDGUN; }
	virtual void	SetActivity( Activity act, float duration );

	bool			CanDualWield() const { return true; }
	bool			CanUseBurstMode() const { return !IsDualWielding() && BaseClass::CanUseBurstMode(); }
};

IMPLEMENT_NETWORKCLASS_DT( CWeapon_CSS_HL2_Glock18_Silenced, DT_Weapon_CSS_HL2_Glock18_Silenced )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( weapon_css_glock_silenced, CWeapon_CSS_HL2_Glock18_Silenced );
#if PRECACHE_REGISTER_CSS_WEAPONS == 1
PRECACHE_WEAPON_REGISTER( weapon_css_glock_silenced );
#endif

BEGIN_DATADESC( CWeapon_CSS_HL2_Glock18_Silenced )

	DEFINE_FIELD( m_hLeftHandGun, FIELD_EHANDLE ),

END_DATADESC()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeapon_CSS_HL2_Glock18_Silenced )
END_PREDICTION_DATA()
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeapon_CSS_HL2_Glock18_Silenced::CWeapon_CSS_HL2_Glock18_Silenced( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_CSS_HL2_Glock18_Silenced::SetActivity( Activity act, float duration )
{
	// HACKHACK: Can't recompile all of the models to have this right now
	if (act == ACT_RANGE_ATTACK_DUAL_PISTOLS && SelectWeightedSequence( act ) == ACTIVITY_NOT_AVAILABLE)
		act = ACT_RANGE_ATTACK_PISTOL;

	BaseClass::SetActivity( act, duration );
}
#endif