//=============================================================================//
//
// Purpose:		Reads level lighting directly so that it can be factored into stealth
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_LIGHTING_H
#define AI_STEALTH_LIGHTING_H

#include "ai_stealth_manager.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CStealthLightingSystem
{
public:

	void	InitLights();
	bool	IsInitialized() const;
	bool	IsAvailable() const;
	void	ClearEntityCache();
	void	Clear();

	float	GetLightRatioForEntity( CBaseEntity *pEntity, CBaseEntity *pLooker = NULL );

	float	GetLightLevelAtEntity( CBaseEntity *pEntity );
	float	GetLightLevelAtPoint( const Vector &vecOrigin );

private:

	int m_nWorldLights;
	dworldlight_t *m_pWorldLights;

	//---------------------------------

	struct EntityLightLevel_t
	{
		EHANDLE	hEnt;
		float	flLightLevel;
		float	flLastCheckTime;
		Vector	vecLastPosition;
	};

	CUtlVector<EntityLightLevel_t>	m_EntLightLevels;
};

extern CStealthLightingSystem g_StealthLightingSystem;

//-----------------------------------------------------------------------------

#endif
