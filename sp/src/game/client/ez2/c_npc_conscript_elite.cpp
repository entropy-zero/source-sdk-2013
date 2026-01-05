//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "colorcorrectionmgr.h"

class C_NPC_ConscriptElite : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_ConscriptElite, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_ConscriptElite();
	~C_NPC_ConscriptElite();

	void		OnDataChanged( DataUpdateType_t type );

	EHANDLE		m_hGunLaser;
	int			m_nGunLaserExcludeDef;
};

LINK_ENTITY_TO_CLASS( npc_conscript_elite, C_NPC_ConscriptElite );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_ConscriptElite, DT_NPC_ConscriptElite, CNPC_ConscriptElite )
	RecvPropEHandle( RECVINFO( m_hGunLaser ) ),
END_RECV_TABLE()

C_NPC_ConscriptElite::C_NPC_ConscriptElite()
{
	m_nGunLaserExcludeDef = -1;
}

C_NPC_ConscriptElite::~C_NPC_ConscriptElite()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_NPC_ConscriptElite::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	// Gun laser is excluded from color correction
	if ( m_nGunLaserExcludeDef == -1 )
	{
		if ( m_hGunLaser )
		{
			m_nGunLaserExcludeDef = g_pColorCorrectionMgr->RegisterExclusionObject( m_hGunLaser );
		}
	}
	else if ( !m_hGunLaser )
	{
		g_pColorCorrectionMgr->UnregisterExclusionObject( m_nGunLaserExcludeDef );
		m_nGunLaserExcludeDef = -1;
	}
}
