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
extern int ACT_GESTURE_FLINCH_SHIELD_TOP;
extern int ACT_GESTURE_FLINCH_SHIELD_MID;
extern int ACT_GESTURE_FLINCH_SHIELD_BOTTOM;
extern int ACT_FLINCH_SHIELD_BIG;

extern int AE_NPC_DRAW_SHIELD;
extern int AE_NPC_HOLSTER_SHIELD;

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

	Activity	Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	void		HandleAnimEvent( animevent_t *pEvent );
	void		Weapon_SetActivity( Activity newActivity, float duration );

	void		InputGivePropShield( inputdata_t &inputdata );
	void		InputDropPropShield( inputdata_t &inputdata );
	void		InputHolsterPropShield( inputdata_t &inputdata );
	void		InputUnholsterPropShield( inputdata_t &inputdata );

	bool		SpawnsWithShield() const { return m_iSpawnsWithShield != TRS_FALSE; }
	bool		HasPropShield() const { return m_hShield != NULL; }
	bool		IsPropShieldEquipped() const { return m_hShield != NULL; }

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

	static CPropShield *CreatePropShield( CAI_BaseActor *pNPC, bool bStartHolstered = false );

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
		this->m_hShield = CPropShield::CreatePropShield( this, this->m_iSpawnsWithShield == TRS_NONE );
		this->m_bShieldEquipped = true;
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
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_TOP );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_MID );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_GESTURE_FLINCH_SHIELD_BOTTOM );
	ADD_CUSTOM_ACTIVITY( ThisClass, ACT_FLINCH_SHIELD_BIG );

	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_DRAW_SHIELD );
	ADD_CUSTOM_ANIMEVENT( ThisClass, AE_NPC_HOLSTER_SHIELD );
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
		this->m_hShield->Drop( info );
		this->m_hShield = NULL;
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Activity CAI_PropShieldUser<BASE_NPC>::Weapon_TranslateActivity( Activity baseAct, bool *pRequired )
{
	if ( this->m_bShieldEquipped )
	{
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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_NPC_HOLSTER_SHIELD )
	{
		this->m_hShield->Holster();
		this->m_bShieldEquipped = false;
		this->ResetActivity();
	}
	else if ( pEvent->event == AE_NPC_DRAW_SHIELD )
	{
		this->m_hShield->Unholster();
		this->m_bShieldEquipped = true;
		this->ResetActivity();
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
		this->m_hShield = CPropShield::CreatePropShield( this );
		this->m_bShieldEquipped = true;
		this->InitActivities();
		this->ResetActivity();
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
		this->AddGesture( (Activity)ACT_DISARM_SHIELD, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_PropShieldUser<BASE_NPC>::InputUnholsterPropShield( inputdata_t &inputdata )
{
	if ( this->m_hShield )
	{
		this->AddGesture( (Activity)ACT_ARM_SHIELD, true );
	}
}

#endif // AI_TACTICAL_SHIELD_H