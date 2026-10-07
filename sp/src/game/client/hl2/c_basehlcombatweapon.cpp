//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_basehlcombatweapon.h"
#include "igamemovement.h"
#ifdef EZ
#include "flashlighteffect.h"
#include "r_efx.h"
#include "dlight.h"
#include "ammodef.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_HLMachineGun, DT_HLMachineGun, CHLMachineGun )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_HLSelectFireMachineGun, DT_HLSelectFireMachineGun, CHLSelectFireMachineGun )
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT( C_BaseHLBludgeonWeapon, DT_BaseHLBludgeonWeapon, CBaseHLBludgeonWeapon )
END_RECV_TABLE()


#ifdef EZ
ConVar muzzleflash_light_projtex( "muzzleflash_light_projtex", "1", FCVAR_ARCHIVE );
ConVar r_muzzleflashlightduration( "r_muzzleflashlightduration", "0.075", FCVAR_CHEAT );
ConVar r_muzzleflashlightduration_buckshot( "r_muzzleflashlightduration_buckshot", "0.125", FCVAR_CHEAT );
ConVar r_muzzleflashlightfov( "r_muzzleflashlightfov", "120", FCVAR_CHEAT );
ConVar r_muzzleflashlightfov_small( "r_muzzleflashlightfov_small", "90", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseHLCombatWeapon::ProcessMuzzleFlashEvent()
{
	if ( !muzzleflash_light_projtex.GetBool() )
	{
		// Silenced weapons don't have traditional muzzle flashes
		if ( IsSilenced() )
			return;

		BaseClass::ProcessMuzzleFlashEvent();
		return;
	}

	Color clr( 255, 192, 64, 128 );
	float flDuration = r_muzzleflashlightduration.GetFloat();
	float flFOV = r_muzzleflashlightfov.GetFloat();
	GetMuzzleFlashData( clr, flDuration, flFOV );

	if ( m_pMuzzleFlashLight )
	{
		m_pMuzzleFlashLight->ResetMuzzleFlash( this, GetOwner(), flDuration, flFOV, clr );
	}
	else
	{
		m_pMuzzleFlashLight = CMuzzleFlashLightEffect::StartMuzzleFlash( this, GetOwner(), flDuration, flFOV, clr );
	}

	Vector vAttachment;
	QAngle dummyAngles;

	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->InFirstPersonView() )
	{
		// Get the viewmodel
		C_BaseViewModel *pVM = pPlayer->GetViewModel( m_nViewModelIndex );
		if ( pVM )
			pVM->GetAttachment(1, vAttachment, dummyAngles); // set 1 instead "attachment"
	}
	else
	{
		GetAttachment( 1, vAttachment, dummyAngles );
	}

	// Accompany with an elight
	m_pMuzzleFlashDLight = effects->CL_AllocElight( LIGHT_INDEX_MUZZLEFLASH + index );
	m_pMuzzleFlashDLight->origin = vAttachment;
	m_pMuzzleFlashDLight->radius = random->RandomInt( 32, 64 );
	m_pMuzzleFlashDLight->decay = m_pMuzzleFlashDLight->radius / flDuration;
	m_pMuzzleFlashDLight->die = gpGlobals->curtime + flDuration;
	m_pMuzzleFlashDLight->color.r = clr[0];
	m_pMuzzleFlashDLight->color.g = clr[1];
	m_pMuzzleFlashDLight->color.b = clr[2];
	m_pMuzzleFlashDLight->color.exponent = 6; // 5

	// Account for brightness
	if ( clr[3] != 128 )
		m_pMuzzleFlashDLight->color.exponent += RemapVal( clr[3], 0, 128, -5, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseHLCombatWeapon::GetMuzzleFlashData( Color &clr, float &flDuration, float &flFOV )
{
	// By default, use hardcoded data based on ammo type.
	// This can be overridden by weapon classes that want to do something different.
	static int g_iAmmoTypeAR2 = GetAmmoDef()->Index( "AR2" );
	static int g_iAmmoTypeGaussPistol = GetAmmoDef()->Index( "GaussPistol" );
	static int g_iAmmoTypePistol = GetAmmoDef()->Index( "Pistol" );
	static int g_iAmmoTypeSMG1 = GetAmmoDef()->Index( "SMG1" );
	static int g_iAmmoTypeBuckshot = GetAmmoDef()->Index( "Buckshot" );
	static int g_iAmmoType357 = GetAmmoDef()->Index( "357" );

	static const Color combineClr( 96, 248, 255, 128 );

	int iAmmoType = GetPrimaryAmmoType();

	if ( iAmmoType == g_iAmmoTypeAR2 )
	{
		clr = combineClr;
		clr[3] = 160;
	}
	else if ( iAmmoType == g_iAmmoTypeGaussPistol )
	{
		clr = combineClr;
		flFOV = r_muzzleflashlightfov_small.GetFloat();
	}
	else if ( iAmmoType == g_iAmmoTypePistol || iAmmoType == g_iAmmoTypeSMG1 )
	{
		flFOV = r_muzzleflashlightfov_small.GetFloat();
	}
	else if ( iAmmoType == g_iAmmoTypeBuckshot )
	{
		flDuration = r_muzzleflashlightduration_buckshot.GetFloat();
		clr[3] = 255;
	}
	else if ( iAmmoType == g_iAmmoType357 )
	{
		clr[3] = 192;
	}

	if ( IsSilenced() )
	{
		// Much less noticeable
		clr[3] *= 0.25f;
		flFOV *= 0.75f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Receive messages from the server
//-----------------------------------------------------------------------------
void C_BaseHLCombatWeapon::ReceiveMessage( int classID, bf_read &msg )
{
	// Is the message for a sub-class?
	if ( classID != GetClientClass()->m_ClassID )
	{
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}
	
	int messageType = msg.ReadByte();
	switch( messageType )
	{
	case BASEHLWEAPON_MUZZLE_FLASH_CUSTOM:
		{
			Color clr;
			clr.SetRawColor( msg.ReadUBitLong( 32 ) );

			if ( clr[3] == 0 )
				clr[3] = 128;

			float flDuration = msg.ReadFloat();
			if ( flDuration == 0.0f )
				flDuration = r_muzzleflashlightduration.GetFloat();
			
			float flFOV = msg.ReadFloat();
			if ( flFOV == 0.0f )
				flFOV = r_muzzleflashlightfov.GetFloat();

			if ( m_pMuzzleFlashLight )
			{
				m_pMuzzleFlashLight->ResetMuzzleFlash( this, GetOwner(), flDuration, flFOV, clr );
			}
			else
			{
				m_pMuzzleFlashLight = CMuzzleFlashLightEffect::StartMuzzleFlash( this, GetOwner(), flDuration, flFOV, clr );
			}

			Vector vAttachment;
			QAngle dummyAngles;

			C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
			if ( pPlayer && pPlayer->InFirstPersonView() )
			{
				// Get the viewmodel
				C_BaseViewModel *pVM = pPlayer->GetViewModel( m_nViewModelIndex );
				if ( pVM )
					pVM->GetAttachment(1, vAttachment, dummyAngles); // set 1 instead "attachment"
			}
			else
			{
				GetAttachment( 1, vAttachment, dummyAngles );
			}

			// Accompany with an elight
			m_pMuzzleFlashDLight = effects->CL_AllocElight( LIGHT_INDEX_MUZZLEFLASH + index );
			m_pMuzzleFlashDLight->origin = vAttachment;
			m_pMuzzleFlashDLight->radius = random->RandomInt( 32, 64 );
			m_pMuzzleFlashDLight->decay = m_pMuzzleFlashDLight->radius / flDuration;
			m_pMuzzleFlashDLight->die = gpGlobals->curtime + flDuration;
			m_pMuzzleFlashDLight->color.r = clr[0];
			m_pMuzzleFlashDLight->color.g = clr[1];
			m_pMuzzleFlashDLight->color.b = clr[2];
			m_pMuzzleFlashDLight->color.exponent = 6; // 5

			// Account for brightness
			if ( clr[3] != 128 )
				m_pMuzzleFlashDLight->color.exponent += RemapVal( clr[3], 0, 128, -5, 0 );
		}
		break;
	default:
		AssertMsg1( false, "Received unknown message %d", messageType);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseHLCombatWeapon::Simulate()
{
	BaseClass::Simulate();

	if ( m_pMuzzleFlashLight )
	{
		matrix3x4_t matAttach;

		C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer && pPlayer->InFirstPersonView() )
		{
			// Get the viewmodel
			C_BaseViewModel *pVM = pPlayer->GetViewModel( m_nViewModelIndex );
			if ( pVM )
				pVM->GetAttachment(1, matAttach); // set 1 instead "attachment"
		}
		else
		{
			GetAttachment(1, matAttach); // set 1 instead "attachment"
		}

		Vector vAttachment, vForward, vRight, vUp;
		MatrixGetColumn( matAttach, 0, vForward );
		MatrixGetColumn( matAttach, 1, vRight );
		MatrixGetColumn( matAttach, 2, vUp );
		MatrixGetColumn( matAttach, 3, vAttachment );

		if ( !m_pMuzzleFlashLight->UpdateMuzzleFlash( vAttachment, vForward, vRight, vUp ) )
		{
			m_pMuzzleFlashLight = NULL;
			m_pMuzzleFlashDLight = NULL;
		}
		else if ( m_pMuzzleFlashDLight )
		{
			if ( pPlayer && pPlayer->InFirstPersonView() )
			{
				// Center so that we can see the light reflect off the viewmodel
				Vector vecEyePos, vecEyeForward;
				pPlayer->EyePositionAndVectors( &vecEyePos, &vecEyeForward, NULL, NULL );

				Vector vecDelta = (vAttachment - vecEyePos);
				m_pMuzzleFlashDLight->origin = vecEyePos + (vecEyeForward * vecDelta.Length());
			}
			else
			{
				// Follow the attachment directly
				m_pMuzzleFlashDLight->origin = vAttachment;
			}
		}
	}
}
#endif