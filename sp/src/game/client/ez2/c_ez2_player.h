#ifndef C_EZ2_PLAYER_H
#define C_EZ2_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlplayer.h"
#include "colorcorrectionmgr.h"
#include "ez2/ai_stealth_shared.h"

class C_PointDetonatable : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_PointDetonatable, C_BaseEntity );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_PointDetonatable();
	~C_PointDetonatable();

	bool		ShouldDraw() { return false; }

	bool		m_bDisabled;
	EHANDLE		m_hThrower;
	EHANDLE		m_hGlowTarget;
};

class C_EZ2_Player : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_EZ2_Player , C_BaseHLPlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_EZ2_Player();
	~C_EZ2_Player();

	void Precache();

	void	OnDataChanged( DataUpdateType_t updateType );
	void	PostDataUpdate( DataUpdateType_t updateType );
	int		DrawModel( int flags );
	void	SetCCWeights();

	bool	IsNVGActive();			// Checks if flashlight is enabled *and* we're using NVG type
	int		GetFlashlightType();

	void BonusChallengeUpdate( );

	void UpdateGlowTargetEffect( void );
	void DestroyGlowTargetEffect( void );

	void UpdateSLAMGlowEffect( void );
	void DestroySLAMGlowEffect( void );

	float	GetPlayerMaxSpeed();

	void	UpdateEnemyGlowEffect( void );
	void	DestroyEnemyGlowEffect( void );
	void	EnemyMarkUpdate( C_BaseEntity *pEnemy, float flLastTimeSeen, int r, int g, int b, int a );

	bool m_bBonusChallengeUpdate;
	
	EHANDLE m_hWarningTarget;
	CGlowObject *m_pGlowTargetEffect;

	CUtlVector<EHANDLE>	m_hActiveSatchels;
	CUtlVector<EHANDLE>	m_hActiveTripmines;
	CUtlVector< CHandle<C_PointDetonatable> >	m_hActiveDetonatables;
	CUtlVector<CGlowObject*>	m_pSLAMGlowEffects;

	CUtlVector<EnemyMarkData_t>	m_MarkedEnemies;
	CUtlVector<CGlowObject*>	m_pEnemyGlowEffects;
	float						m_flNextMarkSightTime;

	inline float	GetCloakFactor() const { return m_flCloakFactor; }
	inline float	GetCloakCompromiseTime() const { return m_flCloakCompromiseTime; }
	inline float	GetCloakVisibleTime() const { return m_flCloakVisibleTime; }
	inline int		GetCloakCompromiseType() const { return m_nLastCloakCompromiseType; }
	inline float	GetCloakTransitionStartTime() const { return m_flCloakTransitionStartTime; }
	inline float	GetCloakPower() const { return m_HL2Local.m_flSuitPower * 0.01f; } // Ratio of suit power (which maxes out at 100)

	ClientCCHandle_t m_NVGCCHandle;

private:

	bool	m_bUseNVG;

	// Assassin
	bool	m_bIsAssassin;

	// Cloaking
	bool	m_bIsCloaking;
	float	m_flCloakFactor;
	float	m_flNextCloakThinkTime;
	float	m_flCloakCompromiseTime;
	float	m_flCloakVisibleTime;
	int		m_nLastCloakCompromiseType;
	float	m_flCloakTransitionStartTime;

	ClientCCHandle_t m_CloakCCHandle;
	int		m_nWarningSoundsUsed;
};


inline C_EZ2_Player* ToEZ2Player( CBaseEntity *pPlayer )
{
	Assert( dynamic_cast<C_EZ2_Player*>( pPlayer ) != NULL );
	return static_cast<C_EZ2_Player*>( pPlayer );
}


#endif // C_SDK_PLAYER_H