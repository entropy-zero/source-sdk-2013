//=============================================================================//
//
// Purpose:		The first combat template. The first PCU. The father of every Combine soldier.
//				The ultimate "Adrian Shephard at home."
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_PROGENITOR_H
#define NPC_PROGENITOR_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_clonecop.h"
#include "npc_conscript_base.h"
#include "ai_prop_shield.h"

class CSoundPatch;
class CSprite;
class CSpriteTrail;
class CRopeKeyframe;

//---------------------------------------------------------

#define PROGENITOR_SHIELD_NUM_CORNERS 4
#define PROGENITOR_SHIELD_SPRITE			"sprites/glow02.vmt"
#define PROGENITOR_SHIELD_THROWN_MODEL		"models/weapons/w_progenitor_energy_shield_thrown.mdl"

//---------------------------------------------------------

class CNPC_Progenitor : public CAI_PropShieldUser< CAI_ConscriptBase<CNPC_CloneCop> >
{
	DECLARE_CLASS( CNPC_Progenitor, CAI_PropShieldUser< CAI_ConscriptBase<CNPC_CloneCop> > );
	DECLARE_DATADESC();

public:
	CNPC_Progenitor();

	void		Spawn( void );
	void		Precache( void );
	void		Activate( void );

	void		UpdateOnRemove();
	void		StopLoopingSounds();

	int			OnTakeDamage_Alive( const CTakeDamageInfo &info );

	void		GatherConditions();
	bool		CanThrowShield();
	bool		ShouldActivateShield();
	bool		ShouldDeactivateShield( bool &bThrow );
	void		BuildScheduleTestBits( void );
	void		PrescheduleThink();
	void		OnScheduleChange( void );
	int			SelectSchedule( void );
	int			TranslateSchedule( int scheduleType );

	void		HandleAnimEvent( animevent_t *pEvent );

	void		StartTask( const Task_t *pTask );
	void		RunTask( const Task_t *pTask );

	const char	*GetBackupWeaponClass() { return "weapon_css_deagle_laser"; }

	bool		HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );
	bool		ShouldDodgeProjectile( CBaseEntity *pProjectile );

	void		ModifyOrAppendCriteria( AI_CriteriaSet& set );

	bool		MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );
	Activity	NPC_TranslateActivity( Activity eNewActivity );

	bool		CanAltFireEnemy( bool bUseFreeKnowledge );
	bool		CanGrenadeEnemy( bool bUseFreeKnowledge = true );

	bool		IsProximitySatchelCapable();
	bool		ShouldThrowProximitySatchel( bool bDrop = false );
	void		OnThrowProximitySatchel( CBaseEntity *pGrenade );

	bool		ShouldSlideToGoal( AILocalMoveGoal_t *pMoveGoal );
	void		StartSlidingToGoal( AILocalMoveGoal_t *pMoveGoal );

	//-------------------------------------------------------------------------

	// TODO: Allow regular tactical shield?
	virtual const char	*GetShieldModelName() { return "models/weapons/w_progenitor_energy_shield.mdl"; }
	virtual bool		RemoveShieldOnHolster() { return true; }
	virtual bool		CanAimWithShield() { return true; } // false
	bool				CanUseShieldDuringAI() const { return m_iSpawnsWithShield == TRS_NONE; }

	virtual void	OnShieldSpawn( CPropShield *pShield );

	void			RemoveShieldEffects( bool bOnShield = true );
	virtual void	OnShieldRemove( CPropShield *pShield );
	virtual void	OnShieldSlam( CPropShield *pShield );
	CBaseEntity		*CreateShieldProjectile( CPropShield *pShield );

	virtual void		OnUpdateShotRegulator();
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	//-------------------------------------------------------------------------

	bool	CanUseGrapple() const;

	inline bool	IsGrapplePreShot() const { return m_iGrapplePhase == GRAPPLE_PHASE_PRESHOT; }
	inline bool	IsGrappleHookShot() const { return m_iGrapplePhase == GRAPPLE_PHASE_SHOT; }
	inline bool	IsGrappleHooked() const { return m_iGrapplePhase == GRAPPLE_PHASE_HOOKED; }
	inline bool	IsGrappling() const { return m_iGrapplePhase == GRAPPLE_PHASE_GRAPPLING; }

	inline bool	IsNavGrapple() const { return m_iGrappleType == GRAPPLE_TYPE_NAV; }
	inline bool	IsForcedGrapple() const { return m_iGrappleType == GRAPPLE_TYPE_FORCED; }
	inline bool	IsPullObjGrapple() const { return m_iGrappleType == GRAPPLE_TYPE_PULL; }

	bool	GrappleHookMove();
	bool	GrappleMove();
	void	StartGrappling( int nType );
	void	StopGrappling( bool bCancel = true );
	void	RemoveGrapplingEntities();

	bool	ProbeGrappleTarget( const Vector &vecOrigin );
	void	CalculateGrappleImpulse( IPhysicsObject *pPhys, float flMagnitude, Vector &vecImpulse, AngularImpulse &vecAngImpulse );
	void	ApplyGrappleImpulse( CBaseEntity *pEntity, Vector &vecImpulse, AngularImpulse &vecAngImpulse );
	void	GetGrappleDestForEntity( CBaseEntity *pEnt, Vector &vecOrigin, QAngle &angAngles );

	void	InputGrappleToTarget( inputdata_t &inputdata );
	void	InputGrapplePullTarget( inputdata_t &inputdata );

	void	AimGun();

	bool	IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const;
	bool	IsTrueJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const;
	void	OnMovementFailed();
	bool	IsInterruptable();

	Vector	GetHealthItemRange( bool bFollowing = false );
	
	//---------------------------------
	// Navigation & Movement
	//---------------------------------
	class CNavigator : public CAI_ComponentWithOuter<CNPC_Progenitor, CAI_Navigator>
	{
		typedef CAI_ComponentWithOuter<CNPC_Progenitor, CAI_Navigator> BaseClass;
	public:
		CNavigator( CNPC_Progenitor *pOuter )
		 :	BaseClass( pOuter )
		{
		}

		AIMoveResult_t MoveJump();

		void 	MoveCalcBaseGoal( AILocalMoveGoal_t *pMoveGoal );

		void	OnNewGoal();
		void	OnNavComplete();
	};

	friend class CNavigator;
	CAI_Navigator *CreateNavigator() { return new CNavigator( this ); }

	//-------------------------------------------------------------------------

	bool		GetGameTextSpeechParams( hudtextparms_t &params );

protected:
	//=========================================================
	// Progenitor schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE_RUN_AWAY_FROM_TARGET = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE_WALK_AWAY_FROM_TARGET,
		SCHED_COMBINE_MOVE_TO_GRAPPLE_LOS,
		SCHED_COMBINE_GRAPPLE_SHOOT,
		SCHED_COMBINE_GRAPPLE,
		SCHED_COMBINE_GRAPPLE_PULL,
		NEXT_SCHEDULE,

		TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET = BaseClass::NEXT_TASK,
		TASK_COMBINE_GET_PATH_TO_GRAPPLE_LOS,
		TASK_COMBINE_SET_GRAPPLE_SCHEDULE,
		TASK_COMBINE_GRAPPLE_SHOOT,
		TASK_COMBINE_GRAPPLE_MOVE,
		TASK_COMBINE_GRAPPLE_END,
		TASK_COMBINE_GRAPPLE_PULL_OBJ,
		NEXT_TASK,
		
		COND_COMBINE_SHIELD_RETREAT = BaseClass::NEXT_CONDITION,
		COND_COMBINE_CAN_GRAPPLE,
		COND_COMBINE_GRAPPLE_FAILED,
		NEXT_CONDITION
	};

	DEFINE_CUSTOM_AI;

private:

	bool	m_bThrowSatchels;
	CUtlVector<EHANDLE>	m_hSatchels;

	float	m_flNextShieldStateCheck;
	float	m_flShieldDeactivateTime;
	CSoundPatch *m_pShieldSound;
	EHANDLE	m_hShieldLight;
	CHandle<CSprite> m_hShieldSprite;
	CHandle<CSpriteTrail> m_hShieldSpriteTrails[PROGENITOR_SHIELD_NUM_CORNERS];

	enum
	{
		GRAPPLE_PHASE_NONE,
		GRAPPLE_PHASE_PRESHOT,		// About to shoot
		GRAPPLE_PHASE_SHOT,			// Have shot the hook
		GRAPPLE_PHASE_HOOKED,		// Hook is attached
		GRAPPLE_PHASE_GRAPPLING,	// Now grappling
		GRAPPLE_PHASE_LANDING,		// Done grappling, now just landing

		GRAPPLE_TYPE_NONE = 0,
		GRAPPLE_TYPE_NAV,			// Grappling during jump navigation
		GRAPPLE_TYPE_FORCED,		// Grappling to a scripted target
		GRAPPLE_TYPE_PULL,			// Pulling an object towards me
	};

	EHANDLE	m_hGrappleDest;
	Vector	m_vecGrappleDest;
	QAngle	m_vecGrappleAngle;
	Vector	m_vecGrappleLastOrigin;
	float	m_flGrappleStartDistSqr;
	CHandle<CBaseAnimating>	m_hGrapplingHook;
	CHandle<CBaseAnimating>	m_hGrapplingHookProjectile;
	CHandle<CRopeKeyframe>	m_hGrapplingHookCable;
	int		m_iGrapplePhase;
	int		m_iGrappleType;
	int		m_nGrappleLayer;		// If we are using a gesture rather than a sequence
	bool	m_bGrappleAllowed;
	bool	m_bNavEvaluatedJump;	// Tells navigator whether we've evaluated this jump
	bool	m_bNavTrueJump;			// Tells navigator we're doing an actual jump, not a grapple
};

// ------------------------------------------------------------------------------------------ //
// Progenitor's thrown shield projectile
// ------------------------------------------------------------------------------------------ //
class CPropProgenitorThrownShield : public CPhysicsProp
{
	DECLARE_CLASS( CPropProgenitorThrownShield, CPhysicsProp );
	DECLARE_DATADESC();
public:
	CPropProgenitorThrownShield();
	
	void Precache();
	void Spawn();

	void AnimateThink();
	void EnableGravityThink();

	bool HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );
	int OnTakeDamage( const CTakeDamageInfo &info );
	void Event_Killed( const CTakeDamageInfo &info );
	void Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info );

	bool OverridePropdata( void ) { return true; }

	float	m_flNextDangerSoundTime;
};

#endif
