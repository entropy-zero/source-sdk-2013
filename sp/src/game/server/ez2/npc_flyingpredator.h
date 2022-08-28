//=============================================================================//
//
// Purpose: Flying enemy with similar behaviors to the bullsquid.
//			Fires spiky projectiles at enemies.
// Author: 1upD
//
//=============================================================================//

#ifndef NPC_FLYINGPREDATOR_H
#define NPC_FLYINGPREDATOR_H

#include "ai_basenpc.h"
#include "npc_basepredator.h"

enum FlyState_t
{
	FlyState_Walking = 0,
	FlyState_Landing,
	FlyState_Falling,
	FlyState_Dive,
	FlyState_Flying,
	FlyState_Ceiling,
	FlyState_Last,
};


class CNPC_FlyingPredator : public CNPC_BasePredator
{
	DECLARE_CLASS( CNPC_FlyingPredator, CNPC_BasePredator );
	DECLARE_DATADESC();

public:
	void Spawn( void );
	void Precache( void );
	
	virtual const char * GetSoundscriptClassname() { return m_bIsBaby ? "NPC_Stukapup" : "NPC_FlyingPredator"; }

	virtual int RangeAttack1Conditions( float flDot, float flDist );

	float GetMaxSpitWaitTime( void );
	float GetMinSpitWaitTime( void );
	float GetEatInCombatPercentHealth( void );

	bool IsPrey( CBaseEntity* pTarget ) { return pTarget->Classify() == CLASS_EARTH_FAUNA || pTarget->Classify() == CLASS_ALIEN_FAUNA; }
	virtual bool IsSameSpecies( CBaseEntity* pTarget ) { return pTarget ? ClassMatches( pTarget->GetClassname() ) : false; } // Is this NPC the same species as me?

	Disposition_t IRelationType( CBaseEntity *pTarget );

	virtual void	GatherConditions( void );
	bool CanLand();
	bool CeilingNear();

	virtual Activity NPC_TranslateActivity( Activity eNewActivity );
	virtual int TranslateSchedule( int scheduleType );
	virtual int SelectSchedule( void );

	void StartTask ( const Task_t *pTask );
	void RunTask( const Task_t * pTask );

	// Override OnFed() to set this stukabat ready to spawn after eating
	virtual void OnFed();
	virtual bool ShouldEatInCombat() { return m_bIsBaby || BaseClass::ShouldEatInCombat(); }

	virtual bool ShouldAttackObstruction( CBaseEntity *pEntity ) { return false; }

	// No fly. Jump good!
	bool IsJumpLegal( const Vector & startPos, const Vector & apex, const Vector & endPos ) const { return true; }


	bool		ShouldGib( const CTakeDamageInfo &info ) { return true; };
	bool		CorpseGib( const CTakeDamageInfo &info );
	void		ExplosionEffect( void );

	bool SpawnNPC( const Vector position, bool isBaby );

	virtual bool	QueryHearSound( CSound *pSound );

	virtual bool	HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

// Flight
public:
	void SetFlyingState( FlyState_t eState );

	virtual bool		OverrideMove( float flInterval );

	void MoveToDivebomb( float flInterval );
	void MoveInDirection( float flInterval, const Vector &targetDir,
		float accelXY, float accelZ, float decay );

	virtual void	StartTouch( CBaseEntity *pOther );

	void InputForceFlying( inputdata_t &inputdata );
	void InputFly( inputdata_t &inputdata );

protected:
	FlyState_t m_tFlyState;
	Vector				m_vecDiveBombDirection;		// The direction we dive bomb.
	float m_flDiveBombRollForce;

	DEFINE_CUSTOM_AI;
};
#endif // NPC_FLYINGPREDATOR_H