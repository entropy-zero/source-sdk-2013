#ifndef C_EZ2_PLAYER_H
#define C_EZ2_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlplayer.h"

class C_EZ2_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_EZ2_Player , C_BaseHLPlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_EZ2_Player();
	~C_EZ2_Player();

	void OnDataChanged( DataUpdateType_t updateType );

	void UpdateGlowTargetEffect( void );
	void DestroyGlowTargetEffect( void );

	bool m_bBonusChallengeUpdate;
	
	EHANDLE m_hWarningTarget;
	CGlowObject *m_pGlowTargetEffect;
};


inline C_EZ2_Player* ToEZ2Player( CBaseEntity *pPlayer )
{
	Assert( dynamic_cast<C_EZ2_Player*>( pPlayer ) != NULL );
	return static_cast<C_EZ2_Player*>( pPlayer );
}


#endif // C_SDK_PLAYER_H