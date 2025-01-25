//=============================================================================//
//
// Purpose:		Combine husks
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_npc_base_husk.h"
#include "particle_parse.h"
#include "functionproxy.h"
#include "toolframework_client.h"
#include "ez2/npc_husk_base_shared.h"
#include "c_npc_crabsynth.h"

//ConVar cl_husk_eye_health_flicker( "cl_husk_eye_health_flicker", "1" );
//ConVar cl_husk_eye_show_aggression( "cl_husk_eye_show_aggression", "0" );

//-----------------------------------------------------------------------------

HUSK_CLIENT_STUB( Soldier, C_AI_BaseNPC, npc_husk_soldier )
HUSK_CLIENT_STUB( Police, C_AI_BaseNPC, npc_husk_police )
//HUSK_CLIENT_STUB( Strider, C_AI_BaseNPC, npc_husk_strider ) // See c_strider.cpp
HUSK_CLIENT_STUB( CrabSynth, C_NPC_CrabSynth, npc_husk_crabsynth )
HUSK_CLIENT_STUB( MortarSynth, C_AI_BaseNPC, npc_husk_mortarsynth )
//HUSK_CLIENT_STUB( ClawScanner, C_AI_BaseNPC, npc_husk_clawscanner )

//-----------------------------------------------------------------------------
// Controls the husk's helmet
//-----------------------------------------------------------------------------
class CProxyHuskHead : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CProxyHuskHead::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CProxyHuskHead::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		if ( pEntity->IsNPC() )
		{
			// TODO: Something more efficient?
			C_AI_HuskSink *pHusk = dynamic_cast<C_AI_HuskSink *>( pEntity );
			if ( pHusk && (pHusk->GetHuskCognitionFlags() & bits_HUSK_COGNITION_BLIND))
			{
				// Blind husks have no visible eyes
				SetFloatResult( -1.0f );
			}
			else
			{
				//if (pEntity->GetHealth() < pEntity->GetMaxHealth() && cl_husk_eye_health_flicker.GetBool())
				//{
				//	SetFloatResult( RandomFloat( ((float)pEntity->GetHealth()) / ((float)pEntity->GetMaxHealth()), 1.0f ) );
				//}
				//else
				{
					SetFloatResult( 1.0f );
				}
			}
		}
		else
		{
			SetFloatResult( -1.0f );
		}
	}
	else
	{
		SetFloatResult( -1.0f );
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CProxyHuskHead, IMaterialProxy, "HuskHead" IMATERIAL_PROXY_INTERFACE_VERSION );
