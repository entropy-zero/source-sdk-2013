#include "cbase.h"
#include "c_ez2_player.h"
#include "point_bonusmaps_accessor.h"
#include "achievementmgr.h"
#include "basegrenade_shared.h"
#include "clienteffectprecachesystem.h"
#include "viewrender.h"
#include "flashlighteffect.h"

#if defined( CEZ2Player )
	#undef CEZ2Player
#endif

ConVar cl_slam_glow( "cl_slam_glow", "0", FCVAR_ARCHIVE );

// Defined in hl2_player.cpp
// Since these are replicated, they are synchronized with the server
ConVar sv_flashlight_cc_enabled( "sv_flashlight_cc_enabled", "1", FCVAR_REPLICATED );
ConVar sv_flashlight_cc_maxweight( "sv_flashlight_cc_maxweight", "1", FCVAR_REPLICATED );

static void OnNVGCCFileChange( IConVar *var, const char *pOldValue, float flOldValue )
{
	for ( int i = 1; i < gpGlobals->maxClients; i++ )
	{
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		// Reset CC handle
		C_EZ2_Player *pEZ2Player = (C_EZ2_Player *)pPlayer;
		if ( pEZ2Player->m_NVGCCHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->RemoveColorCorrection( pEZ2Player->m_NVGCCHandle );
			pEZ2Player->m_NVGCCHandle = INVALID_CLIENT_CCHANDLE;
		}
	}
}

ConVar sv_flashlight_cc_filename( "sv_flashlight_cc_filename", "ez2_nvg.raw", FCVAR_REPLICATED, "", OnNVGCCFileChange );

#define CLOAK_COLORCORRECTION_FILE	"colorcorrection/progenitor_cloak.raw"

#define NUM_WARNING_SOUNDS 13

static float g_flWarningSoundThresholds[NUM_WARNING_SOUNDS] = { 25.0, 18.75, 14.0625, 10.5469, 7.9102, 5.9326, 4.4495, 3.3371, 2.5028, 1.8771, 1.4078, 1.0559, 0.7919 }; // *.75

BEGIN_RECV_TABLE_NOBASE( C_EZ2_Player, DT_EZ2LocalPlayerCloakData )
	RecvPropBool( RECVINFO( m_bIsCloaking ) ),
	RecvPropFloat( RECVINFO( m_flNextCloakThinkTime ) ),
	RecvPropFloat( RECVINFO( m_flCloakCompromiseTime ) ),
	RecvPropFloat( RECVINFO( m_flCloakVisibleTime ) ),
	RecvPropInt( RECVINFO( m_nLastCloakCompromiseType ) ),
	RecvPropFloat( RECVINFO( m_flCloakTransitionStartTime ) ),
END_RECV_TABLE();

IMPLEMENT_CLIENTCLASS_DT( C_EZ2_Player, DT_EZ2_Player, CEZ2_Player )
	RecvPropBool( RECVINFO( m_bUseNVG ) ),
	RecvPropBool( RECVINFO( m_bBonusChallengeUpdate ) ),
	RecvPropEHandle( RECVINFO( m_hWarningTarget ) ),
	RecvPropFloat( RECVINFO( m_flCloakFactor ) ),
	RecvPropDataTable( "ez2p_localcloak", 0, 0, &REFERENCE_RECV_TABLE( DT_EZ2LocalPlayerCloakData ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_EZ2_Player )
END_PREDICTION_DATA()

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEZ2Effects )
	CLIENTEFFECT_MATERIAL( "Ezero/Mask_NvMap" )
	CLIENTEFFECT_MATERIAL( "hud/stealth_cloak_overlay" )
CLIENTEFFECT_REGISTER_END()

C_EZ2_Player::C_EZ2_Player()
{
	m_NVGCCHandle = INVALID_CLIENT_CCHANDLE;
	m_CloakCCHandle = INVALID_CLIENT_CCHANDLE;
}

C_EZ2_Player::~C_EZ2_Player()
{
	DestroyGlowTargetEffect();
	DestroySLAMGlowEffect();

	g_pColorCorrectionMgr->RemoveColorCorrection( m_NVGCCHandle );
	g_pColorCorrectionMgr->RemoveColorCorrection( m_CloakCCHandle );
}

void C_EZ2_Player::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "EZ2Player.AlertTarget_Begin" );
	PrecacheScriptSound( "EZ2Player.AlertTarget_Spot" );

	PrecacheScriptSound( "AssassinPlayer.CloakWarningBlip" );
}

void C_EZ2_Player::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_DATATABLE_CHANGED )
	{
		if ( GetEffects() & EF_DIMLIGHT && m_NVGCCHandle == INVALID_CLIENT_CCHANDLE && IsLocalPlayer() )
		{
			m_NVGCCHandle = g_pColorCorrectionMgr->AddColorCorrection( "cc_nvg", sv_flashlight_cc_filename.GetString() );
		}

		BonusChallengeUpdate();

		if (cl_slam_glow.GetBool())
		{
			if ( m_HL2Local.m_iSatchelCount != m_hActiveSatchels.Count() || m_HL2Local.m_iTripmineCount != m_hActiveTripmines.Count() || m_HL2Local.m_iDetonatableCount != m_hActiveDetonatables.Count() )
			{
				// Recollect active satchels/tripmines
				m_hActiveSatchels.RemoveAll();
				m_hActiveTripmines.RemoveAll();
				m_hActiveDetonatables.RemoveAll();

				C_BaseEntity *pEntity = NULL;
				C_BaseGrenade *pGrenade = NULL;
				const CEntInfo *pInfo = ClientEntityList().FirstEntInfo();
				for ( ;pInfo; pInfo = pInfo->m_pNext )
				{
					pEntity = (C_BaseEntity *)pInfo->m_pEntity;
					if ( !pEntity )
						continue;

					if ( FStrEq( pEntity->GetClassname(), "npc_satchel" ) )
					{
						pGrenade = static_cast<C_BaseGrenade*>(pEntity);
						if (pGrenade->GetThrower() == this && !pGrenade->IsMarkedForDeletion())
							m_hActiveSatchels.AddToTail( pEntity );
					}
					else if ( FStrEq( pEntity->GetClassname(), "npc_tripmine" ) )
					{
						pGrenade = static_cast<C_BaseGrenade*>(pEntity);
						if (pGrenade->GetThrower() == this && !pGrenade->IsMarkedForDeletion())
							m_hActiveTripmines.AddToTail( pEntity );
					}
					else if ( FStrEq( pEntity->GetClassname(), "point_detonatable" ) )
					{
						C_PointDetonatable *pDetonatable = static_cast<C_PointDetonatable*>(pEntity);
						if (pDetonatable->m_hThrower.Get() == this && !pDetonatable->m_bDisabled && !pDetonatable->IsMarkedForDeletion())
							m_hActiveDetonatables.AddToTail( pDetonatable );
					}
				}
			}
		}

		if ( m_bIsCloaking && IsLocalPlayer() && m_CloakCCHandle == INVALID_CLIENT_CCHANDLE)
		{
			m_CloakCCHandle = g_pColorCorrectionMgr->AddColorCorrection( "_ez2p_cloak_cc", CLOAK_COLORCORRECTION_FILE );
		}
	}

	UpdateGlowTargetEffect();
	UpdateSLAMGlowEffect();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_EZ2_Player::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_flCloakFactor > 0.0f && IsLocalPlayer() )
	{
		// Warning sounds
		{
			if (m_nWarningSoundsUsed < NUM_WARNING_SOUNDS && m_HL2Local.m_flSuitPower <= g_flWarningSoundThresholds[m_nWarningSoundsUsed])
			{
				m_nWarningSoundsUsed++;
				EmitSound( "AssassinPlayer.CloakWarningBlip" );
			}
			else if (m_nWarningSoundsUsed > 0 && m_HL2Local.m_flSuitPower > g_flWarningSoundThresholds[m_nWarningSoundsUsed-1])
			{
				// Recharged since this warning sound
				m_nWarningSoundsUsed--;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int C_EZ2_Player::DrawModel( int flags )
{
	if ( m_flCloakFactor == 1.0f )
	{
		view_id_t iCurrentView = CurrentViewID();
		if ( InFirstPersonView() )
		{
			// Don't draw in non-perspective views
			switch ( iCurrentView )
			{
				case VIEW_MONITOR:
				case VIEW_REFRACTION:
				case VIEW_SHADOW_DEPTH_TEXTURE:
				case VIEW_SSAO:
					return 0;
			}
		}
		else
		{
			// Don't draw at all
			return 0;
		}
	}

	return BaseClass::DrawModel( flags );
}

void C_EZ2_Player::SetCCWeights()
{
	C_EZ2_Player *pEZ2Player = this;
	if ( GetObserverMode() == OBS_MODE_IN_EYE && GetObserverTarget() && GetObserverTarget()->IsPlayer() )
	{
		// Use the player we're spectating instead
		pEZ2Player = ToEZ2Player( GetObserverTarget() );
	}

	if ( pEZ2Player->IsNVGActive() && sv_flashlight_cc_enabled.GetBool() )
	{
		if ( pEZ2Player->m_NVGCCHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->SetColorCorrectionWeight( pEZ2Player->m_NVGCCHandle, sv_flashlight_cc_maxweight.GetFloat() );
		}
	}

	float flCloakFactor = pEZ2Player->GetCloakFactor();
	if ( GetCloakFactor() > 0.0f )
	{
		if ( m_CloakCCHandle != INVALID_CLIENT_CCHANDLE )
		{
			g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CloakCCHandle, flCloakFactor );
		}
	}
}

bool C_EZ2_Player::IsNVGActive()
{
	if ( !(GetEffects() & EF_DIMLIGHT) )
		return false;

	if ( !m_bUseNVG )
		return false;

	return true;
}

int C_EZ2_Player::GetFlashlightType()
{
	return m_bUseNVG ? NVG : FLASHLIGHT;
}

void C_EZ2_Player::BonusChallengeUpdate()
{
	if (m_bBonusChallengeUpdate)
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EZ2_Player::UpdateSLAMGlowEffect( void )
{
	// destroy the existing effects
	if ( m_pSLAMGlowEffects.Count() > 0 )
	{
		DestroySLAMGlowEffect();
	}

	if (!cl_slam_glow.GetBool())
		return;

	static Vector vecSatchelColor( 1.0f, 0.2f, 0.2f );
	static Vector vecTripmineColor( 0.5f, 0.75f, 1.0f );
	static Vector vecDetonatableColor( 1.0f, 0.75f, 0.125f );

	for (int i = 0; i < m_hActiveSatchels.Count(); i++)
	{
		int iNewGlow = m_pSLAMGlowEffects.AddToTail();
		m_pSLAMGlowEffects[iNewGlow] = new CGlowObject( m_hActiveSatchels[i], vecSatchelColor, 1.0f, true );
	}

	for (int i = 0; i < m_hActiveTripmines.Count(); i++)
	{
		int iNewGlow = m_pSLAMGlowEffects.AddToTail();
		m_pSLAMGlowEffects[iNewGlow] = new CGlowObject( m_hActiveTripmines[i], vecTripmineColor, 1.0f, true );
	}

	for (int i = 0; i < m_hActiveDetonatables.Count(); i++)
	{
		if (m_hActiveDetonatables[i] && m_hActiveDetonatables[i]->m_hGlowTarget)
		{
			int iNewGlow = m_pSLAMGlowEffects.AddToTail();
			m_pSLAMGlowEffects[iNewGlow] = new CGlowObject( m_hActiveDetonatables[i]->m_hGlowTarget, vecDetonatableColor, 1.0f, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EZ2_Player::DestroySLAMGlowEffect( void )
{
	m_pSLAMGlowEffects.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_PointDetonatable, DT_PointDetonatable, CPointDetonatable )
	RecvPropBool( RECVINFO( m_bDisabled ) ),
	RecvPropEHandle( RECVINFO( m_hThrower ) ),
	RecvPropEHandle( RECVINFO( m_hGlowTarget ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_PointDetonatable )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( point_detonatable, C_PointDetonatable );

C_PointDetonatable::C_PointDetonatable()
{
}

C_PointDetonatable::~C_PointDetonatable()
{
}
