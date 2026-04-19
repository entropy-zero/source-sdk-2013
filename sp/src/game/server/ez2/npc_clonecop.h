//=============================================================================//
//
// Purpose:		Clone Cop, a former man bent on killing anyone who stands in his way.
//				He was trapped under Arbeit 1 for a long time (from his perspective),
//				but now he's back and he's bigger, badder, and possibly even more deranged than ever before.
//				I mean, you could just see the brains of this guy.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_CLONECOP_H
#define NPC_CLONECOP_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_combine.h"

class CSatchelCharge;

class CNPC_CloneCop : public CNPC_Combine
{
	DECLARE_CLASS( CNPC_CloneCop, CNPC_Combine );
	DECLARE_DATADESC();

public:
	CNPC_CloneCop();

	void		Spawn( void );
	void		Precache( void );
	void		Activate( void );

	Class_T		Classify( void ) { return CLASS_COMBINE_NEMESIS; }

	void		DeathSound( const CTakeDamageInfo &info );

	void		ClearAttackConditions( void );
	bool		WeaponLOSCondition( const Vector &ownerPos, const Vector &targetPos, bool bSetConditions );
	void		GatherConditions();
	void		BuildScheduleTestBits( void );
	void		PrescheduleThink();
	int			PrescheduleSelectActionGesture();
	int			SelectSchedule( void );
	int			SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int			TranslateSchedule( int scheduleType );

	void		StartTask( const Task_t *pTask );
	bool		IsCurTaskContinuousMove();

	CBaseEntity *FindNearestHealthItem( const Vector &vecOrigin, float flRadius, bool bComplex = false );
	void		PickupItem( CBaseEntity *pItem );

	bool		IsPlayingActionGesture();
	bool		IsPlayingActionGesture( Activity activity );
	int			AddActionGesture( Activity activity );
	int			AddActionGesture( Activity activity, const Vector &vecPosition, float flImportance = 1.0f );

	// Clone Cop/Bad Cop can pull out a 357, because they're THAT cool.
	const char	*GetBackupWeaponClass() { return "weapon_357"; }

	int			OnTakeDamage( const CTakeDamageInfo &info );
	float		GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );
	Vector		GetShootEnemyDir( const Vector &shootOrigin, bool bNoisy = true );
	Vector		GetActualShootPosition( const Vector &shootOrigin );
	void		HandleAnimEvent( animevent_t *pEvent );

	bool		CanAltFireEnemy( bool bUseFreeKnowledge );
	bool		CanGrenadeEnemy( bool bUseFreeKnowledge = true );

	bool		CanRunAScriptedNPCInteraction( bool bForced = false );

	void		BleedThink();
	void		StartBleeding();
	void		StopBleeding();
	inline bool	IsBleeding() { return m_bIsBleeding; }

	void		SetThrowXenGrenades( bool bEnabled ) { m_bThrowXenGrenades = false; }
	bool		ShouldThrowXenGrenades();

	virtual bool	ShouldThrowProximitySatchel( bool bDrop = false ) { return false; }
	virtual void	OnThrowProximitySatchel( CBaseEntity *pGrenade ) {}

	bool		ShouldUseAvoidantFlanking();

	bool		CanDeployManhack( void );
	void		HandleManhackSpawn( CAI_BaseNPC *pNPC );

	void		ModifyOrAppendCriteria( AI_CriteriaSet& set );

	void		Event_Killed( const CTakeDamageInfo &info );
	void		Event_KilledOther( CBaseEntity *pVictim, const CTakeDamageInfo &info );

	Activity	GetFlinchActivity( bool bHeavyDamage, bool bGesture );
	bool		IsHeavyDamage( const CTakeDamageInfo &info );
	Activity	NPC_TranslateActivity( Activity eNewActivity );
	Activity	Weapon_TranslateActivity( Activity eNewActivity, bool *pRequired = NULL );
	void		Weapon_HandleEquip( CBaseCombatWeapon *pWeapon );

	bool		MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	bool		CanPickupWhileMoving() { return true; }

	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	bool		DoHolster( void );
	bool		DoUnholster( void );
	bool		Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );
	void		PlayDeploySound( CBaseCombatWeapon *pWeapon );

	bool		IsJumpLegal( const Vector & startPos, const Vector & apex, const Vector & endPos ) const;

	bool		GetGameTextSpeechParams( hudtextparms_t &params );

	bool		AllowedToIgnite( void ) { return false; }

	// For special base Combine behaviors
	bool		IsMajorCharacter() { return true; }

	virtual bool	IsBadCop() { return false; }

	int			GetArmorValue() { return m_ArmorValue; }

	//---------------------------------------

	struct SwitchableWeaponData_t
	{
		const char *pszClassname;

		// These are preferred ranges. Weapon data is used for hard ranges
		float	flMinRange;
		float	flMaxRange;

		WeaponSound_t	nDeploySound;
	};

	static		SwitchableWeaponData_t g_SwitchableWeaponData[];

	// Stores classname string pointers to access respective indices in g_SwitchableWeaponData later
	// instead of having to go through the whole query again.
	// Simple optimization, not saved
	CUtlMap<string_t, int>	m_SwitchableWeaponCache;

	int			GetSwitchableWeaponIdx( CBaseCombatWeapon *pWeapon );

	// Old enum (not used by new switching code)
	enum
	{
		// Sorted by range
		WEAPONSWITCH_SHOTGUN,
		WEAPONSWITCH_AR2,
		WEAPONSWITCH_CROSSBOW,

		WEAPONSWITCH_COUNT,
	};

protected:
	//=========================================================
	// Clone Cop schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE_FLANK_LINE_OF_FIRE = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE_FLANK_AWAY_LINE_OF_FIRE,		// Flank to a node at least 300 units away from the enemy
		SCHED_COMBINE_FLANK_BEHIND_LINE_OF_FIRE,	// Flank to a node behind the enemy
		SCHED_COMBINE_MERCILESS_RANGE_ATTACK1,
		SCHED_COMBINE_MERCILESS_SUPPRESS,
		SCHED_COMBINE_MERCILESS_SUPPRESS_CREEP,
		SCHED_COMBINE_THROW_MANHACK,
		NEXT_SCHEDULE,

		COND_COMBINE_WEAPON_SIGHT_OCCLUDED = BaseClass::NEXT_CONDITION,	// Only set if the occlusion is very close to the sight (stops suppression)
		COND_COMBINE_DESIRE_WEAPON_SWITCH,
		COND_COMBINE_PLAYING_ACTION_GESTURE,
		COND_COMBINE_CAN_MELEE_GESTURE,
		NEXT_CONDITION
	};

	DEFINE_CUSTOM_AI;

	//-------------------------------------------------------

	class CCloneCopTripminePlaceBehavior : public CAI_TripminePlaceBehavior
	{
		typedef CAI_TripminePlaceBehavior BaseClass;

		virtual bool	ShouldPlaceTripminesWhileMoving() { return true; }

		virtual int		GetMaxTripminesForContext( TripmineContext_t nContext )
		{
			// UNDONE: Place more tripmines around last known location
			/*if ( nContext == TRIPMINE_CONTEXT_LAST_KNOWN )
			{
			}*/

			return BaseClass::GetMaxTripminesForContext( nContext );
		}
	};

	virtual CAI_TripminePlaceBehavior &GetTripminePlaceBehavior( void ) { return m_TripminePlaceBehavior; }
	CCloneCopTripminePlaceBehavior	m_TripminePlaceBehavior;

private:

	static int gm_nBloodAttachment;
	static float gm_flBodyRadius;

	int		m_ArmorValue;
	bool	m_bIsBleeding;

	bool	m_bThrowXenGrenades;

	float	m_flNextWeaponSwitchTime;

	bool	m_bUseAvoidantFlanking;
	bool	m_bUseGestureAltFire;

	// The closest item we can pick up. Used when doing moving pickup
	EHANDLE	m_hClosestItem;

	// Items that we weren't able to get to with SCHED_GET_HEALTHKIT. Clears after combat ends
	CUtlVector<EHANDLE>	m_hIgnoreItems;

	// A gesture we are playing that would prevent us from shooting,
	// throwing a grenade, etc. until it is finished.
	// Also ensures that we don't play multiple at once
	int		m_nActionGesture;
	float	m_flActionGestureEndTime;
};

class CNPC_BadCop : public CNPC_CloneCop
{
	DECLARE_CLASS( CNPC_BadCop, CNPC_CloneCop );
public:
	CNPC_BadCop();

	void		Spawn( void );
	void		Precache( void );
	void		Activate( void );

	// CLASS_PLAYER could have consequences
	Class_T		Classify( void ) { return CLASS_COMBINE; }

	bool		IsBadCop() { return true; }

	void		HandleManhackSpawn( CAI_BaseNPC *pNPC ) {}
};

#endif
