//=============================================================================//
//
// Purpose: A HUD element which displays NPCs' alertness levels when using stealth senses.
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "hud_stealth_alert.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_ez2_player.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( CHudStealthAlert );
DECLARE_HUD_MESSAGE( CHudStealthAlert, AlertTargetUpdate );
DECLARE_HUD_MESSAGE( CHudStealthAlert, AlertTargetEntersCombat );
DECLARE_HUD_MESSAGE( CHudStealthAlert, EnemyMarkUpdate );

using namespace vgui;

extern int ScreenTransform( const Vector &point, Vector &screen );

ConVar	hud_stealth_alert_circle( "hud_stealth_alert_circle", "1" );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudStealthAlertIcon::CHudStealthAlertIcon( C_BaseEntity *pSource, AlertSourceType_t iType, float flLevel, float flCombatTime, bool bTargetingPlayer,
		Panel *parent, int texCircle, int texSpotted ) : BaseClass( parent, "HudStealthAlertIcon" )
{
	m_hSource = pSource;
	m_iType = iType;
	m_flLevel = flLevel;
	m_bTargetingPlayer = bTargetingPlayer;
	m_flCombatTime = flCombatTime;

	m_iCircleTexture = texCircle;
	m_iSpottedTexture = texSpotted;

	m_iCircleBGTexture = surface()->DrawGetTextureId( "hud/stealth_alert_circle_bg" );

	if ( m_iCircleBGTexture == -1 ) // we didn't find it, so create a new one
	{
		m_iCircleBGTexture = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iCircleBGTexture, "hud/stealth_alert_circle_bg", true, false );
	}

	if ( m_iCircleTexture == -1 )
		m_iCircleTexture = surface()->DrawGetTextureId( "hud/stealth_alert_circle" );

	if ( m_iCircleTexture == -1 ) // we didn't find it, so create a new one
	{
		m_iCircleTexture = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iCircleTexture, "hud/stealth_alert_circle", true, false );
	}

	if (m_iSpottedTexture == -1 )
		m_iSpottedTexture = surface()->DrawGetTextureId( "hud/stealth_alert_spotted" );

	if ( m_iSpottedTexture == -1 ) // we didn't find it, so create a new one
	{
		m_iSpottedTexture = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iSpottedTexture, "hud/stealth_alert_spotted", true, false );
	}

	m_vecWorldPos = vec3_origin;
}

CHudStealthAlertIcon::~CHudStealthAlertIcon()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIcon::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( GetParent() )
	{
		int wide, tall;
		GetSize( wide, tall );
		GetParent()->SetSize( wide, tall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIcon::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	KeyValues *pConditions = NULL;

	m_bCircle = hud_stealth_alert_circle.GetBool();

	if ( m_bCircle )
	{
		if ( !pConditions )
			pConditions = new KeyValues( "conditions" );
		pConditions->FindKey( "if_circle", true );
	}

	if ( m_iType == ALERT_SOURCE_TYPE_HELICOPTER )
	{
		if ( !pConditions )
			pConditions = new KeyValues( "conditions" );
		pConditions->FindKey( "if_heli", true );
	}

	LoadControlSettings( "resource/UI/HudStealthAlertIcon.res", NULL, NULL, pConditions );

	/*if (m_AlertIcons[i].iType == ALERT_SOURCE_TYPE_HELICOPTER)
	{
		// Helicopters are MUCH bigger
		iAlertBarWide *= 2.0;
		iAlertBarTall *= 2.0;
		iAlertBarFillSpacing *= 4.0;
		vecAlertBarWorldPos += Vector( 0, 0, 64 );
	}*/

	if ( m_bCircle )
	{
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetPaintBackgroundType( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIconParent::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBackgroundType( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector &CHudStealthAlertIcon::GetWorldPosition( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( m_hSource && pPlayer && m_flHeadOffsetDist > 0.0f )
	{
		Vector vecToSource = (pPlayer->EyePosition() - m_hSource->EyePosition());

		m_vecWorldPos = m_hSource->EyePosition() + Vector( 0, 0,
			RemapValClamped( vecToSource.Length(), 0.0f, m_flHeadOffsetDist, m_flHeadOffsetMin, m_flHeadOffsetMax ) );
	}

	return m_vecWorldPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIcon::Paint()
{
	if (m_flLevel == ALERT_TARGET_COMBAT_LEVEL)
	{
		// Paint circle and excalamation point at once
		/*if ( m_bCircle )
		{
			PaintCircle();
		}

		int wide, tall;
		GetSize( wide, tall );

		Color clr = m_clrFull;
		clr[3] = GetAlpha();

		// Draw an exclamation point
		surface()->DrawSetTexture( m_iSpottedTexture );
		surface()->DrawSetColor( clr );
		surface()->DrawTexturedRect( 0, 0, wide, tall );*/

		// UNDONE: Font version
		int width = surface()->GetCharacterWidth( m_hAlertSpottedFont, '!');
		//int height = surface()->GetFontTall( m_hAlertSpottedFont );
		
		int textX = (GetWide() * 0.5) - (width * 0.5);
		int textY = 0; //+ iAlertBarTall + height;
		
		surface()->DrawSetTextFont( m_hAlertSpottedFont );
		surface()->DrawSetTextColor( m_bTargetingPlayer ? m_clrFull : m_clrTargetingOther );
		surface()->DrawSetTextPos( textX, textY );
		surface()->DrawPrintText( L"!", 1 );
		
	}
	else
	{
		if ( m_bCircle )
		{
			PaintCircle();
		}
		else
		{
			PaintBar();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIcon::PaintBar()
{
	int iAlertBarWide, iAlertBarTall;
	GetSize( iAlertBarWide, iAlertBarTall );
	
	//surface()->DrawSetColor( m_clrBG );
	//surface()->DrawFilledRect( m_iAlertBarX, m_iAlertBarY, m_iAlertBarX + m_iAlertBarWide, m_iAlertBarY + m_iAlertBarTall );
	//s_pAlertTargetPanel.DrawBox(m_iAlertBarX, m_iAlertBarY, iAlertBarWide, iAlertBarTall, 0, 0, 0, 255, false)

	float flLevel = m_flLevel;
	if (flLevel > 1.0f)
		flLevel = 1.0f;

	Color clr;
	clr[0] = FLerp( m_clrEmpty.r(), m_clrFull.r(), flLevel );
	clr[1] = FLerp( m_clrEmpty.g(), m_clrFull.g(), flLevel );
	clr[2] = FLerp( m_clrEmpty.b(), m_clrFull.b(), flLevel );
	clr[3] = FLerp( m_clrEmpty.a(), m_clrFull.a(), flLevel );
	clr[3] *= ((float)GetAlpha() / 255.0f);
		
	surface()->DrawSetColor( clr );
	surface()->DrawFilledRect( m_iAlertFillMargin, m_iAlertFillMargin, (flLevel * (iAlertBarWide - (m_iAlertFillMargin * 2))), (iAlertBarTall - (m_iAlertFillMargin)) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlertIcon::PaintCircle()
{
	//
	// Below is based on the time panel code from the TF2 SDK
	//
	int wide, tall;
	GetSize( wide, tall );

	float uv1 = 0.0f, uv2 = 1.0f;
	Vector2D uv11( uv1, uv1 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );
	Vector2D uv12( uv1, uv2 );

	Vertex_t verts[4];	
	verts[0].Init( Vector2D( 0, 0 ), uv11 );
	verts[1].Init( Vector2D( wide, 0 ), uv21 );
	verts[2].Init( Vector2D( wide, tall ), uv22 );
	verts[3].Init( Vector2D( 0, tall ), uv12  );

	Color clr = m_clrBG;
	clr[3] = GetAlpha();

	// first, just draw the whole thing inactive.
	surface()->DrawSetTexture( m_iCircleBGTexture );
	surface()->DrawSetColor( clr );
	surface()->DrawTexturedPolygon( 4, verts );

	// we're going to do this using quadrants
	//  -------------------------
	//  |           |           |
	//  |           |           |
	//  |     4     |     1     |
	//  |           |           |
	//  |           |           |
	//  -------------------------
	//  |           |           |
	//  |           |           |
	//  |     3     |     2     |
	//  |           |           |
	//  |           |           |
	//  -------------------------

	float flCompleteCircle = ( 2.0f * M_PI );
	float fl90degrees = flCompleteCircle / 4.0f;

	float flLevel = m_flLevel;
	if ( flLevel > 1.0f || flLevel == ALERT_TARGET_COMBAT_LEVEL )
		flLevel = 1.0f;

	//float flEndAngle = flCompleteCircle * ( 1.0f - m_flProgress ); // count DOWN (counter-clockwise)
	float flEndAngle = flCompleteCircle * flLevel; // count UP (clockwise)

	float flHalfWide = (float)wide / 2.0f;
	float flHalfTall = (float)tall / 2.0f;

	clr[0] = FLerp( m_clrEmpty.r(), m_clrFull.r(), flLevel );
	clr[1] = FLerp( m_clrEmpty.g(), m_clrFull.g(), flLevel );
	clr[2] = FLerp( m_clrEmpty.b(), m_clrFull.b(), flLevel );
	clr[3] = FLerp( m_clrEmpty.a(), m_clrFull.a(), flLevel );
	clr[3] *= ((float)GetAlpha() / 255.0f);

	surface()->DrawSetColor( clr );
	surface()->DrawSetTexture( m_iCircleTexture );

	if ( flEndAngle >= fl90degrees * 3.0f ) // >= 270 degrees
	{
		// draw the first and second quadrants
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 1.0f );
		uv12.Init( 0.5, 1.0f );

		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, tall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, tall ), uv12  );

		surface()->DrawTexturedPolygon( 4, verts );

		// draw the third quadrant
		uv11.Init( 0.0f, 0.5f );
		uv21.Init( 0.5f, 0.5f );
		uv22.Init( 0.5f, 1.0f );
		uv12.Init( 0.0f, 1.0f );

		verts[0].Init( Vector2D( 0.0f, flHalfTall ), uv11 );
		verts[1].Init( Vector2D( flHalfWide, flHalfTall ), uv21 );
		verts[2].Init( Vector2D( flHalfWide, tall ), uv22 );
		verts[3].Init( Vector2D( 0.0f, tall ), uv12  );

		surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial fourth quadrant
		if ( flEndAngle > fl90degrees * 3.5f ) // > 315 degrees
		{
			uv11.Init( 0.0f, 0.0f );
			uv21.Init( 0.5f - ( tan(fl90degrees * 4.0f - flEndAngle) * 0.5 ), 0.0f );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.0f, 0.5f );

			verts[0].Init( Vector2D( 0.0f, 0.0f ), uv11 );
			verts[1].Init( Vector2D( flHalfWide - ( tan(fl90degrees * 4.0f - flEndAngle) * flHalfTall ), 0.0f ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall ), uv12 );

			surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 315 degrees
		{
			uv11.Init( 0.0f, 0.5f );
			uv21.Init( 0.0f, 0.5f - ( tan(flEndAngle - fl90degrees * 3.0f) * 0.5 ) );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.0f, 0.5f );

			verts[0].Init( Vector2D( 0.0f, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( 0.0f, flHalfTall - ( tan(flEndAngle - fl90degrees * 3.0f) * flHalfWide ) ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else if ( flEndAngle >= fl90degrees * 2.0f ) // >= 180 degrees
	{
		// draw the first and second quadrants
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 1.0f );
		uv12.Init( 0.5, 1.0f );

		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, tall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, tall ), uv12  );

		surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial third quadrant
		if ( flEndAngle > fl90degrees * 2.5f ) // > 225 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 0.5f, 1.0f );
			uv22.Init( 0.0f, 1.0f );
			uv12.Init( 0.0f, 0.5f + ( tan(fl90degrees * 3.0f - flEndAngle) * 0.5 ) );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( flHalfWide, tall ), uv21 );
			verts[2].Init( Vector2D( 0.0f, tall ), uv22 );
			verts[3].Init( Vector2D( 0.0f, flHalfTall + ( tan(fl90degrees * 3.0f - flEndAngle) * flHalfWide ) ), uv12 );

			surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 225 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 0.5f, 1.0f );
			uv22.Init( 0.5f - ( tan( flEndAngle - fl90degrees * 2.0f) * 0.5 ), 1.0f );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( flHalfWide, tall ), uv21 );
			verts[2].Init( Vector2D( flHalfWide - ( tan(flEndAngle - fl90degrees * 2.0f) * flHalfTall ), tall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else if ( flEndAngle >= fl90degrees ) // >= 90 degrees
	{
		// draw the first quadrant
		uv11.Init( 0.5f, 0.0f );
		uv21.Init( 1.0f, 0.0f );
		uv22.Init( 1.0f, 0.5f );
		uv12.Init( 0.5f, 0.5f );

		verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
		verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
		verts[2].Init( Vector2D( wide, flHalfTall ), uv22 );
		verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

		surface()->DrawTexturedPolygon( 4, verts );

		// draw the partial second quadrant
		if ( flEndAngle > fl90degrees * 1.5f ) // > 135 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 1.0f, 0.5f );
			uv22.Init( 1.0f, 1.0f );
			uv12.Init( 0.5f + ( tan(fl90degrees * 2.0f - flEndAngle) * 0.5f ), 1.0f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( wide, flHalfTall ), uv21 );
			verts[2].Init( Vector2D( wide, tall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide + ( tan(fl90degrees * 2.0f - flEndAngle) * flHalfTall ), tall ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 135 degrees
		{
			uv11.Init( 0.5f, 0.5f );
			uv21.Init( 1.0f, 0.5f );
			uv22.Init( 1.0f, 0.5f + ( tan(flEndAngle - fl90degrees) * 0.5f ) );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, flHalfTall ), uv11 );
			verts[1].Init( Vector2D( wide, flHalfTall ), uv21 );
			verts[2].Init( Vector2D( wide, flHalfTall + ( tan(flEndAngle - fl90degrees) * flHalfWide ) ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
	}
	else // > 0 degrees
	{
		if ( flEndAngle > fl90degrees / 2.0f ) // > 45 degrees
		{
			uv11.Init( 0.5f, 0.0f );
			uv21.Init( 1.0f, 0.0f );
			uv22.Init( 1.0f, 0.5f - ( tan(fl90degrees - flEndAngle) * 0.5 ) );
			uv12.Init( 0.5f, 0.5f );

			verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
			verts[1].Init( Vector2D( wide, 0.0f ), uv21 );
			verts[2].Init( Vector2D( wide, flHalfTall - ( tan(fl90degrees - flEndAngle) * flHalfWide ) ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, flHalfTall ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
		else // <= 45 degrees
		{
			uv11.Init( 0.5f, 0.0f );
			uv21.Init( 0.5 + ( tan(flEndAngle) * 0.5 ), 0.0f );
			uv22.Init( 0.5f, 0.5f );
			uv12.Init( 0.5f, 0.0f );

			verts[0].Init( Vector2D( flHalfWide, 0.0f ), uv11 );
			verts[1].Init( Vector2D( flHalfWide + ( tan(flEndAngle) * flHalfTall ), 0.0f ), uv21 );
			verts[2].Init( Vector2D( flHalfWide, flHalfTall ), uv22 );
			verts[3].Init( Vector2D( flHalfWide, 0.0f ), uv12  );

			surface()->DrawTexturedPolygon( 4, verts );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudStealthAlert::CHudStealthAlert( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudStealthAlert" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

CHudStealthAlert::~CHudStealthAlert()
{
	for (int i = m_AlertIcons.Count()-1; i >= 0; i--)
	{
		m_AlertIconParents[i]->MarkForDeletion();
		m_AlertIconParents.Remove( i );
		m_AlertIcons[i]->MarkForDeletion();
		m_AlertIcons.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlert::Init( void )
{
	HOOK_HUD_MESSAGE( CHudStealthAlert, AlertTargetUpdate );
	HOOK_HUD_MESSAGE( CHudStealthAlert, AlertTargetEntersCombat );
	HOOK_HUD_MESSAGE( CHudStealthAlert, EnemyMarkUpdate );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetPaintBackgroundType( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlert::Reset( void )
{
	for (int i = m_AlertIcons.Count()-1; i >= 0; i--)
	{
		m_AlertIconParents[i]->MarkForDeletion();
		m_AlertIconParents.Remove( i );
		m_AlertIcons[i]->MarkForDeletion();
		m_AlertIcons.Remove( i );
	}

	Init();

	m_flNextAlertSoundTime = 0.0f;
	m_flNextAlertAmbSoundTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudStealthAlert::ShouldDraw( void )
{
	if ( m_AlertIcons.Count() <= 0 )
		return false;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || pPlayer->GetFlags() & FL_FROZEN )
		return false;

	if (pPlayer->GetFlags() & FL_FROZEN)
		return false;
		
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlert::MsgFunc_AlertTargetUpdate( bf_read &msg )
{
	C_BaseEntity *pEntity = C_BaseEntity::Instance( msg.ReadShort() );
	float flNewLevel = msg.ReadFloat();
	int iType = msg.ReadByte();

	int index = -1;
	for (int i = 0; i < m_AlertIcons.Count(); i++)
	{
		if (m_AlertIcons[i] && m_AlertIcons[i]->GetSource() == pEntity)
		{
			if (m_AlertIcons[i]->GetCombatTime() != -1.0f)
			{
				// Currently displaying alert notice. Replace it with a fresh new one to avoid scaling issues
				m_AlertIconParents[i]->MarkForDeletion();
				m_AlertIconParents.Remove( i );
				m_AlertIcons[i]->MarkForDeletion();
				m_AlertIcons.Remove( i );
			}
			else
			{
				m_AlertIcons[i]->SetLevel( flNewLevel );
				index = i;
			}
			break;
		}
	}

	if (index == -1)
	{
		// New target
		int i = CreateAlertTarget( pEntity, (AlertSourceType_t)iType, flNewLevel, -1.0f, true );

		if ( gpGlobals->curtime > m_flNextAlertAmbSoundTime )
		{
			// First alert target is more prominent
			switch ( iType )
			{
				case ALERT_SOURCE_TYPE_EXTRA:
				case ALERT_SOURCE_TYPE_HELICOPTER:
					pEntity->EmitSound( "EZ2Player.AlertTarget_Extra.Begin_Amb" );
					break;
				default:
					pEntity->EmitSound( "EZ2Player.AlertTarget.Begin_Amb" );
					break;
			}

			m_flNextAlertAmbSoundTime = gpGlobals->curtime + 7.5f;
		}
		
		switch ( iType )
		{
			case ALERT_SOURCE_TYPE_EXTRA:
			case ALERT_SOURCE_TYPE_HELICOPTER:
				pEntity->EmitSound( "EZ2Player.AlertTarget_Extra.Begin" );
				break;
			default:
				pEntity->EmitSound( "EZ2Player.AlertTarget.Begin" );
				break;
		}

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_AlertIconParents[i], "StealthAlertPopup" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlert::MsgFunc_AlertTargetEntersCombat( bf_read &msg )
{
	C_BaseEntity *pEntity = C_BaseEntity::Instance( msg.ReadShort() );
	bool bTargetingMe = msg.ReadOneBit();

	// Mark the alert target as being in combat
	int index = -1;
	for (int i = 0; i < m_AlertIcons.Count(); i++)
	{
		if (m_AlertIcons[i] && m_AlertIcons[i]->GetSource() == pEntity)
		{
			// Stop the ambient sound
			switch ( m_AlertIcons[i]->GetType() )
			{
				case ALERT_SOURCE_TYPE_EXTRA:
				case ALERT_SOURCE_TYPE_HELICOPTER:
					pEntity->StopSound( "EZ2Player.AlertTarget_Extra.Begin_Amb" );
					break;
				default:
					pEntity->StopSound( "EZ2Player.AlertTarget.Begin_Amb" );
					break;
			}

			m_AlertIcons[i]->SetCombatTime( gpGlobals->curtime );
			m_AlertIcons[i]->SetTargetingPlayer( bTargetingMe );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_AlertIconParents[i], "StealthAlertSpotted" );
			index = i;
			break;
		}
	}

	if (index == -1)
	{
		// New target
		int i = CreateAlertTarget( pEntity, ALERT_SOURCE_TYPE_NONE, ALERT_TARGET_COMBAT_LEVEL, gpGlobals->curtime, bTargetingMe );

		if ( bTargetingMe )
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_AlertIconParents[i], "StealthAlertSpotted" );
	}

	if ( bTargetingMe && gpGlobals->curtime > m_flNextAlertSoundTime )
	{
		pEntity->EmitSound( "EZ2Player.AlertTarget.Spot" );
		m_flNextAlertSoundTime = gpGlobals->curtime + 0.5f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudStealthAlert::MsgFunc_EnemyMarkUpdate( bf_read &msg )
{
	C_BaseEntity *pEntity = C_BaseEntity::Instance( msg.ReadShort() );
	float flLastTimeSeen = msg.ReadFloat();
	int r = msg.ReadByte();
	int g = msg.ReadByte();
	int b = msg.ReadByte();
	int a = msg.ReadByte();

	C_EZ2_Player *pEZ2Player = ToEZ2Player( C_BasePlayer::GetLocalPlayer() );
	pEZ2Player->EnemyMarkUpdate( pEntity, flLastTimeSeen, r, g, b, a );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CHudStealthAlert::CreateAlertTarget( CBaseEntity *pEntity, AlertSourceType_t iType, float flLevel, float flCombatTime, bool bTargetingPlayer )
{
	vgui::Panel *pIconParent = new CHudStealthAlertIconParent( this, "HudStealthAlertIconParent" );
	pIconParent->SetBounds( 0, 0, 32, 32 );
	int i = m_AlertIconParents.AddToTail( pIconParent );

	CHudStealthAlertIcon *pIcon = new CHudStealthAlertIcon( pEntity, iType, flLevel, flCombatTime, bTargetingPlayer, pIconParent );
	Verify( i == m_AlertIcons.AddToTail( pIcon ) );

	return i;
}

//-----------------------------------------------------------------------------
// Purpose: updates hud icons
//-----------------------------------------------------------------------------
void CHudStealthAlert::Paint()
{
	//BaseClass::Paint();

	static CUtlVector< Vector2D > occupiedSpaces;

	for (int i = m_AlertIcons.Count()-1; i >= 0; i--)
	{
		if ( m_AlertIcons[i]->GetAlpha() == 0 )
		{
			m_AlertIconParents[i]->MarkForDeletion();
			m_AlertIconParents.Remove( i );
			m_AlertIcons[i]->MarkForDeletion();
			m_AlertIcons.Remove( i );
			continue;
		}
		else if (m_AlertIcons[i]->GetLevel() == 0.0f || m_AlertIcons[i]->GetSource() == NULL
			|| m_AlertIcons[i]->GetSource()->IsMarkedForDeletion() || !(m_AlertIcons[i]->GetSource()->IsAlive()))
		{
			if ( m_AlertIcons[i]->GetAlpha() == 255 )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_AlertIconParents[i], "StealthAlertFadeOut" );
			}
			//continue;
		}
		else if (m_AlertIcons[i]->GetAlpha() != 255 )
		{
			//g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_AlertIconParents[i], "StealthAlertFadeIn" );
		}

		int iWide, iTall;
		m_AlertIcons[i]->GetSize( iWide, iTall );

		int iAlertBarWide, iAlertBarTall;
		m_AlertIcons[i]->GetAlertSize( iAlertBarWide, iAlertBarTall );
		
		Vector screenCoords;
		ScreenTransform( m_AlertIcons[i]->GetWorldPosition(), screenCoords );
		
		//printl("X: " + screenCoords[0] + ", Y: " + screenCoords[1])
		//debugoverlay.ScreenText(0.2, 0.1, "X: " + screenCoords[0] + ", Y: " + screenCoords[1], 255, 255, 255, 255, 0.001)
		
		int rectX = (0.5f * (1.0f + screenCoords[0]) * ScreenWidth()) - (iWide*0.5);
		int rectY = (0.5f * (1.0f - screenCoords[1]) * ScreenHeight()) - (iTall*0.5);

		rectX = clamp( rectX, m_iAlertMinX, m_iAlertMaxX );
		rectY = clamp( rectY, m_iAlertMinY, m_iAlertMaxY );
		
		// TEMPTEMP: For now, only do bounds checking when on the borders
		if (screenCoords[0] == m_iAlertMinX || screenCoords[0] == m_iAlertMaxX ||
			screenCoords[1] == m_iAlertMinY || screenCoords[1] == m_iAlertMaxY)
		{
			// Make sure these coordinates do not occupy another alert target
			for (int j = 0; j < occupiedSpaces.Count(); j++)
			{
				//local crossX = 0;
				//local crossY = 0;
				//
				//if (rectX >= occupiedSpaces[j].x - ALERT_BAR_OUTER_RADIUS && rectX < occupiedSpaces[j].x + ALERT_BAR_OUTER_RADIUS)
				//{
				//	crossX = rectX - occupiedSpaces[j].x;
				//}
				//
				//if (rectY >= occupiedSpaces[j].y - ALERT_BAR_OUTER_RADIUS && rectY < occupiedSpaces[j].y + ALERT_BAR_OUTER_RADIUS)
				//{
				//	crossY = rectY - occupiedSpaces[j].y;
				//}
				//
				//if (abs(crossX) > abs(crossY))
				//{
				//	rectX += iAlertBarWide - ALERT_BAR_OUTER_SPACING;
				//}
				//else
				//{
				//	rectY += iAlertBarTall + ALERT_BAR_OUTER_SPACING;
				//}
				
				//local dist = sqrt((crossX*crossX) + (crossY*crossY))
				//
				//if (crossX > 0)
				//	rectX += dist;
				//else
				//	rectX -= dist;
				//	
				//if (crossY > 0)
				//	rectY += dist;
				//else
				//	rectY -= dist;
			
				if (rectX >= occupiedSpaces[j].x - m_AlertIcons[i]->GetWide() && rectX < occupiedSpaces[j].x + m_AlertIcons[i]->GetWide())
				{
					int xBound = ScreenWidth() * m_iAlertMinX;
					int newRectX = rectX + iAlertBarWide + m_AlertIcons[i]->GetOuterMargin();
					if (newRectX + iAlertBarWide > (ScreenWidth() - xBound))
					{
						// Move to left instead
						newRectX = rectX - iAlertBarWide - m_AlertIcons[i]->GetOuterMargin();
					}
					
					rectX = newRectX;
				}
				//if (rectY > (occupiedSpaces[j].y1 - iAlertBarTall) && rectY < occupiedSpaces[j].y2)
				//{
				//	rectY += iAlertBarTall + ALERT_BAR_OUTER_SPACING;
				//}
			}
		}

		m_AlertIconParents[i]->SetPos( rectX, rectY );
		
		// Make sure this space is occupied
		int j = occupiedSpaces.AddToTail();
		occupiedSpaces[j].x = (rectX + (((float)iAlertBarWide) * 0.5f));
		occupiedSpaces[j].y = (rectY + (((float)iAlertBarTall) * 0.5f));
		//occupiedSpaces.append( { x1 = rectX - ALERT_BAR_OUTER_SPACING, x2 = rectX + iAlertBarWide + ALERT_BAR_OUTER_SPACING,
		//y1 = rectY - ALERT_BAR_OUTER_SPACING, y2 = rectY + iAlertBarTall + ALERT_BAR_OUTER_SPACING } )
	}
	
	occupiedSpaces.RemoveAll();
}


