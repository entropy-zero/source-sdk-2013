//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "basecombatweapon_shared.h"

#ifndef BASEHLCOMBATWEAPON_SHARED_H
#define BASEHLCOMBATWEAPON_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#if defined( CLIENT_DLL )
#define CBaseHLCombatWeapon C_BaseHLCombatWeapon
#endif

class CBaseHLCombatWeapon : public CBaseCombatWeapon
{
#if !defined( CLIENT_DLL )
#ifndef _XBOX
	DECLARE_DATADESC();
#else
protected:
	DECLARE_DATADESC();
private:
#endif
#endif

	DECLARE_CLASS( CBaseHLCombatWeapon, CBaseCombatWeapon );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	virtual bool	WeaponShouldBeLowered( void );

			bool	CanLower();
	virtual bool	Ready( void );
	virtual bool	Lower( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo );
	virtual void	WeaponIdle( void );

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );

	virtual Vector	GetBulletSpread( WeaponProficiency_t proficiency );
	virtual float	GetSpreadBias( WeaponProficiency_t proficiency );

	virtual const	WeaponProficiencyInfo_t *GetProficiencyValues();
	static const	WeaponProficiencyInfo_t *GetDefaultProficiencyValues();

	virtual void	ItemHolsterFrame( void );

#ifdef EZ2
	//
	// Dual wielding
	//
	virtual void		ItemPostFrame( void );
	virtual const char *GetViewModel( int viewmodelindex = 0 ) const;
	
	virtual CHudTexture const	*GetSpriteActive( void ) const;
	virtual CHudTexture const	*GetSpriteInactive( void ) const;

	virtual bool			CanDualWield() const { return false; }
	virtual bool			DualWieldOverridesSecondary() const { return true; }
	bool					IsDualWielding() const { return m_hLeftHandGun != NULL; }

	CBaseAnimating			*GetLeftHandGun() const { return m_hLeftHandGun; }
	void					SetLeftHandGun( CBaseAnimating *pGun ) { m_hLeftHandGun = pGun; }
	
	int GetMaxClip1( void ) const
	{
		return IsDualWielding() ? BaseClass::GetMaxClip1() * 2 : BaseClass::GetMaxClip1();
	}
	int GetDefaultClip1( void ) const
	{
		return IsDualWielding() ? BaseClass::GetDefaultClip1() * 2 : BaseClass::GetDefaultClip1();
	}

#ifdef GAME_DLL
	virtual void		Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );
	virtual void		FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, Vector &vecShootOrigin, Vector &vecShootDir ) {}
	virtual Activity	ActivityOverride( Activity baseAct, bool *pRequired );

	virtual void		Drop( const Vector &vecVelocity );

	CBaseAnimating			*CreateLeftHandGun();
	void					InputCreateLeftHandGun( inputdata_t &inputdata );
	void					InputSetLeftHandGun( inputdata_t &inputdata );

protected:

	CNetworkHandle( CBaseAnimating, m_hLeftHandGun );

private:

	static acttable_t m_dual_acttable[];

public:
#else
	CHandle<C_BaseAnimating>	m_hLeftHandGun;
#endif
#endif

	int				m_iPrimaryAttacks;		// # of primary attacks performed with this weapon
	int				m_iSecondaryAttacks;	// # of secondary attacks performed with this weapon

protected:

	bool			m_bLowered;			// Whether the viewmodel is raised or lowered
	float			m_flRaiseTime;		// If lowered, the time we should raise the viewmodel
	float			m_flHolsterTime;	// When the weapon was holstered
};

#endif // BASEHLCOMBATWEAPON_SHARED_H
