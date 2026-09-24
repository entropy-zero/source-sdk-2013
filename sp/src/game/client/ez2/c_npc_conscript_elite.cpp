//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "c_ai_weaponlaser.h"

class C_NPC_ConscriptElite : public C_AI_WeaponLaserUser<C_AI_BaseNPC>
{
public:
	DECLARE_CLASS( C_NPC_ConscriptElite, C_AI_WeaponLaserUser<C_AI_BaseNPC> );
	DECLARE_CLIENTCLASS();
};

LINK_ENTITY_TO_CLASS( npc_conscript_elite, C_NPC_ConscriptElite );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_ConscriptElite, DT_NPC_ConscriptElite, CNPC_ConscriptElite )
	RecvPropEHandle( RECVINFO( m_hGunLaser ) ),
	RecvPropVector( RECVINFO( m_vecGunLaserDir ) ),
END_RECV_TABLE()
