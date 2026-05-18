//=============================================================================//
//
// Purpose:		A physical shield that can be used by certain NPCs.
//
// Author:		Blixibon
//
//=============================================================================//
#ifndef AI_PROP_SHIELD_H
#define AI_PROP_SHIELD_H

#include "ai_component.h"
#include "prop_armor.h"
#include "ai_movetypes.h"

//-----------------------------------------------------------------------------

#define SHIELD_MODEL_NAME	"models/weapons/w_tactical_shield.mdl"

//-----------------------------------------------------------------------------

// Shield activities
extern int ACT_IDLE_SHIELD;
extern int ACT_IDLE_SHIELD_RELAXED;
extern int ACT_IDLE_SHIELD_STIMULATED;
extern int ACT_IDLE_ANGRY_SHIELD;
extern int ACT_IDLE_AIM_SHIELD_STIMULATED;
extern int ACT_WALK_SHIELD;
extern int ACT_WALK_SHIELD_RELAXED;
extern int ACT_WALK_SHIELD_STIMULATED;
extern int ACT_RUN_SHIELD;
extern int ACT_WALK_AIM_SHIELD;
extern int ACT_WALK_AIM_SHIELD_STIMULATED;
extern int ACT_RUN_AIM_SHIELD;
extern int ACT_COVER_SHIELD_LOW;
extern int ACT_RANGE_AIM_SHIELD_LOW;
extern int ACT_WALK_CROUCH_SHIELD;
extern int ACT_RUN_CROUCH_SHIELD;
extern int ACT_WALK_CROUCH_AIM_SHIELD;
extern int ACT_RUN_CROUCH_AIM_SHIELD;

extern int ACT_RANGE_ATTACK_SHIELD;
extern int ACT_RANGE_ATTACK_SHIELD_LOW;
extern int ACT_MELEE_ATTACK_SHIELD;
extern int ACT_RELOAD_SHIELD;

extern int ACT_ARM_SHIELD;
extern int ACT_DISARM_SHIELD;
extern int ACT_ACTIVATE_SHIELD;
extern int ACT_DEACTIVATE_SHIELD;
extern int ACT_THROW_SHIELD;
extern int ACT_GESTURE_FLINCH_SHIELD_TOP;
extern int ACT_GESTURE_FLINCH_SHIELD_MID;
extern int ACT_GESTURE_FLINCH_SHIELD_BOTTOM;
extern int ACT_FLINCH_SHIELD_BIG;

extern int AE_NPC_DRAW_SHIELD;
extern int AE_NPC_HOLSTER_SHIELD;
extern int AE_NPC_THROW_SHIELD;
extern int AE_NPC_SET_SHIELD_ANIM;

//-----------------------------------------------------------------------------

#define	DEFINE_PROPSHIELD_DATADESC() \
	DEFINE_KEYFIELD( m_iSpawnsWithShield, FIELD_INTEGER, "SpawnsWithShield" ),	\
	DEFINE_FIELD( m_bShieldEquipped, FIELD_BOOLEAN ),	\
	DEFINE_FIELD( m_hShield, FIELD_EHANDLE ),	\
	DEFINE_INPUTFUNC( FIELD_VOID,	"GivePropShield",	InputGivePropShield ),	\
	DEFINE_INPUTFUNC( FIELD_VOID,	"DropPropShield",	InputDropPropShield ),	\
	DEFINE_INPUTFUNC( FIELD_VOID,	"HolsterPropShield",	InputHolsterPropShield ),	\
	DEFINE_INPUTFUNC( FIELD_VOID,	"UnholsterPropShield",	InputUnholsterPropShield ),	\

class CPropShield;
class CAI_BaseActor;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_PropShieldUser : public BASE_NPC
{
	DECLARE_CLASS_NOFRIEND( CAI_PropShieldUser, BASE_NPC );

public:
	CAI_PropShieldUser()
	{
		this->m_iSpawnsWithShield = TRS_FALSE;
		this->m_bShieldEquipped = false;
	}

	void	Spawn();
	void	Precache();
	void	InitActivities();

	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void	Event_Killed( const CTakeDamageInfo &info );

	void		GatherConditions();
	bool		ShouldMoveAndShoot();
	bool		OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval );

	Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		Weapon_SetActivity( Activity newActivity, float duration );
	Vector		Weapon_ShootPosition();

	void		InputGivePropShield( inputdata_t &inputdata );
	void		InputDropPropShield( inputdata_t &inputdata );
	void		InputHolsterPropShield( inputdata_t &inputdata );
	void		InputUnholsterPropShield( inputdata_t &inputdata );

	bool		SpawnsWithShield() const { return m_iSpawnsWithShield != TRS_FALSE; }
	bool		HasPropShield() const { return m_hShield != NULL; }
	bool		IsPropShieldEquipped() const { return m_hShield != NULL; }

	virtual const char	*GetShieldModelName() { return SHIELD_MODEL_NAME; }
	virtual bool	RemoveShieldOnHolster() { return false; }
	virtual bool	CanAimWithShield() { return true; }

	virtual void		OnShieldSpawn( CPropShield *pShield ) {}
	virtual void		OnShieldRemove( CPropShield *pShield ) {}
	virtual CBaseEntity *CreateShieldProjectile( CPropShield *pShield ) { return NULL; }

protected:

	ThreeState_t	m_iSpawnsWithShield;	// This is a three-state because there's another "holstered" option
	bool			m_bShieldEquipped;
	CHandle< CPropShield >	m_hShield;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPropShield : public CArmorProp
{
	DECLARE_CLASS( CPropShield, CArmorProp );
public:
	DECLARE_DATADESC();

	CPropShield();

	static CPropShield *CreatePropShield( CAI_BaseActor *pNPC, bool bStartHolstered = false, const char *pszModelName = SHIELD_MODEL_NAME );

	void	Spawn();
	void	Precache();

	bool	ShouldBlockTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void	ModifyTraceAttackDamage( CTakeDamageInfo &info, float flPenetrationScale = 1.0f );

	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );

	void	Holster();
	void	Unholster();

	void	Drop( const CTakeDamageInfo &info );

	int GetHitgroup() const
	{
		return m_bHolstered ? HITGROUP_CHEST : HITGROUP_LEFTARM;
	}

private:
	bool	m_bHolstered;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::Spawn()
{
	BaseClass::Spawn();

	if ( this->m_iSpawnsWithShield != TRS_FALSE )
	{
		if ( !this->RemoveShieldOnHolster() || this->m_iSpawnsWithShield == TRS_TRUE )
		{
			this->m_hShield = CPropShield::CreatePropShield( this, this->m_iSpawnsWithShield == TRS_NONE, this->GetShieldModelName() );
			this->m_bShieldEquipped = true;

			this->OnShieldSpawn( this->m_hShield );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::Precache()
{
	BaseClass::Precache();
	
	if ( this->m_iSpawnsWithShield )
	{
		this->InitActivities();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InitActivities()
{
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_IDLE_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_IDLE_SHIELD_RELAXED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_IDLE_SHIELD_STIMULATED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_IDLE_ANGRY_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_IDLE_AIM_SHIELD_STIMULATED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_SHIELD_RELAXED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_SHIELD_STIMULATED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RUN_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_AIM_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_AIM_SHIELD_STIMULATED );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RUN_AIM_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_COVER_SHIELD_LOW );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RANGE_AIM_SHIELD_LOW );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_CROUCH_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RUN_CROUCH_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_WALK_CROUCH_AIM_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RUN_CROUCH_AIM_SHIELD );

	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RANGE_ATTACK_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RANGE_ATTACK_SHIELD_LOW );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_MELEE_ATTACK_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_RELOAD_SHIELD );

	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_ARM_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_DISARM_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_ACTIVATE_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_DEACTIVATE_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_THROW_SHIELD );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_TOP );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_MID );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_BOTTOM );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_FLINCH_SHIELD_BIG );

	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_DRAW_SHIELD );
	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_HOLSTER_SHIELD );
	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_THROW_SHIELD );
	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_SET_SHIELD_ANIM );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	// If ptr->m_pEnt isn't this, then this came from the shield and shouldn't be redirected
	if ( this->m_hShield && !this->m_hShield->IsMarkedForDeletion() && ptr->m_pEnt == this )
	{
		// Check to see if we're actually colliding with the shield
		Ray_t ray;
		trace_t tr;
		ICollideable *pCollide = this->m_hShield->CollisionProp();
		Vector vecTraceDist = (vecDir * 5.0f);
		ray.Init( ptr->endpos - vecTraceDist, ptr->endpos + vecTraceDist );
		enginetrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &tr );
		if ( tr.fraction != 1.0f )
		{
			DevMsg( "%s: Redirected TraceAttack to shield\n", this->GetDebugName() );
			this->m_hShield->TraceAttack( info, vecDir, ptr, pAccumulator );
			return;
		}
	}

	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::Event_Killed( const CTakeDamageInfo &info )
{
	if ( this->m_hShield )
	{
		if ( this->RemoveShieldOnHolster() )
		{
			this->OnShieldRemove( this->m_hShield );
			UTIL_Remove( this->m_hShield );
		}
		else
			this->m_hShield->Drop( info );

		this->m_hShield = NULL;
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::GatherConditions()
{
	BaseClass::GatherConditions();

	if ( !this->CanAimWithShield() && this->IsPropShieldEquipped() )
		this->ClearCondition( COND_CAN_RANGE_ATTACK1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_PropShieldUser<BASE_NPC>::ShouldMoveAndShoot()
{
	if ( !this->CanAimWithShield() && this->IsPropShieldEquipped() )
		return false;

	return BaseClass::ShouldMoveAndShoot();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_PropShieldUser<BASE_NPC>::OverrideMoveFacing( const AILocalMoveGoal_t &move, float flInterval )
{
	if ( this->IsPropShieldEquipped() && ( this->HasCondition( COND_SEE_ENEMY ) /*|| this->FVisible( this->GetEnemyLKP() )*/ ) )
	{
		// Always face enemy while shield is equipped
		Vector vecEnemyLKP = this->GetEnemyLKP();
		this->AddFacingTarget( this->GetEnemy(), vecEnemyLKP, 1.0, 0.8 );
	}

	return BaseClass::OverrideMoveFacing( move, flInterval );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Activity CAI_PropShieldUser<BASE_NPC>::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	if ( this->m_bShieldEquipped )
	{
		if ( !this->CanAimWithShield() )
		{
			// TODO: Better solution for this
			switch ( baseAct )
			{
				case ACT_IDLE_AIM_STIMULATED:		baseAct = ACT_IDLE_STIMULATED; break;
				case ACT_IDLE_ANGRY:
				case ACT_IDLE_AIM_AGITATED:			baseAct = ACT_IDLE; break;
				case ACT_WALK_AIM:
				case ACT_WALK_AIM_STIMULATED:
				case ACT_WALK_AGITATED:
				case ACT_WALK_AIM_AGITATED:			baseAct = ACT_WALK; break;
				case ACT_RUN_AIM:
				case ACT_RUN_AIM_STIMULATED:
				case ACT_RUN_AGITATED:
				case ACT_RUN_AIM_AGITATED:			baseAct = ACT_RUN; break;
				case ACT_RANGE_AIM_LOW:				baseAct = ACT_COVER_LOW; break;
				case ACT_WALK_CROUCH_AIM:			baseAct = ACT_WALK_CROUCH; break;
				case ACT_RUN_CROUCH_AIM:			baseAct = ACT_RUN_CROUCH; break;
			}
		}

		switch ( baseAct )
		{
			case ACT_IDLE:						return (Activity)ACT_IDLE_SHIELD;
			case ACT_IDLE_RELAXED:				return (Activity)ACT_IDLE_SHIELD_RELAXED;
			case ACT_IDLE_STIMULATED:			return (Activity)ACT_IDLE_SHIELD_STIMULATED;
			case ACT_IDLE_AGITATED:				return (Activity)ACT_IDLE_ANGRY_SHIELD;
			case ACT_IDLE_ANGRY:				return (Activity)ACT_IDLE_ANGRY_SHIELD;
			case ACT_IDLE_AIM_STIMULATED:		return (Activity)ACT_IDLE_AIM_SHIELD_STIMULATED;
			case ACT_IDLE_AIM_AGITATED:			return (Activity)ACT_IDLE_ANGRY_SHIELD;
			case ACT_WALK:						return (Activity)ACT_WALK_SHIELD;
			case ACT_WALK_RELAXED:				return (Activity)ACT_WALK_SHIELD_RELAXED;
			case ACT_WALK_STIMULATED:			return (Activity)ACT_WALK_SHIELD_STIMULATED;
			case ACT_WALK_AGITATED:				return (Activity)ACT_WALK_SHIELD;
			case ACT_WALK_AIM:					return (Activity)ACT_WALK_AIM_SHIELD;
			case ACT_WALK_AIM_STIMULATED:		return (Activity)ACT_WALK_AIM_SHIELD_STIMULATED;
			case ACT_WALK_AIM_AGITATED:			return (Activity)ACT_WALK_AIM_SHIELD;
			case ACT_RUN:						return (Activity)ACT_RUN_SHIELD;
			case ACT_RUN_STIMULATED:			return (Activity)ACT_RUN_SHIELD;
			case ACT_RUN_AGITATED:				return (Activity)ACT_RUN_AIM_SHIELD;
			case ACT_RUN_AIM:					return (Activity)ACT_RUN_AIM_SHIELD;
			case ACT_RUN_AIM_STIMULATED:		return (Activity)ACT_RUN_AIM_SHIELD;
			case ACT_RUN_AIM_AGITATED:			return (Activity)ACT_RUN_AIM_SHIELD;
			case ACT_COVER_LOW:					return (Activity)ACT_COVER_SHIELD_LOW;
			case ACT_RANGE_AIM_LOW:				return (Activity)ACT_RANGE_AIM_SHIELD_LOW;
			case ACT_WALK_CROUCH:				return (Activity)ACT_WALK_CROUCH_SHIELD;
			case ACT_WALK_CROUCH_AIM:			return (Activity)ACT_WALK_CROUCH_AIM_SHIELD;
			case ACT_RUN_CROUCH:				return (Activity)ACT_RUN_CROUCH_SHIELD;
			case ACT_RUN_CROUCH_AIM:			return (Activity)ACT_RUN_CROUCH_AIM_SHIELD;

			case ACT_RANGE_ATTACK1:				return (Activity)ACT_RANGE_ATTACK_SHIELD;
			case ACT_RANGE_ATTACK1_LOW:			return (Activity)ACT_RANGE_ATTACK_SHIELD_LOW;
			case ACT_MELEE_ATTACK1:				return (Activity)ACT_MELEE_ATTACK_SHIELD;
			case ACT_RELOAD:					return (Activity)ACT_RELOAD_SHIELD;

			//case ACT_ARM:						return (Activity)ACT_ARM_SHIELD;
			//case ACT_DISARM:					return (Activity)ACT_DISARM_SHIELD;
		}
	}

	return BaseClass::Weapon_TranslateActivity( baseAct, pRequired );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::Weapon_SetActivity( Activity newActivity, float duration )
{
	if ( this->m_bShieldEquipped )
	{
		if ( newActivity == (Activity)ACT_RANGE_ATTACK_SHIELD )
		{
			newActivity = BaseClass::Weapon_TranslateActivity( ACT_RANGE_ATTACK1, NULL );
		}
	}

	BaseClass::Weapon_SetActivity( newActivity, duration );
}


//-----------------------------------------------------------------------------
// Purpose: Get shoot position of BCC at an arbitrary position
// Input  :
// Output :
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Vector CAI_PropShieldUser<BASE_NPC>::Weapon_ShootPosition()
{
	if ( this->m_bShieldEquipped )
	{
		// Projectile weapons have issues with this, so try to just use the muzzle directly if there is one
		if ( this->GetActiveWeapon() )
		{
			int nAttach = this->GetActiveWeapon()->LookupAttachment( "muzzle" );
			if ( nAttach != -1 )
			{
				Vector vecMuzzle;
				this->GetActiveWeapon()->GetAttachment( nAttach, vecMuzzle );
				return vecMuzzle;
			}
		}
	}

	return BaseClass::Weapon_ShootPosition();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_NPC_HOLSTER_SHIELD )
	{
		if ( this->m_hShield )
		{
			if ( this->RemoveShieldOnHolster() )
			{
				CPropShield *pShield = this->m_hShield;
				this->m_hShield = NULL;

				this->OnShieldRemove( pShield );
				UTIL_Remove( pShield );
			}
			else
				this->m_hShield->Holster();
		}

		this->m_bShieldEquipped = false;
		this->ResetActivity();
	}
	else if ( pEvent->event == AE_NPC_DRAW_SHIELD )
	{
		if ( this->m_hShield )
			this->m_hShield->Unholster();
		else if ( this->RemoveShieldOnHolster() )
		{
			// Create a new one
			this->m_hShield = CPropShield::CreatePropShield( this, false, this->GetShieldModelName() );

			this->OnShieldSpawn( this->m_hShield );

			if ( pEvent->options && *pEvent->options )
				this->m_hShield->PropSetSequence( this->m_hShield->LookupSequence( pEvent->options ) );
		}

		this->m_bShieldEquipped = true;
		this->ResetActivity();
	}
	else if ( pEvent->event == AE_NPC_THROW_SHIELD )
	{
		if ( this->m_hShield )
		{
			CPropShield *pShield = this->m_hShield;
			this->m_hShield = NULL;

			this->CreateShieldProjectile( pShield );
		}

		this->m_bShieldEquipped = false;
		this->ResetActivity();
	}
	else if ( pEvent->event == AE_NPC_SET_SHIELD_ANIM )
	{
		if ( this->m_hShield )
		{
			this->m_hShield->PropSetSequence( this->m_hShield->LookupSequence( pEvent->options ) );
		}
	}
	else
		BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InputGivePropShield( inputdata_t &inputdata )
{
	if ( !this->m_hShield )
	{
		this->m_hShield = CPropShield::CreatePropShield( this, false, this->GetShieldModelName() );
		this->m_bShieldEquipped = true;
		this->InitActivities();
		this->ResetActivity();

		this->OnShieldSpawn( this->m_hShield );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InputDropPropShield( inputdata_t &inputdata )
{
	if ( this->m_hShield )
	{
		this->OnShieldRemove( this->m_hShield );

		CTakeDamageInfo info;
		this->m_hShield->Drop( info );
		this->m_bShieldEquipped = false;
		this->ResetActivity();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InputHolsterPropShield( inputdata_t &inputdata )
{
	if ( this->m_hShield )
	{
		this->AddGesture( this->RemoveShieldOnHolster() ? (Activity)ACT_DEACTIVATE_SHIELD : (Activity)ACT_DISARM_SHIELD, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InputUnholsterPropShield( inputdata_t &inputdata )
{
	if ( this->m_hShield || this->RemoveShieldOnHolster() )
	{
		this->AddGesture( this->RemoveShieldOnHolster() ? (Activity)ACT_ACTIVATE_SHIELD : (Activity)ACT_ARM_SHIELD, true );
	}
}

#endif // AI_TACTICAL_SHIELD_H