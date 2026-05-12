//=============================================================================//
//
// Purpose:		Items and props that hang off of NPCs and can be grabbed by players.
//
// Author:		Blixibon
//
//=============================================================================//
#ifndef AI_BODY_ACCESSORY_H
#define AI_BODY_ACCESSORY_H

#include "ai_component.h"
#include "mapbase/ai_grenade.h"
#include "items.h"
#include "saverestore_utlvector.h"
#include "eventqueue.h"

//-----------------------------------------------------------------------------

#define	DEFINE_BODYACCESSORY_DATADESC() \
	DEFINE_UTLVECTOR( m_hBodyAccessories, FIELD_EHANDLE ),	\
	DEFINE_KEYFIELD( m_bAutoBodyAccessories, FIELD_BOOLEAN, "AutoBodyAccessories" ),	\
	DEFINE_INPUTFUNC( FIELD_STRING, "GiveBodyItem", InputGiveBodyItem ),	\
	DEFINE_INPUTFUNC( FIELD_STRING, "GiveBodyItemTarget", InputGiveBodyItemTarget ),	\
	DEFINE_INPUTFUNC( FIELD_STRING, "RemoveItemFromBody", InputRemoveItemFromBody ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "KillBodyItems", InputKillBodyItems ),	\
	DEFINE_INPUTFUNC( FIELD_VOID, "_EnableBodyItemCollisions", InputEnableBodyItemCollisions ),	\

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_BodyAccessoryUser : public BASE_NPC
{
	DECLARE_CLASS_NOFRIEND( CAI_BodyAccessoryUser, BASE_NPC );

public:
	CAI_BodyAccessoryUser()
	{
		this->m_bAutoBodyAccessories = false;
	}

	virtual void	AddAutoAccessories() {}

	void			Event_Killed( const CTakeDamageInfo &info );
	void			PickupItem( CBaseEntity *pItem );
	bool			WantsItemForSelf( CBaseEntity *pItem );
	void			HandleAnimEvent( animevent_t *pEvent );
	
	bool			UsesGrabbableAccessories() { return true; }
	int				GetNumGrabbableAccessories() { return m_hBodyAccessories.Count(); }
	int				GetMaxGrabbableAccessories();
	bool			CanGrabAccessory( CBaseEntity *pAccessory );
	CBaseEntity		*GetGrabbableAccessory( int nIdx );
	CBaseEntity		*GetGrabbableAccessoryFromGrab( CBasePlayer *pGrabActivator );
	void			RemoveGrabbableAccessory( CBaseEntity *pAccessory );

	const char		*GetBodyAccessoryName( CBaseEntity *pItem );
	int				GetBodyAccessoryAttachment( CBaseEntity *pItem, int nBodyIdx = -1 );
	void			AddItemToBody( const char *pszClassname, int nBodyIdx = -1 );
	void			AddItemToBody( CBaseEntity *pItem, int nBodyIdx = -1 );
	void			RemoveItemFromBody( int nBodyIdx );
	void			InputGiveBodyItem( inputdata_t &inputdata );
	void			InputGiveBodyItemTarget( inputdata_t &inputdata );
	void			InputRemoveItemFromBody( inputdata_t &inputdata );
	void			InputKillBodyItems( inputdata_t &inputdata );
	void			InputEnableBodyItemCollisions( inputdata_t &inputdata );

protected:

	CUtlVector<EHANDLE> m_hBodyAccessories;
	bool				m_bAutoBodyAccessories;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::Event_Killed( const CTakeDamageInfo &info )
{
	if ( m_hBodyAccessories.Count() > 0 )
	{
		FOR_EACH_VEC_BACK( m_hBodyAccessories, i )
		{
			if ( m_hBodyAccessories[i] )
			{
				RemoveItemFromBody( i );
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::PickupItem( CBaseEntity *pItem )
{
	// Is this not an item we need for ourselves?
	if ( !WantsItemForSelf( pItem ) )
	{
		// Then should we put this item on our body?
		if ( CanGrabAccessory( pItem ) )
		{
			AddItemToBody( pItem );
			return;
		}
	}

	BaseClass::PickupItem( pItem );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BodyAccessoryUser<BASE_NPC>::WantsItemForSelf( CBaseEntity *pItem )
{
	if ( pItem->IsCombatItem() )
	{
		if ( pItem->ClassMatches( "item_health*" ) )
		{
			// Use on ourselves if at least half of the item would be useful towards our health
			if ( ( this->GetMaxHealth() - this->GetHealth() ) > ( static_cast<CItem *>(pItem)->GetItemAmount() * 0.5f ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == COMBINE_AE_GREN_TOSS )
	{
		// Check if we have a grenade on our body. If we do, act as if we used it and remove it
		FOR_EACH_VEC( m_hBodyAccessories, i )
		{
			CBaseEntity *pItem = m_hBodyAccessories[i];
			if ( pItem && pItem->ClassMatches( "weapon_frag" ) )
			{
				UTIL_Remove( pItem );
				m_hBodyAccessories.Remove( i );
				break;
			}
		}
	}
	else if ( pEvent->event == COMBINE_AE_ALTFIRE )
	{
		// Check if we have a SMG grenade on our body. If we do, act as if we used it and remove it
		FOR_EACH_VEC( m_hBodyAccessories, i )
		{
			CBaseEntity *pItem = m_hBodyAccessories[i];
			if ( pItem && pItem->ClassMatches( "item_ammo_smg1_grenade" ) )
			{
				UTIL_Remove( pItem );
				m_hBodyAccessories.Remove( i );
				break;
			}
		}
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_BodyAccessoryUser<BASE_NPC>::GetMaxGrabbableAccessories()
{
	// How many attachment sets do we have?
	// Check the model directly
	CStudioHdr *pStudioHdr = this->GetModelPtr();
	int nHighestIdx = -1;
	if ( pStudioHdr && pStudioHdr->SequencesAvailable() )
	{
		for ( int i = 0; i < pStudioHdr->GetNumAttachments(); i++ )
		{
			if ( !V_strnicmp( "body_a", pStudioHdr->pAttachment( i ).pszName(), 6 ) )
			{
				int nIdx = atoi( pStudioHdr->pAttachment( i ).pszName() + 6 );
				if ( nIdx > nHighestIdx )
					nIdx = nHighestIdx;
			}
		}
	}

	return nHighestIdx + 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_BodyAccessoryUser<BASE_NPC>::CanGrabAccessory( CBaseEntity *pAccessory )
{
	if ( m_hBodyAccessories.Count() >= GetMaxGrabbableAccessories() )
		return false;

	// Don't allow if we don't have an available attachment for it
	// (the inputs ignore this)
	if ( GetBodyAccessoryAttachment( pAccessory ) <= 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
CBaseEntity *CAI_BodyAccessoryUser<BASE_NPC>::GetGrabbableAccessory( int nIdx )
{
	Assert( m_hBodyAccessories.IsValidIndex( nIdx ) );
	return m_hBodyAccessories[nIdx];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
CBaseEntity *CAI_BodyAccessoryUser<BASE_NPC>::GetGrabbableAccessoryFromGrab( CBasePlayer *pGrabActivator )
{
	int iBestItemIdx = -1;
	float flBestDot = DOT_10DEGREE;	// Maximum angle

	FOR_EACH_VEC( m_hBodyAccessories, i )
	{
		CBaseEntity *pItem = m_hBodyAccessories[i];
		if ( pItem )
		{
			// Make sure the player is actually on the correct side of the peacekeeper
			Vector2D vecNPCToItem2D = (pItem->WorldSpaceCenter().AsVector2D() - this->GetAbsOrigin().AsVector2D());
			Vector2DNormalize( vecNPCToItem2D );

			Vector2D vecNPCToPlayer2D = (pGrabActivator->GetAbsOrigin().AsVector2D() - this->GetAbsOrigin().AsVector2D());
			Vector2DNormalize( vecNPCToPlayer2D );

			if ( DotProduct2D( vecNPCToPlayer2D, vecNPCToItem2D ) < -DOT_45DEGREE )
				continue;

			// Grab the item closest to our cursor
			Vector vecPlayerToItem = (pItem->WorldSpaceCenter() - pGrabActivator->EyePosition());
			VectorNormalize( vecPlayerToItem );
			float flDot = DotProduct( pGrabActivator->EyeDirection3D(), vecPlayerToItem );
			if ( flDot > flBestDot )
			{
				iBestItemIdx = i;
				flBestDot = flDot;
			}
		}
	}

	if ( iBestItemIdx != -1 )
	{
		CBaseEntity *pItem = m_hBodyAccessories[iBestItemIdx];
		RemoveItemFromBody( iBestItemIdx );
		return pItem;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::RemoveGrabbableAccessory( CBaseEntity *pAccessory )
{
	EHANDLE hAccessory = pAccessory;
	int i = m_hBodyAccessories.Find( hAccessory );
	if ( i != m_hBodyAccessories.InvalidIndex() )
		RemoveItemFromBody( i );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::AddItemToBody( const char *pszClassname, int nBodyIdx )
{
	CBaseEntity *pItem = CBaseEntity::CreateNoSpawn( pszClassname, this->GetAbsOrigin(), vec3_angle, this );
	if ( pItem )
	{
		// Make the item match our variant
		pItem->SetEZVariant( this->GetEZVariant() );

		DispatchSpawn( pItem );
		AddItemToBody( pItem, nBodyIdx );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
const char *CAI_BodyAccessoryUser<BASE_NPC>::GetBodyAccessoryName( CBaseEntity *pItem )
{
	const char *pszClassname = pItem->GetClassname();

	if ( V_strncmp( pszClassname, "item_", 5 ) == 0 )
	{
		pszClassname += 5;
	}
	else if ( V_strncmp( pszClassname, "weapon_", 7 ) == 0 )
	{
		pszClassname += 7;
	}
	else if ( V_strncmp( pszClassname, "prop_", 5 ) == 0 )
	{
		// Try more explicit cases
		const char *pszHeadwearType = pItem->GetContextValue( "headwear" );
		if ( pszHeadwearType && *pszHeadwearType )
			return pszHeadwearType; // Either helmet or beret
		else if ( pItem->FindContextByName( "keycard" ) != -1 )
			return "keycard";

		// Otherwise, just the model name
		char szModelName[MAX_PATH];
		V_FileBase( STRING( pItem->GetModelName() ), szModelName, sizeof( szModelName ) );
		return STRING( AllocPooledString( szModelName ) );
	}

	return pszClassname;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
int CAI_BodyAccessoryUser<BASE_NPC>::GetBodyAccessoryAttachment( CBaseEntity *pItem, int nBodyIdx )
{
	const char *pszType = GetBodyAccessoryName( pItem );

	if ( nBodyIdx == -1 )
		nBodyIdx = m_hBodyAccessories.Count();

	const char *pszAttachName = UTIL_VarArgs( "body_a%i_%s", nBodyIdx, pszType );
	return this->LookupAttachment( pszAttachName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::AddItemToBody( CBaseEntity *pItem, int nBodyIdx )
{
	if ( nBodyIdx == -1 )
		nBodyIdx = m_hBodyAccessories.Count();

	// Which attachment should this item use?
	int iAttach = GetBodyAccessoryAttachment( pItem, nBodyIdx );
	if ( iAttach <= 0 )
	{
		DevMsg( "%s: %s has no available attachment for body (\"%s\" invalid), falling back to default\n", GetDebugName(), pItem->GetDebugName(), UTIL_VarArgs( "body_a%i_%s", nBodyIdx, GetBodyAccessoryName( pItem ) ) );

		iAttach = this->LookupAttachment( UTIL_VarArgs( "body_a%i_default", nBodyIdx ) );
	}

	pItem->SetAbsVelocity( vec3_origin );
	pItem->RemoveSolidFlags( FSOLID_TRIGGER );
	pItem->RemoveEffects( EF_ITEM_BLINK );
	pItem->VPhysicsDestroyObject();

	pItem->SetParent( this, iAttach );
	pItem->SetMoveType( MOVETYPE_NONE );
	pItem->AddSolidFlags( FSOLID_NOT_SOLID );
	pItem->SetLocalOrigin( vec3_origin );
	pItem->SetLocalAngles( vec3_angle );

	// Should no longer be visible to NPCs or tracked as a seen item
	if ( g_hStealthManager )
	{
		g_hStealthManager->RemoveSeenObject( pItem );
	}

	g_AI_SensedObjectsManager.RemoveEntity( pItem );

	m_hBodyAccessories.AddToTail( pItem );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::RemoveItemFromBody( int nBodyIdx )
{
	CBaseEntity *pItem = m_hBodyAccessories[nBodyIdx];

	pItem->SetParent( NULL );
	pItem->RemoveSolidFlags( FSOLID_NOT_SOLID );
	pItem->SetGravity( 1.0 );

	if ( pItem->IsCombatItem() || pItem->IsBaseCombatWeapon() )
	{
		pItem->SetMoveType( MOVETYPE_FLYGRAVITY );

		if ( pItem->ClassMatches( "weapon_frag" ) || pItem->ClassMatches( "weapon_slam" ) )
		{
			// Act as if we've lost all of our grenades
			m_iNumGrenades = 0;
		}

		// Same as in DropItem()
		this->m_OnItemDrop.Set( pItem, pItem, this );

		if ( g_hStealthManager && !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_LOUD ) )
		{
			// Allow NPCs to visually notice this item
			g_AI_SensedObjectsManager.AddEntity( pItem );
		}
	}
	else
	{
		pItem->SetMoveType( MOVETYPE_VPHYSICS );

		if ( g_hStealthManager && !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_LOUD ) )
		{
			g_hStealthManager->MakePropPerceivable( pItem );
		}
	}

	pItem->VPhysicsDestroyObject();
	pItem->Spawn();	// So that we re-initialize as a physical item

	// Need to make sure props aren't dropped immediately after if they're intersecting with the NPC
	if ( pItem->VPhysicsGetObject() )
	{
		PhysDisableObjectCollisions( pItem->VPhysicsGetObject(), this->VPhysicsGetObject() );

		variant_t var;
		g_EventQueue.AddEvent( this, "_EnableBodyItemCollisions", var, 1.0f, pItem, this );
	}

	m_hBodyAccessories.Remove( nBodyIdx );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::InputGiveBodyItem( inputdata_t &inputdata )
{
	char szClassname[64];
	V_strncpy( szClassname, inputdata.value.String(), sizeof( szClassname ) );

	int nBodyIdx = -1;

	const char *pszSpace = V_strnchr( szClassname, ' ', sizeof( szClassname ) );
	if ( pszSpace )
	{
		// Extract explicit body index
		nBodyIdx = atoi( pszSpace + 1 );
		*const_cast<char*>(pszSpace) = '\0';
	}

	AddItemToBody( szClassname, nBodyIdx );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::InputGiveBodyItemTarget( inputdata_t &inputdata )
{
	char szTargetName[64];
	V_strncpy( szTargetName, inputdata.value.String(), sizeof( szTargetName ) );

	int nBodyIdx = -1;

	const char *pszSpace = V_strnchr( szTargetName, ' ', sizeof( szTargetName ) );
	if ( pszSpace )
	{
		// Extract explicit body index
		nBodyIdx = atoi( pszSpace + 1 );
		*const_cast<char*>(pszSpace) = '\0';
	}

	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, szTargetName, this, inputdata.pActivator, inputdata.pCaller );
	if ( pEnt )
	{
		AddItemToBody( pEnt, nBodyIdx );
	}
	else
	{
		Warning( "%s GiveBodyItemTarget: Failed to find target \"%s\"\n", this->GetDebugName(), szTargetName );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::InputRemoveItemFromBody( inputdata_t &inputdata )
{
	FOR_EACH_VEC_BACK( m_hBodyAccessories, i )
	{
		CBaseEntity *pItem = m_hBodyAccessories[i];
		if ( pItem && ( pItem->ClassMatches( inputdata.value.String() ) || pItem->NameMatches( inputdata.value.String() ) ) )
		{
			RemoveItemFromBody( i );
			continue; // For wildcards, similarly named items, etc.
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::InputKillBodyItems( inputdata_t &inputdata )
{
	FOR_EACH_VEC_BACK( m_hBodyAccessories, i )
	{
		UTIL_Remove( m_hBodyAccessories[i] );
		m_hBodyAccessories.Remove( i );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_BodyAccessoryUser<BASE_NPC>::InputEnableBodyItemCollisions( inputdata_t &inputdata )
{
	if ( inputdata.pActivator && inputdata.pActivator->VPhysicsGetObject() && this->VPhysicsGetObject() )
	{
		PhysEnableObjectCollisions( inputdata.pActivator->VPhysicsGetObject(), this->VPhysicsGetObject() );
	}
}

#endif // AI_BODY_ACCESSORY_H