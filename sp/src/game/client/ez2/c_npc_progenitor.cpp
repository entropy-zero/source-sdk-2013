//=============================================================================//
//
// Purpose:		The first combat template. The first PCU. The father of every Combine soldier.
//				The ultimate "Adrian Shephard at home."
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "c_ai_weaponlaser.h"

class C_NPC_Progenitor : public C_AI_WeaponLaserUser<C_AI_BaseNPC>
{
public:
	DECLARE_CLASS( C_NPC_Progenitor, C_AI_WeaponLaserUser<C_AI_BaseNPC> );
	DECLARE_CLIENTCLASS();

	C_BaseAnimating *GetActiveLaserWeapon() { return m_hLeftHandGun ? m_hLeftHandGun->GetBaseAnimating() : GetActiveWeapon(); }

	EHANDLE	m_hLeftHandGun;
};

LINK_ENTITY_TO_CLASS( npc_progenitor, C_NPC_Progenitor );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_Progenitor, DT_NPC_Progenitor, CNPC_Progenitor )
	RecvPropEHandle( RECVINFO( m_hGunLaser ) ),
	RecvPropEHandle( RECVINFO( m_hLeftHandGun ) ),
	RecvPropVector( RECVINFO( m_vecGunLaserDir ) ),
END_RECV_TABLE()
