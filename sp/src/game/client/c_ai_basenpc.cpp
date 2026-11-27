//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_ai_basenpc.h"
#include "engine/ivdebugoverlay.h"

#if defined( HL2_DLL ) || defined( HL2_EPISODIC )
#include "c_basehlplayer.h"
#endif

#include "death_pose.h"

#ifdef EZ
#include "flashlighteffect.h"
#include "r_efx.h"
#include "dlight.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PING_MAX_TIME	2.0

IMPLEMENT_CLIENTCLASS_DT( C_AI_BaseNPC, DT_AI_BaseNPC, CAI_BaseNPC )
	RecvPropInt( RECVINFO( m_lifeState ) ),
	RecvPropBool( RECVINFO( m_bPerformAvoidance ) ),
	RecvPropBool( RECVINFO( m_bIsMoving ) ),
	RecvPropBool( RECVINFO( m_bFadeCorpse ) ),
	RecvPropInt( RECVINFO ( m_iDeathPose) ),
	RecvPropInt( RECVINFO( m_iDeathFrame) ),
	RecvPropInt( RECVINFO( m_iSpeedModRadius ) ),
	RecvPropInt( RECVINFO( m_iSpeedModSpeed ) ),
	RecvPropInt( RECVINFO( m_bSpeedModActive ) ),
	RecvPropBool( RECVINFO( m_bImportanRagdoll ) ),
	RecvPropFloat( RECVINFO( m_flTimePingEffect ) ),
END_RECV_TABLE()

extern ConVar cl_npc_speedmod_intime;

bool NPC_IsImportantNPC( C_BaseAnimating *pAnimating )
{
	C_AI_BaseNPC *pBaseNPC = dynamic_cast < C_AI_BaseNPC* > ( pAnimating );

	if ( pBaseNPC == NULL )
		return false;

	return pBaseNPC->ImportantRagdoll();
}

C_AI_BaseNPC::C_AI_BaseNPC()
{
}

//-----------------------------------------------------------------------------
// Makes ragdolls ignore npcclip brushes
//-----------------------------------------------------------------------------
unsigned int C_AI_BaseNPC::PhysicsSolidMaskForEntity( void ) const 
{
	// This allows ragdolls to move through npcclip brushes
	if ( !IsRagdoll() )
	{
		return MASK_NPCSOLID; 
	}
	return MASK_SOLID;
}


void C_AI_BaseNPC::ClientThink( void )
{
	BaseClass::ClientThink();

#ifdef HL2_DLL
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );

	if ( ShouldModifyPlayerSpeed() == true )
	{
		if ( pPlayer )
		{
			float flDist = (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr();

			if ( flDist <= GetSpeedModifyRadius() )
			{
				if ( pPlayer->m_hClosestNPC )
				{
					if ( pPlayer->m_hClosestNPC != this )
					{
						float flDistOther = (pPlayer->m_hClosestNPC->GetAbsOrigin() - pPlayer->GetAbsOrigin()).Length();

						//If I'm closer than the other NPC then replace it with myself.
						if ( flDist < flDistOther )
						{
							pPlayer->m_hClosestNPC = this;
							pPlayer->m_flSpeedModTime = gpGlobals->curtime + cl_npc_speedmod_intime.GetFloat();
						}
					}
				}
				else
				{
					pPlayer->m_hClosestNPC = this;
					pPlayer->m_flSpeedModTime = gpGlobals->curtime + cl_npc_speedmod_intime.GetFloat();
				}
			}
		}
	}
#endif // HL2_DLL

#ifdef HL2_EPISODIC
	C_BaseHLPlayer *pPlayer = dynamic_cast<C_BaseHLPlayer*>( C_BasePlayer::GetLocalPlayer() );

	if ( pPlayer && m_flTimePingEffect > gpGlobals->curtime )
	{
		float fPingEffectTime = m_flTimePingEffect - gpGlobals->curtime;
		
		if ( fPingEffectTime > 0.0f )
		{
			Vector vRight, vUp;
			Vector vMins, vMaxs;

			float fFade;

			if( fPingEffectTime <= 1.0f )
			{
				fFade = 1.0f - (1.0f - fPingEffectTime);
			}
			else
			{
				fFade = 1.0f;
			}

			GetRenderBounds( vMins, vMaxs );
			AngleVectors (pPlayer->GetAbsAngles(), NULL, &vRight, &vUp );
			Vector p1 = GetAbsOrigin() + vRight * vMins.x + vUp * vMins.z;
			Vector p2 = GetAbsOrigin() + vRight * vMaxs.x + vUp * vMins.z;
			Vector p3 = GetAbsOrigin() + vUp * vMaxs.z;

			int r = 0 * fFade;
			int g = 255 * fFade;
			int b = 0 * fFade;

			debugoverlay->AddLineOverlay( p1, p2, r, g, b, true, 0.05f );
			debugoverlay->AddLineOverlay( p2, p3, r, g, b, true, 0.05f );
			debugoverlay->AddLineOverlay( p3, p1, r, g, b, true, 0.05f );
		}
	}
#endif
}

void C_AI_BaseNPC::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( ( ShouldModifyPlayerSpeed() == true ) || ( m_flTimePingEffect > gpGlobals->curtime ) )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void C_AI_BaseNPC::GetRagdollInitBoneArrays( matrix3x4_t *pDeltaBones0, matrix3x4_t *pDeltaBones1, matrix3x4_t *pCurrentBones, float boneDt )
{
	ForceSetupBonesAtTime( pDeltaBones0, gpGlobals->curtime - boneDt );
	GetRagdollCurSequenceWithDeathPose( this, pDeltaBones1, gpGlobals->curtime, m_iDeathPose, m_iDeathFrame );
	float ragdollCreateTime = PhysGetSyncCreateTime();
	if ( ragdollCreateTime != gpGlobals->curtime )
	{
		// The next simulation frame begins before the end of this frame
		// so initialize the ragdoll at that time so that it will reach the current
		// position at curtime.  Otherwise the ragdoll will simulate forward from curtime
		// and pop into the future a bit at this point of transition
		ForceSetupBonesAtTime( pCurrentBones, ragdollCreateTime );
	}
	else
	{
		SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}
}

#ifdef EZ
extern ConVar muzzleflash_light_projtex;
extern ConVar r_muzzleflashlightduration;
extern ConVar r_muzzleflashlightfov;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::ProcessMuzzleFlashEvent( void )
{
	if ( !muzzleflash_light_projtex.GetBool() )
	{
		BaseClass::ProcessMuzzleFlashEvent();
		return;
	}
	
	Color clr( 255, 192, 64, 128 );
	float flDuration = r_muzzleflashlightduration.GetFloat();
	float flFOV = r_muzzleflashlightfov.GetFloat();
	m_nMuzzleFlashAttach = -1;
	GetMuzzleFlashData( clr, flDuration, flFOV, m_nMuzzleFlashAttach );

	if ( m_nMuzzleFlashAttach <= 0 )
	{
		// Just go back to the dlight implementation
		BaseClass::ProcessMuzzleFlashEvent();
		return;
	}

	if ( m_pMuzzleFlashLight )
	{
		m_pMuzzleFlashLight->ResetMuzzleFlash( this, this, flDuration, flFOV, clr );
	}
	else
	{
		m_pMuzzleFlashLight = CMuzzleFlashLightEffect::StartMuzzleFlash( this, this, flDuration, flFOV, clr );
	}

	Vector vAttachment;
	QAngle dummyAngles;
	GetAttachment( m_nMuzzleFlashAttach, vAttachment, dummyAngles );

	// Accompany with an elight
	dlight_t *el = effects->CL_AllocElight( LIGHT_INDEX_MUZZLEFLASH + index );
	el->origin = vAttachment;
	el->radius = random->RandomInt( 32, 64 );
	el->decay = el->radius / flDuration;
	el->die = gpGlobals->curtime + flDuration;
	el->color.r = clr[0];
	el->color.g = clr[1];
	el->color.b = clr[2];
	el->color.exponent = 5;

	// Account for brightness
	if ( clr[3] != 128 )
		el->color.exponent += RemapVal( clr[3], 0, 128, -5, 0 );
}

void C_AI_BaseNPC::Simulate()
{
	BaseClass::Simulate();

	if ( m_pMuzzleFlashLight )
	{
		matrix3x4_t matAttach;
		GetAttachment(m_nMuzzleFlashAttach, matAttach);

		Vector vAttachment, vForward, vRight, vUp;
		MatrixGetColumn( matAttach, 0, vForward );
		MatrixGetColumn( matAttach, 1, vRight );
		MatrixGetColumn( matAttach, 2, vUp );
		MatrixGetColumn( matAttach, 3, vAttachment );

		if ( !m_pMuzzleFlashLight->UpdateMuzzleFlash( vAttachment, vForward, vRight, vUp ) )
		{
			m_pMuzzleFlashLight = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::UpdateOnRemove()
{
	BaseClass::UpdateOnRemove();

	if ( m_pMuzzleFlashLight )
	{
		m_pMuzzleFlashLight->TurnOff();
		m_pMuzzleFlashLight = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AI_BaseNPC::GetMuzzleFlashData( Color &clr, float &flDuration, float &flFOV, int &nAttach )
{
	nAttach = LookupAttachment( "muzzle" );
	if ( nAttach <= 0 )
	{
		// HACKHACK: Consider this to mean it's a hunter, due to lack of client class
		nAttach = LookupAttachment( "bottom_eye" );
		if ( nAttach > 0 )
		{
			static Color hunterClr( 64, 255, 255, 192 );
			clr = hunterClr;
		}

		// HACKHACK: Consider this to mean it's a turret, due to lack of client class
		if ( LookupAttachment( "light" ) )
		{
			nAttach = LookupAttachment( "eyes" );
			if ( nAttach > 0 )
			{
				static const Color combineClr( 96, 248, 255, 128 );
				clr = combineClr;
			}
		}
	}
}
#endif

