//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"

#include <Color.h>
#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#ifdef MAPBASE
#include <vgui/ILocalize.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#ifdef EZ2
// TEMP!!!
ConVar	hud_malfunction_enable( "hud_malfunction_enable", "0" );
ConVar	hud_malfunction_intensity( "hud_malfunction_intensity", "0.5" );
ConVar	hud_malfunction_number_chance( "hud_malfunction_number_chance", "0.1" );
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudNumericDisplay::CHudNumericDisplay(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iValue = 0;
	m_LabelText[0] = 0;
	m_iSecondaryValue = 0;
	m_bDisplayValue = true;
	m_bDisplaySecondaryValue = false;
	m_bIndent = false;
	m_bIsTime = false;
}

//-----------------------------------------------------------------------------
// Purpose: Resets values on restore/new map
//-----------------------------------------------------------------------------
void CHudNumericDisplay::Reset()
{
	m_flBlur = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetDisplayValue(int value)
{
	m_iValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetSecondaryValue(int value)
{
	m_iSecondaryValue = value;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetShouldDisplayValue(bool state)
{
	m_bDisplayValue = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetShouldDisplaySecondaryValue(bool state)
{
	m_bDisplaySecondaryValue = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetLabelText(const wchar_t *text)
{
#ifdef MAPBASE
	// Refuse further inputs from the code if the control settings have their own definition
	if ( m_bOverrideLabel )
		return;
#endif

	wcsncpy(m_LabelText, text, sizeof(m_LabelText) / sizeof(wchar_t));
	m_LabelText[(sizeof(m_LabelText) / sizeof(wchar_t)) - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetIndent(bool state)
{
	m_bIndent = state;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudNumericDisplay::SetIsTime(bool state)
{
	m_bIsTime = state;
}

//-----------------------------------------------------------------------------
// Purpose: paints a number at the specified position
//-----------------------------------------------------------------------------
void CHudNumericDisplay::PaintNumbers(HFont font, int xpos, int ypos, int value)
{
#ifdef EZ2
	if ( hud_malfunction_enable.GetBool() )
	{
		// Small chance of the number suffering imprecision
		if ( RandomFloat( 0.0f, 1.0f ) < Square( hud_malfunction_number_chance.GetFloat() * hud_malfunction_intensity.GetFloat() ) )
		{
			int max = (10.0f * hud_malfunction_intensity.GetFloat());
			value += RandomInt( -max, max );
		}
	}
#endif

	surface()->DrawSetTextFont(font);
	wchar_t unicode[6];
	if ( !m_bIsTime )
	{
		V_snwprintf(unicode, ARRAYSIZE(unicode), L"%d", value);
	}
	else
	{
		int iMinutes = value / 60;
		int iSeconds = value - iMinutes * 60;
#ifdef PORTAL
		// portal uses a normal font for numbers so we need the seperate to be a renderable ':' char
		if ( iSeconds < 10 )
			V_snwprintf( unicode, ARRAYSIZE(unicode), L"%d:0%d", iMinutes, iSeconds );
		else
			V_snwprintf( unicode, ARRAYSIZE(unicode), L"%d:%d", iMinutes, iSeconds );		
#else
		if ( iSeconds < 10 )
			V_snwprintf( unicode, ARRAYSIZE(unicode), L"%d`0%d", iMinutes, iSeconds );
		else
			V_snwprintf( unicode, ARRAYSIZE(unicode), L"%d`%d", iMinutes, iSeconds );
#endif
	}

	// adjust the position to take into account 3 characters
	int charWidth = surface()->GetCharacterWidth(font, '0');
	if (value < 100 && m_bIndent)
	{
		xpos += charWidth;
	}
	if (value < 10 && m_bIndent)
	{
		xpos += charWidth;
	}

	surface()->DrawSetTextPos(xpos, ypos);
	surface()->DrawUnicodeString( unicode );
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudNumericDisplay::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_bOverrideLabel = false;

	const char *text = inResourceData->GetString( "text", NULL );
	if (text)
	{
		// Override label text
		wchar_t *tempString = g_pVGuiLocalize->Find( text );
		if ( tempString )
		{
			SetLabelText( tempString );
		}
		else
		{
			wchar_t szText[128];
			V_UTF8ToUnicode( text, szText, sizeof( szText ) );
			SetLabelText( szText );
		}

		m_bOverrideLabel = true;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: draws the text
//-----------------------------------------------------------------------------
void CHudNumericDisplay::PaintLabel( void )
{
	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(GetFgColor());
	surface()->DrawSetTextPos(text_xpos, text_ypos);
	
#ifdef EZ2
	if ( hud_malfunction_enable.GetBool() )
	{
		float flIntensitySqr = Square( hud_malfunction_intensity.GetFloat() );

		wchar_t	wszLabelText[32];
		V_wcsncpy( wszLabelText, m_LabelText, sizeof( wszLabelText ) );

		int len = V_wcslen( wszLabelText );
		for ( int i = 0; i < len; i++ )
		{
			// Chance of replacing each character with a random ASCII character
			// TODO: Consider Unicode characters?
			if ( RandomFloat( 0.0f, 1.0f ) < flIntensitySqr )
			{
				wszLabelText[i] = (wchar_t)(RandomInt( '!', '~' ));
			}
		}

		// Flicker slightly
		Color clr = GetFgColor();
		float flClrIntensity = flIntensitySqr * 0.25f;
		clr[0] *= Clamp( RandomGaussianFloat( 1.0f, flClrIntensity ), 0.0f, 1.0f );
		clr[1] *= Clamp( RandomGaussianFloat( 1.0f, flClrIntensity ), 0.0f, 1.0f );
		clr[2] *= Clamp( RandomGaussianFloat( 1.0f, flClrIntensity ), 0.0f, 1.0f );
		clr[3] *= Clamp( RandomGaussianFloat( 1.0f, hud_malfunction_intensity.GetFloat() ), 0.0f, 1.0f );
		surface()->DrawSetTextColor( clr );

		surface()->DrawUnicodeString( wszLabelText );
		return;
	}
#endif

	surface()->DrawUnicodeString( m_LabelText );
}

//-----------------------------------------------------------------------------
// Purpose: renders the vgui panel
//-----------------------------------------------------------------------------
void CHudNumericDisplay::Paint()
{
	if (m_bDisplayValue)
	{
		// draw our numbers
		surface()->DrawSetTextColor(GetFgColor());
		PaintNumbers(m_hNumberFont, digit_xpos, digit_ypos, m_iValue);

		// draw the overbright blur
		for (float fl = m_flBlur; fl > 0.0f; fl -= 1.0f)
		{
			if (fl >= 1.0f)
			{
				PaintNumbers(m_hNumberGlowFont, digit_xpos, digit_ypos, m_iValue);
			}
			else
			{
				// draw a percentage of the last one
				Color col = GetFgColor();
				col[3] *= fl;
				surface()->DrawSetTextColor(col);
				PaintNumbers(m_hNumberGlowFont, digit_xpos, digit_ypos, m_iValue);
			}
		}
	}

	// total ammo
	if (m_bDisplaySecondaryValue)
	{
#ifdef MAPBASE
		// Bonus progress uses this now (was previously unused)
		surface()->DrawSetTextColor( UsesUniqueSecondaryColor() ? m_Ammo2Color : GetFgColor() );
#else
		surface()->DrawSetTextColor(GetFgColor());
#endif
		PaintNumbers(m_hSmallNumberFont, digit2_xpos, digit2_ypos, m_iSecondaryValue);
	}

	PaintLabel();
}



