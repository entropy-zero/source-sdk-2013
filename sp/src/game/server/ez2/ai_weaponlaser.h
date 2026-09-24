//=============================================================================//
//
// Purpose:		Gun-attached laser that can compromise cloaked targets.
//
// Author:		Blixibon
//
//=============================================================================//
#ifndef AI_WEAPONLASER_H
#define AI_WEAPONLASER_H

#include "ai_component.h"
#include "ai_stealth_utils.h"
#include "beam_shared.h"

//-----------------------------------------------------------------------------

// Since our laser material doesn't support shading out anymore, this has to be long enough that the player
// is unlikely to see it awkwardly end
#define GUN_LASER_LENGTH		5000
#define GUN_LASER_LERP_TIME		0.5

#define WEAPON_LASER_THINK_CONTEXT	"ConscriptEliteLaserThink"

extern int	ACT_LASER_ENABLE;
extern int	ACT_LASER_DISABLE;
extern int	AE_CONSCRIPT_ENABLE_LASER;
extern int	AE_CONSCRIPT_DISABLE_LASER;

//-----------------------------------------------------------------------------

#define	DEFINE_WEAPONLASER_DATADESC() \
	DEFINE_KEYFIELD( m_bLaserOn, FIELD_BOOLEAN, "LaserStartsOn" ),	\
	DEFINE_INPUT( m_bCanUseLaserDuringAI, FIELD_BOOLEAN, "SetCanUseLaserDuringAI" ),	\
	DEFINE_KEYFIELD( m_bAlwaysAddLaser, FIELD_BOOLEAN, "AlwaysAddLaser" ),	\
	DEFINE_FIELD( m_hGunLaser, FIELD_EHANDLE ),	\
	DEFINE_FIELD( m_hGunLaserEnd, FIELD_EHANDLE ),	\
	DEFINE_FIELD( m_hGunLaserHitTarget, FIELD_EHANDLE ),	\
	DEFINE_FIELD( m_bLaserAimsAtEnemy, FIELD_BOOLEAN ),	\
	DEFINE_FIELD( m_flLaserTargetTime, FIELD_TIME ),	\
	DEFINE_FIELD( m_vecGunLaserDir, FIELD_VECTOR ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnLaser", InputTurnOnLaser ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffLaser", InputTurnOffLaser ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOnLaserInstant", InputTurnOnLaserInstant ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOffLaserInstant", InputTurnOffLaserInstant ),	\

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_WeaponLaserUser : public BASE_NPC, public ICloakCompromisable
{
	DECLARE_CLASS_NOFRIEND( CAI_WeaponLaserUser, BASE_NPC );

public:
	CAI_WeaponLaserUser()
	{
		this->m_bLaserOn = false;
		this->m_bCanUseLaserDuringAI = true;
		this->m_bAlwaysAddLaser = false;
	}

	void			Precache();
	void			InitActivities();
	void			OnRestore( void );

	virtual CBaseAnimating	*GetActiveLaserWeapon() { return this->GetActiveWeapon(); }
	virtual CBaseEntity		*GetLaserTarget() { return this->GetEnemy(); }
	virtual Vector			GetLaserTargetPos( CBaseEntity *pTarget, const Vector &posSrc ) { return pTarget->BodyTarget( posSrc, true ); }

	virtual bool	GunSupportsLaser( CBaseAnimating *pWeapon, int &iAttachment, bool bForce = false );
	void			AddLaserToGun( CBaseAnimating *pWeapon );
	void			RemoveLaserFromGun( CBaseAnimating *pWeapon );
	void			TurnOnLaser();
	void			TurnOffLaser();

	void			ModifyOrAppendCriteria( AI_CriteriaSet &set );
	
	void			OnScheduleChange( void );
	
	void			Weapon_Equip( CBaseCombatWeapon *pWeapon );			// Adds weapon to player
	void			Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	bool			Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	bool			DoHolster( void );
	Activity		Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	void			HandleAnimEvent( animevent_t *pEvent );

	float			GetLaserDotToTarget( CBaseEntity *pTarget );
	float			GetEyeDotToTarget( CBaseEntity *pTarget );
	bool			TargetCrossingLaser( CBaseEntity *pTarget );
	virtual bool	ShouldAimLaserAtEnemy( CBaseEntity *pEnemy );
	
	virtual void	StartLaserThink() {}
	virtual void	StopLaserThink() {}
	float			DoLaserThink();

	bool			CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType );

	void			InputTurnOnLaser( inputdata_t &inputdata );
	void			InputTurnOffLaser( inputdata_t &inputdata );
	void			InputTurnOnLaserInstant( inputdata_t &inputdata ) { TurnOnLaser(); }
	void			InputTurnOffLaserInstant( inputdata_t &inputdata ) { TurnOffLaser(); }

public:

	// Need to be public to be accessible by derived network table
	CNetworkHandle( CBeam, m_hGunLaser );
	CNetworkVar( Vector, m_vecGunLaserDir );

protected:

	bool			m_bLaserOn;
	bool			m_bCanUseLaserDuringAI;		// Allows elite to use laser dynamically
	bool			m_bAlwaysAddLaser;			// Always adds a laser to the elite's gun, even if it doesn't have an attachment

	EHANDLE			m_hGunLaserEnd;
	EHANDLE			m_hGunLaserHitTarget;
	int				m_nGunLaserAttachment;

	bool			m_bLaserAimsAtEnemy;
	float			m_flLaserTargetTime;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::Precache()
{
	BaseClass::Precache();
	
	//if ( this->m_bCanUseLaserDuringAI )
	{
		this->InitActivities();
	}

	PrecacheMaterial( "effects/progenitor_redlaser1_elite.vmt" );

	PrecacheScriptSound( "NPC_ConscriptElite.LaserOn" );
	PrecacheScriptSound( "NPC_ConscriptElite.LaserOff" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::InitActivities()
{
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_LASER_ENABLE );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_LASER_DISABLE );

	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_CONSCRIPT_ENABLE_LASER );
	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_CONSCRIPT_DISABLE_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::OnRestore()
{
	BaseClass::OnRestore();

	if ( m_hGunLaser && GetActiveLaserWeapon() )
	{
		m_nGunLaserAttachment = GetActiveLaserWeapon()->LookupAttachment( "laser" );
		if ( m_nGunLaserAttachment <= 0 )
		{
			m_nGunLaserAttachment = GetActiveLaserWeapon()->LookupAttachment( "muzzle" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::GunSupportsLaser( CBaseAnimating *pWeapon, int &iAttachment, bool bForce )
{
	iAttachment = pWeapon->LookupAttachment( "laser" );
	if (iAttachment <= 0)
	{
		if (!bForce)
			return false;

		// No laser attachment, but the gun can still have a laser
		// Fall back to the muzzle
		iAttachment = pWeapon->LookupAttachment( "muzzle" );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::AddLaserToGun( CBaseAnimating *pWeapon )
{
	if (!GunSupportsLaser( pWeapon, m_nGunLaserAttachment, m_bAlwaysAddLaser ))
		return;

	//if ( m_nGunLaserAttachment <= 0 )
	//	return;

	// Muzzle Target - Where the laser ends
	m_hGunLaserEnd = CBaseEntity::CreateNoSpawn( "info_target", vec3_origin, vec3_angle, this );
	m_hGunLaserEnd->SetParent( pWeapon, m_nGunLaserAttachment );
	m_hGunLaserEnd->SetLocalOrigin( Vector( GUN_LASER_LENGTH, 0, 0 ) );
	m_hGunLaserEnd->SetLocalAngles( vec3_angle );

	m_hGunLaser = CBeam::BeamCreate( "effects/progenitor_redlaser1_elite.vmt", 2.0 );
	if (m_hGunLaser != NULL)
	{
		m_hGunLaser->EntsInit( pWeapon, m_hGunLaserEnd );
		m_hGunLaser->SetStartAttachment( m_nGunLaserAttachment );
		m_hGunLaser->SetWidth( 2.0 );
		m_hGunLaser->SetBrightness( 150 );
		m_hGunLaser->SetColor( 255, 0, 0 );
		m_hGunLaser->RelinkBeam();
		m_hGunLaser->SetNoise( 0 );
		m_hGunLaser->SetBeamFlags( FBEAM_FADEOUT );
		m_hGunLaser->SetNetworkNodraw( true );

		m_hGunLaser->SetParent( pWeapon, m_nGunLaserAttachment );
		m_hGunLaser->SetLocalOrigin( vec3_origin );
		m_hGunLaser->SetLocalAngles( vec3_angle );

		if (m_bLaserOn)
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

	// Set any laser attachment bodygroup
	int nLaserBody = pWeapon->FindBodygroupByName( "laser" );
	if (nLaserBody != -1)
	{
		pWeapon->SetBodygroup( nLaserBody, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::RemoveLaserFromGun( CBaseAnimating *pWeapon )
{
	if (m_hGunLaser)
	{
		UTIL_Remove( m_hGunLaser );
		m_hGunLaser = NULL;
	}

	if (m_hGunLaserEnd)
	{
		UTIL_Remove( m_hGunLaserEnd );
		m_hGunLaserEnd = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::TurnOnLaser()
{
	if (!m_hGunLaser)
	{
		Warning( "%s: Can't turn on invalid laser\n", GetDebugName() );
		return;
	}

	if (m_bLaserOn)
		return;

	m_bLaserOn = true;
	m_hGunLaser->RemoveEffects( EF_NODRAW );
	EmitSound( "NPC_ConscriptElite.LaserOn" );

	StartLaserThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::TurnOffLaser()
{
	if (!m_hGunLaser)
	{
		Warning( "%s: Can't turn off invalid laser\n", GetDebugName() );
		return;
	}

	if (!m_bLaserOn)
		return;

	m_bLaserOn = false;
	m_hGunLaser->AddEffects( EF_NODRAW );
	EmitSound( "NPC_ConscriptElite.LaserOff" );

	m_hGunLaserHitTarget = NULL;

	SetContextThink( NULL, TICK_NEVER_THINK, WEAPON_LASER_THINK_CONTEXT );
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::InputTurnOnLaser( inputdata_t &inputdata )
{
	if (m_bLaserOn)
		return;

	AddGesture( (Activity)ACT_LASER_ENABLE );
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::InputTurnOffLaser( inputdata_t &inputdata )
{
	if (!m_bLaserOn)
		return;

	AddGesture( (Activity)ACT_LASER_DISABLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	if (m_bLaserOn)
	{
		set.AppendCriteria( "laser", "1" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	if ( IsCurSchedule( SCHED_COMBAT_FACE, false ) && m_bLaserOn )
	{
		// Always aim while combat facing
		SetReadinessLevel( AIRL_AGITATED, false, false );
	}

	if ( m_bCanUseLaserDuringAI && m_hGunLaser )
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
				if ( GetState() == NPC_STATE_IDLE && GetEnemies()->NumEnemies() == 0 &&
					( !IsUsingStealthSenses() || gpGlobals->curtime - GetStealthSenses()->GetLastSoundTime() > 20.0f ) )
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
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_WeaponLaserUser<BASE_NPC>::GetLaserDotToTarget( CBaseEntity *pTarget )
{
	Vector vecToTarget = (pTarget->GetAbsOrigin() - GetAbsOrigin());
	VectorNormalize( vecToTarget );

	Vector vecGunLaserForward;
	if ( GetActiveLaserWeapon() )
	{
		Vector vecOrigin;
		GetActiveLaserWeapon()->GetAttachment( m_nGunLaserAttachment, vecOrigin, &vecGunLaserForward );
	}

	float flDotLaser = DotProduct( vecToTarget, vecGunLaserForward );
	
	return flDotLaser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_WeaponLaserUser<BASE_NPC>::GetEyeDotToTarget( CBaseEntity *pTarget )
{
	Vector vecToTarget = (pTarget->GetAbsOrigin() - GetAbsOrigin());
	VectorNormalize( vecToTarget );

	float flDotEye = DotProduct( vecToTarget, HeadDirection3D() ); // EyeDirection3D
	
	return flDotEye;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::TargetCrossingLaser( CBaseEntity *pTarget )
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
	if ( GetActiveLaserWeapon() )
	{
		Vector vecOrigin;
		GetActiveLaserWeapon()->GetAttachment( m_nGunLaserAttachment, vecOrigin, &vecGunLaserForward );
	}

	float flDotLaser = DotProduct( vecToTarget, vecGunLaserForward );

	if (flDotLaser > 0.98 && flDotEye > 0.9)
		return true;*/
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For assassin cloaking
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType )
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
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::ShouldAimLaserAtEnemy( CBaseEntity *pEnemy )
{
	if ( !pEnemy )
		return false;
	
	switch ( this->GetActivity() )
	{
		case ACT_RANGE_ATTACK1:
		case ACT_IDLE:
		case ACT_RUN_AIM:
		case ACT_WALK_AIM:
			break;
		default:
			//printl("Not in right act: " + self.GetActivity())
			return false;
	}

	if ( GetLaserDotToTarget( pEnemy ) < 0.97 || !this->FVisible( pEnemy ) )
	{
		//printl("Dot: " + GetLaserDotToTarget(self.GetEnemy()))
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_WeaponLaserUser<BASE_NPC>::DoLaserThink( void )
{
	if ( m_bLaserOn && m_hGunLaser )
	{
		CBaseEntity *pLaserTarget = GetLaserTarget();
		if ( ShouldAimLaserAtEnemy( pLaserTarget ) )
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
		
		if ( GetActiveLaserWeapon() )
		{
			GetActiveLaserWeapon()->GetAttachment( m_nGunLaserAttachment, vecLaserStartOrigin, &vecLaserForward );
			
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
					vecLaserEnd = vecLaserStartOrigin + (GetLaserTargetPos( pLaserTarget, vecLaserStartOrigin ) - vecLaserStartOrigin).Normalized() * GUN_LASER_LENGTH;
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
					vecLaserEnd = vecLaserStartOrigin + (GetLaserTargetPos( pLaserTarget, vecLaserStartOrigin ) - vecLaserStartOrigin).Normalized() * GUN_LASER_LENGTH;
			}
		}
	
		trace_t tr;
		UTIL_TraceLine( vecLaserStartOrigin, vecLaserEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		
		//printf("Frac = %f\n", frac)
		//debugoverlay.Cross3D(laserEnd, 3.0, 255, (255 * frac), 0, true, 4.0)
		
		m_hGunLaserEnd->SetAbsOrigin( tr.endpos );
		m_hGunLaserHitTarget = tr.m_pEnt;

		m_vecGunLaserDir = tr.endpos - tr.startpos;
		VectorNormalize( m_vecGunLaserDir.GetForModify() );
	}

	float flNextThink = 0.5f;
	if (m_flLaserTargetTime != -1 || this->IsMoving())
		flNextThink = TICK_INTERVAL;
	else if (HasCondition( COND_SEE_PLAYER ))
		flNextThink = 0.1f;
	else if (HasCondition( COND_IN_PVS ))
		flNextThink = 0.2f;

	return flNextThink;
}

//-----------------------------------------------------------------------------
// Purpose:	Gives character new weapon and equips it
// Input  : New weapon
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::Weapon_Equip( CBaseCombatWeapon *pWeapon )
{
	BaseClass::Weapon_Equip( pWeapon );

	if ( GetActiveLaserWeapon() )
	{
		AddLaserToGun( GetActiveLaserWeapon() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Drop the active weapon, optionally throwing it at the given target position.
// Input  : pWeapon - Weapon to drop/throw.
//			pvecTarget - Position to throw it at, NULL for none.
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget /* = NULL */, const Vector *pVelocity /* = NULL */ )
{
	BaseClass::Weapon_Drop( pWeapon, pvecTarget, pVelocity );

	if ( pWeapon )
	{
		if ( IsAlive() && GetState() != NPC_STATE_COMBAT )
		{
			// Remove gun laser immediately
			RemoveLaserFromGun( pWeapon );
		}
		else
		{
			// Gun laser stays on for a moment
			if ( m_hGunLaser )
			{
				m_hGunLaser->LiveForTime( 0.75f );
				m_hGunLaser = NULL;
			}

			if ( m_hGunLaserEnd )
			{
				m_hGunLaserEnd->SetThink( &CBaseEntity::SUB_Remove );
				m_hGunLaserEnd->SetNextThink( gpGlobals->curtime + 0.75f );
				m_hGunLaserEnd = NULL;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex /*=0*/ )
{
	CBaseCombatWeapon *pOldWeapon = this->GetActiveWeapon();

	if ( !BaseClass::Weapon_Switch( pWeapon, viewmodelindex ) )
		return false;

	if ( pOldWeapon )
		RemoveLaserFromGun( pOldWeapon );

	if ( pWeapon )
	{
		AddLaserToGun( pWeapon );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Switches to the given weapon (providing it has ammo)
// Input  :
// Output : true is switch succeeded
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_WeaponLaserUser<BASE_NPC>::DoHolster()
{
	if ( this->GetActiveWeapon() )
		RemoveLaserFromGun( this->GetActiveWeapon() );

	return BaseClass::DoHolster();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Activity CAI_WeaponLaserUser<BASE_NPC>::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
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
template <class BASE_NPC>
void CAI_WeaponLaserUser<BASE_NPC>::HandleAnimEvent( animevent_t *pEvent )
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

#endif // AI_WEAPONLASER_H