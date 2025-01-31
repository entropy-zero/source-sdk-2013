//=============================================================================//
//
// Purpose:		Strider husks.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_husk_strider.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Husks generally use 90% of the original NPC's health
ConVar	sk_husk_strider_health( "sk_husk_strider_health", "315" ); // From 350

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_husk_strider, CNPC_HuskStrider );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_HuskStrider )

	DEFINE_BASE_HUSK_DATADESC()

END_DATADESC()

//---------------------------------------------------------
// Custom Client entity
//---------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CNPC_HuskStrider, DT_NPC_HuskStrider )

	DEFINE_BASE_HUSK_SENDPROPS()

END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_HuskStrider::CNPC_HuskStrider()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_HuskStrider::Spawn( void )
{
	BaseClass::Spawn();

	m_flFieldOfView = 0.5f;

	SetHealth( sk_husk_strider_health.GetInt() );
	SetMaxHealth( GetHealth() );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_HuskStrider::Precache()
{
	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( MAKE_STRING( "models/husks/husk_strider.mdl" ) );
	}

	PrecacheScriptSound( "NPC_HuskStrider.Suspicious" );
	PrecacheScriptSound( "NPC_HuskStrider.Startled" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

//AI_BEGIN_CUSTOM_NPC( npc_husk_strider, CNPC_HuskStrider )

//AI_END_CUSTOM_NPC()
