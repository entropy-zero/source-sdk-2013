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
#include "ai_weaponlaser.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNPC_ConscriptElite : public CAI_WeaponLaserUser< CAI_ConscriptBase<CNPC_Combine> >
{
	DECLARE_CLASS( CNPC_ConscriptElite, CAI_WeaponLaserUser< CAI_ConscriptBase<CNPC_Combine> > );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	CNPC_ConscriptElite();

	void			Spawn();
	void			Precache( void );
	
	const char		*GetSquadIDPrefix();

	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual bool	IsLightDamage( const CTakeDamageInfo &info );
	virtual bool	IsHeavyDamage( const CTakeDamageInfo &info );
	virtual int		OnTakeDamage_Alive( const CTakeDamageInfo &info );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	virtual bool	CanBeSneakAttacked( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	float			GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info );
	
	void			StartLaserThink();
	void			LaserThink();

	virtual void	PrescheduleThink( void );
	int 			TranslateSchedule( int scheduleType );

	WeaponProficiency_t		CalcWeaponProficiency( CBaseCombatWeapon *pWeapon );

private:

	DEFINE_CUSTOM_AI;
};

#endif
