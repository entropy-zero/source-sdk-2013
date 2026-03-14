//=============================================================================//
//
// Purpose:		AI component dedicated to stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_senses.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_area.h"
#include "ai_basenpc.h"
#include "ai_behavior_actbusy.h"
#include "ai_hint.h"
#include "saverestore_utlvector.h"
#include "ez2_player.h"
#include "basehlcombatweapon.h"
#include "eventqueue.h"
#include "scripted.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ALERT_LEVEL_THRESHOLD_FACE 0.34
#define ALERT_LEVEL_INCREASE_DISTANCE 2048.0
#define ALERT_LEVEL_INCREASE_SPEED 400.0
#define ALERT_LEVEL_RADIUS_AVG 35.0			// Props less than this radius are harder to notice, larger are harder to not notice
#define ALERT_LEVEL_MAX_LEVEL 5.0			// The maximum amount of level that can be accumulated before going all in
#define ALERT_LEVEL_MULT_DEFAULT 0.03
#define ALERT_LEVEL_MULT_EASY 0.7
#define ALERT_LEVEL_MULT_NORM 0.85
#define ALERT_LEVEL_MULT_HARD 1.0
//#define ALERT_LEVEL_FORGET_TIME 15.0
#define ALERT_LEVEL_DECAY 0.015

// This uses the direct dot product instead of a viewcone in degrees, similar to npc_enemyfinder.
#define AI_FOV_IDLE 0.3 // equivalent to ~70 degrees
#define AI_FOV_ALERT 0.1 // equivalent to ~85 degrees

#define AI_FOV_Z_DIFF_THRESHOLD_IDLE 384.0
#define AI_FOV_Z_DIFF_THRESHOLD_ALERT 512.0

#define AI_SIGHT_BOX_MAX_DIST 4096.0
#define AI_SIGHT_BOX_MAX_DIMS_IDLE 32.0
#define AI_SIGHT_BOX_MAX_DIMS_ALERT 16.0

#define AI_SIGHT_DARKNESS_MAX_DIST_ALERT 256.0
#define AI_SIGHT_DARKNESS_MAX_SOUND_DIST 128.0

#define AI_INJURY_SOUND_DIST 64.0

#define AI_STEALTH_FREE_KNOWLEDGE_DURATION		0.3
#define AI_STEALTH_ENEMY_LKP_ALWAYS_SEE_DIST	200.0

// TODO: Make into dedicated console group? See mapbase_con_groups.cpp
const Color DbgStealthColor = Color( 64, 255, 255, 255 );

//-----------------------------------------------------------------------------

#define NUM_STEALTH_SOUND_CHANNELS (SOUNDENT_CHANNEL_STEALTH_LAST - SOUNDENT_CHANNEL_STEALTH_FIRST + 1)

static const char *g_pszStealthSoundChannels[NUM_STEALTH_SOUND_CHANNELS] = {
	"Discovered Body",			//	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY,
	"Discovered Open Door",		//	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_OPEN_DOOR,
	"Discovered Enemy",			//	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_ENEMY,

	"Prop Broken",				//	SOUNDENT_CHANNEL_STEALTH_PROP_BREAK,
	"Small Prop Broken",		//	SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK,
	"Prop Impact",				//	SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT,
	"Prop Interesting",			//	SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING,
	"Prop Moving",				//	SOUNDENT_CHANNEL_STEALTH_PROP_MOVING,

	"Hit by Object",			//	SOUNDENT_CHANNEL_STEALTH_HIT_BY_OBJECT,
	"Speech Interrupted",		//	SOUNDENT_CHANNEL_STEALTH_SPEECH_INTERRUPTED,
	"Announce Attacked",		//	SOUNDENT_CHANNEL_STEALTH_ANNOUNCE_ATTACKED,

	"Saw Suspicious",			//	SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS,
};

//-----------------------------------------------------------------------------

ConVar	ai_stealth_alertness_cap( "ai_stealth_alertness_cap", "0" );
ConVar	ai_stealth_alertness_high_freq_look( "ai_stealth_alertness_high_freq_look", "1" );
ConVar	ai_stealth_allow_default_stealth_manager( "ai_stealth_allow_default_stealth_manager", "1" );
ConVar	ai_stealth_alertness_big_health( "ai_stealth_alertness_big_health", "500" );

ConVar	g_debug_stealth_senses( "g_debug_stealth_senses", "0" );
ConVar	g_debug_stealth_alertness( "g_debug_stealth_alertness", "0" );

extern ConVar ai_stealth_force;

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( AlertLevel_t )

	DEFINE_FIELD( hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( flLevel, FIELD_FLOAT ),
	DEFINE_FIELD( flPrevLevel, FIELD_FLOAT ),
	DEFINE_FIELD( flTotalLevel, FIELD_FLOAT ),
	DEFINE_FIELD( flStartTime, FIELD_TIME ),
	DEFINE_FIELD( flLastUpdate, FIELD_TIME ),

END_DATADESC();

//-----------------------------------------------------------------------------

BEGIN_DATADESC_NO_BASE( CAI_StealthSenses )

	DEFINE_UTLVECTOR( m_AlertLevels, FIELD_EMBEDDED ),
	DEFINE_FIELD( m_flNextAlertLevelThink, FIELD_TIME ),

END_DATADESC();

//-----------------------------------------------------------------------------

CAI_StealthSenses::CAI_StealthSenses( CAI_BaseNPC *pOuter )
	: CAI_Component( pOuter )
{
	m_flNextAlertLevelThink = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::InitStealthSenses()
{
	// Note that CAI_Enemies currently exposes no method for reverting this.
	// That would only be an issue if you want to disable stealth senses later
	GetEnemies()->SetFreeKnowledgeDuration( AI_STEALTH_FREE_KNOWLEDGE_DURATION );

	if (!g_hStealthManager)
	{
		if ( ai_stealth_allow_default_stealth_manager.GetBool() )
		{
			// Make sure there isn't one that's just disabled in the level before making a default one
			if ( !gEntList.FindEntityByClassname( NULL, "ai_stealth_manager" ) )
			{
				CBaseEntity *pDefaultManager = CBaseEntity::Create( "ai_stealth_manager", GetAbsOrigin(), GetAbsAngles() );
				if ( !g_hStealthManager )
				{
					// Didn't work
					UTIL_Remove( pDefaultManager );
				}
				else
				{
					Msg( "Created a default ai_stealth_manager for stealth sensing NPC %s\n", GetOuter()->GetDebugName() );
				}
			}
		}

		if (!g_hStealthManager)
			Warning( "No ai_stealth_manager detected. Some stealth functionality may be missing or degraded\n" );
	}

	if ( ai_stealth_force.GetBool() && GetOuter()->GetHealth() >= ai_stealth_alertness_big_health.GetInt() )
	{
		// If we're using non-mapper stealth senses, automatically use the extra danger flag for dangerous enemies
		GetOuter()->AddStealthFlags( STEALTH_F_EXTRA_DANGER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::StopStealthSenses()
{
	GetOuter()->SetUsingHighFrequencyLook( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::InitSquad( CAI_Squad *pSquad )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::RunStealthSenses()
{
	if ( m_flNextAlertLevelThink < gpGlobals->curtime )
	{
		MaintainAlertLevels();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::QueryHearSound( CSound *pSound )
{
	if ( pSound->IsSoundType( SOUND_DANGER ) )
		return true;

	// HACKHACK: This is currently how we check for silenced gunfire sounds, as player gunfire doesn't use SOUND_CONTEXT_GUNFIRE. Not at all ideal!
	if ( pSound->m_hOwner && pSound->m_hOwner->IsCombatCharacter() &&
		( pSound->SoundType() == SOUND_COMBAT || pSound->SoundType() == (SOUND_COMBAT|SOUND_CONTEXT_GUNFIRE) ) && pSound->Volume() < 1000 )
	{
		CHLMachineGun *pHLWeapon = dynamic_cast<CHLMachineGun*>(pSound->m_hOwner->MyCombatCharacterPointer()->GetActiveWeapon());
		if ( pHLWeapon && pHLWeapon->IsSilenced() )
		{
			// If the silenced gunshot isn't visible, reduce its range to 40%
			// (e.g. 750 -> 300)
			float flDistToSoundSqr = (GetOuter()->EarPosition() - pSound->GetSoundOrigin()).LengthSqr();
			if ( flDistToSoundSqr < Square(((float)pSound->Volume()) * 0.4f) || GetOuter()->FVisible( pSound->GetSoundOrigin() ) )
			{
				return true;
			}

			return false;
		}
	}
	
	if (pSound->SoundChannel() == SOUNDENT_CHANNEL_INJURY)
	{
		// Ignore injury sounds we don't see (unless we're really close to it)
		if ( (!GetOuter()->FVisible(pSound->GetSoundOrigin()) || !GetOuter()->FInViewCone(pSound->GetSoundOrigin())) && (GetAbsOrigin() - pSound->GetSoundOrigin()).LengthSqr() > Square(AI_INJURY_SOUND_DIST) )
		{
			if ( g_debug_stealth_senses.GetBool() )
				EntityPrint( GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_SOUND ), Color( 255, 255, 128 ), 4.0f, "Ignoring unseen injury" );

			return false;
		}

		// Ignore injury sounds from invalid owners
		if ( !pSound->m_hOwner || pSound->m_hOwner->Classify() == CLASS_BULLSEYE )
			return false;
	}
	
	// For now, ignore scents (the guards keep smelling bodies they shouldn't know about, but that means they go alert; maybe they should just investigate instead?)
	if (pSound->FIsScent())
	{
		return false;
	}
	
	// Don't hear stealth sounds if we have a real combat or danger sound
	if (IsStealthSound(pSound) && (GetOuter()->HasCondition(COND_HEAR_COMBAT) || GetOuter()->HasCondition(COND_HEAR_DANGER)))
	{
		CSound *pBestSound = GetOuter()->GetBestSound( SOUND_COMBAT | SOUND_DANGER );
		if (pBestSound && !IsStealthSound(pBestSound))
		{
			if ( g_debug_stealth_senses.GetBool() )
				EntityPrint( GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_SOUND ), Color( 255, 255, 128 ), 4.0f,
					"Ignoring stealth sound '%s' due to hearing real combat sound", GetStealthSoundChannelName( pSound->SoundChannel() ) );

			//printl("Ignoring stealth sound")
			return false;
		}
	}
	
	// Ignore injury sounds we don't see
	//if (sound.SoundType() == SOUND_BULLET_IMPACT)
	//{
	//	printl("Ignoring bullet impact")
	//	return false;
	//}
	
	if ( pSound->m_hTarget != GetOuter() )
	{
		if (IsExclusiveStealthSound(pSound))
		{
			/*if ( g_debug_stealth_senses.GetBool() )
			{
				EntityPrint( GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_SOUND ), Color( 255, 255, 128 ), 4.0f,
					"Ignoring stealth sound '%s' due to it being an exclusive stealth sound", GetStealthSoundChannelName( pSound->SoundChannel() ) );
				EntityPrint( GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_SOUND ) + 1, Color( 255, 255, 128 ), 4.0f,
					"[Target: %s]", pSound->m_hTarget ? pSound->m_hTarget->GetDebugName() : "<null>" );
			}*/

			// Only the person who saw it
			return false;
		}

		if (!pSound->IsSoundType(SOUND_COMBAT))
		{
			// If it's a non-combat sound, various conditions/distractions can reduce its effective volume
			float flVolume = (float)pSound->Volume();

			// Not being visible is a significant factor
			if ( !GetOuter()->FVisible( pSound->GetSoundOrigin() ) )
				flVolume *= 0.5f;

			if ( GetOuter()->IsMoving() )
			{
				// Decrease based on how fast we're going (running vs. walking, etc.)
				float flSpeed = GetOuter()->GetSequenceGroundSpeed( GetOuter()->GetSequence() );
				if ( flSpeed > 0.0f )
					flVolume -= flSpeed;
			}

			if ( GetOuter()->GetExpresser() )
			{
				// If we're speaking, we won't hear it over ourselves
				if ( GetOuter()->GetExpresser()->GetTimeSpeechCompleteWithoutDelay() > gpGlobals->curtime )
					flVolume *= 0.5f;
			}

			float flDistToSoundSqr = (GetOuter()->EarPosition() - pSound->GetSoundOrigin()).LengthSqr();
			if ( flDistToSoundSqr > Square( flVolume ) )
			{
				return false;
			}
		}
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::QuerySeeEntity( CBaseEntity *pEntity )
{
	NPC_STATE eNPCState = GetOuter()->GetState();
	if (eNPCState == NPC_STATE_IDLE || eNPCState == NPC_STATE_ALERT || eNPCState == NPC_STATE_COMBAT || eNPCState == NPC_STATE_SCRIPT)
	{
		// Always see allies
		if ( GetOuter()->IRelationType( pEntity ) == D_LI )
			return true;

		if ( pEntity == GetEnemy() )
		{
			AI_EnemyInfo_t *pMemory = GetOuter()->GetEnemies()->Find( pEntity );
			if ( pMemory )
			{
				// If the enemy is where we would expect them to be, just always see them
				float flDistToLKPSqr = (pEntity->GetAbsOrigin() - pMemory->vLastKnownLocation).LengthSqr();
				if ( flDistToLKPSqr < Square( AI_STEALTH_ENEMY_LKP_ALWAYS_SEE_DIST ) )
					return true;
			}
		}

		// When idle, have a smaller view cone
		Vector vecLookPos, vecLookDir;
		GetStealthLookVectors( vecLookPos, vecLookDir );

		Vector vecDelta = pEntity->EyePosition() - vecLookPos;
		float flDot = vecDelta.Normalized().Dot( vecLookDir );

		if ( pEntity->IsPlayer() && gpGlobals->curtime - GetOuter()->GetLastPlayerDamageTime() < 1.0f )
		{
			if ( !EvalAlertLevel( pEntity, vecDelta, flDot ) )
				return false;

			return true;
		}

		if (!pEntity->IsPlayer() || gpGlobals->curtime - ToBasePlayer( pEntity )->MuzzleFlashTime() > 0.5)
		{
			// Also see less of the stuff above us (unless we hear a sound near this entity)
			if ( GetOuter()->HasCondition( COND_HEAR_COMBAT ) || GetOuter()->HasCondition( COND_HEAR_PLAYER ) || GetOuter()->HasCondition( COND_HEAR_WORLD ) )
			{
				CSound *pSound = GetOuter()->GetBestSound( ALL_SOUNDS );
				if (!pSound || (pSound->GetSoundOrigin() - pEntity->GetAbsOrigin()).Length() > pSound->Volume() || (g_hStealthManager && g_hStealthManager->GetSoundGrace() > gpGlobals->curtime))
				{
					float flEyeZDiff = abs( vecLookPos.z - GetOuter()->EyePosition().z );
					flDot -= (flEyeZDiff / (eNPCState == NPC_STATE_IDLE ? AI_FOV_Z_DIFF_THRESHOLD_IDLE : AI_FOV_Z_DIFF_THRESHOLD_ALERT));
				}
				//else
				//	printl( "Not changing sensing Z due to hearing sound" )
			}
		}

		if (flDot > GetDotToSee(eNPCState))
		{
			// Check if a hull trace can see the pEntity
			float flDist = vecDelta.Length() / AI_SIGHT_BOX_MAX_DIST;
			float flDim = flDist * (eNPCState == NPC_STATE_IDLE ? AI_SIGHT_BOX_MAX_DIMS_IDLE : AI_SIGHT_BOX_MAX_DIMS_ALERT);
			Vector vecHullMaxs = Vector( flDim, flDim, flDim );
			Vector vecHullMins = Vector( -flDim, -flDim, -flDim );

			trace_t tr;
			Vector vecEndPos = pEntity->EyePosition() + Vector( 0, 0, flDim );
			UTIL_TraceHull( vecLookPos, vecEndPos, vecHullMins, vecHullMaxs, MASK_BLOCKLOS_AND_NPCS, GetOuter(), COLLISION_GROUP_NONE, &tr );
			if (tr.fraction != 1.0 && tr.m_pEnt != pEntity)
			{
				/*if ( g_debug_stealth_senses.GetBool() )
				{
					if ( ( g_debug_stealth_senses.GetInt() == 1 && pEntity->IsPlayer() ) || ( g_debug_stealth_senses.GetInt() == 2 && !pEntity->IsPlayer() ) )
						EntityPrint( GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_SIGHT ), Color( 128, 255, 255 ), 4.0f,
							"Test sight debug" );
				}*/

				return false;
			}
		}
		else
		{
			return false;
		}

		bool bCuriousObject = false;

		if ( g_hStealthManager )
		{
			// Check to see if it's in an area
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForEntity( pEntity );
			if ( pArea )
			{
				if ( !ShouldSeeInArea( pEntity, pArea ) )
					return false;
			}

			if ( IsCuriousObject( pEntity ) )
			{
				// Only see curious objects if they're in a suspicious place
				if ( !g_hStealthManager->ShouldSeeObject( pEntity ) )
				{
					if ( IsCuriousObjectMoving( pEntity ) )
					{
						// If the prop isn't in a suspicious place, then we can still see it if it's moving
						// Add a context so that we know why we were interested in case it stops moving by the time we're fully alert
						pEntity->AddContext( "obj_saw_moved", "1", gpGlobals->curtime + 2.0f );
					}
					else
					{
						return false;
					}
				}

				bCuriousObject = true;
			}
		}

		if ( pEntity->GetWaterLevel() != WL_NotInWater )
		{
			if ( !ShouldSeeInWater( pEntity, pEntity->GetWaterLevel() ) )
				return false;
		}

		// Alert level - NPCs don't see hostiles or objects unless they're alert
		if ((GetOuter()->IRelationType( pEntity ) < D_LI && pEntity->Classify() != CLASS_BULLSEYE) || bCuriousObject)
		{
			if ( !EvalAlertLevel( pEntity, vecDelta, flDot ) )
				return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if (!GetOuter()->IsAlive() && GetOuter()->GetState() == NPC_STATE_IDLE)
	{
		// UNDONE: Do not inform if we just died while idle
		//printl("Died while idle")
		//return false;
	}
	else if (pInformer && pInformer != GetOuter() && pInformer->IsNPC() && !GetEnemies()->Find(pEnemy) && pInformer->MyNPCPointer()->GetEnemies()->Find(pEnemy) && GetOuter()->GetState() != NPC_STATE_COMBAT)
	{
		// Don't update memory yet if we've only been engaged for a couple seconds *and* the pInformer isn't visible
		if (gpGlobals->curtime - pInformer->MyNPCPointer()->GetEnemies()->TimeAtFirstHand(pEnemy) < AI_SQUAD_ALERT_DELAY && (!GetOuter()->FVisible(pInformer) || !GetOuter()->FInViewCone(pInformer)))
		{
			variant_t var;
			var.SetVector3D( position );
			g_EventQueue.AddEvent( GetOuter(), "UpdateInformedStealthMemory", var, AI_SQUAD_ALERT_DELAY, pInformer, pInformer );
			return false;
		}
		else if (GetOuter()->GetSquad() && pEnemy && pEnemy->IsAlive() && pEnemy->IsPlayer())
		{
			if (g_hStealthManager)
				g_hStealthManager->SquadSawPlayer( GetOuter(), GetOuter()->GetSquad() );
		}
	}
	else if ((!pInformer || pInformer == GetOuter()))
	{
		if (GetOuter()->GetExpresser() && GetOuter()->GetExpresser()->IsSpeaking() && CompareConcepts(GetOuter()->GetExpresser()->GetLastSpokeConcept(), TLK_STARTCOMBAT))
		{
			if (g_hStealthManager && !g_hStealthManager->IsStealthLevel(STEALTH_LEVEL_LOUD))
			{
				// Hear shouting regardless of squad info
				InsertStealthSound( SOUND_COMBAT | SOUND_CONTEXT_REACT_TO_SOURCE, position, 256, 1.0, pEnemy, SOUNDENT_CHANNEL_STEALTH_DISCOVERED_ENEMY, GetOuter() );
			
				CBaseEntity *pNearNPC = gEntList.FindEntityInSphere( NULL, GetAbsOrigin(), 128.0f );
				while (pNearNPC)
				{
					if (pNearNPC->IsAlive() && pNearNPC->IsNPC() && GetOuter()->IRelationPriority(pNearNPC) == D_LI && pNearNPC->MyNPCPointer()->GetSquad() != GetOuter()->GetSquad())
					{
						pNearNPC->MyNPCPointer()->UpdateEnemyMemory( pEnemy, position, GetOuter() );
					}
				
					pNearNPC = gEntList.FindEntityInSphere( pNearNPC, GetAbsOrigin(), 128.0f );
				}
			}
		}
	}
	else if (pInformer)
	{
		if (pInformer->IsPlayer())
		{
			// Don't update from cloaked player sounds
			// UNDONE: unless we're not moving
			//if (GetOuter()->IsMoving() && pInformer->GetScriptScope().m_flCloakFactor > 0.5)
			//if (pInformer->GetScriptScope().m_flCloakFactor > 0.5)
			if (!GetOuter()->QuerySeeEntity(pInformer))
				return false;
		}
		else if (GetOuter()->GetSquad() && pEnemy && pEnemy->IsAlive() && pEnemy->IsPlayer())
		{
			if (g_hStealthManager)
				g_hStealthManager->SquadSawPlayer( GetOuter(), GetOuter()->GetSquad() );
		}
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::OnStateChange( int eNPCState )
{
	if (g_hStealthManager)
		g_hStealthManager->CheckStillAlert( GetOuter() );
	
	if (eNPCState == NPC_STATE_COMBAT)
	{
		StopRunningActBusy();

		if (m_AlertLevels.Count() > 0)
		{
			// Disengage alert level AI
			// TODO: Investigate alertness levels for players when distracted by combat with NPCs
			CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
			if (pPlayer)
			{
				CEZ2_Player *pEZ2Player = static_cast<CEZ2_Player *>(pPlayer);
				pEZ2Player->AlertLevelEngageEnemy( GetOuter(), GetOuter()->GetEnemy() );
			}

			for ( int i = m_AlertLevels.Count()-1; i >= 0; i-- )
			{
				// Have to check for alert level here because a squadmate could've been the one to spot them
				if (m_AlertLevels[i].hTarget == GetEnemy() && m_AlertLevels[i].flLevel == 1.0f)
				{
					OnSpotEnemyFromAlert( m_AlertLevels[i].hTarget, i );
				}

				m_AlertLevels.Remove( i );
			}
		
			m_flNextAlertLevelThink = gpGlobals->curtime + 1.0f;
		}
	}
	else if (GetOuter()->GetState() == NPC_STATE_ALERT)
	{
		m_flNextAlertLevelThink = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsRunningActBusy()
{
	CAI_ActBusyBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		return pBehavior->IsRunning();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsRunningStartActBusy()
{
	CAI_ActBusyBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		return pBehavior->IsStartBusying();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::StopRunningActBusy()
{
	CAI_ActBusyBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		if (pBehavior->GetActBusyGoal() && !pBehavior->GetActBusyGoal()->IsActive())
		{
			// Forced actbusy
			pBehavior->Disable();
		}
		else
		{
			// Procedural busy, just stop so that we can potentially resume later
			pBehavior->StopBusying();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsStealthSound( CSound *pSound )
{
	if ( !pSound )
		return false;

	return IsStealthSound( pSound->SoundChannel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsCalmStealthSound( CSound *pSound )
{
	if ( !pSound )
		return false;

	return IsCalmStealthSound( pSound->SoundChannel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsExclusiveStealthSound( CSound *pSound )
{
	if ( !pSound )
		return false;

	return IsExclusiveStealthSound( pSound->SoundChannel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldStayAtSound( CSound *pSound )
{
	if ( !pSound )
		return false;

	int soundChannel = pSound->SoundChannel();
	switch (soundChannel)
	{
		case SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY:
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthSenses::GetSoundStopDistance( CSound *pSound )
{
	if ( !pSound )
		return 0.0f;

	int soundChannel = pSound->SoundChannel();
	switch (soundChannel)
	{
		case SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY:
		case SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT:
		case SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING:
		case SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS:
			{
				if ( pSound->m_hOwner )
				{
					// If it's alive, it probably moved by now
					if ( pSound->m_hOwner->IsAlive() )
						return 0.0f;

					// Try to use the subject's approximate size
					return pSound->m_hOwner->BoundingRadius() + 48.0f;
				}

				// Otherwise fall through
			}

		case SOUNDENT_CHANNEL_STEALTH_PROP_BREAK:
		case SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK:
			return 32.0f;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CAI_StealthSenses::GetStealthSoundChannelName( int nSoundChannel )
{
	if ( nSoundChannel < SOUNDENT_CHANNEL_STEALTH_FIRST || nSoundChannel > SOUNDENT_CHANNEL_STEALTH_LAST )
		return "";

	return g_pszStealthSoundChannels[nSoundChannel - SOUNDENT_CHANNEL_STEALTH_FIRST];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::GetStealthLookVectors( Vector &vecPos, Vector &vecDir )
{
	vecPos = GetOuter()->EyePosition();

	// Head direction is more communicative than eye direction
	vecDir = GetOuter()->HeadDirection3D();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthSenses::GetDotToSee( int eNPCState )
{
	return eNPCState == NPC_STATE_IDLE ? AI_FOV_IDLE : AI_FOV_ALERT;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::HasStealthFlags( int iFlags ) const
{
	return GetOuter()->GetStealthFlags() & iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	// Note that global stealth criteria is handled within CAI_StealthManager::ModifyOrAppendCriteria().

	StealthSquadInfo_t *pSquadInfo = GetStealthSquadInfo();
	if ( pSquadInfo )
	{
		// Overrides the criterion in CAI_BaseNPC
		set.AppendCriteria( "squadmates", UTIL_VarArgs( "%i", g_hStealthManager->GetKnownLivingSquadMembers( GetStealthSquadInfo() ) ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldSeeInArea( CBaseEntity *pEntity, CTriggerStealthArea *pArea )
{
	if ( pArea->IsHiddenTo( GetOuter() ) )
		return ShouldSeeInDark( pEntity );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldSeeInDark( CBaseEntity *pEntity )
{
	// If the player just used their guns, then see them
	if (pEntity->IsPlayer() && ToBasePlayer( pEntity )->GetTimeSinceWeaponFired() <= 0.5f)
		return true;

	// Check if we hear a sound near this entity
	CSound *pSound = GetOuter()->GetBestSound( ALL_SOUNDS );
	if (pSound)
	{
		float flSoundDistToEntitySqr = (pEntity->EyePosition() - pSound->GetSoundOrigin()).LengthSqr();
		if (flSoundDistToEntitySqr < Square( AI_SIGHT_DARKNESS_MAX_SOUND_DIST ))
			return true;
	}

	NPC_STATE eNPCState = GetOuter()->GetState();
	if (eNPCState == NPC_STATE_IDLE)
	{
		return false;
	}
	//else if (state == NPC_STATE_ALERT)
	//{
	//	local flDist = (self.EyePosition() - enemy.EyePosition()).Length()
	//	if (flDist > AI_SIGHT_DARKNESS_MAX_DIST_ALERT)
	//		return false;
	//}
	else if (eNPCState == NPC_STATE_COMBAT)
	{
		// If this isn't our enemy, don't pay attention
		if ( pEntity != GetEnemy() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldSeeInWater( CBaseEntity *pEntity, int nWaterLevel )
{
	// If the player just used their guns, then see them
	if (pEntity->IsPlayer() && ToBasePlayer( pEntity )->GetTimeSinceWeaponFired() <= 0.5f)
		return true;

	// Check if we hear a sound near this entity
	CSound *pSound = GetOuter()->GetBestSound( ALL_SOUNDS );
	if (pSound)
	{
		float flSoundDistToEntitySqr = (pEntity->EyePosition() - pSound->GetSoundOrigin()).LengthSqr();
		if (flSoundDistToEntitySqr < Square( AI_SIGHT_DARKNESS_MAX_SOUND_DIST ))
			return true;
	}

	NPC_STATE eNPCState = GetOuter()->GetState();
	if (eNPCState == NPC_STATE_IDLE)
	{
		return false;
	}
	else if (eNPCState == NPC_STATE_ALERT)
	{
		float flDistSqr = (GetOuter()->EyePosition() - pEntity->EyePosition()).LengthSqr();
		if (flDistSqr > Square(AI_SIGHT_DARKNESS_MAX_DIST_ALERT))
			return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldInvestigateSounds()
{
	CSound *pSound = GetOuter()->GetBestSound( ALL_SOUNDS );
	if (pSound)
	{
		if (pSound->SoundType() & (SOUND_BULLET_IMPACT | SOUND_PLAYER | SOUND_WORLD) || pSound->SoundChannel() == SOUNDENT_CHANNEL_INJURY)
		{
			// Just face it if we can see it and it's close enough
			if ( (GetAbsOrigin() - pSound->GetSoundReactOrigin()).LengthSqr() < Square( 128.0f ) )
			{
				CBaseEntity *pBlocker = NULL;
				if (GetOuter()->FVisible( pSound->GetSoundOrigin(), MASK_BLOCKLOS, &pBlocker ) || (pBlocker && pBlocker == pSound->m_hOwner))
					return false;
			}
		}
		else if (pSound->SoundChannel() == SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS)
		{
			// UNDONE: Only investigate if our alert level is high enough
			/*float flThreshold = 0.5f;
			if ( g_hStealthManager )
			{
				switch ( g_hStealthManager->GetStealthLevel() )
				{
					case STEALTH_LEVEL_GUARD:
						flThreshold = 0.35f;
						break;
					case STEALTH_LEVEL_TENSE:
						flThreshold = 0.2f;
						break;
				}
			}

			if (pSound->m_hOwner && pSound->m_hOwner->IsCombatCharacter() && GetAlertLevelForTarget( pSound->m_hOwner ) < flThreshold)
				return false;*/
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::IsInvestigatingSound()
{
	return GetOuter()->IsCurSchedule( SCHED_INVESTIGATE_SOUND );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::BuildScheduleTestBits()
{
	if (IsRunningStartActBusy())
	{
		// Ensure that combat will interrupt us when we're moving
		GetOuter()->SetCustomInterruptCondition( COND_SEE_ENEMY );
		GetOuter()->SetCustomInterruptCondition( COND_HEAR_COMBAT );
	}
	else if (GetOuter()->IsCurSchedule( SCHED_INVESTIGATE_SOUND ) || GetOuter()->IsCurSchedule( SCHED_ALERT_FACE_BESTSOUND ))
	{
		CSound *pBestCombatSound = GetOuter()->HasCondition( COND_HEAR_COMBAT ) ? GetOuter()->GetBestSound( SOUND_COMBAT ) : NULL;
		//if (pBestCombatSound && IsStealthSound(pBestCombatSound))
		{
			// Used to ensure we are interrupted by combat
			GetOuter()->SetCustomInterruptCondition( COND_HEAR_BULLET_IMPACT );
			//printl("Assigning interrupt condition")
		}

		CSound *pBestWorldSound = GetOuter()->HasCondition( COND_HEAR_WORLD ) ? GetOuter()->GetBestSound( SOUND_WORLD ) : NULL;
		if (!pBestWorldSound && GetOuter()->HasCondition( COND_HEAR_PLAYER ))
			pBestWorldSound = GetOuter()->GetBestSound( SOUND_PLAYER );

		if (pBestWorldSound && !pBestCombatSound)
		{
			// Make sure we're also interrupted when investigating world sounds
			GetOuter()->SetCustomInterruptCondition( COND_HEAR_COMBAT );
			GetOuter()->SetCustomInterruptCondition( COND_HEAR_BULLET_IMPACT );
		}
	}
	else if (GetOuter()->IsCurSchedule( SCHED_PATROL_WALK, false ) || GetOuter()->IsCurSchedule( SCHED_ALERT_WALK, false ))
	{
		GetOuter()->SetCustomInterruptCondition( COND_HEAR_COMBAT );
		GetOuter()->SetCustomInterruptCondition( COND_HEAR_BULLET_IMPACT );
	}

	if ( GetOuter()->ConditionInterruptsCurSchedule( COND_HEAR_PLAYER ) )
	{
		// Let world sounds also interrupt it
		GetOuter()->SetCustomInterruptCondition( COND_HEAR_WORLD );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::MaintainAlertLevels()
{
	bool bInCombatWithEnemy = false;
	if ( GetEnemy() && GetEnemy()->Classify() != CLASS_BULLSEYE && GetOuter()->HasCondition( COND_SEE_ENEMY ) )
	{
		bInCombatWithEnemy = true;
		
		//m_flNextAlertLevelThink = gpGlobals->curtime + 1.0f;
		//return;
	}

	bool bCheckingPlayer = false;
	for ( int i = m_AlertLevels.Count()-1; i >= 0; i-- )
	{
		if ( !m_AlertLevels[i].hTarget || (!m_AlertLevels[i].hTarget->IsAlive() && !(m_AlertLevels[i].hTarget->GetFlags() & FL_OBJECT))
			/*|| (m_AlertLevels[i].flLevel == 0.0f && gpGlobals->curtime - m_AlertLevels[i].flLastUpdate < ALERT_LEVEL_FORGET_TIME)*/ )
		{
			m_AlertLevels.Remove( i );
			continue;
		}

		if ( bInCombatWithEnemy )
		{
			// If we're already in combat, then dynamically mark alert levels as spotted
			if (m_AlertLevels[i].hTarget == GetEnemy() || m_AlertLevels[i].flLevel >= 1.0f)
			{
				if (m_AlertLevels[i].hTarget->IsPlayer())
				{
					CEZ2_Player *pEZ2Player = static_cast<CEZ2_Player *>(m_AlertLevels[i].hTarget.Get());
					pEZ2Player->AlertLevelEngageEnemy( GetOuter(), m_AlertLevels[i].hTarget );
				}

				m_AlertLevels.Remove( i );
				continue;
			}
		}

		float flDecayAmt = 0.0f;
		if (m_AlertLevels[i].flLevel == m_AlertLevels[i].flPrevLevel && gpGlobals->curtime - m_AlertLevels[i].flLastUpdate > 0.5f)
			flDecayAmt = 1.0f;

		if ( flDecayAmt > 0.0f && ( IsInvestigatingSound() || GetOuter()->IsCurSchedule( SCHED_ALERT_FACE_BESTSOUND ) ) )
		{
			// Don't decay if we're currently investigating this, unless we're returning to our previous position
			if ( !GetOuter()->GetNavigator()->IsGoalSet() || GetOuter()->GetNavigator()->GetGoalPos() != GetOuter()->m_vecLastPosition )
			{
				if ( GetLastSoundChannel() == SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS )
					flDecayAmt = 0.0f;
				else
					flDecayAmt *= 0.1f;
			}
		}
	
		// Decay if it hasn't changed
		if ( flDecayAmt > 0.0f )
		{
			if (GetOuter()->GetState() != NPC_STATE_IDLE)
			{
				flDecayAmt *= 0.6f;
			}

			m_AlertLevels[i].flLevel -= ALERT_LEVEL_DECAY * flDecayAmt;
			
			if (m_AlertLevels[i].flLevel < 0.0)
				m_AlertLevels[i].flLevel = 0.0;
		}

		if (!bCheckingPlayer && m_AlertLevels[i].hTarget->IsPlayer())
		{
			if (m_AlertLevels[i].flPrevLevel > 0.0)
			{
				bCheckingPlayer = true;

				CEZ2_Player *pEZ2Player = static_cast<CEZ2_Player *>(m_AlertLevels[i].hTarget.Get());
				pEZ2Player->AlertLevelUpdate( GetOuter(), m_AlertLevels[i].flLevel, GetAlertSourceType() );
			}
		}
	
		m_AlertLevels[i].flPrevLevel = m_AlertLevels[i].flLevel;
	}

	GetOuter()->SetUsingHighFrequencyLook( bCheckingPlayer && !bInCombatWithEnemy && ai_stealth_alertness_high_freq_look.GetBool() );

	if ( m_AlertLevels.Count() > 0 )
	{
		if ( g_debug_stealth_alertness.GetBool() )
		{
			DebugPrintAlertLevels();
		}

		if ( bCheckingPlayer )
			m_flNextAlertLevelThink = gpGlobals->curtime + TICK_INTERVAL;
		else
			m_flNextAlertLevelThink = gpGlobals->curtime + 0.1f;
	}
	else
		m_flNextAlertLevelThink = gpGlobals->curtime + 0.5f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::EvalAlertLevel( CBaseEntity *pTarget, const Vector &vecDelta, float flDot )
{
	if (GetEnemy() == pTarget && gpGlobals->curtime - GetOuter()->GetLastDamageTime() < 1.0f)
	{
		return true;
	}

	float flAlertLevelIncrease = 0.0f;
	
	int idx = 0;
	for (; idx < m_AlertLevels.Count(); idx++)
	{
		if (m_AlertLevels[idx].hTarget == pTarget)
		{
			if ( m_AlertLevels[idx].flLastUpdate == gpGlobals->curtime )
			{
				// Use previously calculated value
				return (m_AlertLevels[idx].flLevel >= 1.0f) ? true : false;
			}

			flAlertLevelIncrease += m_AlertLevels[idx].flTotalLevel / ALERT_LEVEL_MAX_LEVEL;
			break;
		}
	}
	
	if (idx == m_AlertLevels.Count())
	{
		// Create new alert level for this target
		idx = m_AlertLevels.AddToTail();
		m_AlertLevels[idx].hTarget = pTarget;
		m_AlertLevels[idx].flLevel = 0.0f;
		m_AlertLevels[idx].flPrevLevel = 0.0f;
		m_AlertLevels[idx].flTotalLevel = 0.0f;
		m_AlertLevels[idx].flStartTime = m_AlertLevels[idx].flLastUpdate = gpGlobals->curtime;

		// Maintain it immediately
		m_flNextAlertLevelThink = gpGlobals->curtime;
	}

	// If we were just attacked, become alerted quickly
	if ( gpGlobals->curtime - GetOuter()->GetLastDamageTime() < 2.0f && pTarget->IsCombatCharacter() )
		flAlertLevelIncrease += 0.75f;
	
	// Distance
	//{
		flAlertLevelIncrease += (1.0 - (vecDelta.LengthSqr() / Square(ALERT_LEVEL_INCREASE_DISTANCE)));
		flAlertLevelIncrease *= flDot;
		if (pTarget->IsPlayer())
			flAlertLevelIncrease *= 0.5;
		
		//printl("Distance alert: " + flAlertLevelIncrease)
		
		if (flAlertLevelIncrease < 0.0)
			flAlertLevelIncrease = 0.0;
	//}
	
	Vector vecVelocity = pTarget->GetAbsVelocity();
	if (!pTarget->IsCombatCharacter() && pTarget->VPhysicsGetObject())
	{
		pTarget->VPhysicsGetObject()->GetVelocity( &vecVelocity, NULL );

		flAlertLevelIncrease *= (pTarget->BoundingRadius() / ALERT_LEVEL_RADIUS_AVG);
	}
	
	if (vecVelocity.LengthSqr() > 0)
	{
		flAlertLevelIncrease += (vecVelocity.LengthSqr() / Square(ALERT_LEVEL_INCREASE_SPEED)) * 0.2;
		//printl("Velocity alert: " + ((vecVelocity.LengthSqr() / (ALERT_LEVEL_INCREASE_SPEED*ALERT_LEVEL_INCREASE_SPEED)) * 0.2))
	}
	
	if (pTarget->IsPlayer())
	{
		// TODO: Cloak factor
		// Make the laser actually reduce player's cloak instead of doing this
		/*if (pTarget->GetScriptScope().m_flCloakFactor > 0.0)
		{
			local cloakFactor = pTarget->GetScriptScope().m_flCloakFactor;
			if (rawin("TargetCrossingLaser") && TargetCrossingLaser(target))
			{
				// If we have a laser the player is crossing, act like cloak is at less effectiveness
				cloakFactor *= 0.4;
			}
		
			flAlertLevelIncrease *= (1.0 - cloakFactor);
		}*/

		CBasePlayer *pPlayer = ToBasePlayer( pTarget );
		if ( pPlayer )
		{
			if (gpGlobals->curtime - pPlayer->MuzzleFlashTime() < 0.5)
				flAlertLevelIncrease += 0.5;
			
			if (pPlayer->GetUseEntity())
				flAlertLevelIncrease += 0.1;

			/*CEZ2_Player *pEZ2Player = static_cast<CEZ2_Player *>(pPlayer);
			if ( pEZ2Player->GetCloakFactor() > 0.0f )
			{
				flAlertLevelIncrease *= (1.0 - pEZ2Player->GetCloakFactor());
			}*/
		}
			
		if (pTarget->GetFlags() & FL_DUCKING)
			flAlertLevelIncrease *= 0.5;
		
		if (pTarget->GetWaterLevel() > 0)
		{
			switch (pTarget->GetWaterLevel())
			{
				case 2:		flAlertLevelIncrease *= 0.8; break; // Waist
				case 3:		flAlertLevelIncrease *= 0.4; break; // Eyes (fully submerged)
			}
		}

		if ( g_hStealthManager )
		{
			// Check to see if it's in an area
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForEntity( pTarget );
			if ( pArea )
			{
				flAlertLevelIncrease *= pArea->GetAlertLevelMultiplier();
			}
		}
	}
	
	// Slower if idle
	if (GetOuter()->GetState() == NPC_STATE_IDLE && (!g_hStealthManager || g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET )))
		flAlertLevelIncrease *= 0.5;
	else if (GetNumBodiesFound() <= 0)
		flAlertLevelIncrease *= 0.75;

	// Use enemy memory if we have it
	AI_EnemyInfo_t *pMemory = GetOuter()->GetEnemies()->Find( pTarget );
	if ( pMemory )
	{
		// Vary based on whether the enemy is where we would expect them to be
		float flDistToLKPSqr = (pTarget->GetAbsOrigin() - pMemory->vLastKnownLocation).LengthSqr();
		flAlertLevelIncrease *= (2.0f - RemapValClamped( flDistToLKPSqr, Square( 128.0f ), Square( 4000.0f ), 0.0f, 1.5f ));
	}
		
	// Civilians less ready
	if (HasStealthFlags(STEALTH_F_SLOW))
		flAlertLevelIncrease *= 0.75;
		
	// Vary by difficulty
	switch (g_pGameRules->GetSkillLevel())
	{
		case 1:		flAlertLevelIncrease *= ALERT_LEVEL_MULT_EASY; break;
		case 2:		flAlertLevelIncrease *= ALERT_LEVEL_MULT_NORM; break;
		case 3:		flAlertLevelIncrease *= ALERT_LEVEL_MULT_HARD; break;
	}

	// Account for high frequency look times
	if (GetOuter()->IsUsingHighFrequencyLook())
		flAlertLevelIncrease *= (TICK_INTERVAL / .5f);
	
	if (flAlertLevelIncrease > 0.0)
	{
		m_AlertLevels[idx].flTotalLevel += flAlertLevelIncrease;
		m_AlertLevels[idx].flLevel += flAlertLevelIncrease;
		if (m_AlertLevels[idx].flLevel > 1.0)
			m_AlertLevels[idx].flLevel = 1.0;
		m_AlertLevels[idx].flLastUpdate = gpGlobals->curtime;
	
		if (pTarget && m_AlertLevels[idx].flLevel > GetAlertFaceThreshold(pTarget, idx))
		{
			if (!GetOuter()->ClassMatches("generic_actor"))
			{
				if (ShouldEmitSawSuspicious( pTarget, idx ))
				{
					// Make a sound to get us to turn and look
					Vector vecSoundPos = pTarget->GetAbsOrigin() + pTarget->GetViewOffset();
					InsertStealthSound( SOUND_COMBAT, vecSoundPos, 2048, 2.0f, pTarget, SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS, GetOuter() );
				}
				
				GetOuter()->AddLookTarget( pTarget, 0.8f, 4.0f, 2.0f );
			}
			else
			{
				// Just manually set NPC state for actors
				GetOuter()->SetIdealState( NPC_STATE_ALERT );
			}
		}

		if (ai_stealth_alertness_cap.GetFloat() > 0.0f && m_AlertLevels[idx].flLevel > ai_stealth_alertness_cap.GetFloat())
		{
			m_AlertLevels[idx].flLevel = ai_stealth_alertness_cap.GetFloat();
		}
	
		if (m_AlertLevels[idx].flLevel >= 1.0)
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const AlertLevel_t &CAI_StealthSenses::GetAlertLevel( int i ) const
{
	Assert( i >= 0 && i < m_AlertLevels.Count() );
	return m_AlertLevels[i];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const int CAI_StealthSenses::GetAlertLevelCount() const
{
	return m_AlertLevels.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const float CAI_StealthSenses::GetAlertLevelForTarget( CBaseEntity *pTarget ) const
{
	for (int i = 0; i < m_AlertLevels.Count(); i++)
	{
		if (m_AlertLevels[i].hTarget == pTarget)
			return m_AlertLevels[i].flLevel;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthSenses::GetHighestAlertLevel() const
{
	float flHighestAlertLevel = 0.0f;
	for (int i = 0; i < m_AlertLevels.Count(); i++)
	{
		if (m_AlertLevels[i].flLevel > flHighestAlertLevel)
		{
			flHighestAlertLevel = m_AlertLevels[i].flLevel;
			break;
		}
	}

	return flHighestAlertLevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSenses::GetAlertSourceType() const
{
	if ( HasStealthFlags( STEALTH_F_EXTRA_DANGER ) )
		return ALERT_SOURCE_TYPE_EXTRA;

	return ALERT_SOURCE_TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthSenses::GetAlertFaceThreshold( CBaseEntity *pTarget, int i ) const
{
	float flThreshold = 0.34f;
	if ( g_hStealthManager )
	{
		switch ( g_hStealthManager->GetStealthLevel() )
		{
			case STEALTH_LEVEL_GUARD:
				flThreshold = 0.5f;
				break;
			case STEALTH_LEVEL_TENSE:
				flThreshold = 0.67f;
				break;
		}
	}

	return flThreshold;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSenses::ShouldEmitSawSuspicious( CBaseEntity *pTarget, int i )
{
	if (GetOuter()->HasCondition( COND_HEAR_COMBAT ))
		return false;

	return true;
}

//=============================================================================
// Debugging
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::EntityPrint( int iLine, Color clr, float flDuration, const char *pMsg, ... )
{
	// Format the string.
	char str[128];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	if (GetOuter()->m_debugOverlays & OVERLAY_TEXT_BIT)
		iLine -= ( m_AlertLevels.Count() - 8 );

	GetOuter()->EntityText( iLine, str, flDuration, clr[0], clr[1], clr[2], 255 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSenses::GetOffsetForDebugType( int iStartLine )
{
	int iReturnLine = iStartLine;

	switch ( iStartLine )
	{
		case STEALTH_SENSE_DEBUG_LINE_INVESTIGATE:
			{
				// Account for alertness levels
				if ( g_debug_stealth_alertness.GetBool() )
					iReturnLine += m_AlertLevels.Count();
			}
			break;
	}

	return iReturnLine;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSenses::DebugPrintAlertLevels()
{
	const float flTextTime = 0.25f;
	int iLine = GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_ALERTNESS );

	for ( int i = 0; i < m_AlertLevels.Count(); i++, iLine++ )
	{
		CBaseEntity *pTarget = m_AlertLevels[i].hTarget;
		if ( !pTarget )
		{
			EntityPrint( iLine, Color( 64, 64, 64 ), flTextTime, "%i [?]:\tInvalid alert target", i );
			continue;
		}

		if ( !pTarget->IsAlive() && !(pTarget->GetFlags() & FL_OBJECT) )
		{
			EntityPrint( iLine, Color( 64, 64, 64 ), flTextTime, "%i [%i : %s]:\tInvalid alert target", i, pTarget->entindex(), pTarget->GetDebugName() );
			continue;
		}

		Color clr( 255, 128, 128 );
		clr[0] = RemapValClamped( m_AlertLevels[i].flLevel, 0.0f, 1.0f, 128.0f, 255.0f );
		clr[1] = RemapValClamped( 1.0f - m_AlertLevels[i].flLevel, 0.0f, 1.0f, 64.0f, 128.0f );
		clr[2] = clr[1];

		EntityPrint( iLine, clr, flTextTime, "%i [%i : %s]:\tLevel %.3f\t(Prev: %.3f)", i, pTarget->entindex(), pTarget->GetDebugName(), m_AlertLevels[i].flLevel, m_AlertLevels[i].flPrevLevel );
	}
}
