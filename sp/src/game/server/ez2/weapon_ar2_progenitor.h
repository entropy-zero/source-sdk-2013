//=============================================================================//
//
// Purpose:		An experimental pulse MG carried by the Progenitor.
//				(the prototype AR2 coughs awkwardly from the audience)
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef	WEAPONAR2PROGENITOR_H
#define	WEAPONAR2PROGENITOR_H

#include "weapon_ar2.h"
#include "grenade_energy.h"

class CWeaponAR2Progenitor : public CWeaponAR2
{
public:
	DECLARE_CLASS( CWeaponAR2Progenitor, CWeaponAR2 );

	CWeaponAR2Progenitor();

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void	Precache();

	void	PrimaryAttack(void);
	void	SecondaryAttack( void );
	void	DelayedAttack( void );

	void	AddViewKick( void );

	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );

	int		GetMinBurst( void ) { return 5; }
	int		GetMaxBurst( void ) { return 10; }
	float	GetFireRate( void ) { return 0.1f; }

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone;

		cone = VECTOR_CONE_6DEGREES;

		return cone;
	}
};

class CGrenadeProgenitorEnergy : public CGrenadeEnergy
{
public:
	DECLARE_CLASS( CGrenadeProgenitorEnergy, CGrenadeEnergy );

	static CGrenadeProgenitorEnergy *Shoot( CBaseEntity* pOwner, const Vector &vStart, const Vector &vVelocity, QAngle &vShootAng, float flDetonateTime );

public:
	void		Spawn( void );
	void		Precache( void );
	void		GrenadeEnergyThink( void );

	virtual void Detonate( void );

	DECLARE_DATADESC();

	float		m_flNextShockTime;

	int			m_nBeamModelIndex;
};

#endif	//WEAPONAR2PROGENITOR_H
