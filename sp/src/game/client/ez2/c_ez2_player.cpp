#include "cbase.h"
#include "c_EZ2_player.h"
#include "point_bonusmaps_accessor.h"
#include "achievementmgr.h"

#if defined( CEZ2Player )
	#undef CEZ2Player
#endif


IMPLEMENT_CLIENTCLASS_DT( C_EZ2_Player, DT_EZ2_Player, CEZ2_Player )
	RecvPropBool( RECVINFO( m_bBonusChallengeUpdate ) ),
	RecvPropEHandle( RECVINFO( m_hWarningTarget ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_EZ2_Player )
END_PREDICTION_DATA()

C_EZ2_Player::C_EZ2_Player()
{
}

C_EZ2_Player::~C_EZ2_Player()
{
	DestroyGlowTargetEffect();
}

void C_EZ2_Player::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	UpdateGlowTargetEffect();

	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED && m_bBonusChallengeUpdate )
	{
		// Borrow the achievement manager for this
		CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>(engine->GetAchievementMgr());
		if (pAchievementMgr)
		{
			if (pAchievementMgr->WereCheatsEverOn())
				return;
		}

		char szChallengeFileName[128];
		char szChallengeMapName[128];
		char szChallengeName[128];
		BonusMapChallengeNames( szChallengeFileName, szChallengeMapName, szChallengeName );
		BonusMapChallengeUpdate( szChallengeFileName, szChallengeMapName, szChallengeName, GetBonusProgress() );

		m_bBonusChallengeUpdate = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EZ2_Player::UpdateGlowTargetEffect( void )
{
	// destroy the existing effect
	if ( m_pGlowTargetEffect )
	{
		DestroyGlowTargetEffect();
	}

	if (!m_hWarningTarget)
	{
		return;
	}

	// create a new effect
	//if ( !m_bGlowDisabled )
	{
		Vector4D vecColor( 1.0f, 0, 0, 1.0f );
		m_pGlowTargetEffect = new CGlowObject( m_hWarningTarget, vecColor.AsVector3D(), vecColor.w, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EZ2_Player::DestroyGlowTargetEffect( void )
{
	if ( m_pGlowTargetEffect )
	{
		delete m_pGlowTargetEffect;
		m_pGlowTargetEffect = NULL;
	}
}
