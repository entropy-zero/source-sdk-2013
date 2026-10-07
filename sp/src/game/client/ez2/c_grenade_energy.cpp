//=============================================================================//
//
// Purpose:		Custom clientside implementation of CGrenadeEnergy
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "basegrenade_shared.h"
#include "r_efx.h"
#include "dlight.h"

ConVar	cl_dlight_grenade_energy( "cl_dlight_grenade_energy", "1" );

#define DLIGHT_RADIUS		200.0f
#define DLIGHT_FADE_TIME	0.3f

class C_GrenadeEnergy : public C_BaseGrenade
{
public:
	DECLARE_CLASS( C_GrenadeEnergy, C_BaseGrenade );
	DECLARE_CLIENTCLASS();

	C_GrenadeEnergy();
	~C_GrenadeEnergy();

	void	OnDataChanged( DataUpdateType_t type );
	void	ClientThink();
	
	dlight_t	*m_pDLight;
};

LINK_ENTITY_TO_CLASS( grenade_energy, C_GrenadeEnergy );

IMPLEMENT_CLIENTCLASS_DT( C_GrenadeEnergy, DT_GrenadeEnergy, CGrenadeEnergy )
END_RECV_TABLE()

C_GrenadeEnergy::C_GrenadeEnergy()
{
}

C_GrenadeEnergy::~C_GrenadeEnergy()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GrenadeEnergy::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED && cl_dlight_grenade_energy.GetBool() && !m_pDLight )
	{
		m_pDLight = effects->CL_AllocDlight ( index );
		m_pDLight->origin = GetAbsOrigin();
		m_pDLight->color.r = 73;
		m_pDLight->color.g = 135;
		m_pDLight->color.b = 255;
		m_pDLight->color.exponent = 4;
		m_pDLight->radius = DLIGHT_RADIUS;
		m_pDLight->die = gpGlobals->curtime + DLIGHT_FADE_TIME;
		m_pDLight->decay = DLIGHT_RADIUS / DLIGHT_FADE_TIME;

		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_GrenadeEnergy::ClientThink()
{
	if ( m_pDLight != NULL )
	{
		m_pDLight->origin = GetAbsOrigin();
		m_pDLight->radius = DLIGHT_RADIUS;
		m_pDLight->die = gpGlobals->curtime + DLIGHT_FADE_TIME;
		m_pDLight->decay = DLIGHT_RADIUS / DLIGHT_FADE_TIME;
	}
}
