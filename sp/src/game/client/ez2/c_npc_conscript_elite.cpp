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
#include "glow_overlay.h"
#include "c_ez2_player.h"
#include "clienteffectprecachesystem.h"

class C_EliteLaserGlowOverlay : public CGlowOverlay
{
public:
	bool ExcludeFromColorCorrection()
	{
		return true;
	}

	void CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor )
	{
		// Ignore the player's angle
		CGlowOverlay::CalcSpriteColorAndSize( 1.0f, pSprite, flHorzSize, flVertSize, vColor );
	}
};

class C_NPC_ConscriptElite : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_ConscriptElite, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_ConscriptElite();
	~C_NPC_ConscriptElite();

	void		OnDataChanged( DataUpdateType_t type );

	void		Simulate();

	EHANDLE		m_hGunLaser;
	int			m_nGunLaserExcludeDef;
	Vector		m_vecGunLaserDir;

	int			m_nGunLaserAttachment;

	C_EliteLaserGlowOverlay	m_LaserGlow;
};

LINK_ENTITY_TO_CLASS( npc_conscript_elite, C_NPC_ConscriptElite );

IMPLEMENT_CLIENTCLASS_DT( C_NPC_ConscriptElite, DT_NPC_ConscriptElite, CNPC_ConscriptElite )
	RecvPropEHandle( RECVINFO( m_hGunLaser ) ),
	RecvPropVector( RECVINFO( m_vecGunLaserDir ) ),
END_RECV_TABLE()

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectConscriptElite )
	CLIENTEFFECT_MATERIAL( "sprites/animglow01_animproxy" )
CLIENTEFFECT_REGISTER_END()

C_NPC_ConscriptElite::C_NPC_ConscriptElite()
{
	m_nGunLaserExcludeDef = -1;

	m_LaserGlow.m_bDirectional = false;
	m_LaserGlow.m_bInSky = false;
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

	if ( GetActiveWeapon() )
	{
		m_nGunLaserAttachment = GetActiveWeapon()->LookupAttachment( "laser" );
		if ( m_nGunLaserAttachment <= 0 )
		{
			m_nGunLaserAttachment = GetActiveWeapon()->LookupAttachment( "muzzle" );
		}
	}

	if ( type == DATA_UPDATE_CREATED )
	{
		// Setup our light glow.
		m_LaserGlow.m_nSprites = 1;
		m_LaserGlow.m_flProxyRadius = 2.0f;
		m_LaserGlow.m_Sprites[0].m_pMaterial = materials->FindMaterial( "sprites/animglow01_animproxy", TEXTURE_GROUP_CLIENT_EFFECTS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_NPC_ConscriptElite::Simulate()
{
	BaseClass::Simulate();

	C_EZ2_Player *pEZ2Player = ToEZ2Player( C_BasePlayer::GetLocalPlayer() );
	if ( pEZ2Player && m_hGunLaser && !m_hGunLaser->IsDormant() )
	{
		Vector vecLaserOrigin;
		if ( GetActiveWeapon() )
		{
			GetActiveWeapon()->GetAttachment( m_nGunLaserAttachment, vecLaserOrigin );
		}

		Vector vecLaserToEye = pEZ2Player->EyePosition() - vecLaserOrigin;
		float flDot = m_vecGunLaserDir.Dot( vecLaserToEye.Normalized() );

		const float LASER_MIN_FLARE_ANGLE = 0.98f; // 10 degrees

		if ( flDot > LASER_MIN_FLARE_ANGLE )
		{
			const float LASER_SIZE = 0.035f;
			const float LASER_NOISE_MIN_DIST_SQR = 0.0f;
			const float LASER_NOISE_MAX_DIST_SQR = Square( 2500.0f );

			m_LaserGlow.Activate();
			m_LaserGlow.m_vPos = vecLaserOrigin;
			m_LaserGlow.m_Sprites[0].m_vColor.x = RemapValClamped( flDot, 1.0f, LASER_MIN_FLARE_ANGLE, 0.8f, 0.0f );
			m_LaserGlow.m_Sprites[0].m_vColor.y = m_LaserGlow.m_Sprites[0].m_vColor.z = (m_LaserGlow.m_Sprites[0].m_vColor.x * 0.25f);

			float flSize = RemapValClamped( flDot, 1.0f, LASER_MIN_FLARE_ANGLE, LASER_SIZE, 0.0f );

			// Add noise based on distance
			float flNoise = RemapValClamped( vecLaserToEye.LengthSqr(), LASER_NOISE_MIN_DIST_SQR, LASER_NOISE_MAX_DIST_SQR, 0.25f, 0.75f );
			flSize *= RandomFloat( 1.0f - flNoise, 1.0f + flNoise );

			m_LaserGlow.m_Sprites[0].m_flVertSize = m_LaserGlow.m_Sprites[0].m_flHorzSize = flSize;
		}
		else
			m_LaserGlow.Deactivate();
	}
	else
		m_LaserGlow.Deactivate();
}
