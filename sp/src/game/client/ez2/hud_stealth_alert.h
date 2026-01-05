//=============================================================================//
//
// Purpose: A HUD element which displays NPCs' alertness levels when using stealth senses.
// 
// Author: Blixibon
//
//=============================================================================//

#ifndef C_HUD_STEALTH_ALERT_H
#define C_HUD_STEALTH_ALERT_H
#ifdef _WIN32
#pragma once
#endif

#include "hud.h"
#include "hudelement.h"
#include "vgui_controls/EditablePanel.h"
#include "ez2/ai_stealth_shared.h"

#define ALERT_TARGET_COMBAT_LEVEL			-1		// Assigned manually when they enter combat
#define ALERT_TARGET_COMBAT_TIME_HOLD		1		// Hold fading alert target for X seconds
#define ALERT_TARGET_COMBAT_TIME_FADE		3		// After done holding, fade alert target over X seconds

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudStealthAlertIcon : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudStealthAlertIcon, vgui::EditablePanel );
public:
	CHudStealthAlertIcon( C_BaseEntity *pSource, AlertSourceType_t iType, float flLevel, float flCombatTime, bool bTargetingPlayer,
		Panel *parent, int texCircle = -1, int texSpotted = -1 );
	~CHudStealthAlertIcon();

	virtual void	PerformLayout();
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );

	void	SetLevel( float flLevel ) { m_flLevel = flLevel; }
	void	SetCombatTime( float flCombatTime ) { m_flCombatTime = flCombatTime; m_flLevel = ALERT_TARGET_COMBAT_LEVEL; }
	void	SetTargetingPlayer( bool bTargetingPlayer ) { m_bTargetingPlayer = bTargetingPlayer; }

	C_BaseEntity *GetSource() const { return m_hSource; }
	AlertSourceType_t GetType() const { return m_iType; }
	float GetLevel() const { return m_flLevel; }
	float GetCombatTime() const { return m_flCombatTime; }

	void	GetAlertSize( int &wide, int &tall ) { GetSize( wide, tall ); }
	int		GetOuterMargin() const { return m_iAlertOuterMargin; }
	const Vector	&GetWorldPosition();

protected:
	virtual void Paint();
	void PaintBar();
	void PaintCircle();

private:

	EHANDLE				m_hSource;
	AlertSourceType_t	m_iType;
	float				m_flLevel;
	float				m_flCombatTime = -1.0f;
	bool				m_bTargetingPlayer;

	Vector		m_vecWorldPos;

	bool	m_bCircle;

	int		m_iCircleBGTexture;
	int		m_iCircleTexture;
	int		m_iSuspiciousTexture;
	int		m_iSpottedTexture;

	CPanelAnimationVar( Color, m_clrBG, "color_bg", "BgColor" );

	CPanelAnimationVar( Color, m_clrEmpty, "color_empty", "Stealth.Alert_Caution" );
	CPanelAnimationVar( Color, m_clrFull, "color_full", "Stealth.Alert_Compromised" );
	CPanelAnimationVar( Color, m_clrTargetingOther, "color_targeting_other", "Stealth.Alert_Caution" );

	CPanelAnimationVar( vgui::HFont, m_hAlertSpottedFont, "AlertSpottedFont", "AlertSpotted" );

	CPanelAnimationVarAliasType( int, m_iAlertFillMargin, "alert_fill_margin", "6", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iAlertOuterMargin, "alert_outer_margin", "4", "proportional_int" );

	CPanelAnimationVarAliasType( float, m_flHeadOffsetMin, "head_offset_min", "8", "float" );
	CPanelAnimationVarAliasType( float, m_flHeadOffsetMax, "head_offset_max", "16", "float" );
	CPanelAnimationVarAliasType( float, m_flHeadOffsetDist, "head_offset_dist", "1000", "float" );
};

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudStealthAlertIconParent : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudStealthAlertIconParent, vgui::Panel );
public:
	CHudStealthAlertIconParent( Panel *parent, const char *panelName ) : vgui::Panel( parent, panelName ) {}
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
};

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudStealthAlert : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudStealthAlert, vgui::Panel );

public:
	CHudStealthAlert( const char *pElementName );
	~CHudStealthAlert();
	virtual void Init( void );
	virtual void Reset( void );
	virtual void Paint( void );
	bool ShouldDraw();

	void MsgFunc_AlertTargetUpdate( bf_read &msg );
	void MsgFunc_AlertTargetEntersCombat( bf_read &msg );

	int CreateAlertTarget( CBaseEntity *pEntity, AlertSourceType_t iType, float flLevel, float flCombatTime, bool bTargetingPlayer );

private:

	CUtlVector<vgui::Panel*>			m_AlertIconParents;
	CUtlVector<CHudStealthAlertIcon*>	m_AlertIcons;
	float								m_flNextAlertSoundTime;
	float								m_flNextAlertAmbSoundTime;

	CPanelAnimationVarAliasType( int, m_iAlertMinX, "alert_min_x", "90", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iAlertMaxX, "alert_max_x", "670", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iAlertMinY, "alert_min_y", "60", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iAlertMaxY, "alert_max_y", "360", "proportional_int" );
};

#endif // C_HUD_STEALTHALERT_H