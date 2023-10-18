//=============================================================================//
//
// Purpose:		Clientside implementation of the Crab Synth NPC. Currently only
//				used for the grenade's material proxy.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "basegrenade_shared.h"
#include "particle_parse.h"
#include "functionproxy.h"
#include "toolframework_client.h"

class C_CrabGrenade : public C_BaseGrenade
{
public:
	DECLARE_CLASS( C_CrabGrenade, C_BaseGrenade );
	DECLARE_CLIENTCLASS();

	//C_CrabGrenade();
	//~C_CrabGrenade();

	float m_flStartTime;
	float m_flClientDetonateTime;
};

LINK_ENTITY_TO_CLASS( npc_crab_grenade, C_CrabGrenade );

IMPLEMENT_CLIENTCLASS_DT( C_CrabGrenade, DT_CrabGrenade, CCrabGrenade )

	RecvPropFloat( RECVINFO( m_flStartTime ) ),
	RecvPropFloat( RECVINFO( m_flClientDetonateTime ) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Controls the crab grenade
//-----------------------------------------------------------------------------
class CProxyCrabGrenade : public IMaterialProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
	void Release();
	IMaterial *GetMaterial();

	IMaterialVar *m_RefractAmount;
	float m_flStartRefractAmount;

	IMaterialVar *m_RefractTint;
	Vector m_vecStartRefractTint;

	IMaterialVar *m_WaveSineMax;
	float m_flStartWaveSineMax;
};

bool CProxyCrabGrenade::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_RefractAmount = pMaterial->FindVar( "$refractamount", &foundVar );
	if ( !m_RefractAmount )
		return false;

	m_flStartRefractAmount = m_RefractAmount->GetFloatValue();
	
	m_RefractTint = pMaterial->FindVar( "$refracttint", &foundVar );
	if ( !m_RefractTint)
		return false;

	const float *flRefractTint = m_RefractTint->GetVecValue();
	m_vecStartRefractTint[0] = flRefractTint[0];
	m_vecStartRefractTint[1] = flRefractTint[1];
	m_vecStartRefractTint[2] = flRefractTint[2];
	
	m_WaveSineMax = pMaterial->FindVar( "$TempMax", &foundVar );
	if ( !m_WaveSineMax )
		return false;

	m_flStartWaveSineMax = m_WaveSineMax->GetFloatValue();

	return true;
}

void CProxyCrabGrenade::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = ((IClientRenderable*)pC_BaseEntity)->GetIClientUnknown()->GetBaseEntity();
	if ( pEntity )
	{
		C_CrabGrenade *pGrenade = dynamic_cast<C_CrabGrenade*>( pEntity );
		if ( pGrenade && pGrenade->m_bIsLive )
		{
			float flProgress = RemapVal( gpGlobals->curtime, pGrenade->m_flStartTime, pGrenade->m_flClientDetonateTime, 0.0f, 1.0f );

			m_RefractAmount->SetFloatValue( RemapVal( flProgress, 0.0f, 1.0f, m_flStartRefractAmount, 1.0f ) );
			m_WaveSineMax->SetFloatValue( RemapVal( flProgress, 0.0f, 1.0f, m_flStartWaveSineMax, 10.0f ) );

			m_RefractTint->SetVecValue( RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[0], 10.0f ),
				RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[1], 10.0f ),
				RemapVal( flProgress, 0.0f, 1.0f, m_vecStartRefractTint[2], 10.0f ) );
		}
		else
		{
			m_RefractAmount->SetFloatValue( m_flStartRefractAmount );
			m_WaveSineMax->SetFloatValue( m_flStartWaveSineMax );

			m_RefractTint->SetVecValue( m_vecStartRefractTint.Base(), 3);
		}
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

void CProxyCrabGrenade::Release( void )
{ 
	delete this; 
}

IMaterial *CProxyCrabGrenade::GetMaterial()
{
	return m_RefractAmount->GetOwningMaterial();
}

EXPOSE_INTERFACE( CProxyCrabGrenade, IMaterialProxy, "CrabGrenade" IMATERIAL_PROXY_INTERFACE_VERSION );
