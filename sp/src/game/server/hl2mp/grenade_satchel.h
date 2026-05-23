//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Satchel Charge
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef	SATCHEL_H
#define	SATCHEL_H

#ifdef _WIN32
#pragma once
#endif

#include "basegrenade_shared.h"
#include "hl2mp/weapon_slam.h"

class CSoundPatch;
class CSprite;

class CSatchelCharge : public CBaseGrenade
{
public:
	DECLARE_CLASS( CSatchelCharge, CBaseGrenade );
#ifdef EZ2
	DECLARE_SERVERCLASS();
#endif

	void			Spawn( void );
	void			Precache( void );
	void			BounceSound( void );
	void			SatchelTouch( CBaseEntity *pOther );
	void			SatchelThink( void );
	
	// Input handlers
	void			InputExplode( inputdata_t &inputdata );

#ifdef EZ2
	void			Event_Killed( const CTakeDamageInfo &info );
	void			Explode( trace_t *pTrace, int bitsDamageType );
	int				UpdateTransmitState();
	bool			CanBeSeenBy( CAI_BaseNPC *pNPC );
	void			SetVisibleToNPCs( bool bVisible ) { m_bVisibleToNPCs = bVisible; }

	void			SetProximitySatchel( bool bToggle ) { m_bProximitySatchel = bToggle; }
	void			ProximitySatchelThink( void );
	void			ProximitySatchelPreDetonateThink( void );
#endif

	float			m_flNextBounceSoundTime;
	bool			m_bInAir;
	Vector			m_vLastPosition;

public:
	CWeapon_SLAM*	m_pMyWeaponSLAM;	// Who shot me..
	bool			m_bIsAttached;
	void			Deactivate( void );

	CSatchelCharge();
	void			UpdateOnRemove();

	DECLARE_DATADESC();

private:
	void				CreateEffects( void );
	CHandle<CSprite>	m_hGlowSprite;

#ifdef EZ2
	EHANDLE				m_hAttacker;

	bool				m_bVisibleToNPCs;

	// Satchel that explodes when an enemy gets near (requires thrower)
	bool				m_bProximitySatchel;
	bool				m_bProximityWarnAlt;
	int					m_nSatchelWarnTicks;
	EHANDLE				m_hProximityLight;
#endif
};

#ifdef EZ2
CSatchelCharge *Satchel_CreateProximitySatchel( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner );
#endif

#endif	//SATCHEL_H
