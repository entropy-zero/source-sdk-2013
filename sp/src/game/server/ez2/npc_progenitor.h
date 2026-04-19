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

	void		GatherConditions();
	bool		ShouldActivateShield();
	bool		ShouldDeactivateShield();
	void		BuildScheduleTestBits( void );
	void		PrescheduleThink();
	int			SelectSchedule( void );
	int			TranslateSchedule( int scheduleType );

	void		StartTask( const Task_t *pTask );

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

	// TODO: Allow regular tactical shield?
	virtual const char	*GetShieldModelName() { return "models/weapons/w_progenitor_energy_shield.mdl"; }
	virtual bool		RemoveShieldOnHolster() { return true; }
	virtual bool		CanAimWithShield() { return true; } // false
	bool				CanUseShieldDuringAI() const { return m_iSpawnsWithShield == TRS_NONE; }

	virtual void	OnShieldSpawn( CPropShield *pShield );
	virtual void	OnShieldRemove( CPropShield *pShield );

	virtual void		OnUpdateShotRegulator();
	WeaponProficiency_t CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	bool		GetGameTextSpeechParams( hudtextparms_t &params );

protected:
	//=========================================================
	// Progenitor schedules
	//=========================================================
	enum
	{
		SCHED_COMBINE_RUN_AWAY_FROM_TARGET = BaseClass::NEXT_SCHEDULE,
		SCHED_COMBINE_WALK_AWAY_FROM_TARGET,
		NEXT_SCHEDULE,

		TASK_COMBINE_FIND_BACKAWAY_FROM_TARGET = BaseClass::NEXT_TASK,
		NEXT_TASK,
		
		COND_COMBINE_SHIELD_RETREAT = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION
	};

	DEFINE_CUSTOM_AI;

private:

	bool	m_bThrowSatchels;
	CUtlVector<EHANDLE>	m_hSatchels;

	float	m_flNextShieldStateCheck;
	CSoundPatch *m_pShieldSound;
	EHANDLE	m_hShieldLight;
	CHandle<CSprite> m_hShieldSprite;
};

#endif
