//=============================================================================//
//
// Purpose: Biological soldiers. Predators that serve as grunts for Race X
//		alongside Shock Troopers.
//
//		They have similar behavior to other predators such as bullsquids,
//		but have a limited "clip" of spines that they must reload.
// Author: 1upD
//
//=============================================================================//

#ifndef NPC_PITDRONE_H
#define NPC_PITDRONE_H

#include "ai_basenpc.h"
#include "npc_basepredator.h"

class CNPC_PitDrone : public CNPC_BasePredator
{
	DECLARE_CLASS( CNPC_PitDrone, CNPC_BasePredator );
	DECLARE_DATADESC();

public:
	CNPC_PitDrone();

	void Spawn( void );
	void Precache( void );
	Class_T	Classify( void );
	
	virtual const char * GetSoundscriptClassname() { return "NPC_PitDrone"; }

	float MaxYawSpeed ( void );

	virtual bool ShouldEatInCombat();

	int RangeAttack1Conditions( float flDot, float flDist );

	void HandleAnimEvent( animevent_t *pEvent );

	virtual int TranslateSchedule( int scheduleType );

	// Projectile methods
	virtual float GetProjectileDamge();
	virtual float GetBiteDamage( void );
	virtual float GetWhipDamage( void );

	// On feeding
	virtual void OnFed();

	// Update bodygroups based on ammo count
	void UpdateAmmoBodyGroups( void );

	// Pit drones in opposing force hated headcrabs - this was an oversight, but let's keep it
	bool IsPrey( CBaseEntity* pTarget ) { return pTarget->Classify() == CLASS_HEADCRAB; }

	DEFINE_CUSTOM_AI;

private:	
	int m_iClip;
	int m_iAmmo;
};
#endif // NPC_PITDRONE_H