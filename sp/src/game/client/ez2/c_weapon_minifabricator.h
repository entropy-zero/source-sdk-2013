#ifndef C_Weapon_MiniFabricator_H
#define C_Weapon_MiniFabricator_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlcombatweapon.h"

class C_Weapon_MiniFabricator : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_Weapon_MiniFabricator, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	bool			CanBeSelected( void );

	bool			CanSwitchToWhileEmpty() { return true; } // Can switch to while empty
};

#endif // C_Weapon_MiniFabricator_H