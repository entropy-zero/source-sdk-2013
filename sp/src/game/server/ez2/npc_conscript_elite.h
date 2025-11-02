//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_CONSCRIPT_ELITE_H
#define NPC_CONSCRIPT_ELITE_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_conscript_base.h"
#include "npc_combine.h"
#include "ai_stealth_utils.h"

class CBeam;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_ConscriptElite : public CAI_ConscriptBase<CNPC_Combine>, public ICloakCompromisable
{
	DECLARE_CLASS( CNPC_ConscriptElite, CAI_ConscriptBase<CNPC_Combine> );
	DECLARE_DATADESC();

public:
	CNPC_ConscriptElite();

	void			Spawn();
	void			Precache( void );
	void			OnRestore( void );

	void			AddLaserToGun( CBaseCombatWeapon *pWeapon );
	void			TurnOnLaser();
	void			TurnOffLaser();

	void			ModifyOrAppendCriteria( AI_CriteriaSet &set );

	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual bool	IsLightDamage( const CTakeDamageInfo &info );
	virtual bool	IsHeavyDamage( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual bool	CanBeSneakAttacked( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	float			GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );

	float			GetLaserDotToTarget( CBaseEntity *pTarget );
	float			GetEyeDotToTarget( CBaseEntity *pTarget );
	bool			TargetCrossingLaser( CBaseEntity *pTarget );
	bool			ShouldAimLaserAtEnemy( CBaseEntity *pEnemy );
	void			LaserThink();

	virtual void	PrescheduleThink( void );
	void			OnScheduleChange( void );
	int 			TranslateSchedule( int scheduleType );

	Activity		Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	void			HandleAnimEvent( animevent_t *pEvent );
	WeaponProficiency_t		CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	bool			CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType );

	void			InputTurnOnLaser( inputdata_t &inputdata ) { TurnOnLaser(); }
	void			InputTurnOffLaser( inputdata_t &inputdata ) { TurnOffLaser(); }
	void			InputStartLaserTracking( inputdata_t &inputdata ) { m_bLaserTrackEnemy = true; }
	void			InputStopLaserTracking( inputdata_t &inputdata ) { m_bLaserTrackEnemy = false; }

private:

	bool			m_bLaserOn;
	bool			m_bLaserTrackEnemy;

	CHandle<CBeam>	m_hGunLaser;
	EHANDLE			m_hGunLaserEnd;
	EHANDLE			m_hGunLaserHitTarget;
	int				m_nGunLaserAttachment;

	bool			m_bLaserAimsAtEnemy;
	float			m_flLaserTargetTime;

	DEFINE_CUSTOM_AI;
};

#endif
