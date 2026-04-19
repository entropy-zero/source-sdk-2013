//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GRENADE_TRIPMINE_H
#define GRENADE_TRIPMINE_H
#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"

class CBeam;
class CSprite;

#ifdef MAPBASE
#define SF_TRIPMINE_START_INACTIVE (1 << 0)
#endif

class CTripmineGrenade : public CBaseGrenade
{
public:
	DECLARE_CLASS( CTripmineGrenade, CBaseGrenade );
#ifdef EZ2
	DECLARE_SERVERCLASS();
#endif

	CTripmineGrenade();
	void Spawn( void );
	void Precache( void );
	void UpdateOnRemove( void );

#if 0 // FIXME: OnTakeDamage_Alive() is no longer called now that base grenade derives from CBaseAnimating
	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
#endif	
	void WarningThink( void );
	void PowerupThink( void );
	void BeamBreakThink( void );
	void DelayDeathThink( void );
	void Event_Killed( const CTakeDamageInfo &info );

	void MakeBeam( void );
	void KillBeam( void );

#ifdef EZ2
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	int UpdateTransmitState();

	bool CanBeSeenBy( CAI_BaseNPC *pNPC );
	bool IsTripmineVisibleTo( CAI_BaseNPC *pNPC, const Vector &vecOrigin );
#endif

#ifdef MAPBASE
	void PowerUp();

	void InputActivate( inputdata_t &inputdata );
	void InputDeactivate( inputdata_t &inputdata );
	void InputSetOwner( inputdata_t &inputdata ) { m_hOwner = inputdata.value.Entity(); }

	COutputEvent m_OnExplode;
#endif

#ifdef EZ2
	virtual bool	TargetShouldDetonate(CBaseEntity* pTarget);

	const Vector	&GetEnd() const { return m_vecEnd; }
	const Vector	&GetDir() const { return m_vecDir; }
	const Vector	GetActualEnd() const { return GetAbsOrigin() + m_vecDir * (2048 * m_flBeamLength); }

	Class_T			TripmineClassify() { return m_nTripmineClass; }

	void		SetVisibleToNPCs( bool bVisible ) { m_bVisibleToNPCs = bVisible; }
#endif


public:
	EHANDLE		m_hOwner;

#ifdef MAPBASE
	float		m_flPowerUpTime;
	EHANDLE		m_hAttacker;
#endif

private:
	float		m_flPowerUp;
	Vector		m_vecDir;
	Vector		m_vecEnd;
	float		m_flBeamLength;

	CBeam		*m_pBeam;
	Vector		m_posOwner;
	Vector		m_angleOwner;

#ifdef EZ2
	Class_T     m_nTripmineClass;
	string_t	m_nTripmineClassString;
	color32     m_TripmineColor;

	// Would be better if these were clientside
	CHandle<CSprite>	m_hStartSprite;
	CHandle<CSprite>	m_hEndSprite;

	bool		m_bTripped;

	EHANDLE		m_hPlacer;

	bool		m_bVisibleToNPCs;
#endif

	DECLARE_DATADESC();
};

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: Custom trace filter used for tripmine laser traces
//-----------------------------------------------------------------------------
class CTraceFilterTripmineBeam : public CTraceFilterSimple
{
public:
	CTraceFilterTripmineBeam( CTripmineGrenade *pTripmine, int collisionGroup );
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

private:
	CTripmineGrenade	*m_pTripmine;
};
#endif

#endif // GRENADE_TRIPMINE_H
