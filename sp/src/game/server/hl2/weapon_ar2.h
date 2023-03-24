//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "basehlcombatweapon.h"

class CWeaponAR2 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHLMachineGun );

	CWeaponAR2();

	DECLARE_SERVERCLASS();

	#define MIN_SPREAD_COMPONENT 0.00001 // Was 0.13053
										 // Default is: 0.03490

	#define MAX_SPREAD_COMPONENT 0.03490 // Was 0.25881
										 // Default is: 0.34202

	#define MIN_NPC_SPREAD_COMPONENT 0.13053 // Employee8: We don't want NPCs to be as accurate as the player!

	#define MAX_NPC_SPREAD_COMPONENT 0.34202

	void	ItemPostFrame( void );
	void	Precache( void );

#ifdef EZ1
	void	PrimaryAttack(void); // Breadman
#endif	
	void	SecondaryAttack( void );
	void	BurstAttack(int burstSize, float cycleRate); // Mark: Thing i believe is from 1upD's weapon_smg2.cpp
													     // it for AR2 Burst Fire Code
	void	DelayedAttack( void );

	const char *GetTracerType( void ) { return "AR2Tracer"; }

	void	AddViewKick( void );

	void	FireNPCPrimaryAttack( CBaseCombatCharacter* pOperator, Vector& vecShootOrigin, Vector& vecShootDir);
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	float	GetBurstCycleRate(void) { return 0.3f; };
	int		GetMinBurst( void ) { return 3; }
	int		GetMaxBurst( void ) { return 3; }
	int		GetBurstSize(void) { return 3; };
#ifdef EZ1
	float	GetFireRate(void) { return 0.1f; } // Breadman - lowered for prototype
											    // Mark: but changed again for EZU, used to be 0.012f
#else
	float	GetFireRate( void ) { return 0.1f; }
#endif

	bool	CanHolster( void );
	bool	Reload( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity( void );
	
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	virtual const Vector& GetBulletSpread(void)
	{

		float l_flSpreadRegen = ((gpGlobals->curtime - m_flLastPrimaryAttack) / GetBurstCycleRate()) * (MAX_SPREAD_COMPONENT - MIN_SPREAD_COMPONENT) / 2;
		m_flSpreadComponent -= l_flSpreadRegen;

		// Minimum spread
		if (m_flSpreadComponent < MIN_SPREAD_COMPONENT)
			m_flSpreadComponent = MIN_SPREAD_COMPONENT;

		// Maximum spread
		if (m_flSpreadComponent > MAX_SPREAD_COMPONENT)
			m_flSpreadComponent = MAX_SPREAD_COMPONENT;


		static const Vector cone = Vector(m_flSpreadComponent, m_flSpreadComponent, m_flSpreadComponent);
		return cone;
	}
	// Employee8: NPCs initially use the above function, which still used the player values. I'm rerouting them to this one, just to be sure.
		virtual const Vector& GetNPCBulletSpread(void)
	{

		float l_flSpreadRegen = ((gpGlobals->curtime - m_flLastPrimaryAttack) / GetBurstCycleRate()) * (MAX_NPC_SPREAD_COMPONENT - MIN_NPC_SPREAD_COMPONENT) / 2;
		m_flSpreadComponent -= l_flSpreadRegen;

		// Minimum spread
		if (m_flSpreadComponent < MIN_NPC_SPREAD_COMPONENT)
			m_flSpreadComponent = MIN_NPC_SPREAD_COMPONENT;

		// Maximum spread
		if (m_flSpreadComponent > MAX_NPC_SPREAD_COMPONENT)
			m_flSpreadComponent = MAX_NPC_SPREAD_COMPONENT;


		static const Vector cone = Vector(m_flSpreadComponent, m_flSpreadComponent, m_flSpreadComponent);
		return cone;
	}
	
	const WeaponProficiencyInfo_t *GetProficiencyValues();

protected:

	int						m_iBurstSize;
	float					m_flLastPrimaryAttack;
	float					m_flSpreadComponent;
	float					m_flLastAttack;
	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;
	
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
};


#endif	//WEAPONAR2_H
