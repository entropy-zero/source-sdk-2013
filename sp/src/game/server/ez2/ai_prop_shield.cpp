//=============================================================================//
//
// Purpose:		A physical shield that can be used by certain NPCs.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "ai_prop_shield.h"
#include "ai_baseactor.h"
#include "IEffects.h"

//-----------------------------------------------------------------------------

// Shield activities
int ACT_IDLE_SHIELD;
int ACT_IDLE_SHIELD_RELAXED;
int ACT_IDLE_SHIELD_STIMULATED;
int ACT_IDLE_ANGRY_SHIELD;
int ACT_IDLE_AIM_SHIELD_STIMULATED;
int ACT_WALK_SHIELD;
int ACT_WALK_SHIELD_RELAXED;
int ACT_WALK_SHIELD_STIMULATED;
int ACT_WALK_AIM_SHIELD;
int ACT_WALK_AIM_SHIELD_STIMULATED;
int ACT_RUN_SHIELD;
int ACT_RUN_AIM_SHIELD;
int ACT_COVER_SHIELD_LOW;
int ACT_RANGE_AIM_SHIELD_LOW;
int ACT_WALK_CROUCH_SHIELD;
int ACT_RUN_CROUCH_SHIELD;
int ACT_WALK_CROUCH_AIM_SHIELD;
int ACT_RUN_CROUCH_AIM_SHIELD;

int ACT_RANGE_ATTACK_SHIELD;
int ACT_RANGE_ATTACK_SHIELD_LOW;
int ACT_MELEE_ATTACK_SHIELD;
int ACT_RELOAD_SHIELD;

int ACT_ARM_SHIELD;
int ACT_DISARM_SHIELD;
int ACT_ACTIVATE_SHIELD;
int ACT_DEACTIVATE_SHIELD;
int ACT_THROW_SHIELD;
int ACT_SLAM_SHIELD;
int ACT_GESTURE_FLINCH_SHIELD_TOP;
int ACT_GESTURE_FLINCH_SHIELD_MID;
int ACT_GESTURE_FLINCH_SHIELD_BOTTOM;
int ACT_FLINCH_SHIELD_BIG;

int AE_NPC_DRAW_SHIELD;
int AE_NPC_HOLSTER_SHIELD;
int AE_NPC_THROW_SHIELD;
int AE_NPC_SLAM_SHIELD;
int AE_NPC_SET_SHIELD_ANIM;

//-----------------------------------------------------------------------------

#define SHIELD_SKIN_CONSCRIPT		0
#define SHIELD_SKIN_COMBINE			1
#define SHIELD_SKIN_METROPOLICE		2

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CPropShield )
	DEFINE_FIELD( m_bHolstered, FIELD_BOOLEAN ),
END_DATADESC();

LINK_ENTITY_TO_CLASS( prop_shield, CPropShield );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropShield::CPropShield()
{
	m_bHolstered = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropShield *CPropShield::CreatePropShield( CAI_BaseActor *pNPC, bool bStartHolstered, const char *pszModelName )
{
	CPropShield *pShield = (CPropShield*)CreateNoSpawn( "prop_shield", pNPC->GetAbsOrigin(), pNPC->GetAbsAngles(), pNPC );
	pShield->SetModelName( MAKE_STRING( pszModelName ) );
	pShield->SetParent( pNPC, pNPC->LookupAttachment( bStartHolstered ? "shield_back" : "shield" ) );
	pShield->SetLocalOrigin( vec3_origin );
	pShield->SetLocalAngles( vec3_angle );

	pShield->KeyValue( "solid", "6" );
	PhysDisableEntityCollisions( pNPC, pShield );

	pShield->m_bHolstered = bStartHolstered;
	DispatchSpawn( pShield );
	//pShield->m_takedamage = DAMAGE_YES;

	switch ( pNPC->Classify() )
	{
		case CLASS_COMBINE:
			pShield->m_nSkin = SHIELD_SKIN_COMBINE;
			break;
		case CLASS_METROPOLICE:
			pShield->m_nSkin = SHIELD_SKIN_METROPOLICE;
			break;
	}

	return pShield;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Spawn()
{
	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Precache()
{
	BaseClass::Precache();

	PrecacheModel( SHIELD_MODEL_NAME );

	PrecacheScriptSound( "Weapon_TacticalShield.Holster" );
	PrecacheScriptSound( "Weapon_TacticalShield.Unholster" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropShield::ShouldBlockTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	return info.GetDamage() < 10.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::ModifyTraceAttackDamage( CTakeDamageInfo &info, float flPenetrationScale )
{
	if ( !m_bHolstered )
	{
		int nSeq = -1;

		if ( ( info.GetDamage() * flPenetrationScale ) > 40.0f )
		{
			// BIG flinch
			// TODO: Proper seqeunce, make them stop
			nSeq = GetNPCParent()->SelectWeightedSequence( ACT_GESTURE_BIG_FLINCH );
		}
		else
		{
			float flZ = (info.GetDamagePosition().z - GetAbsOrigin().z);
			if ( flZ > 8.0f )
			{
				nSeq = GetNPCParent()->SelectWeightedSequence( (Activity)ACT_GESTURE_FLINCH_SHIELD_TOP );
			}
			else if ( flZ < -8.0f )
			{
				nSeq = GetNPCParent()->SelectWeightedSequence( (Activity)ACT_GESTURE_FLINCH_SHIELD_BOTTOM );
			}
			else
			{
				nSeq = GetNPCParent()->SelectWeightedSequence( (Activity)ACT_GESTURE_FLINCH_SHIELD_MID );
			}
		}

		if ( nSeq != -1 )
		{
			int iLayer = GetNPCParent()->AddGestureSequence( nSeq, true );
			GetNPCParent()->GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + GetNPCParent()->GetLayerDuration( iLayer ) );
		}
	}

	info.SetDamage( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );

	g_pEffects->Sparks( ptr->endpos );

	if ( HasNPCParent() && GetNPCParent()->GetHealth() > 0 )
	{
		CAI_PropShieldUserSink *pShieldUser = GetNPCShieldUser();
		if ( pShieldUser )
		{
			pShieldUser->OnShieldTraceAttack( this, info );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Event_Killed( const CTakeDamageInfo &info )
{
	if ( HasNPCParent() && GetNPCParent()->GetHealth() > 0 )
	{
		CAI_PropShieldUserSink *pShieldUser = GetNPCShieldUser();
		if ( pShieldUser )
		{
			if ( !pShieldUser->OnShieldBreak( this, info ) )
			{
				// Skip traditional breakage code
				CBaseEntity::Event_Killed( info );
				return;
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Holster()
{
	if ( !HasNPCParent() || m_bHolstered )
		return;

	int nAttachment = GetNPCParent()->LookupAttachment( "shield_back" );
	if ( nAttachment == -1 )
		return;

	SetParent( GetParent(), nAttachment );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
	m_bHolstered = true;

	EmitSound( "Weapon_TacticalShield.Holster" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Unholster()
{
	if ( !HasNPCParent() || !m_bHolstered )
		return;

	int nAttachment = GetNPCParent()->LookupAttachment( "shield" );
	if ( nAttachment == -1 )
		return;

	SetParent( GetParent(), nAttachment );
	SetLocalOrigin( vec3_origin );
	SetLocalAngles( vec3_angle );
	m_bHolstered = false;

	EmitSound( "Weapon_TacticalShield.Unholster" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropShield::Drop( const CTakeDamageInfo &info )
{
	CBaseEntity *pGib = CreateNoSpawn( "prop_physics", GetAbsOrigin(), GetAbsAngles() );
	if (pGib)
	{
		pGib->GetBaseAnimating()->CopyAnimationDataFrom( this );
		pGib->AddSpawnFlags( SF_PHYSPROP_DEBRIS | SF_PHYSPROP_IS_GIB );
		DispatchSpawn( pGib );

		if (pGib->VPhysicsGetObject() && pGib->VPhysicsGetObject()->GetMass() > 0.0f)
		{
			Vector velocity = info.GetDamageForce();
			velocity /= pGib->VPhysicsGetObject()->GetMass();

			pGib->VPhysicsGetObject()->SetVelocity(&velocity, NULL);
		}

		if (info.GetDamageType() & DMG_DISSOLVE)
		{
			pGib->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
		else
		{
			/*if ( g_hStealthManager )
			{
				pGib->AddContext( "curious_prop", "1", 0.0 );
				pGib->AddContext( "pickup_prop", "1", 0.0 );
				pGib->AddContext( "headwear", pszType, 0.0 );
			}
			else
			{
				pGib->SUB_StartFadeOut( 10.0f, false );
			}*/
		}
	}
}
