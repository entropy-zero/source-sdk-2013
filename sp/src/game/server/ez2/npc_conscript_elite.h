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
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	CNPC_ConscriptElite();

	void			Spawn();
	void			Precache( void );
	void			OnRestore( void );

	bool			GunSupportsLaser( CBaseCombatWeapon *pWeapon, int &iAttachment, bool bForce = false );
	void			AddLaserToGun( CBaseCombatWeapon *pWeapon );
	void			RemoveLaserFromGun( CBaseCombatWeapon *pWeapon );
	void			TurnOnLaser();
	void			TurnOffLaser();

	void			ModifyOrAppendCriteria( AI_CriteriaSet &set );
	const char		*GetSquadIDPrefix();

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

	void			Weapon_Equip( CBaseCombatWeapon *pWeapon );			// Adds weapon to player
	void			Weapon_Drop( CBaseCombatWeapon *pWeapon, const Vector *pvecTarget = NULL, const Vector *pVelocity = NULL );
	bool			Weapon_Switch( CBaseCombatWeapon *pWeapon, int viewmodelindex = 0 );		// Switch to given weapon if has ammo (false if failed)
	Activity		Weapon_TranslateActivity( Activity baseAct, bool *pRequired );
	void			HandleAnimEvent( animevent_t *pEvent );
	WeaponProficiency_t		CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

	bool			CanSeeThroughCloak( CBaseCombatCharacter *pCloaker, float flCloakFactor, int &iCompromiseType );

	void			InputTurnOnLaser( inputdata_t &inputdata );
	void			InputTurnOffLaser( inputdata_t &inputdata );
	void			InputTurnOnLaserInstant( inputdata_t &inputdata ) { TurnOnLaser(); }
	void			InputTurnOffLaserInstant( inputdata_t &inputdata ) { TurnOffLaser(); }

private:

	bool			m_bLaserOn;
	bool			m_bCanUseLaserDuringAI;		// Allows elite to use laser dynamically
	bool			m_bAlwaysAddLaser;			// Always adds a laser to the elite's gun, even if it doesn't have an attachment

	CNetworkHandle( CBeam,	m_hGunLaser );
	EHANDLE			m_hGunLaserEnd;
	EHANDLE			m_hGunLaserHitTarget;
	int				m_nGunLaserAttachment;

	bool			m_bLaserAimsAtEnemy;
	float			m_flLaserTargetTime;

	CNetworkVar( Vector, m_vecGunLaserDir );

	DEFINE_CUSTOM_AI;
};

#endif
