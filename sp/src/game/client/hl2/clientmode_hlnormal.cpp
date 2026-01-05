//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Draws the normal TF2 or HL2 HUD.
//
//=============================================================================
#include "cbase.h"
#include "clientmode_hlnormal.h"
#include "vgui_int.h"
#include "hud.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include "iinput.h"
#include "ienginevgui.h"
#ifdef MAPBASE
#include "c_basehlplayer.h"
#include "mapbase/protagonist_system.h"
#endif
#ifdef EZ2
#include "ez2/c_ez2_player.h"
#include "materialsystem/itexture.h"
#include "view_scene.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool g_bRollingCredits;

ConVar fov_desired( "fov_desired", "75", FCVAR_ARCHIVE | FCVAR_USERINFO, "Sets the base field-of-view.", true, 75.0, true, 110.0 );

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
vgui::HScheme g_hVGuiCombineScheme = 0;


// Instance the singleton and expose the interface to it.
IClientMode *GetClientModeNormal()
{
	static ClientModeHLNormal g_ClientModeNormal;
	return &g_ClientModeNormal;
}


//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		gHUD.InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual void CreateDefaultPanels( void ) { /* don't create any panels yet*/ };
};


//-----------------------------------------------------------------------------
// ClientModeHLNormal implementation
//-----------------------------------------------------------------------------
ClientModeHLNormal::ClientModeHLNormal()
{
	m_pViewport = new CHudViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeHLNormal::~ClientModeHLNormal()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeHLNormal::Init()
{
	BaseClass::Init();

	// Load up the combine control panel scheme
	g_hVGuiCombineScheme = vgui::scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), IsXbox() ? "resource/ClientScheme.res" : "resource/CombinePanelScheme.res", "CombineScheme" );
	if (!g_hVGuiCombineScheme)
	{
		Warning( "Couldn't load combine panel scheme!\n" );
	}
}

bool ClientModeHLNormal::ShouldDrawCrosshair( void )
{
	return ( g_bRollingCredits == false );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeHLNormal::ReloadScheme()
{
	C_BaseHLPlayer *pPlayer = static_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );
	if ( pPlayer && pPlayer->GetProtagonistIndex() != -1 )
	{
		// Use the protagonist's scheme instead, if it exists
		const char *pszClientScheme = g_ProtagonistSystem.GetProtagonist_ClientScheme( pPlayer );
		if ( pszClientScheme != NULL )
		{
			SetCustomClientScheme( pszClientScheme );
			return;
		}
	}

	BaseClass::ReloadScheme();
}
#endif

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeHLNormal::DoPostScreenSpaceEffectsPostViewModel( const CViewSetup *pSetup )
{
	if ( !BaseClass::DoPostScreenSpaceEffectsPostViewModel( pSetup ) )
		return false;
	
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		C_EZ2_Player *pEZ2Player = ToEZ2Player( pPlayer );
		if ( pEZ2Player->IsNVGActive() )
		{
			IMaterial *pOverlayMaterial = materials->FindMaterial( "Ezero/Mask_NvMap", TEXTURE_GROUP_OTHER, true );
			DrawScreenEffectMaterial( pOverlayMaterial, 0, 0, pSetup->width, pSetup->height );
		}

		// If the player is cloaking, render the cloak overlay
		if ( pEZ2Player->GetCloakFactor() > 0.0f )
		{
			// UNDONE: Cloak factor offset (works better with material proxy)
			/*int offsetX = 0;
			int offsetY = 0;
			if ( pEZ2Player->GetCloakFactor() < 1.0f )
			{
				offsetX = (pSetup->width * 0.5f) * (1.0f - pEZ2Player->GetCloakFactor());
				offsetY = (pSetup->height * 0.5f) * (1.0f - pEZ2Player->GetCloakFactor());
			}*/

			IMaterial *pOverlayMaterial = materials->FindMaterial( "hud/stealth_cloak_overlay", TEXTURE_GROUP_OTHER );
			DrawScreenEffectMaterial( pOverlayMaterial, 0, 0, pSetup->width, pSetup->height );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeHLNormal::OnColorCorrectionWeightsReset( void )
{
	BaseClass::OnColorCorrectionWeightsReset();
	
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		// Set the player's color correction weights
		C_EZ2_Player *pEZ2Player = ToEZ2Player( pPlayer );
		pEZ2Player->SetCCWeights();
	}
}
#endif



