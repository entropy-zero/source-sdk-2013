//=============================================================================//
//
// Purpose: Unfortunate vortiguants that have succumbed to headcrabs
// 		either in Xen or in the temporal anomaly in the Arctic.
//
//=============================================================================//

#include "cbase.h"
#include "npcevent.h"
#include "npc_zombigaunt.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_zombigaunt_health( "sk_zombigaunt_health", "300" );

extern int AE_VORTIGAUNT_CLAW_LEFT;
extern int AE_VORTIGAUNT_CLAW_RIGHT;
extern int AE_VORTIGAUNT_SWING_SOUND;

#define ZOMBIE_BLOOD_LEFT_HAND		0
#define ZOMBIE_BLOOD_RIGHT_HAND		1

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_Zombigaunt )
// Saved fields go here
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_zombigaunt, CNPC_Zombigaunt );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Spawn( void )
{
	// Disable back-away
	AddSpawnFlags( SF_NPC_NO_PLAYER_PUSHAWAY );

	BaseClass::Spawn();

	CapabilitiesAdd( bits_CAP_MOVE_JUMP );

	m_iMaxHealth = sk_zombigaunt_health.GetFloat();
	m_iHealth = m_iMaxHealth;

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_ZOMBIE );
	}

	SetViewOffset( Vector ( 0, 0, 64 ) );// position of the eyes relative to monster's origin.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::Precache()
{
	// Allow multiple models but default to zombigaunt.mdl
	char *szModel = (char *)STRING( GetModelName() );
	if (!szModel || !*szModel)
	{
		szModel = "models/zombie/zombigaunt.mdl";
		SetModelName( AllocPooledString( szModel ) );
	}

	BaseClass::Precache();
}

//------------------------------------------------------------------------------
// Purpose : Translate some activites for the Vortigaunt
//------------------------------------------------------------------------------
Activity CNPC_Zombigaunt::NPC_TranslateActivity( Activity eNewActivity )
{
	// Zombigaunts use 'carry' activities while idle to look creepier
	if ( eNewActivity == ACT_IDLE )
		return ACT_IDLE_CARRY;
	if ( eNewActivity == ACT_WALK )
		return ACT_WALK_CARRY;

	// NOTE: This is a stand-in until the readiness system can handle non-weapon holding NPC's
	if ( eNewActivity == ACT_IDLE_CARRY )
	{
		// More than relaxed means we're stimulated
		if ( GetReadinessLevel() >= AIRL_STIMULATED )
			return ACT_IDLE_STIMULATED;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

#define HAND_BOTH	2

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->event == AE_VORTIGAUNT_SWING_SOUND)
	{
		// Make hands glow if not already glowing
		if ( m_fGlowAge == 0 )
		{
			StartHandGlow( VORTIGAUNT_BEAM_ZAP, HAND_BOTH );
		}
		m_fGlowAge = 1;
	}

	if (pEvent->event == AE_VORTIGAUNT_CLAW_RIGHT)
	{
		Vector right;
		AngleVectors( GetLocalAngles(), NULL, &right, NULL );
		right = right * -50;

		QAngle angle( -3, -5, -3 );

		ClawAttack( 64, 3, angle, right, ZOMBIE_BLOOD_RIGHT_HAND, DMG_SHOCK );
		EndHandGlow();
		return;
	}

	if (pEvent->event == AE_VORTIGAUNT_CLAW_LEFT)
	{
		Vector right;
		AngleVectors( GetLocalAngles(), NULL, &right, NULL );
		right = right * 50;
		QAngle angle( -3, 5, -3 );

		ClawAttack( 64, 3, angle, right, ZOMBIE_BLOOD_LEFT_HAND, DMG_SHOCK );
		EndHandGlow();
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );

}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Zombigaunt::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	const float MAX_JUMP_RISE		= 220.0f;
	const float MAX_JUMP_DISTANCE	= 512.0f;
	const float MAX_JUMP_DROP		= 384.0f;

	if (BaseClass::IsJumpLegal( startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE ))
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

bool CNPC_Zombigaunt::MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost )
{
	float delta = vecEnd.z - vecStart.z;

	float multiplier = 1;
	if (moveType == bits_CAP_MOVE_JUMP)
	{
		multiplier = (delta < 0) ? 0.5 : 1.5;
	}
	else if (moveType == bits_CAP_MOVE_CLIMB)
	{
		multiplier = (delta > 0) ? 0.5 : 4.0;
	}

	*pCost *= multiplier;

	return (multiplier != 1);
}

//-----------------------------------------------------------------------------
// Purpose: Overrides ai_playerally idle speech, which was heavily dependent on player presence
//-----------------------------------------------------------------------------
bool CNPC_Zombigaunt::SelectIdleSpeech( AISpeechSelection_t *pSelection )
{
	if ( !IsOkToSpeak( SPEECH_IDLE ) )
		return false;

	if ( /*ShouldSpeakRandom( TLK_IDLE, 2 ) &&*/ SelectSpeechResponse( TLK_IDLE, NULL, NULL, pSelection ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Zombigaunt::OnStartSchedule( int scheduleType )
{
	BaseClass::OnStartSchedule( scheduleType );

	// Make weird motions while charging
	if (scheduleType == SCHED_CHASE_ENEMY)
		Speak( TLK_VORT_CHARGE );
}
