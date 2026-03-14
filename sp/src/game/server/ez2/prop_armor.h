//=============================================================================//
//
// Purpose:		Dynamic prop intended to be parented to NPCs and protect against
//				bullets or projectiles.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef PROP_ARMOR_H
#define PROP_ARMOR_H
#ifdef _WIN32
#pragma once
#endif

#include "props.h"

class CArmorProp : public CDynamicProp
{
	DECLARE_CLASS( CArmorProp, CDynamicProp );

public:
	void Spawn();
	void Precache( void );

	bool	PassesDamageFilter( const CTakeDamageInfo &info );
	int		OnTakeDamage( const CTakeDamageInfo &inputInfo );
	void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void	Event_Killed( const CTakeDamageInfo &info );
	virtual bool	ShouldBlockTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator ) { return false; }
	virtual void	ModifyTraceAttackDamage( CTakeDamageInfo &info, float flPenetrationScale = 1.0f ) {}
	virtual int		GetHitgroup() const { return HITGROUP_GENERIC; }

	static float	GetPenetrationScale( const CTakeDamageInfo &info );

	inline bool HasNPCParent() { return (GetParent() && GetParent()->IsNPC()); }
	inline CAI_BaseNPC *GetNPCParent() { return GetParent()->MyNPCPointer(); }

private:

	// Ammo types used to calculate armor penetration.
	// Consider standardizing this if more ammo types have unique penentration values.
	static bool m_sbAmmoTypesLoaded;
	static int m_nAmmoTypePistol, m_nAmmoTypeGaussPistol, m_nAmmoTypeAR2, m_nAmmoType556mm, m_nAmmoType762mm;
};

#endif
