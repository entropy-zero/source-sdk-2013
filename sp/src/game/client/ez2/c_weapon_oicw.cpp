//=============================================================================//
//
// Purpose: OICW scope effects.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_basehlcombatweapon.h"
#include "tier0/icommandline.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "baseviewmodel_shared.h"
#include "hud_crosshair.h"

class C_WeaponOICW : public C_BaseHLCombatWeapon
{
public:
	DECLARE_CLASS( C_WeaponOICW, C_BaseHLCombatWeapon );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponOICW();
	~C_WeaponOICW();

	bool			IsDynamicScopeZoomed( void ) const { return m_bZoomed; }

	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual	float	CalcViewmodelBob( void );
	void			DrawCrosshair( void );

	bool m_bZoomed;
	bool m_bZoomTransition;
};

LINK_ENTITY_TO_CLASS( weapon_oicw, C_WeaponOICW );

IMPLEMENT_CLIENTCLASS_DT( C_WeaponOICW, DT_WeaponOICW, CWeaponOICW )
	RecvPropBool( RECVINFO( m_bZoomed ) ),
	RecvPropBool( RECVINFO( m_bZoomTransition ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_WeaponOICW )
END_PREDICTION_DATA()

C_WeaponOICW::C_WeaponOICW()
{
}

C_WeaponOICW::~C_WeaponOICW()
{
}

void C_WeaponOICW::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	//if (m_bZoomed)
	//	return;

	BaseClass::AddViewmodelBob( viewmodel, origin, angles );
}

extern float g_lateralBob;
extern float g_verticalBob;

float C_WeaponOICW::CalcViewmodelBob( void )
{
	BaseClass::CalcViewmodelBob();

	// Reduce bob while zoomed
	if (m_bZoomed)
	{
		g_lateralBob *= 0.05f;
		g_verticalBob *= 0.1f;
	}

	// Return value not used (see base class)
	return 0.0f;
}

void C_WeaponOICW::DrawCrosshair( void )
{
	// Don't draw crosshair when zoomed
	if (m_bZoomed)
	{
		CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
		if ( !crosshair )
			return;

		crosshair->ResetCrosshair();
		return;
	}

	BaseClass::DrawCrosshair();
}

//-----------------------------------------------------------------------------
// Apply effects when the pulse pistol is charging
//-----------------------------------------------------------------------------
class COICWScopeMaterialProxy : public CEntityMaterialProxy
{
public:
	COICWScopeMaterialProxy() { m_ZoomVar = NULL; }
	virtual ~COICWScopeMaterialProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_ZoomVar;
};

bool COICWScopeMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundAnyVar = false;
	bool foundVar;

	const char *pszChargeVar = pKeyValues->GetString( "outputZoom" );
	m_ZoomVar = pMaterial->FindVar( pszChargeVar, &foundVar, true );
	if (foundVar)
		foundAnyVar = true;

	return foundAnyVar;
}

void COICWScopeMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if (!m_ZoomVar)
		return;

	// Need both the weapon and the viewmodel
	C_BaseViewModel *pVM = NULL;
	C_WeaponOICW *pOICW = NULL;
	if ( pEnt->IsBaseCombatWeapon() )
	{
		pOICW = assert_cast<C_WeaponOICW*>( pEnt );
		if (C_BasePlayer *pPlayer = ToBasePlayer( pOICW->GetOwner() ))
			pVM = pPlayer->GetViewModel( pOICW->m_nViewModelIndex );
	}
	else
	{
		pVM = dynamic_cast<C_BaseViewModel*>( pEnt );
		if (pVM)
			pOICW = assert_cast<C_WeaponOICW*>( pVM->GetOwningWeapon() );
	}

	if (!pOICW || !pVM)
		return;

	if ( pOICW )
	{
		if (pOICW->IsDynamicScopeZoomed())
		{
			// Zoomed
			if (pOICW->m_bZoomTransition)
			{
				// Transition to zoom using viewmodel cycle
				m_ZoomVar->SetFloatValue( pVM->GetCycle() );
			}
			else
				m_ZoomVar->SetFloatValue( 1.0f );
		}
		else
		{
			// Not zoomed
			if (pOICW->m_bZoomTransition)
			{
				// Transition from zoom using viewmodel cycle
				m_ZoomVar->SetFloatValue( 1.0f - pVM->GetCycle() );
			}
			else
				m_ZoomVar->SetFloatValue( 0.0f );
		}
	}
	else
	{
		m_ZoomVar->SetFloatValue( 0.0f );
	}
}

IMaterial *COICWScopeMaterialProxy::GetMaterial()
{
	if ( !m_ZoomVar )
		return NULL;

	return m_ZoomVar->GetOwningMaterial();
}

EXPOSE_INTERFACE( COICWScopeMaterialProxy, IMaterialProxy, "OICWScope" IMATERIAL_PROXY_INTERFACE_VERSION );

//-----------------------------------------------------------------------------
// Purpose: System for managing the OICW scope render target
// 
// TODO: More formal system for managing weapon scope RTs? I don't know how this
// is "supposed" to work relative to games which actually use these
// Maybe something like what's described in baseclientrendertargets.cpp's header?
//-----------------------------------------------------------------------------
class CWeaponOICWScopeSystem : public CAutoGameSystem
{
public:
	CWeaponOICWScopeSystem() : CAutoGameSystem( "CWeaponOICWScopeSystem" )
	{
	}

	virtual bool Init()
	{
		InitializeRTs();
		return true;
	}

	//-----------------------------------------------------------------------------
	// Initialize custom RT textures if necessary
	//-----------------------------------------------------------------------------
	void InitializeRTs()
	{
		if (!m_bInitializedRTs)
		{
			// Cancel if we shouldn't run with the scope RT
			int nNoScopeRT = CommandLine()->ParmValue( "-no_scope_rt", 0 );
			if (nNoScopeRT == 1)
				return;

			materials->BeginRenderTargetAllocation();

			m_ScopeTexture.Init( g_pMaterialSystem->CreateNamedRenderTargetTextureEx2(
				"_rt_Scope",
				512, 512, RT_SIZE_OFFSCREEN,
				g_pMaterialSystem->GetBackBufferFormat(),
				MATERIAL_RT_DEPTH_SHARED,
				TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
				CREATERENDERTARGETFLAGS_HDR ) );

			materials->EndRenderTargetAllocation();

			m_bInitializedRTs = true;
		}
	}

	void Shutdown()
	{
		if (m_bInitializedRTs)
		{
			m_ScopeTexture.Shutdown();
			m_bInitializedRTs = false;
		}
	}

private:

	CTextureReference m_ScopeTexture;
	bool m_bInitializedRTs = false;
};

CWeaponOICWScopeSystem m_OICWScopeSystem;
