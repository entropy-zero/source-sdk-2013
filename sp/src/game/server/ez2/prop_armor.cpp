//=============================================================================//
//
// Purpose:		Dynamic prop intended to be parented to NPCs and protect against
//				bullets or projectiles.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "prop_armor.h"
#include "ai_basenpc.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS( prop_armor, CArmorProp );

//-----------------------------------------------------------------------------

bool CArmorProp::m_sbAmmoTypesLoaded = false;
int CArmorProp::m_nAmmoTypePistol = -1;
int CArmorProp::m_nAmmoTypeGaussPistol = -1;
int CArmorProp::m_nAmmoTypeAR2 = -1;
int CArmorProp::m_nAmmoType556mm = -1;
int CArmorProp::m_nAmmoType762mm = -1;

ConVar	sk_armor_penetration_pistol( "sk_armor_penetration_pistol", "0.9" );
ConVar	sk_armor_penetration_gauss_pistol( "sk_armor_penetration_gauss_pistol", "0.8" );
ConVar	sk_armor_penetration_ar2( "sk_armor_penetration_ar2", "1.3" );
ConVar	sk_armor_penetration_556mm( "sk_armor_penetration_556mm", "1.4" );
ConVar	sk_armor_penetration_762mm( "sk_armor_penetration_762mm", "1.4" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArmorProp::Spawn()
{
	m_bDisableBoneFollowers = true;

	BaseClass::Spawn();

	SetBlocksLOS( false );
	SetNavIgnore();

	if ( !m_sbAmmoTypesLoaded )
	{
		CAmmoDef *pAmmoDef = GetAmmoDef();
		m_nAmmoTypePistol = pAmmoDef->Index( "Pistol" );
		m_nAmmoTypeGaussPistol = pAmmoDef->Index( "GaussPistol" );
		m_nAmmoTypeAR2 = pAmmoDef->Index( "AR2" );
		m_nAmmoType556mm = pAmmoDef->Index( "556mm" );
		m_nAmmoType762mm = pAmmoDef->Index( "762mm" );
		m_sbAmmoTypesLoaded = true;
	}

	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		pPhys->EnableGravity( false );
		pPhys->GetShadowController()->SetPhysicallyControlled( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArmorProp::Precache()
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CArmorProp::PassesDamageFilter( const CTakeDamageInfo &info )
{
	if (!BaseClass::PassesDamageFilter( info ))
		return false;

	// Don't take damage our parent would block
	if ( HasNPCParent() && !GetNPCParent()->PassesDamageFilter( info ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CArmorProp::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	if (!BaseClass::OnTakeDamage( inputInfo ))
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArmorProp::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );

	if ( HasNPCParent() )
	{
		if ( !ShouldBlockTraceAttack( info, vecDir, ptr, pAccumulator ) )
		{
			// Greater than our protection grade
			CTakeDamageInfo newInfo = info;
			ModifyTraceAttackDamage( newInfo, GetPenetrationScale( info ) );

			if (newInfo.GetDamage() < 0.0f)
				newInfo.SetDamage( 0.0f );

			ptr->hitgroup = GetHitgroup();
			ptr->m_pEnt = this;

			if ( newInfo.GetDamage() > 0.0f )
			{
				GetNPCParent()->TraceAttack( newInfo, vecDir, ptr, pAccumulator );
			}
			else
			{
				// NPC should still know when it's being attacked
				GetNPCParent()->SetLastDamageTime( gpGlobals->curtime );
			}
		}
		else
		{
			// NPC should still know when it's being attacked
			GetNPCParent()->SetLastDamageTime( gpGlobals->curtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CArmorProp::GetPenetrationScale( const CTakeDamageInfo &info )
{
	int nAmmoType = info.GetAmmoType();
	if ( nAmmoType == m_nAmmoTypePistol )
	{
		return sk_armor_penetration_pistol.GetFloat();
	}
	else if ( nAmmoType == m_nAmmoTypeGaussPistol )
	{
		return sk_armor_penetration_gauss_pistol.GetFloat();
	}
	else if ( nAmmoType == m_nAmmoTypeAR2 )
	{
		return sk_armor_penetration_ar2.GetFloat();
	}
	else if ( nAmmoType == m_nAmmoType556mm )
	{
		return sk_armor_penetration_556mm.GetFloat();
	}
	else if ( nAmmoType == m_nAmmoType762mm )
	{
		return sk_armor_penetration_762mm.GetFloat();
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CArmorProp::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );
}