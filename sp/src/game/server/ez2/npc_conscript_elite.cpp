//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_conscript_elite.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if 0 // TODO
//-----------------------------------------------------------------------------

ConVar sk_conscript_elite_health( "sk_conscript_elite_health", "95" );

//-----------------------------------------------------------------------------

static const char *g_ppszConscriptRandomHeads[] =
{
	"male_01.mdl",
	"male_02.mdl",
	"female_01.mdl",
	"male_03.mdl",
	"female_02.mdl",
	"male_04.mdl",
	"female_03.mdl",
	"male_05.mdl",
	"female_04.mdl",
	"male_06.mdl",
	"female_06.mdl",
	"male_07.mdl",
	"female_07.mdl",
	"male_08.mdl",
	"male_09.mdl",
};

static const char *g_ppszConscriptModelLocs[] =
{
	"group_conscripts",
	"group_conscripts",
	"group_conscripts_reb",
};

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_Conscript )
	DEFINE_KEYFIELD( m_Subtype, FIELD_INTEGER, "citizensubtype" ),

	DEFINE_FIELD( m_bFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bShouldPoint, FIELD_BOOLEAN ),

	DEFINE_CONSCRIPT_DATADESC()
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_conscript, CNPC_Conscript );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Spawn()
{
	BaseClass::Spawn();

	m_iHealth = sk_conscript_elite_health.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Precache()
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::PrecacheAllOfType( CitizenType_t type )
{
	if (type == CT_UNIQUE)
		return;

	int nHeads = ARRAYSIZE( g_ppszConscriptRandomHeads );
	int i;
	for ( i = 0; i < nHeads; ++i )
	{
		PrecacheModel( CFmtStr( "models/Humans/%s/%s", (const char *)(CFmtStr(g_ppszConscriptModelLocs[type], "")), g_ppszConscriptRandomHeads[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::SelectModel()
{
	// If making reslists, precache everything!!!
	static bool madereslists = false;

	if ( CommandLine()->CheckParm("-makereslists") && !madereslists )
	{
		madereslists = true;

		PrecacheAllOfType( CT_CONSCRIPT_C17 );
		PrecacheAllOfType( CT_CONSCRIPT_C17_REBEL );
	}

	const char *pszModelName = NULL;

	if ( GetCitizenType() == CT_DEFAULT )
	{
		// TODO: Consider adding gamerules default?
		/*if (HL2GameRules()->GetDefaultCitizenType() != CT_DEFAULT)
		{
			SetCitizenType( static_cast<CitizenType_t>(HL2GameRules()->GetDefaultCitizenType()) );
		}
		else*/
		{
			// Simple determination based on alignment for now
			CitizenType_t nType = CT_CONSCRIPT_DEFAULT;

			if ( IsCombineAligned() )
			{
				nType = CT_CONSCRIPT_C17;
			}
			else
			{
				nType = CT_CONSCRIPT_C17_REBEL;
			}

			SetCitizenType( nType );
		}
	}

	// We need to use the base for tracking which heads were used, the VScript hook, etc.
	// This is safe now that we've selected a default model
	BaseClass::SelectModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Citizen::SetCitizenModel( const char *pszHeadName )
{
	SetModelName( AllocPooledString( CFmtStr( "models/Humans/%s/%s", g_ppszConscriptModelLocs[ GetCitizenType() ], pszHeadName ) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Conscript::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( !BaseClass::OnTakeDamage_Alive( info ) )
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_conscript, CNPC_Conscript )

DECLARE_CONDITION( COND_FLORA_EXTEND )
DECLARE_CONDITION( COND_FLORA_RETRACT )

AI_END_CUSTOM_NPC()
#endif
