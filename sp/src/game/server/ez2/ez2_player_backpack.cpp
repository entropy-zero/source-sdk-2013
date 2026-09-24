//=============================================================================//
//
// Purpose: Creates custom metadata for each save file
//
//=============================================================================//

#include "cbase.h"
#include "ez2_player.h"
#include "props.h"
#include "eventqueue.h"
#include "mapbase_matchers_base.h"

ConVar	sk_backpack_max( "sk_backpack_max", "4", FCVAR_REPLICATED, "", true, 0, true, MAX_BACKPACK_ITEMS );

ConVar	player_backpack_prop_max_radius( "player_backpack_prop_max_radius", "12" );
ConVar	player_backpack_prop_max_mass( "player_backpack_prop_max_mass", "5" );
ConVar	player_backpack_store_move( "player_backpack_store_move", "8" );
ConVar	player_backpack_store_move_angle( "player_backpack_store_move_angle", "0.5" );
ConVar	player_backpack_store_fade( "player_backpack_store_fade", "8" );

extern bool PropIsGib( CBaseEntity *pEntity );

//-----------------------------------------------------------------------------

struct BackpackItemColor_t
{
	const char *pszName;
	byte r, g, b;
};

// Hardcoded at the moment. Consider putting into a script or model KV
static BackpackItemColor_t g_BackpackItemColors[] = {
	{ "blue",		16,		96,		224 },
	{ "green",		16,		224,	96 },
	{ "red",		224,	48,		48 },
	{ "yellow",		224,	224,	48 },
	{ "orange",		224,	128,	24 },
	{ "purple",		192,	48,		224 },
	{ "cyan",		48,		224,	224 },
	{ "black",		112,	112,	112 },
	{ "white",		224,	224,	224 },
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::IsBackpackEnabled() const
{
	return m_bBackpackEnabled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::SetBackpackEnabled( bool bEnabled )
{
	m_bBackpackEnabled = bEnabled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::IsItemInBackpack( CBaseEntity *pItem ) const
{
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( m_hBackpackItems[i] == pItem )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CEZ2_Player::GetBackpackItem( int nIdx )
{
	return m_hBackpackItems[nIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEZ2_Player::GetMaxBackpackItems() const
{
	return sk_backpack_max.GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::CanStoreItemInBackpack( CBaseEntity *pItem, bool *pbFull )
{
	int nNumItems = 0;
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( m_hBackpackItems[i] )
		{
			nNumItems++;

			// Already in backpack
			if ( m_hBackpackItems[i] == pItem )
				return false;
		}
	}

	if ( nNumItems >= sk_backpack_max.GetInt() )
	{
		if (pbFull)
			*pbFull = true;
		return false;
	}

	// Only small objects that could fit on a belt
	if ( pItem->ClassMatches( "prop_physics" ) )
	{
		CPhysicsProp *pProp = static_cast<CPhysicsProp *>(pItem);

		// Gibs are meant to be short-lived
		if ( pProp->IsGib() )
			return false;

		if ( pProp->BoundingRadius() > player_backpack_prop_max_radius.GetFloat() )
			return false;

		if ( pProp->GetMass() > player_backpack_prop_max_mass.GetFloat() )
			return false;
	}
	else if ( !pItem->ClassMatches( "item_healthvial" )
			&& !pItem->ClassMatches( "item_battery" )
			&& !pItem->ClassMatches( "item_ammo_smg1_grenade" )
		)
	{
		return false;
	}

	return true;
}

extern ConVar sk_suit_maxarmor;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::ShouldHintBackpackStore( CBaseEntity *pItem )
{
	if ( !CanStoreItemInBackpack( pItem ) || m_hLastBackpackItem == pItem )
		return false;

	if ( pItem->ClassMatches( "item_health*" ) )
	{
		// Extra health
		if ( GetHealth() >= GetMaxHealth() )
			return true;
	}
	else if ( pItem->ClassMatches( "item_battery" ) )
	{
		// Extra armor
		if ( ArmorValue() >= sk_suit_maxarmor.GetInt() )
			return true;
	}
	else if ( pItem->FindContextByName( "keycard" ) != -1 )
	{
		// Keycard
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::ShouldHintBackpackDeploy( int &nSlot )
{
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( m_hBackpackItems[i] )
		{
			if ( m_hBackpackItems[i]->ClassMatches( "item_health*" ) )
			{
				if ( GetHealth() < GetMaxHealth() )
				{
					nSlot = i;
					return true;
				}
			}
			else if ( m_hBackpackItems[i]->ClassMatches( "item_battery" ) )
			{
				if ( ArmorValue() < sk_suit_maxarmor.GetInt() )
				{
					nSlot = i;
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::GetBackpackDataForItem( CBaseEntity *pItem, unsigned char &icon, byte &r, byte &g, byte &b )
{
	if ( pItem->ClassMatches( "item_healthvial" ) )
	{
		icon = '+';
	}
	else if ( pItem->ClassMatches( "item_battery" ) )
	{
		icon = ',';
	}
	else
	{
		int nContext = pItem->FindContextByName( "keycard" );
		if ( nContext != -1 )
		{
			icon = '-';
		}
		else
		{
			// Generic color
			nContext = pItem->FindContextByName( "backpack_clr" );
		}
		
		if ( nContext != -1 )
		{
			const char *pszColor = pItem->GetContextValue( nContext );

			// Go through our color presets
			int i = 0;
			for ( ; i < ARRAYSIZE( g_BackpackItemColors ); i++ )
			{
				if ( FStrEq( pszColor, g_BackpackItemColors[i].pszName ) )
				{
					r = g_BackpackItemColors[i].r;
					g = g_BackpackItemColors[i].g;
					b = g_BackpackItemColors[i].b;
					break;
				}
			}

			if ( i == ARRAYSIZE( g_BackpackItemColors ) )
			{
				// See if it's a custom color
				int tmp[3];
				UTIL_StringToIntArray( tmp, 3, pszColor );
				r = tmp[0];
				g = tmp[1];
				b = tmp[2];
			}
		}

		if ( icon == 0 )
		{
			// Generic from model name
			if ( Matcher_NamesMatch( "*bottle*", STRING( pItem->GetModelName() ) ) )
			{
				icon = '.';
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::GetBackpackPos( CBaseEntity *pItem, Vector &vecOutPos )
{
	vecOutPos = WorldSpaceCenter();
	//vecOutPos.z -= 8.0f;

	// Appear to go down towards the HUD element
	Vector vecRight, vecUp;
	GetVectors( NULL, &vecRight, &vecUp );
	vecOutPos += (vecRight * -40.0f);
	//vecOutPos += (vecUp * -4.0f);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::StoreItemInBackpack( CBaseEntity *pItem, int nIdx, bool bInstant )
{
	if ( nIdx == -1 )
	{
		nIdx = 0;
		for ( ; nIdx < sk_backpack_max.GetInt(); nIdx++ )
		{
			if ( !m_hBackpackItems[nIdx] )
				break;
		}
	}

	if ( nIdx == sk_backpack_max.GetInt() )
		return;

	//m_hBackpackItems.Set( nIdx, pItem );
	m_hBackpackItems[nIdx] = pItem;
	m_iBackpackBits |= (1 << nIdx);

	ClearUseEntity();

	pItem->SetAbsVelocity( vec3_origin );
	pItem->RemoveSolidFlags( FSOLID_TRIGGER );
	pItem->RemoveEffects( EF_ITEM_BLINK );
	pItem->VPhysicsDestroyObject();

	pItem->AddSolidFlags( FSOLID_NOT_SOLID );

	if ( bInstant )
	{
		pItem->SetMoveType( MOVETYPE_NONE );
		pItem->AddEffects( EF_NODRAW );
		pItem->SetParent( this );
		pItem->SetLocalOrigin( vec3_origin );
		pItem->SetLocalAngles( vec3_angle );
	}
	else
	{
		m_hLastBackpackItem = pItem;
		pItem->SetMoveType( MOVETYPE_NOCLIP );
		pItem->SetRenderMode( kRenderTransColor );
	}

	if ( pItem->GetFlags() & FL_OBJECT )
	{
		g_AI_SensedObjectsManager.RemoveEntity( pItem );
	}

	Color clr;
	GetBackpackDataForItem( pItem, clr[3], clr[0], clr[1], clr[2] );

	// We send the color and icon together as a 32-bit int
	m_BackpackIconClrs.Set( nIdx, clr.GetRawColor() );

	CSingleUserRecipientFilter user( this );
	user.MakeReliable();
	UserMessageBegin( user, "BackpackItemAdded" );
	WRITE_BYTE( nIdx );
	MessageEnd();

	variant_t var;
	FirePlayerProxyOutput( "OnBackpackStoreItem", var, pItem, this );

	if ( m_flNextBackpackHintTime - gpGlobals->curtime > ( BACKPACK_HINT_COOLDOWN - 7.0f ) )
	{
		// Hint active, end it
		UTIL_HudHintText( this, "" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::RemoveItemFromBackpack( CBaseEntity *pItem, bool bPickup, bool bDelete, bool bLost, bool bConsumed )
{
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( m_hBackpackItems[i] == pItem )
			return RemoveItemFromBackpack( i, bPickup, bDelete, bLost, bConsumed );
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::RemoveItemFromBackpack( int nIdx, bool bPickup, bool bDelete, bool bLost, bool bConsumed )
{
	if ( nIdx < 0 || nIdx >= MAX_BACKPACK_ITEMS )
		return false;

	CBaseEntity *pItem = m_hBackpackItems[nIdx];

	if ( !pItem )
		return false;

	CSingleUserRecipientFilter user( this );
	user.MakeReliable();

	if ( bDelete || TryConsumeBackpackItem( pItem ) )
	{
		// Just get rid of it directly
		UTIL_Remove( pItem );
		m_hBackpackItems[nIdx] = NULL;
		m_iBackpackBits &= ~(1 << nIdx);

		UserMessageBegin( user, "BackpackItemRemoved" );
		WRITE_BYTE( nIdx );
		WRITE_BOOL( bLost );
		WRITE_BOOL( !bDelete || bConsumed );
		MessageEnd();

		return true;
	}

	if ( m_hLastBackpackItem == pItem )
	{
		m_hLastBackpackItem->SetRenderColorA( 255 );
		m_hLastBackpackItem->SetRenderMode( kRenderNormal );

		m_hLastBackpackItem->SetAbsVelocity( vec3_origin );
		m_hLastBackpackItem->SetLocalAngularVelocity( vec3_angle );

		//m_hLastBackpackItem = NULL;
	}

	pItem->SetParent( NULL );

#if 1
	// Putting it by our side is too unpredictable and causes an annoying impact sound due to the pickup
	pItem->SetAbsOrigin( WorldSpaceCenter() );
#else
	Vector vecTargetPos;
	GetBackpackPos( pItem, vecTargetPos );

	// Make sure it actually traces to it
	trace_t tr;
	CTraceFilterSkipTwoEntities filter( this, pItem, COLLISION_GROUP_NONE );
	UTIL_TraceHull( WorldSpaceCenter(), vecTargetPos, pItem->WorldAlignMins(), pItem->WorldAlignMaxs(), MASK_SOLID, &filter, &tr );

	pItem->SetAbsOrigin( tr.endpos );
#endif

	pItem->RemoveSolidFlags( FSOLID_NOT_SOLID );
	pItem->RemoveEffects( EF_NODRAW );
	pItem->SetGravity( 1.0 );

	if ( pItem->IsCombatItem() || pItem->IsBaseCombatWeapon() )
	{
		pItem->SetMoveType( MOVETYPE_FLYGRAVITY );
	}
	else
	{
		pItem->SetMoveType( MOVETYPE_VPHYSICS );
	}

	pItem->VPhysicsDestroyObject();
	pItem->Spawn();	// So that we re-initialize as a physical item

	//m_hBackpackItems.Set( nIdx, NULL );
	m_hBackpackItems[nIdx] = NULL;
	m_iBackpackBits &= ~(1 << nIdx);

	EmitSound( "EZ2Player.BackpackDeploy" );

	if ( bPickup )
	{
		PickupObject( pItem, false );
		OnUseEntity( pItem );
	}

	UserMessageBegin( user, "BackpackItemRemoved" );
	WRITE_BYTE( nIdx );
	WRITE_BOOL( bLost );
	WRITE_BOOL( bConsumed );
	MessageEnd();

	variant_t varEmpty;
	FirePlayerProxyOutput( "OnBackpackDeployItem", varEmpty, pItem, this );

	if ( m_flNextBackpackHintTime - gpGlobals->curtime > ( BACKPACK_HINT_COOLDOWN - 7.0f ) )
	{
		// Hint active, end it
		UTIL_HudHintText( this, "" );
	}

	return true;
}

extern ConVar sk_healthvial;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEZ2_Player::TryConsumeBackpackItem( CBaseEntity *pItem )
{
	if ( pItem->ClassMatches( "item_healthvial" ) )
	{
		if ( TakeHealth( sk_healthvial.GetFloat(), DMG_GENERIC ) )
		{
			EmitSound( "HealthVial.Touch" ); // HealthVial.BackpackConsume
			return true;
		}
	}
	else if ( pItem->ClassMatches( "item_battery" ) )
	{
		return ApplyBattery( 1.0f );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::BackpackPingEffect( int nSlot )
{
	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "BackpackItemPing" );
	WRITE_BYTE( nSlot );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::BackpackDenyEffect( int nSlot )
{
	CSingleUserRecipientFilter filter( this );
	filter.MakeReliable();
	UserMessageBegin( filter, "BackpackItemDeny" );
	WRITE_BYTE( nSlot );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::BackpackItemFadeThink()
{
	byte a = m_hLastBackpackItem->GetRenderColor().a;

	if ( a <= 0 || a < player_backpack_store_fade.GetInt() )
	{
		m_hLastBackpackItem->SetMoveType( MOVETYPE_NONE );
		m_hLastBackpackItem->AddEffects( EF_NODRAW );
		m_hLastBackpackItem->SetRenderColorA( 255 );
		m_hLastBackpackItem->SetRenderMode( kRenderNormal );

		m_hLastBackpackItem->SetAbsVelocity( vec3_origin );
		m_hLastBackpackItem->SetLocalAngularVelocity( vec3_angle );
		m_hLastBackpackItem->SetParent( this );
		m_hLastBackpackItem->SetLocalOrigin( vec3_origin );
		m_hLastBackpackItem->SetLocalAngles( vec3_angle );

		//m_hLastBackpackItem = NULL;
		return;
	}

	m_hLastBackpackItem->SetRenderColorA( a - player_backpack_store_fade.GetInt() );

	Vector vecTargetPos;
	GetBackpackPos( m_hLastBackpackItem, vecTargetPos );

	Vector vecToPlayer = (vecTargetPos - m_hLastBackpackItem->GetAbsOrigin());
	//vecToPlayer *= player_backpack_store_move.GetFloat();
	//m_hLastBackpackItem->SetAbsOrigin( vecToPlayer );

	float flDist = VectorNormalize( vecToPlayer );
	float flSpeed = Square(flDist / 16.0f);
	m_hLastBackpackItem->SetAbsVelocity( vecToPlayer * flSpeed * player_backpack_store_move.GetFloat() );

	// Try to right to default position
	QAngle vecAngVel;
	vecAngVel.x = AngleNormalize( -GetLocalAngles().x );
	vecAngVel.y = AngleNormalize( -GetLocalAngles().y );
	vecAngVel.z = AngleNormalize( -GetLocalAngles().z );
	m_hLastBackpackItem->SetLocalAngularVelocity( vecAngVel * player_backpack_store_move_angle.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputAddBackpackItem( inputdata_t &inputdata )
{
	char szParam[128];
	V_strncpy( szParam, inputdata.value.String(), sizeof( szParam ) );

	int nIdx = -1;

	char *pszSpace = V_strstr( szParam, " " );
	if ( pszSpace )
	{
		// Index specified
		nIdx = atoi( pszSpace + 1 );
		*pszSpace = '\0';
	}

	CBaseEntity *pItem = gEntList.FindEntityByName( NULL, szParam, this, inputdata.pActivator, inputdata.pCaller );
	if ( !pItem )
		return;

	StoreItemInBackpack( pItem, nIdx );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputAddBackpackItemInstant( inputdata_t &inputdata )
{
	char szParam[128];
	V_strncpy( szParam, inputdata.value.String(), sizeof( szParam ) );

	int nIdx = -1;

	char *pszSpace = V_strstr( szParam, " " );
	if ( pszSpace )
	{
		// Index specified
		nIdx = atoi( pszSpace + 1 );
		*pszSpace = '\0';
	}

	CBaseEntity *pItem = gEntList.FindEntityByName( NULL, szParam, this, inputdata.pActivator, inputdata.pCaller );
	if ( !pItem )
		return;

	StoreItemInBackpack( pItem, nIdx, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputRemoveBackpackItem( inputdata_t &inputdata )
{
	int nIdx = -1;
	CBaseEntity *pItem = gEntList.FindEntityByName( NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
	if ( !pItem )
	{
		// See if it's a direct index
		if ( AppearsToBeANumber( inputdata.value.String() ) )
		{
			nIdx = atoi( inputdata.value.String() );
		}
	}
	else
	{
		// Find index of this item
		for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
		{
			if ( m_hBackpackItems[i] == pItem )
			{
				nIdx = i;
				break;
			}
		}
	}

	if ( nIdx < 0 || nIdx >= MAX_BACKPACK_ITEMS )
		return;

	RemoveItemFromBackpack( nIdx, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputDeleteBackpackItem( inputdata_t &inputdata )
{
	int nIdx = -1;
	CBaseEntity *pItem = gEntList.FindEntityByName( NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller );
	if ( !pItem )
	{
		// See if it's a direct index
		if ( AppearsToBeANumber( inputdata.value.String() ) )
		{
			nIdx = atoi( inputdata.value.String() );
		}
	}
	else
	{
		// Find index of this item
		for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
		{
			if ( m_hBackpackItems[i] == pItem )
			{
				nIdx = i;
				break;
			}
		}
	}

	if ( nIdx < 0 || nIdx >= MAX_BACKPACK_ITEMS )
		return;

	RemoveItemFromBackpack( nIdx, false, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEZ2_Player::InputClearBackpack( inputdata_t &inputdata )
{
	for ( int i = 0; i < sk_backpack_max.GetInt(); i++ )
	{
		if ( m_hBackpackItems[i] )
			RemoveItemFromBackpack( i, false, true );
	}
}

extern CBaseEntity *GetPlayerHeldEntity( CBasePlayer *pPlayer );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( player_backpack_select, "Selects specified backpack index, if valid" )
{
	if ( args.ArgC() < 2 )
		return;

	CEZ2_Player *pPlayer = (CEZ2_Player*)UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	int nItem = atoi( args[1] );
	if ( nItem < 0 || nItem >= MAX_BACKPACK_ITEMS )
		return;

	CBaseEntity *pHeldEnt = GetPlayerHeldEntity( pPlayer );
	if ( pHeldEnt )
	{
		// If this space is vacant, then try to occupy it
		if ( !pPlayer->GetBackpackItem( nItem ) )
		{
			if ( pPlayer->CanStoreItemInBackpack( pHeldEnt ) )
			{
				pPlayer->StoreItemInBackpack( pHeldEnt, nItem );
				pPlayer->EmitSound( "EZ2Player.BackpackPickup" );
			}
			else
				pPlayer->EmitSound( "EZ2Player.BackpackDeny" );
		}
		else
		{
			pPlayer->EmitSound( "EZ2Player.BackpackDeny" );
			pPlayer->BackpackDenyEffect( nItem );
		}
	}
	else
	{
		if ( !pPlayer->RemoveItemFromBackpack( nItem ) )
		{
			pPlayer->EmitSound( "EZ2Player.BackpackDeny" );
			pPlayer->BackpackDenyEffect( nItem );
		}
	}
}
