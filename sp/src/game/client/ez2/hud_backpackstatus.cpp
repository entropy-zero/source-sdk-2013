//=============================================================================//
//
// Purpose: Generic Backpack
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "hud_backpackstatus.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_ez2_player.h"
#include "ez2/ez2_player_backpack_shared.h"
#include "in_buttons.h"
#include "input.h"
#include "vgui_controls/AnimationController.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


DECLARE_HUDELEMENT( CHudBackpackStatus );
DECLARE_HUD_MESSAGE( CHudBackpackStatus, BackpackItemAdded );
DECLARE_HUD_MESSAGE( CHudBackpackStatus, BackpackItemRemoved );
DECLARE_HUD_MESSAGE( CHudBackpackStatus, BackpackItemDeny );
DECLARE_HUD_MESSAGE( CHudBackpackStatus, BackpackItemPing );

using namespace vgui;

ConVar	sk_backpack_max( "sk_backpack_max", "4", FCVAR_REPLICATED, "", true, 0, true, MAX_BACKPACK_ITEMS );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBackpackStatus::CHudBackpackStatus( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBackpackStatus" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

CHudBackpackStatus::~CHudBackpackStatus()
{
	/*
	if (vgui::surface())
	{
		if (m_textureID_BackpackItem != -1)
		{
			vgui::surface()->DestroyTextureID( m_textureID_BackpackItem );
			m_textureID_BackpackItem = -1;
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBackpackStatus::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBackpackStatus, BackpackItemAdded );
	HOOK_HUD_MESSAGE( CHudBackpackStatus, BackpackItemRemoved );
	HOOK_HUD_MESSAGE( CHudBackpackStatus, BackpackItemDeny );
	HOOK_HUD_MESSAGE( CHudBackpackStatus, BackpackItemPing );
	m_iNumBackpackItems = 0;
	m_iMaxBackpackItems = 0;
	m_bBackpackItemLost = false;
	m_iLastBackpackItemSlot = -1;
	SetAlpha( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBackpackStatus::Reset( void )
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudBackpackStatus::ShouldDraw( void )
{
	C_EZ2_Player *pPlayer = (C_EZ2_Player *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	bool bNeedsDraw = false;

	bNeedsDraw = ( m_iNumBackpackItems != 0 || pPlayer->m_iBackpackBits != 0 ||
					m_LastItemColor[3] > 0 || m_SelectionNumbersColor[3] > 0 ||
					( (::input->GetButtonBits(0) & IN_BACKPACK) && pPlayer->m_bCarryingObject ) );
		
	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudBackpackStatus::ShouldTakeMenuInput( void )
{
	C_EZ2_Player *pPlayer = (C_EZ2_Player *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if ( !(::input->GetButtonBits(0) & IN_BACKPACK) )
		return false;

	// Note that we cannot currently distinguish non-pickup use ents, but that's tolerable for this
	if ( pPlayer->m_bCarryingObject )
		return true;
	
	if ( HasItems() )
		return true;
		
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: selects an item from the menu
//-----------------------------------------------------------------------------
void CHudBackpackStatus::SelectMenuItem( int menu_item )
{
	char szbuf[32];
	Q_snprintf( szbuf, sizeof( szbuf ), "player_backpack_select %d\n", menu_item - 1 );
	engine->ClientCmd( szbuf );
}

//-----------------------------------------------------------------------------
// Purpose: updates hud icons
//-----------------------------------------------------------------------------
void CHudBackpackStatus::OnThink( void )
{
	C_EZ2_Player *pPlayer = (C_EZ2_Player *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;
		
	Assert( sk_backpack_max.GetInt() <= MAX_BACKPACK_ITEMS );

	int nNumItems = 0;
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( pPlayer->m_iBackpackBits & (1 << i) )
			nNumItems++;
	}

	//if ( nNumItems > 0 )
	{
		// Check if we should change focus state
		bool bPlayerFocus = ShouldTakeMenuInput();
		if ( m_bInFocus != bPlayerFocus )
		{
			if ( bPlayerFocus )
			{
				// The player may be trying to get an item
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackStatusFocus" );
			}
			else
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackStatusUnfocus" );
			}
			m_bInFocus = bPlayerFocus;
		}
	}

	if ( nNumItems == m_iNumBackpackItems && !m_bInFocus )
		return;

	// update status display
	if ( nNumItems > 0 || m_bInFocus )
	{
		// we have items, show the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackStatusShow" );
	}
	else if (m_LastItemColor[3] <= 0 && !m_bInFocus)
	{
		// no items, hide the display
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackStatusHide" );
	}

	if ( nNumItems > m_iNumBackpackItems )
	{
		// someone is added
		// reset the last icon color and animate
		m_LastItemColor = m_ItemIconColor;
		m_LastItemColor[3] = 0;
		m_bBackpackItemAdded = true;
		DevMsg( "Sequence: BackpackItemAdded\n" );
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemAdded" ); 
	}
	else if ( nNumItems < m_iNumBackpackItems)
	{
		//Msg( "BackpackItem status thinking (Player's: %i/%i) (Ours: %i/%i)\n",
		//	BackpackItems, tripmines,
		//	m_iNumBackpackItems, m_iNumTripmines );

		// someone has left
		// reset the last icon color and animate
		m_LastItemColor = m_ItemIconColor;
		m_bBackpackItemAdded = false;

		if (m_bBackpackItemLost)
		{
			DevMsg( "Sequence: BackpackItemDied\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemDied" );

			m_bBackpackItemLost = false;
		}
		else if (m_bBackpackItemConsumed)
		{
			DevMsg( "Sequence: BackpackItemConsumed\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemConsumed" );

			m_bBackpackItemConsumed = false;
		}
		else
		{
			DevMsg( "Sequence: BackpackItemLeft\n" );
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemLeft" ); 
		}
	}

	m_iNumBackpackItems = nNumItems;
	
	if (sk_backpack_max.GetInt() != m_iMaxBackpackItems)
	{
		m_iMaxBackpackItems = sk_backpack_max.GetInt();

		// Expand the menu
		switch (m_iMaxBackpackItems)
		{
			case 0:
			case 1:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeOne" );
				break;

			case 2:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeTwo" );
				break;

			case 3:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeThree" );
				break;

			case 4:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeFour" );
				break;

			case 5:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeFive" );
				break;

			case 6:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeSix" );
				break;

			case 7:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeSeven" );
				break;

			default:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeMax" );
			case 8:
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeEight" );
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudBackpackStatus::MsgFunc_BackpackItemAdded( bf_read &msg )
{
	m_iLastBackpackItemSlot = msg.ReadByte();

	if ( m_iNumBackpackItems <= 0 )
		m_iNumBackpackItems = -1;

#if 0
	//int nNumItems = msg.ReadByte();

	int nSlot = msg.ReadByte();

	//for ( int i = 0; i < nNumItems; i++ )
	{
		C_BaseEntity *pEntity = C_BaseEntity::Instance( msg.ReadShort() );
		if ( !pEntity )
			return;

		// Find first valid idx
		/*int nFirstValid = -1;
		for ( int j = 0; j < sk_backpack_max.GetInt(); j++ )
		{
			if (!m_BackpackItems[j].ent)
				nFirstValid = j;
			else if ( m_BackpackItems[j].ent == pEntity )
			{
				// Already added
				return;
			}
		}

		if ( nFirstValid == -1 )
		{
			Warning( "CHudBackpackStatus: No room for item\n" );
			return;
		}*/

		m_BackpackItems[nSlot].ent = pEntity;
		m_BackpackItems[nSlot].icon = msg.ReadChar();
		m_BackpackItems[nSlot].clr.r = msg.ReadByte();
		m_BackpackItems[nSlot].clr.g = msg.ReadByte();
		m_BackpackItems[nSlot].clr.b = msg.ReadByte();

		m_iLastBackpackItemSlot = nSlot;

		// Default icon
		if ( m_BackpackItems[nSlot].icon == 0 )
			m_BackpackItems[nSlot].icon = '*';
	}

	OnBackpackStatusChange();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudBackpackStatus::MsgFunc_BackpackItemRemoved( bf_read &msg )
{
	m_iLastBackpackItemSlot = msg.ReadByte();
	m_bBackpackItemLost = msg.ReadOneBit();
	m_bBackpackItemConsumed = msg.ReadOneBit();

#if 0
	//int nNumItems = msg.ReadByte();

	bool bLost = msg.ReadOneBit();
	if ( bLost )
		m_bBackpackItemLost = true;

	//for ( int i = 0; i < nNumItems; i++ )
	{
		C_BaseEntity *pEntity = C_BaseEntity::Instance( msg.ReadShort() );
		if ( !pEntity )
			return;

		// Find the index for this item
		int j = 0;
		for ( ; j < sk_backpack_max.GetInt(); j++ )
		{
			if (m_BackpackItems[j].ent == pEntity)
				break;
		}

		if ( j == sk_backpack_max.GetInt() )
		{
			//Warning( "CHudBackpackStatus: Cannot find item %s\n", pEntity->GetDebugName() );
			return;
		}

		m_BackpackItems[j].ent = NULL;

		m_iLastBackpackItemSlot = j;

		// Set up last item color with this one, if valid
		if ( m_BackpackItems[j].clr.r > 0 || m_BackpackItems[j].clr.g > 0 || m_BackpackItems[j].clr.b > 0 )
		{
			m_LastItemColor[0] = m_BackpackItems[j].clr.r;
			m_LastItemColor[1] = m_BackpackItems[j].clr.g;
			m_LastItemColor[2] = m_BackpackItems[j].clr.b;
		}
	}

	OnBackpackStatusChange();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudBackpackStatus::MsgFunc_BackpackItemDeny( bf_read &msg )
{
	m_iLastBackpackItemSlot = msg.ReadByte();
	if ( m_iLastBackpackItemSlot == -1 )
	{
		// Hit max, no specific item
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemElementSizeMax" );
	}
	else
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemDeny" );
		m_bBackpackItemAdded = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notification of squad member being killed
//-----------------------------------------------------------------------------
void CHudBackpackStatus::MsgFunc_BackpackItemPing( bf_read &msg )
{
	m_iLastBackpackItemSlot = msg.ReadByte();
	if (m_iLastBackpackItemSlot == -1)
	{
		// TODO
	}
	else
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "BackpackItemPing" );
		m_bBackpackItemAdded = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws the power bar
//-----------------------------------------------------------------------------
void CHudBackpackStatus::Paint()
{
	C_EZ2_Player *pPlayer = (C_EZ2_Player *)C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	// draw the suit power bar
	//surface()->DrawSetTextColor( m_BackpackItemIconColor );
	surface()->DrawSetTextFont( m_hIconFont );

	bool bShowHighlight = (m_bBackpackItemAdded || m_LastItemColor[3]);

	//Msg( "BackpackItem Paint: %s - (%i/%i), (%i/%i)\n", bShowHighlight ? "Showing highlight" : "Not showing highlight",
	//	m_iNumBackpackItems, m_iNumBackpackItemsDiff,
	//	m_iNumTripmines, m_iNumTripminesDiff );

	int xpos = m_flIconInsetX, ypos = m_flIconInsetY;
	//int iconSize = m_flIconGap - 2;
	for (int i = 0; i < m_iMaxBackpackItems; i++)
	{
		if ( pPlayer->m_iBackpackBits & (1 << i) || ( bShowHighlight && m_iLastBackpackItemSlot == i ) )
		{
			Color clr = m_ItemIconColor;
		
			if ( bShowHighlight && m_iLastBackpackItemSlot == i )
				clr = m_LastItemColor;

			// Extract color and icon
			Color itemClr;
			itemClr.SetRawColor( pPlayer->m_BackpackIconClrs[i] );
		
			if ( itemClr[0] > 0 || itemClr[1] > 0 || itemClr[2] > 0 )
			{
				clr[0] = itemClr[0];
				clr[1] = itemClr[1];
				clr[2] = itemClr[2];
			}

			// Default icon
			if ( itemClr[3] == 0 )
				itemClr[3] = '*';

			surface()->DrawSetTextColor( clr );
			surface()->DrawSetTextPos( xpos, ypos );
			surface()->DrawUnicodeChar( itemClr[3] );
			//surface()->DrawSetTexture( m_textureID_BackpackItem );
			//surface()->DrawTexturedRect( xpos, ypos, xpos + iconSize, ypos + iconSize );
		}

		if ( m_SelectionNumbersColor[3] > 0 )
		{
			// Draw numbers below each icon
			int nNumX = xpos + m_flIconNumberOffsetX;
			int nNumY = ypos + m_flIconNumberOffsetY;

			char cChar = '1' + i;
			nNumX -= (surface()->GetCharacterWidth( m_hIconNumberFont, cChar ) * 0.5f);
			
			Color clr = m_SelectionNumbersColor;
		
			if ( m_iLastBackpackItemSlot == i && m_SelectionLastNumberColor[3] != clr[3] && m_SelectionLastNumberColor[3] )
				clr = m_SelectionLastNumberColor;

			surface()->DrawSetTextFont( m_hIconNumberFont );
			surface()->DrawSetTextColor( clr );
			surface()->DrawSetTextPos( nNumX, nNumY );
			surface()->DrawUnicodeChar( cChar );

			// Reset font
			surface()->DrawSetTextFont( m_hIconFont );
		}

		xpos += m_flIconGap;
	}
}


