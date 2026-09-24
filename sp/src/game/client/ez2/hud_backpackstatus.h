//=============================================================================//
//
// Purpose: Generic Backpack
// 
// Author: Blixibon
//
//=============================================================================//

#ifndef C_HUD_BACKPACKSTATUS_H
#define C_HUD_BACKPACKSTATUS_H
#ifdef _WIN32
#pragma once
#endif

#include "hud.h"
#include "hudelement.h"
#include "vgui_controls/EditablePanel.h"
#include "vgui_controls/Label.h"

struct BackpackItem_t
{
	EHANDLE		ent;		// The ent associated with this item
	char		icon;		// The icon this item should use
	color24		clr;		// The color of the icon (0 for default)
};

// The maximum technically supported. Actual allowed value is sk_backpack_max, which cannot exceed this
#define MAX_BACKPACK_ITEMS		8

//-----------------------------------------------------------------------------
// Purpose: Shows the sprint power bar
//-----------------------------------------------------------------------------
class CHudBackpackStatus : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBackpackStatus, vgui::Panel );

public:
	CHudBackpackStatus( const char *pElementName );
	~CHudBackpackStatus();
	virtual void Init( void );
	virtual void Reset( void );
	virtual void OnThink( void );
	bool ShouldDraw();

	bool ShouldTakeMenuInput();
	void SelectMenuItem( int menu_item );
	inline bool HasItems() const { return m_iNumBackpackItems > 0; }

	// So that we can move out of the way
	void SuitPowerDevicesChanged( int nNumDevices );

	void MsgFunc_BackpackItemAdded( bf_read &msg );
	void MsgFunc_BackpackItemRemoved( bf_read &msg );
	void MsgFunc_BackpackItemDeny( bf_read &msg );
	void MsgFunc_BackpackItemPing( bf_read &msg );

protected:
	virtual void Paint();

private:
	CPanelAnimationVar( vgui::HFont, m_hIconFont, "IconFont", "HudNumbers" );
	CPanelAnimationVarAliasType( float, m_flIconInsetX, "IconInsetX", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconInsetY, "IconInsetY", "8", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconGap, "IconGap", "20", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconNumberOffsetX, "IconNumberOffsetX", "10", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flIconNumberOffsetY, "IconNumberOffsetY", "20", "proportional_float" );
	CPanelAnimationVar( vgui::HFont, m_hIconNumberFont, "IconNumberFont", "HudSelectionNumbers" );

	CPanelAnimationVar( Color, m_ItemIconColor, "BackpackItemIconColor", "255 220 0 160" );
	CPanelAnimationVar( Color, m_LastItemColor, "LastBackpackItemColor", "255 220 0 0" );
	CPanelAnimationVar( Color, m_SelectionNumbersColor, "BackpackSelectionNumbers", "255 220 0 160" );
	CPanelAnimationVar( Color, m_SelectionLastNumberColor, "BackpackSelectionLastNumber", "255 220 0 160" );

	int m_iNumBackpackItems;
	int m_iMaxBackpackItems;
	int m_iLastBackpackItemSlot;	// Which slot we just added an item to

	bool m_bInFocus;

	bool m_bBackpackItemAdded;
	bool m_bBackpackItemLost;
	bool m_bBackpackItemConsumed;

	CPanelAnimationVarAliasType( int, m_iDefaultX, "xpos_default", "r120", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_iDefaultWide, "wide_default", "104", "proportional_int" );
};

#endif // C_HUD_STEALTHALERT_H