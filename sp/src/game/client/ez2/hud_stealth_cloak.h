//=============================================================================//
//
// Purpose: A HUD element which displays the player's cloak state.
// 
// Author: Blixibon
//
//=============================================================================//

#ifndef C_HUD_STEALTH_CLOAK_H
#define C_HUD_STEALTH_CLOAK_H
#ifdef _WIN32
#pragma once
#endif

#include "hud.h"
#include "hudelement.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/Label.h"

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudStealthCloak : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudStealthCloak, vgui::EditablePanel );

public:
	CHudStealthCloak( const char *pElementName );
	~CHudStealthCloak();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Init( void );
	virtual void Reset( void );
	virtual void Paint( void );
	bool ShouldDraw();

	enum CloakLabelType_t
	{
		CLOAK_LABEL_NONE,
		CLOAK_LABEL_VISIBLE,		// We could be seen by enemies
		CLOAK_LABEL_COMPROMISED,	// Enemies already see us
	};

private:

	vgui::Label *m_pCloakLabel;

	int		m_iCloakIconVisible;
	int		m_iCloakIconCompromised;
	int		m_iCloakIconLaser;
	int		m_iCloakIconTouch;

	CPanelAnimationVar( Color, m_clrCaution, "color_empty", "Stealth.Alert_Caution" );
	CPanelAnimationVar( Color, m_clrCompromised, "color_full", "Stealth.Alert_Compromised" );

	CPanelAnimationVarAliasType( float, m_flCloakIconX, "cloak_icon_x", "210", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flCloakIconY, "cloak_icon_y", "100", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flCloakIconWide, "cloak_icon_wide", "60", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flCloakIconTall, "cloak_icon_tall", "60", "proportional_float" );
};

#endif // C_HUD_STEALTHALERT_H