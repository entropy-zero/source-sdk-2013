//=============================================================================//
//
// Purpose: A HUD element which displays NPCs' alertness levels when using stealth senses.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "hud_stealth_cloak.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_ez2_player.h"
#include "ez2/ai_stealth_shared.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>
#include "functionproxy.h"
#include "toolframework_client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudStealthCloak );

using namespace vgui;

extern int ScreenTransform( const Vector &point, Vector &screen );

#define CLOAK_COMPROMISE_TIME	1.0
#define CLOAK_VISIBLE_TIME	0.5

void FindOrCreateTextureID( int &iTextureID, const char *pszTextureFile )
{
	iTextureID = surface()->DrawGetTextureId( pszTextureFile );

	if ( iTextureID == -1 ) // we didn't find it, so create a new one
	{
		iTextureID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( iTextureID, pszTextureFile, true, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudStealthCloak::CHudStealthCloak( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudStealthCloak" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );

	FindOrCreateTextureID( m_iCloakIconVisible, "hud/stealth_cloak_icon_visible" );
	FindOrCreateTextureID( m_iCloakIconCompromised, "hud/stealth_cloak_icon_compromised" );
	FindOrCreateTextureID( m_iCloakIconLaser, "hud/stealth_cloak_icon_laser" );
	FindOrCreateTextureID( m_iCloakIconTouch, "hud/stealth_cloak_icon_touch" );
	FindOrCreateTextureID( m_iCloakIconMyLaser, "hud/stealth_cloak_icon_mylaser" );
}

CHudStealthCloak::~CHudStealthCloak()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthCloak::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/UI/HudStealthCloak.res" );

	m_pCloakLabel = dynamic_cast<Label *>( FindChildByName( "CloakLabel" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthCloak::Init( void )
{
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBackgroundType( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthCloak::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudStealthCloak::ShouldDraw( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return false;

	C_EZ2_Player *pEZ2Player = ToEZ2Player( pPlayer );
	if (pEZ2Player->GetCloakFactor() <= 0.0f)
		return false;
		
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: updates hud icons
//-----------------------------------------------------------------------------
void CHudStealthCloak::Paint()
{
	BaseClass::Paint();

	C_EZ2_Player *pEZ2Player = ToEZ2Player( C_BasePlayer::GetLocalPlayer() );
	Assert( pEZ2Player != NULL );

	Color clrCloakLabel = Color( 255, 255, 255, 255 );

	if ( m_pCloakLabel )
	{
		CloakLabelType_t nCloakLabel = CLOAK_LABEL_NONE;

		if ( gpGlobals->curtime - pEZ2Player->GetCloakCompromiseTime() < CLOAK_COMPROMISE_TIME )
		{
			nCloakLabel = CLOAK_LABEL_COMPROMISED;
		}
		else if ( gpGlobals->curtime - pEZ2Player->GetCloakVisibleTime() < CLOAK_VISIBLE_TIME )
		{
			nCloakLabel = CLOAK_LABEL_VISIBLE;
		}
		/*else if ( pEZ2Player->GetCloakCompromiseType() == COMPROMISE_TYPE_SIGHT && pEZ2Player->GetCloakFactor() < 1.0f )
		{
			// Still let the cloak label appear slightly
			nCloakLabel = CLOAK_LABEL_VISIBLE;
		}*/

		if ( nCloakLabel != CLOAK_LABEL_NONE )
		{
			const char *pszCloakLabel = NULL;
			int iTexture = m_iCloakIconVisible;

			switch (nCloakLabel)
			{
				case CLOAK_LABEL_VISIBLE:
					{
						pszCloakLabel = "#Suit_HUD_Cloak_WarnVisible";
						clrCloakLabel = m_clrCaution;

						if ( pEZ2Player->GetCloakCompromiseType() == COMPROMISE_TYPE_SIGHT )
							clrCloakLabel[3] = RemapValClamped( pEZ2Player->GetCloakFactor() + 0.2f, 0.0f, 1.0f, 255, 0 );
					}
					break;
				case CLOAK_LABEL_COMPROMISED:
					{
						pszCloakLabel = "#Suit_HUD_Cloak_WarnCompromised";
						clrCloakLabel = m_clrCompromised;
						iTexture = m_iCloakIconCompromised;
					}
					break;
			}

			switch ( pEZ2Player->GetCloakCompromiseType() )
			{
				case COMPROMISE_TYPE_LASER:
					iTexture = m_iCloakIconLaser;
					break;
				case COMPROMISE_TYPE_TOUCH:
					iTexture = m_iCloakIconTouch;
					break;
				case COMPROMISE_TYPE_MYLASER:
					iTexture = m_iCloakIconMyLaser;
					pszCloakLabel = "#Suit_HUD_Cloak_WarnMyLaserVisible";
					break;
			}

			m_pCloakLabel->SetText( pszCloakLabel );
			m_pCloakLabel->SetFgColor( clrCloakLabel );
			m_pCloakLabel->SetVisible( true );

			surface()->DrawSetTexture( iTexture );
			surface()->DrawSetColor( clrCloakLabel );
			surface()->DrawTexturedRect( m_flCloakIconX, m_flCloakIconY,
				m_flCloakIconX + m_flCloakIconWide, m_flCloakIconY + m_flCloakIconTall );
		}
		else
		{
			m_pCloakLabel->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Controls assassin cloak, but always corresponds to local player or observer (for HUD elements)
//-----------------------------------------------------------------------------
class CProxyAssassinCloakOverlay : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	IMaterialVar* m_pAmtRemainingVar;
};

bool CProxyAssassinCloakOverlay::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;
	
	char const* pWarnTint = pKeyValues->GetString( "amtRemainingVar" );
	if( pWarnTint )
	{
		bool foundVar = false;
		m_pAmtRemainingVar = pMaterial->FindVar( pWarnTint, &foundVar, false );
	}

	return true;
}

void CProxyAssassinCloakOverlay::OnBind( void *pC_BaseEntity )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		// Use the player we're spectating instead
		pPlayer = ToBasePlayer( pPlayer->GetObserverTarget() );
	}
	
	if ( !pPlayer )
		return;

	C_EZ2_Player *pEZ2Player = ToEZ2Player( pPlayer );
	if ( pEZ2Player->GetCloakFactor() > 0.0f )
	{
		SetFloatResult( pEZ2Player->GetCloakFactor() /*+ RandomFloat( 0.0f, 0.05f )*/ );

		if ( m_pAmtRemainingVar )
		{
			float flCloakPower = pEZ2Player->GetCloakPower();
			if ( flCloakPower <= 0.0f )
				flCloakPower = 0.01f;

			m_pAmtRemainingVar->SetFloatValue( flCloakPower );
		}
	}
	else
	{
		SetFloatResult( 0.0f );
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CProxyAssassinCloakOverlay, IMaterialProxy, "AssassinCloakOverlay" IMATERIAL_PROXY_INTERFACE_VERSION );


