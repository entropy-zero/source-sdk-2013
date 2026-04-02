//=============================================================================//
//
// Purpose:		Tripmine that activates turrets.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef PROP_TURRET_MINE_H
#define PROP_TURRET_MINE_H
#ifdef _WIN32
#pragma once
#endif

#include "props.h"

class CBeam;
class CNPC_FloorTurret;

DECLARE_AUTO_LIST( IPropTurretMineAutoList );
class CPropTurretMine : public CDynamicProp, public IPropTurretMineAutoList
{
	DECLARE_CLASS( CPropTurretMine, CDynamicProp );
	DECLARE_DATADESC();

public:
	void Spawn();
	void Activate();
	void Precache( void );

	void MakeBeam();
	void TurnOnLaser();
	void TurnOffLaser();

	void Enable( CBaseEntity *pActivator );
	void Disable( CBaseEntity *pActivator );

	void Trip( CBaseEntity *pActivator );

	void	LaserTurnOnThink();
	void	LaserThink();
	void	ActivatedThink();

	static void		OnTurretActivate( CNPC_FloorTurret *pTurret );
	static void		OnTurretDeath( CNPC_FloorTurret *pTurret );
	static void		OnTurretRetire( CNPC_FloorTurret *pTurret );
	static void		OnTurretRevive( CNPC_FloorTurret *pTurret );

	// Inputs
	void	InputToggle( inputdata_t &inputdata );
	void	InputEnable( inputdata_t &inputdata );
	void	InputDisable( inputdata_t &inputdata );
	void	InputTrip( inputdata_t &inputdata );

private:

	bool	m_bDisabled;
	int		m_nBeamAttach = -1; // not saved

	EHANDLE		m_hLastHitEntity;
	CHandle<CBeam>		m_hLaser;
	float		m_flGracePeriod = 0.5f;

	string_t	m_iszTurretName;
	CUtlVector< CHandle<CNPC_FloorTurret> >	m_hTurrets;

	COutputEvent	m_OnTripped;
	COutputEvent	m_OnEnabled;
	COutputEvent	m_OnDisabled;
};

#endif
