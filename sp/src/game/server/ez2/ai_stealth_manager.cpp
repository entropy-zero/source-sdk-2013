//=============================================================================//
//
// Purpose:		AI component dedicated to stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_manager.h"
#include "ai_stealth_senses.h"
#include "ai_stealth_area.h"
#include "ai_stealth_behavior_alarm.h"
#include "ai_stealth_behavior_curious.h"
#include "ai_stealth_behavior_search.h"
#include "ai_basenpc.h"
#include "ai_squad.h"
#include "ai_hint.h"
#include "ai_senses.h"
#include "BasePropDoor.h"
#include "saverestore_utlvector.h"
#include "eventqueue.h"
#include "con_nprint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

extern ISoundEmitterSystemBase *soundemitterbase;

CAI_StealthManager *g_hStealthManager;

ConVar	ai_stealth_obj_min_dist_change( "ai_stealth_obj_min_dist_change", "75" );

ConVar	ai_stealth_area_time_min( "ai_stealth_area_time_min", "120" );
ConVar	ai_stealth_area_time_max( "ai_stealth_area_time_max", "300" );
ConVar	ai_stealth_area_enemydist_min( "ai_stealth_area_enemydist_min", "500" );
ConVar	ai_stealth_area_enemydist_max( "ai_stealth_area_enemydist_max", "1500" );
ConVar	ai_stealth_area_sound_min( "ai_stealth_area_sound_min", "250" );
ConVar	ai_stealth_area_sound_max( "ai_stealth_area_sound_max", "1000" );
ConVar	ai_stealth_area_contaminate_plr_dist( "ai_stealth_area_contaminate_plr_dist", "1500" );
ConVar	ai_stealth_area_contaminate_sound_dist( "ai_stealth_area_contaminate_sound_dist", "750" );

ConVar	g_debug_stealth_show_seen( "g_debug_stealth_show_seen", "0" );
ConVar	g_debug_stealth_show_state( "g_debug_stealth_show_state", "0" );
ConVar	g_debug_stealth_show_areas( "g_debug_stealth_show_areas", "0" );
ConVar	g_debug_stealth_show_squads( "g_debug_stealth_show_squads", "0" );
ConVar	g_debug_stealth_search_weight( "g_debug_stealth_search_weight", "0" );

#define STEALTH_MANAGER_THINK					1.0f
#define SEEN_OBJECT_CHECK_COOLDOWN				5.0f
#define STEALTH_MANAGER_DEBUG_SHOW_DURATION		1.03f

// How much time in the beginning and end of a response to ignore them being interrupted
#define RESPONSE_ALERT_PADDING	0.25f

// Response contexts used by the stealth manager to identify certain entities
// TODO: Actual lists?
#define CONTEXT_PERCEIVABLE			"curious_prop"		// Identifies props managed by the stealth manager
#define CONTEXT_STEALTH_SPEAKING	"stealth_speaking"	// Identifies NPCs who are speaking in such a way that killing them is noticeable
#define CONTEXT_ALARM_DISABLED		"disabled"			// Identifies alarms which have been sabotaged by the player

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( StealthObjectState_t )

	DEFINE_FIELD( hEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( vecLastPosition, FIELD_POSITION_VECTOR ),

	//DEFINE_FIELD( flLastTimeChecked, FIELD_TIME ),
	DEFINE_FIELD( flTimeEnteredArea, FIELD_TIME ),

	//DEFINE_FIELD( bResult, FIELD_BOOLEAN ),
	DEFINE_FIELD( nTimesFound, FIELD_SHORT ),

END_DATADESC();

BEGIN_SIMPLE_DATADESC( StealthInterestPoint_t )

	DEFINE_FIELD( vecOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( flExpireTime, FIELD_TIME ),
	DEFINE_FIELD( nType, FIELD_INTEGER ),

END_DATADESC();

BEGIN_SIMPLE_DATADESC( StealthSquadMemberInfo_t )

	DEFINE_FIELD( hEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( bKnownAlive, FIELD_BOOLEAN ),

	DEFINE_FIELD( vecLastKnownLocation, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( flLastKnownTime, FIELD_TIME ),
	DEFINE_FIELD( hLastInformer, FIELD_EHANDLE ),

	DEFINE_FIELD( iszID, FIELD_STRING ),
	DEFINE_FIELD( nGender, FIELD_INTEGER ),

	DEFINE_FIELD( nDamageType, FIELD_INTEGER ),

END_DATADESC();

BEGIN_SIMPLE_DATADESC( StealthSquadInfo_t )

	DEFINE_FIELD( iszSquadName, FIELD_STRING ),
	DEFINE_FIELD( bAlerted, FIELD_BOOLEAN ),
	DEFINE_UTLVECTOR( m_Members, FIELD_EMBEDDED ),

END_DATADESC();

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CAI_StealthManager )

	//DEFINE_FIELD( m_nStealthLevel, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_nMinStealthLevel, FIELD_INTEGER, "MinStealthLevel" ),

	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_KEYFIELD( m_bCleanupWhenDisabled, FIELD_BOOLEAN, "CleanupWhenDisabled" ),

	DEFINE_FIELD( m_bAlerted, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flSoundGrace, FIELD_FLOAT ),

	DEFINE_UTLVECTOR( m_SeenObjects, FIELD_EMBEDDED ),
	DEFINE_UTLVECTOR( m_SquadInfo, FIELD_EMBEDDED ),
	DEFINE_UTLVECTOR( m_StealthAreas, FIELD_EHANDLE ),

	DEFINE_KEYFIELD( m_bAlarmsEnabled, FIELD_BOOLEAN, "AlarmsEnabled" ),
	DEFINE_FIELD( m_bAlarmRaised, FIELD_BOOLEAN ),
	DEFINE_INPUT( m_nBodiesToRaiseAlarm, FIELD_INTEGER, "SetBodiesToRaiseAlarm" ),

	DEFINE_KEYFIELD( m_bPlayMusic, FIELD_BOOLEAN, "PlayMusic" ),
	DEFINE_KEYFIELD( m_bResumeStealthMusic, FIELD_BOOLEAN, "ResumeStealthMusic" ),
	DEFINE_KEYFIELD( m_iszAlertMusic, FIELD_STRING, "AlertMusic" ),
	DEFINE_FIELD( m_hAlertMusic, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iszStealthMusic, FIELD_STRING, "StealthMusic" ),
	DEFINE_FIELD( m_hStealthMusic, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( StealthManagerThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "SetMinStealthLevelQuiet", InputSetMinStealthLevelQuiet ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetMinStealthLevelGuard", InputSetMinStealthLevelGuard ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetMinStealthLevelTense", InputSetMinStealthLevelTense ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EnableAlarms", InputEnableAlarms ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableAlarms", InputDisableAlarms ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RaiseAlarm", InputRaiseAlarm ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ResetAlarm", InputResetAlarm ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "ForceThisNPCToRaiseAlarm", InputForceThisNPCToRaiseAlarm ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EnableMusic", InputEnableMusic ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableMusic", InputDisableMusic ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetStealthMusic", InputSetStealthMusic ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "SetAlertMusic", InputSetAlertMusic ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnableResumeStealthMusic", InputEnableResumeStealthMusic ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisableResumeStealthMusic", InputDisableResumeStealthMusic ),

	// Internal, not intended for direct use
	DEFINE_INPUTFUNC( FIELD_INTEGER, "NPCHeardSuspiciousSound", InputNPCHeardSuspiciousSound ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "NPCStartedSpeaking", InputNPCStartedSpeaking ),

	DEFINE_OUTPUT( m_OnAlarmRaised, "OnAlarmRaised" ),
	DEFINE_OUTPUT( m_OnAlarmReset, "OnAlarmReset" ),
	DEFINE_OUTPUT( m_OnNPCGoToRaiseAlarm, "OnNPCGoToRaiseAlarm" ),
	DEFINE_OUTPUT( m_OnNPCStopRaiseAlarm, "OnNPCStopRaiseAlarm" ),
	DEFINE_OUTPUT( m_OnSquadAlerted, "OnSquadAlerted" ),
	DEFINE_OUTPUT( m_OnSquadLostPlayer, "OnSquadLostPlayer" ),
	DEFINE_OUTPUT( m_OnAlertEnd, "OnAlertEnd" ),
	DEFINE_OUTPUT( m_OnBodyFound, "OnBodyFound" ),

END_DATADESC();

LINK_ENTITY_TO_CLASS( ai_stealth_manager, CAI_StealthManager );

//-------------------------------------

CAI_StealthManager::CAI_StealthManager()
{
	m_bDisabled = false;
	m_bCleanupWhenDisabled = true;
	m_nMinStealthLevel = STEALTH_LEVEL_QUIET;
}

CAI_StealthManager::~CAI_StealthManager()
{
	if ( g_hStealthManager == this )
		g_hStealthManager = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::Spawn()
{
	BaseClass::Spawn();

	if ( m_iszStealthMusic != NULL_STRING )
	{
		m_hStealthMusic = gEntList.FindEntityByName( NULL, STRING( m_iszStealthMusic ), this );
	}

	if ( m_iszAlertMusic != NULL_STRING )
	{
		m_hAlertMusic = gEntList.FindEntityByName( NULL, STRING( m_iszAlertMusic ), this );
	}

	CheckStealthManagerState();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::OnRestore()
{
	BaseClass::OnRestore();

	CheckStealthManagerState();

	// Hack to fix issues with NPCs hearing sounds on save/restore
	m_flSoundGrace = gpGlobals->curtime + 3.0;

	// Repopulate squads
	for ( int i = 0; i < m_SquadInfo.Count(); i++ )
	{
		m_SquadInfo[i].pSquad = g_AI_SquadManager.FindSquad( m_SquadInfo[i].iszSquadName );
		Assert( m_SquadInfo[i].pSquad );
	}

	// Force a think to get the stealth level
	StealthManagerThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::UpdateOnRemove()
{
	Cleanup();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::StealthManagerThink()
{
	// Evaluate stealth level
	m_nStealthLevel = STEALTH_LEVEL_NONE;

	/*if ( m_bAlerted )
	{
		m_nStealthLevel = STEALTH_LEVEL_LOUD;
	}
	else*/
	{
		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i] == NULL || !ppAIs[i]->IsUsingStealthSenses() || !ppAIs[i]->IsAlive() || !ppAIs[i]->HasCondition( COND_IN_PVS ) )
				continue;
		
			switch ( ppAIs[i]->GetState() )
			{
				case NPC_STATE_IDLE:
				case NPC_STATE_ALERT:
					m_nStealthLevel = STEALTH_LEVEL_QUIET;
					break;

				case NPC_STATE_COMBAT:
					{
						if ( ppAIs[i]->GetEnemy() && ppAIs[i]->GetEnemies()->TimeAtFirstHand( ppAIs[i]->GetEnemy() ) > AI_SQUAD_ALERT_DELAY )
							m_nStealthLevel = STEALTH_LEVEL_LOUD;
						else if ( m_nStealthLevel == STEALTH_LEVEL_NONE )
							m_nStealthLevel = STEALTH_LEVEL_QUIET;
					}
					break;
			}

			// Won't get any higher than this
			if ( m_nStealthLevel == (NUM_STEALTH_LEVELS-1) )
				break;
		}

		if ( m_nStealthLevel != STEALTH_LEVEL_NONE && m_nStealthLevel < m_nMinStealthLevel )
		{
			m_nStealthLevel = m_nMinStealthLevel;
		}
	}

	for ( int i = m_SeenObjects.Count()-1; i >= 0; i-- )
	{
		// Check for invalid objects
		if ( !m_SeenObjects[i].hEntity || m_SeenObjects[i].hEntity->IsMarkedForDeletion() )
		{
			m_SeenObjects.Remove( i );
			continue;
		}

		if ( !m_SeenObjects[i].bResult && ShouldSeeObject( m_SeenObjects[i].hEntity ) )
		{
			// Previously inert object has become visible
			// Could this be the player's doing?
			CBasePlayer *pPlayer = AI_GetSinglePlayer();
			if ( pPlayer && (pPlayer->GetAbsOrigin() - m_SeenObjects[i].hEntity->GetAbsOrigin()).LengthSqr() < Square( 128.0f ) )
			{
				PlayerMovedObject( pPlayer, m_SeenObjects[i].hEntity );
			}
		}
	}

	if ( g_debug_stealth_show_seen.GetBool() )
	{
		char szTemp[128] = { 0 };
		for ( int i = 0; i < m_SeenObjects.Count(); i++ )
		{
			V_snprintf( szTemp, sizeof( szTemp ), "[%i] Last Seen Position", i );
			NDebugOverlay::EntityTextAtPosition( m_SeenObjects[i].vecLastPosition, 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 192, 192 );

			V_snprintf( szTemp, sizeof( szTemp ), "[%i] Actual Position", i );
			NDebugOverlay::EntityTextAtPosition( m_SeenObjects[i].hEntity->GetAbsOrigin(), 1, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 192, 255, 192 );

			V_snprintf( szTemp, sizeof( szTemp ), "-- Last time entered area: %.2f", gpGlobals->curtime - m_SeenObjects[i].flTimeEnteredArea );
			NDebugOverlay::EntityTextAtPosition( m_SeenObjects[i].hEntity->GetAbsOrigin(), 2, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 192, 255, 192 );

			int r, g, b;
			
			if ( ShouldSeeObject( m_SeenObjects[i].hEntity ) )
			{
				r = 255; g = 64; b = 64;
			}
			else if ( m_SeenObjects[i].hEntity->GetFlags() & FL_OBJECT )
			{
				r = 255; g = 255; b = 255;
			}
			else
			{
				r = 128; g = 128; b = 128;
			}

			NDebugOverlay::Circle( m_SeenObjects[i].vecLastPosition, QAngle( 90, 0, 0 ), ai_stealth_obj_min_dist_change.GetFloat(), r, g, b, 255, true, STEALTH_MANAGER_DEBUG_SHOW_DURATION );
		}
	}

	if ( g_debug_stealth_show_state.GetBool() )
	{
		DebugShowStealthState( 5 );
	}

	if ( g_debug_stealth_show_areas.GetBool() )
	{
		char szTemp[128] = { 0 };
		for ( int i = 0; i < m_StealthAreas.Count(); i++ )
		{
			const Vector &vecAreaPos = m_StealthAreas[i]->GetAbsOrigin();

			V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s", i, m_StealthAreas[i]->GetDebugName() );
			NDebugOverlay::EntityTextAtPosition( vecAreaPos, 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 64, 255 );

			V_snprintf( szTemp, sizeof( szTemp ), "- %s", m_StealthAreas[i]->GetAreaContext() );
			NDebugOverlay::EntityTextAtPosition( vecAreaPos, 1, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 64, 255 );

			if ( m_StealthAreas[i]->IsSearchable() )
			{
				V_snprintf( szTemp, sizeof( szTemp ), "- Current searchers: %i (max %i)",
					m_StealthAreas[i]->GetSearcherCount(), m_StealthAreas[i]->GetMaxSearchers() );
				NDebugOverlay::EntityTextAtPosition( vecAreaPos, 2, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 64, 255 );

				V_snprintf( szTemp, sizeof( szTemp ), "- Time since searched: %f",
					m_StealthAreas[i]->GetTimeLastSearched() == -1.0f ? -1.0f : gpGlobals->curtime - m_StealthAreas[i]->GetTimeLastSearched() );
				NDebugOverlay::EntityTextAtPosition( vecAreaPos, 3, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 64, 255 );
			}

			for ( int j = 0; j < m_StealthAreas[i]->GetSearchPointCount(); j++ )
			{
				CAI_Hint *pHint = m_StealthAreas[i]->GetSearchPoint( j );
				if ( pHint )
				{
					V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s", j, pHint->GetDebugName() );
					NDebugOverlay::EntityTextAtPosition( pHint->GetAbsOrigin(), 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 192, 255 );

					NDebugOverlay::Line( vecAreaPos, pHint->GetAbsOrigin(), 255, 192, 255, true, STEALTH_MANAGER_DEBUG_SHOW_DURATION );
				}
			}

			for ( int j = 0; j < m_StealthAreas[i]->GetDoorCount(); j++ )
			{
				CBasePropDoor *pDoor = m_StealthAreas[i]->GetDoor( j );
				if ( pDoor )
				{
					V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s", j, pDoor->GetDebugName() );
					NDebugOverlay::EntityTextAtPosition( pDoor->GetAbsOrigin(), 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 255, 192 );

					V_snprintf( szTemp, sizeof( szTemp ), "- Expected state: %s", m_StealthAreas[i]->IsDoorMeantToBeOpen(j) ? "Open" : "Closed" );
					NDebugOverlay::EntityTextAtPosition( pDoor->GetAbsOrigin(), 1, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 255, 255, 192 );

					NDebugOverlay::Line( vecAreaPos, pDoor->GetAbsOrigin(), 255, 255, 192, true, STEALTH_MANAGER_DEBUG_SHOW_DURATION );
				}
			}

			NDebugOverlay::EntityBounds( m_StealthAreas[i], 255, 64, 255, 16, STEALTH_MANAGER_DEBUG_SHOW_DURATION );
		}

	}

	if ( g_debug_stealth_show_squads.GetBool() )
	{
		char szTemp[128] = { 0 };
		for ( int i = 0; i < m_SquadInfo.Count(); i++ )
		{
			Vector vecAveragePos = vec3_origin;
			for ( int j = 0; j < m_SquadInfo[i].m_Members.Count(); j++)
			{
				if ( j == 0 )
					vecAveragePos = m_SquadInfo[i].m_Members[j].vecLastKnownLocation;
				else
				{
					vecAveragePos.x = Lerp( 0.5f, vecAveragePos.x, m_SquadInfo[i].m_Members[j].vecLastKnownLocation.x );
					vecAveragePos.y = Lerp( 0.5f, vecAveragePos.y, m_SquadInfo[i].m_Members[j].vecLastKnownLocation.y );
					vecAveragePos.z = Lerp( 0.5f, vecAveragePos.z, m_SquadInfo[i].m_Members[j].vecLastKnownLocation.z );
				}
			}

			V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s - %i members",
				i, STRING( m_SquadInfo[i].iszSquadName ), m_SquadInfo[i].m_Members.Count() );
			NDebugOverlay::EntityTextAtPosition( vecAveragePos, 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 224, 255, 224 );

			for ( int j = 0; j < m_SquadInfo[i].m_Members.Count(); j++)
			{
				int r, g, b;
				r = 128; g = 128; b = 128;
				
				if ( m_SquadInfo[i].m_Members[j].hEntity )
				{
					if ( m_SquadInfo[i].m_Members[j].hEntity->IsAlive() )
					{
						r = 0; g = 255; b = 0;
					}
					else
					{
						r = 224; g = 0; b = 0;
					}
				}
				NDebugOverlay::Line( m_SquadInfo[i].m_Members[j].vecLastKnownLocation, vecAveragePos, r, g, b, true, STEALTH_MANAGER_DEBUG_SHOW_DURATION );

				V_snprintf( szTemp, sizeof( szTemp ), "[%i: %i] %s",
					i, j, m_SquadInfo[i].m_Members[j].hEntity ? m_SquadInfo[i].m_Members[j].hEntity->GetDebugName() : "<null>" );
				NDebugOverlay::EntityTextAtPosition( m_SquadInfo[i].m_Members[j].vecLastKnownLocation, 0, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 224, 255, 224 );
				
				V_snprintf( szTemp, sizeof( szTemp ), "-------- %s",
					m_SquadInfo[i].m_Members[j].bKnownAlive ? "Squad thinks alive" : "Squad knows not alive" );
				NDebugOverlay::EntityTextAtPosition( m_SquadInfo[i].m_Members[j].vecLastKnownLocation, 1, szTemp, STEALTH_MANAGER_DEBUG_SHOW_DURATION, 224, 255, 224 );
			}
		}
	}

	SetContextThink( &CAI_StealthManager::StealthManagerThink, gpGlobals->curtime + STEALTH_MANAGER_THINK, "StealthManagerThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::SetMinStealthLevel( StealthLevel_t nStealthLevel )
{
	m_nMinStealthLevel = nStealthLevel;
	StealthManagerThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::CheckStealthManagerState()
{
	if ( m_bDisabled )
	{
		if ( g_hStealthManager == this )
		{
			g_hStealthManager = NULL;
		}
		else if ( g_hStealthManager != NULL )
		{
			// Being taken over by another stealth manager

			if ( m_bAlerted && m_bPlayMusic && m_hAlertMusic )
			{
				// Stop the alert music if it differs from the other's
				if ( g_hStealthManager->GetAlertMusic() != m_hAlertMusic )
					SendMusicInput( m_hAlertMusic, "FadeOut", "3", 0.0, g_hStealthManager );
			}
		}

		if ( m_bCleanupWhenDisabled )
		{
			// Clean up
			Cleanup();
		}

		SetContextThink( NULL, TICK_NEVER_THINK, "StealthManagerThink" );
	}
	else
	{
		// Take over existing stealth manager
		if ( g_hStealthManager != NULL )
		{
			if ( g_hStealthManager != this )
			{
				CAI_StealthManager *pOldManager = g_hStealthManager;
				g_hStealthManager = this;
				pOldManager->Disable();
				pOldManager->CheckStealthManagerState();
			}
		}
		else
		{
			g_hStealthManager = this;
		}

		SetContextThink( &CAI_StealthManager::StealthManagerThink, gpGlobals->curtime, "StealthManagerThink" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::Cleanup()
{
	m_SeenObjects.Purge();
	m_SquadInfo.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::CheckStillAlert( CAI_BaseNPC *pNPC )
{
	if (!m_bAlerted)
		return;

	bool bStillAlert = false;
	for ( int i = 0; i < m_SquadInfo.Count(); i++ )
	{
		if ( m_SquadInfo[i].bAlerted )
		{
			for ( int j = 0; j < m_SquadInfo[i].m_Members.Count(); j++)
			{
				if ( !m_SquadInfo[i].m_Members[j].hEntity || !m_SquadInfo[i].m_Members[j].hEntity->IsAlive() )
					continue;

				CAI_BaseNPC *pNPC = m_SquadInfo[i].m_Members[j].hEntity->MyNPCPointer();
				if ( pNPC && pNPC->GetState() != NPC_STATE_IDLE )
				{
					bStillAlert = true;
					break;
				}
			}
		}
	}

	if (!bStillAlert)
	{
		if (m_bPlayMusic)
		{
			SendMusicInput( m_hAlertMusic, "FadeOut", "3", 0.0, pNPC );
			
			if (m_bResumeStealthMusic)
			{
				SendMusicInput( m_hStealthMusic, "Volume", "10.0" );
			}
		}
			
		m_bAlerted = false;
		m_OnAlertEnd.FireOutput( pNPC, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCFoundBody( CAI_BaseNPC *pNPC, CBaseEntity *pBody )
{
	if ( pNPC->GetState() != NPC_STATE_COMBAT )
	{
		variant_t var;
		g_EventQueue.AddEvent( this, "ShowFoundBodyHint", var, 1.5f, pNPC, pBody );
	}

	SetMinStealthLevel( STEALTH_LEVEL_TENSE );

	m_OnBodyFound.FireOutput( pNPC, pBody );

	UpdateDeadSquadMember( pBody );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCSawPlayer( CAI_BaseNPC *pNPC )
{
	// Unused at the moment
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCKilled( const CTakeDamageInfo &info, CAI_BaseNPC *pNPC, CBaseEntity *pRagdoll, StealthSquadInfo_t *pSquadInfo, StealthSquadMemberInfo_t *pSquadMemberInfo )
{
	if ( IsStealthLevel( STEALTH_LEVEL_LOUD ) )
	{
		// Presume the squad is aware of deaths in combat

		// Still set the info to track the body, but also mark that they're dead
		if (pSquadMemberInfo)
		{
			pSquadMemberInfo->bKnownAlive = false;
			pSquadMemberInfo->hEntity = pRagdoll;
			pSquadMemberInfo->nDamageType = info.GetDamageType();
		}
	}
	else
	{
		// Make our body perceivable by other NPCs
		if ( pRagdoll )
			g_AI_SensedObjectsManager.AddEntity( pRagdoll );

		// Set the info to track the body
		if (pSquadMemberInfo)
		{
			pSquadMemberInfo->hEntity = pRagdoll;
			pSquadMemberInfo->nDamageType = info.GetDamageType();
		}

		if ( pNPC->HasContext( CONTEXT_STEALTH_SPEAKING, "1" ))
		{
			// Died mid-sentence
			InsertStealthSound( SOUND_COMBAT, pNPC->GetAbsOrigin(), 1024, 2.0f, NULL, SOUNDENT_CHANNEL_STEALTH_SPEECH_INTERRUPTED, NULL );
		}
	}

	if ( m_bCleanupWhenDisabled ) // TODO: Dedicated keyvalue?
	{
		// If this is the last living squad member, remove the squad from our memory
		// (can't use GetSquad() directly as it may be cleaned up by this point)
		if ( pSquadInfo )
		{
			bool bRemove = true;
			for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
			{
				if ( pSquadInfo->m_Members[i].bKnownAlive )
				{
					bRemove = false;
					break;
				}
			}

			if ( bRemove )
			{
				for ( int i = 0; i < m_SquadInfo.Count(); i++ )
				{
					if ( &m_SquadInfo[i] == pSquadInfo )
					{
						m_SquadInfo.Remove( i );
						break;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCSilencedAfterSeeingPlayer( CAI_BaseNPC *pNPC )
{
	// Unused at the moment
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCRaisingAlarm( CAI_BaseNPC *pNPC )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "stealth_raising_alarm" );
	if ( event )
	{
		event->SetInt( "npc", pNPC->entindex() );
		gameeventmanager->FireEvent( event );
	}

	m_OnNPCGoToRaiseAlarm.FireOutput( pNPC, pNPC->GetHintNode() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCStoppedRaisingAlarm( CAI_BaseNPC *pNPC, bool bFailed )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "stealth_raising_alarm_stop" );
	if ( event )
	{
		event->SetInt( "npc", pNPC->entindex() );
		gameeventmanager->FireEvent( event );
	}

	m_OnNPCStopRaiseAlarm.FireOutput( pNPC, this );
}

extern bool IsRunningScriptedSceneWithSpeechAndNotPaused( CBaseFlex *pActor, bool bIgnoreInstancedScenes = false );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCStartedSpeaking( CAI_BaseNPC *pNPC, const char *concept, AI_Response *response )
{
	// Not long enough to note
	float flDuration = pNPC->GetExpresser()->GetResponseDuration( response );
	if ( flDuration < (RESPONSE_ALERT_PADDING*2) )
		return;

	if ( response->GetType() == ResponseRules::RESPONSE_SCENE )
	{
		// Don't do this for scenes without speech
		if ( !IsRunningScriptedSceneWithSpeechAndNotPaused( pNPC, false ) )
			return;
	}

	variant_t var;
	var.SetFloat( flDuration );
	g_EventQueue.AddEvent( this, "NPCStartedSpeaking", var, RESPONSE_ALERT_PADDING, pNPC, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputNPCStartedSpeaking( inputdata_t &inputdata )
{
	CAI_BaseNPC *pNPC = inputdata.pActivator ? inputdata.pActivator->MyNPCPointer() : NULL;
	if ( !pNPC || !pNPC->IsAlive() || !pNPC->IsUsingStealthSenses() )
		return;

	StealthSquadMemberInfo_t *pSquadMemberInfo = pNPC->GetStealthSenses()->GetStealthSquadMemberInfo();
	if ( !pSquadMemberInfo )
		return;

	// Update squad info
	UpdateSeenSquadMember( pSquadMemberInfo, pNPC );

	// See CAI_StealthManager::NPCKilled()
	pNPC->AddContext( CONTEXT_STEALTH_SPEAKING, "1", gpGlobals->curtime + inputdata.value.Float() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCHeardSuspiciousSound( CAI_BaseNPC *pNPC, CSound *pSound )
{
	// Make all search areas around the sound dirty
	Vector vecSoundOrigin = pSound->GetSoundReactOrigin();
	ResetAreaSearches( vecSoundOrigin, ai_stealth_area_contaminate_sound_dist.GetFloat() );

	if ( ( g_debug_stealth_show_areas.GetBool() || g_debug_stealth_search_weight.GetBool() ) && pNPC->GetState() != NPC_STATE_COMBAT )
	{
		NDebugOverlay::Circle( vecSoundOrigin, QAngle( 90, 0, 0 ), ai_stealth_area_contaminate_sound_dist.GetFloat(), 255, 0, 0, 255, true, 5.0f );
	}

	if ( CAI_StealthSenses::IsStealthSound( pSound ) )
	{
		variant_t var;
		var.SetInt( pSound->SoundChannel() );
		g_EventQueue.AddEvent( this, "NPCHeardSuspiciousSound", var, 1.5f, pSound->m_hOwner ? pSound->m_hOwner : this, pNPC );
	}
	else if ( pSound->IsSoundType( SOUND_COMBAT ) )
	{
		// Non-stealth combat sound could only be bad news
		SetMinStealthLevel( STEALTH_LEVEL_TENSE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputNPCHeardSuspiciousSound( inputdata_t &inputdata )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "stealth_suspicious_sound" );
	if ( event && inputdata.pActivator && inputdata.pCaller && AI_GetSinglePlayer() )
	{
		// If you want to make this compatible with MP, you would need to figure out how to associate stealth sounds with
		// individual players outside of the sound owner (e.g. who left a door open)
		event->SetInt( "userid", AI_GetSinglePlayer()->GetUserID() );
		event->SetInt( "channel", inputdata.value.Int() );
		event->SetInt( "body", inputdata.pActivator->entindex() );
		event->SetInt( "npc", inputdata.pCaller->entindex() );
		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCOpenDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor )
{
	if ( !pNPC->GetSquad() )
		return;

	// UNDONE: Do not update the door state directly anymore.
	// Stealth sensing NPCs can now individually remember the door's state.
	// Instead, remove the object flag so that, if the player had touched this door before,
	// an NPC opening it back up doesn't cause other NPCs to freak out.
	// Then, update this NPC's memory of the door.
	RemovePropPerceivable( pDoor );

	int i = 0;
	CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForDoor( pDoor, &i );
	if (pArea)
	{
		// TODO: Allow creation of new memory but only with door state?
		pNPC->GetStealthSenses()->UpdateAreaMemory( pArea, false );
		StealthAreaMemory_t *pMemory = pNPC->GetStealthSenses()->GetAreaMemory( pArea );
		if ( pMemory )
		{
			pMemory->iDoorState |= (1 << i);
		}
	}

	/*CTriggerStealthArea *pArea = GetStealthAreaForDoor( pDoor );
	if ( pArea )
	{
		pArea->UpdateDoorState( pDoor, TRS_TRUE );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::NPCFindOpenDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor )
{
	if ( !pNPC->GetSquad() )
		return;

	// UNDONE: Do not update the door state directly anymore.
	// Stealth sensing NPCs can now individually remember the door's state.
	// Instead, remove the object flag so that, if the player had touched this door before,
	// an NPC opening it back up doesn't cause other NPCs to freak out.
	// Then, update this NPC's memory of the door.
	RemovePropPerceivable( pDoor );

	int i = 0;
	CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForDoor( pDoor, &i );
	if (pArea)
	{
		StealthAreaMemory_t *pMemory = pNPC->GetStealthSenses()->GetAreaMemory( pArea );
		if ( pMemory )
		{
			bool bOpen = (pDoor->IsDoorOpen() || pDoor->IsDoorOpening());
			if (bOpen)
			{
				pMemory->iDoorState |= (1 << i);
			}
			else
			{
				pMemory->iDoorState &= ~(1 << i);
			}
		}
	}

	/*CTriggerStealthArea *pArea = GetStealthAreaForDoor( pDoor );
	if ( pArea )
	{
		pArea->UpdateDoorState( pDoor, TRS_TRUE );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::SquadSawPlayer( CAI_BaseNPC *pNPC, CAI_Squad *pSquad )
{
	StealthSquadInfo_t *pSquadInfo = FindSquadInfo( pSquad );
	if ( pSquadInfo && !pSquadInfo->bAlerted )
	{
		pSquadInfo->bAlerted = true;
	}

	if (!m_bAlerted)
	{
		if (m_bPlayMusic)
		{
			SendMusicInput( m_hAlertMusic, "PlaySound", "", 0.0, pNPC );

			if (m_hStealthMusic)
			{
				if (!m_bResumeStealthMusic)
					SendMusicInput( m_hStealthMusic, "FadeOut", "1" );
				else
					SendMusicInput( m_hStealthMusic, "Volume", "0.1" );
			}
		}

		m_bAlerted = true;
		SetMinStealthLevel( STEALTH_LEVEL_TENSE );

		AISquadIter_t iter;
		for (CAI_BaseNPC *pSquadmate = pSquad->GetFirstMember( &iter ); pSquadmate; pSquadmate = pSquad->GetNextMember( &iter ))
		{
			if (!pSquadmate->IsAlive() || !pSquadmate->IsUsingStealthSenses())
				continue;

			// Forced actbusies need this(?)
			if ( pSquadmate->GetStealthSenses()->IsRunningActBusy() )
			{
				pSquadmate->GetStealthSenses()->StopRunningActBusy();
			}
		}

		m_OnSquadAlerted.Set( MAKE_STRING( pSquad->GetName() ), pNPC, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::SquadLostPlayer( CAI_BaseNPC *pNPC, CAI_Squad *pSquad )
{
	if (m_bAlerted)
	{
		m_OnSquadAlerted.Set( MAKE_STRING( pSquad->GetName() ), pNPC, this );
	}

	// Make all search areas around the player dirty
	Vector vecLastKnownPos = pNPC->GetEnemyLKP();
	ResetAreaSearches( vecLastKnownPos, ai_stealth_area_contaminate_plr_dist.GetFloat() );

	if ( g_debug_stealth_show_areas.GetBool() || g_debug_stealth_search_weight.GetBool() )
	{
		NDebugOverlay::Circle( vecLastKnownPos, QAngle( 90, 0, 0 ), ai_stealth_area_contaminate_plr_dist.GetFloat(), 255, 0, 0, 255, true, 5.0f );
	}

	// If there's any active laser dots owned by our enemy, make them perceivable again
	CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "env_laserdot" );
	for ( ; pEnt != NULL; pEnt = gEntList.FindEntityByClassname( pEnt, "env_laserdot" ) )
	{
		if ( pEnt->GetEffects() & EF_NODRAW || pEnt->GetFlags() & FL_OBJECT || !pEnt->GetOwnerEntity() || pNPC->IRelationType( pEnt->GetOwnerEntity() ) > D_FR )
			continue;

		g_AI_SensedObjectsManager.AddEntity( pEnt );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::SquadQuieted( CAI_BaseNPC *pNPC, CAI_Squad *pSquad )
{
	StealthSquadInfo_t *pSquadInfo = FindSquadInfo( pSquad );
	if ( pSquadInfo && !pSquadInfo->bAlerted )
	{
		pSquadInfo->bAlerted = true;
	}

	if (!m_bAlerted)
	{
		if (m_bPlayMusic)
		{
			SendMusicInput( m_hAlertMusic, "PlaySound", "", 0.0, pNPC );
		}

		m_bAlerted = true;
		SetMinStealthLevel( STEALTH_LEVEL_TENSE );
		m_OnSquadAlerted.Set( MAKE_STRING( pSquad->GetName() ), pNPC, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::PlayerMovedObject( CBasePlayer *pPlayer, CBaseEntity *pEntity )
{
	CTriggerStealthArea *pArea = GetStealthAreaForEntity( pEntity );
	if ( pArea )
	{
		// Don't do the event if nobody's seen this area before
		bool bMemoryExists = false;
		for ( int i = 0; i < m_SquadInfo.Count(); i++ )
		{
			for ( int j = 0; j < m_SquadInfo[i].m_Members.Count(); j++)
			{
				if ( m_SquadInfo[i].m_Members[j].hEntity && m_SquadInfo[i].m_Members[j].hEntity->IsNPC() )
				{
					StealthAreaMemory_t *pMemory = m_SquadInfo[i].m_Members[j].hEntity->MyNPCPointer()->GetStealthSenses()->GetAreaMemory( pArea );
					if ( pMemory )
					{
						bMemoryExists = true;
						break;
					}
				}
			}

			if ( bMemoryExists )
				break;
		}

		if ( !bMemoryExists )
			return;
	}

	IGameEvent *event = gameeventmanager->CreateEvent( "stealth_moved_object" );
	if ( event && pEntity && pPlayer )
	{
		event->SetInt( "userid", pPlayer->GetUserID() );
		event->SetInt( "object", pEntity->entindex() );
		gameeventmanager->FireEvent( event );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthManager::IsAlarmDisabled( CBaseEntity *pAlarm )
{
	return pAlarm->HasContext( CONTEXT_ALARM_DISABLED, "1" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::RaiseAlarm( CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	if (pCaller && IsAlarmDisabled( pCaller ))
		return;

	if (!m_bAlarmRaised)
	{
		if (m_bPlayMusic)
			SendMusicInput( m_hAlertMusic, "FadeOut", "3", 0.0f, pActivator );

		m_bAlarmRaised = true;
		m_OnAlarmRaised.FireOutput( pActivator, pCaller );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::ResetAlarm( CBaseEntity *pActivator )
{
	if (m_bAlarmRaised)
	{
		m_bAlarmRaised = false;
		m_OnAlarmReset.FireOutput( pActivator, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputForceThisNPCToRaiseAlarm( inputdata_t &inputdata )
{
	if ( !inputdata.value.Entity() )
	{
		Warning( "%s ForceThisNPCToRaiseAlarm: No entity\n", GetDebugName() );
		return;
	}

	CAI_BaseNPC *pNPC = inputdata.value.Entity()->MyNPCPointer();
	if ( !pNPC )
	{
		Warning( "%s ForceThisNPCToRaiseAlarm: '%s' is not a NPC\n", GetDebugName(), inputdata.value.Entity()->GetDebugName() );
		return;
	}

	CAI_StealthAlarmBehavior *pAlarmBehavior;
	if ( pNPC->GetBehavior( &pAlarmBehavior ) )
	{
		if ( !pAlarmBehavior->IsRaisingAlarm() )
		{
			pAlarmBehavior->ForceRaiseAlarm();
		}
	}
	else
	{
		Warning( "%s ForceThisNPCToRaiseAlarm: '%s' does not support alarm behavior\n", GetDebugName(), pNPC->GetDebugName() );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::AddSeenObject( CBaseEntity *pEntity )
{
	// FL_OBJECT will be readded when this is influenced again (e.g. via player +USE)
	g_AI_SensedObjectsManager.RemoveEntity( pEntity );

	int iType = GetStealthObjectType( pEntity );
	if (iType != STEALTH_OBJ_RAGDOLL &&
		/*iType != STEALTH_OBJ_DOOR &&*/
		iType != STEALTH_OBJ_PROP )
		return;

	int i = 0;
	for ( ; i < m_SeenObjects.Count(); i++ )
	{
		if ( m_SeenObjects[i].hEntity == pEntity )
			break;
	}

	if ( i == m_SeenObjects.Count() )
	{
		i = m_SeenObjects.AddToTail();
		m_SeenObjects[i].hEntity = pEntity;
		m_SeenObjects[i].nTimesFound = 0;
		m_SeenObjects[i].flTimeEnteredArea = 0;	// This prop would've already been in the area for an unknown time
	}
	else
	{
		m_SeenObjects[i].nTimesFound++;
	}

	m_SeenObjects[i].vecLastPosition = pEntity->GetAbsOrigin();
	m_SeenObjects[i].flLastTimeChecked = gpGlobals->curtime + SEEN_OBJECT_CHECK_COOLDOWN;
	m_SeenObjects[i].bResult = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthManager::ShouldSeeObject( CBaseEntity *pEntity )
{
	for ( int i = 0; i < m_SeenObjects.Count(); i++ )
	{
		if ( m_SeenObjects[i].hEntity == pEntity )
		{
			if ( m_SeenObjects[i].flLastTimeChecked < gpGlobals->curtime )
			{
				m_SeenObjects[i].bResult = false;

				// Check if the object has moved from where it previously was
				if ( (m_SeenObjects[i].hEntity->GetAbsOrigin() - m_SeenObjects[i].vecLastPosition).LengthSqr() > Square( ai_stealth_obj_min_dist_change.GetFloat() ))
					m_SeenObjects[i].bResult = true;

				m_SeenObjects[i].flLastTimeChecked = gpGlobals->curtime + SEEN_OBJECT_CHECK_COOLDOWN;
			}

			return m_SeenObjects[i].bResult;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::MakePropPerceivable( CBaseEntity *pEntity )
{
	pEntity->AddContext( CONTEXT_PERCEIVABLE, "1" );
	g_AI_SensedObjectsManager.AddEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::RemovePropPerceivable( CBaseEntity *pEntity )
{
	pEntity->RemoveContext( CONTEXT_PERCEIVABLE );
	g_AI_SensedObjectsManager.RemoveEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthManager::ShouldPropBePerceivable( CBaseEntity *pEntity ) const
{
	if ( pEntity->GetBaseAnimating() && pEntity->GetBaseAnimating()->IsRagdoll() )
		return true;

	return pEntity->HasContext( CONTEXT_PERCEIVABLE, "1" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthObjectState_t *CAI_StealthManager::GetStealthObjectState( CBaseEntity *pEntity )
{
	for ( int i = 0; i < m_SeenObjects.Count(); i++ )
	{
		if ( m_SeenObjects[i].hEntity == pEntity )
		{
			return &m_SeenObjects[i];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthObjectType_t CAI_StealthManager::GetStealthObjectType( CBaseEntity *pEntity ) const
{
	if ( !pEntity )
		return STEALTH_OBJ_NONE;

	const char *pszClassname = pEntity->GetClassname();

	if ( pEntity->GetBaseAnimating() )
	{
		if ( pEntity->GetBaseAnimating()->IsRagdoll() ) // V_strncmp( pszClassname, "prop_r", 6 )
		{
			return STEALTH_OBJ_RAGDOLL;
		}
		else if ( V_strncmp( pszClassname, "prop_", 5 ) == 0 )
		{
			if ( pszClassname[5] == 'd' && pszClassname[6] == 'o' ) // prop_door_rotating
			{
				return STEALTH_OBJ_DOOR;
			}
			else if ( pszClassname[5] == 'p' ) // prop_physics
			{
				if ( pEntity->HasContext( "pickup_prop", "1" ) )
					return STEALTH_OBJ_PROP_PICKUP;

				return STEALTH_OBJ_PROP;
			}
		}
	}
	else if ( V_strncmp( pszClassname, "env_l", 5 ) == 0 ) // env_laserdot
	{
		return STEALTH_OBJ_LASER_DOT;
	}
	// TODO: Generic stealth interest object, can be used to mark bloodstains or missing objects
	//else if ( FStrEq( pszClassname, "ai_stealth_obj" ) )
	//{
	//
	//}

	return STEALTH_OBJ_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthSquadInfo_t *CAI_StealthManager::FindSquadInfo( CAI_Squad *pSquad, bool bCreate )
{
	for ( int i = 0; i < m_SquadInfo.Count(); i++ )
	{
		if ( m_SquadInfo[i].pSquad == pSquad )
			return &m_SquadInfo[i];
	}

	if ( bCreate )
	{
		int i = m_SquadInfo.AddToTail();
		m_SquadInfo[i].pSquad = pSquad;
		m_SquadInfo[i].iszSquadName = MAKE_STRING( pSquad->GetName() );
		m_SquadInfo[i].bAlerted = false;

		// Initialize members
		AISquadIter_t iter;
		for (CAI_BaseNPC *pSquadmate = pSquad->GetFirstMember( &iter ); pSquadmate; pSquadmate = pSquad->GetNextMember( &iter ))
		{
			if (!pSquadmate->IsAlive() || pSquadmate->IsSilentSquadMember())
				continue;

			int j = m_SquadInfo[i].m_Members.AddToTail();
			m_SquadInfo[i].m_Members[j].hEntity = pSquadmate;
			m_SquadInfo[i].m_Members[j].bKnownAlive = true;
			m_SquadInfo[i].m_Members[j].vecLastKnownLocation = pSquadmate->GetAbsOrigin();
			m_SquadInfo[i].m_Members[j].flLastKnownTime = gpGlobals->curtime;
			m_SquadInfo[i].m_Members[j].hLastInformer = NULL;
			m_SquadInfo[i].m_Members[j].iszID = GenerateSquadMemberID( &m_SquadInfo[i], pSquadmate );
			m_SquadInfo[i].m_Members[j].nGender = soundemitterbase->GetActorGender( STRING( GetModelName() ) );
			m_SquadInfo[i].m_Members[j].nDamageType = 0;
		}

		return &m_SquadInfo[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthSquadMemberInfo_t *CAI_StealthManager::FindSquadMemberInfo( StealthSquadInfo_t *pSquadInfo, CBaseEntity *pSquadmate, bool bCreate )
{
	for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
	{
		if ( pSquadInfo->m_Members[i].hEntity == pSquadmate )
			return &pSquadInfo->m_Members[i];
	}

	if ( bCreate )
	{
		int i = pSquadInfo->m_Members.AddToTail();
		pSquadInfo->m_Members[i].hEntity = pSquadmate;
		pSquadInfo->m_Members[i].bKnownAlive = true;
		pSquadInfo->m_Members[i].vecLastKnownLocation = pSquadmate->GetAbsOrigin();
		pSquadInfo->m_Members[i].flLastKnownTime = gpGlobals->curtime;
		pSquadInfo->m_Members[i].hLastInformer = NULL;
		pSquadInfo->m_Members[i].iszID = GenerateSquadMemberID( pSquadInfo, pSquadmate );
		pSquadInfo->m_Members[i].nGender = soundemitterbase->GetActorGender( STRING( GetModelName() ) );
		pSquadInfo->m_Members[i].nDamageType = 0;
		return &pSquadInfo->m_Members[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::RemoveSquadMemberInfo( CAI_Squad *pSquad, CAI_BaseNPC *pSquadmate )
{
	StealthSquadInfo_t *pSquadInfo = FindSquadInfo( pSquad, false );
	if ( pSquadInfo )
	{
		for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
		{
			if ( pSquadInfo->m_Members[i].hEntity == pSquadmate )
			{
				pSquadInfo->m_Members.Remove( i );
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
string_t CAI_StealthManager::GenerateSquadMemberID( StealthSquadInfo_t *pSquadInfo, CBaseEntity *pSquadmate )
{
	const char *pszOverrideID = pSquadmate->GetContextValue( "id" );
	if ( pszOverrideID && *pszOverrideID )
		return FindPooledString( pszOverrideID );	// If it's in the NPC's contexts, then it's already going to be in there

	// Generate a new one based on the classname and squad member count
	// (e.g. "npc_citizen-1", "npc_citizen-2"...)
	char szNewID[32] = { 0 };
	V_snprintf( szNewID, sizeof( szNewID ), "%s-%i", pSquadmate->GetClassname(), pSquadInfo->m_Members.Count() );

	// Also add it to the squad member as a context
	pSquadmate->AddContext( "id", szNewID );

	return AllocPooledString( szNewID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::UpdateSeenSquadMember( CAI_BaseNPC *pSquadmate, CAI_BaseNPC *pInformer )
{
	StealthSquadMemberInfo_t *pSquadMemberInfo = pSquadmate->GetStealthSenses()->GetStealthSquadMemberInfo();
	if ( !pSquadMemberInfo )
		return;

	UpdateSeenSquadMember( pSquadMemberInfo, pInformer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::UpdateSeenSquadMember( StealthSquadMemberInfo_t *pSquadMemberInfo, CAI_BaseNPC *pInformer )
{
	if ( pSquadMemberInfo->hEntity->IsAlive() )
	{
		pSquadMemberInfo->vecLastKnownLocation = pSquadMemberInfo->hEntity->GetAbsOrigin();
		pSquadMemberInfo->flLastKnownTime = gpGlobals->curtime;
		pSquadMemberInfo->hLastInformer = pInformer;
	}
	else
	{
		// We don't track the location on ragdolls because that's handled by m_SeenObjects,
		// and also because information from when they were last alive may be useful
		pSquadMemberInfo->bKnownAlive = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::UpdateDeadSquadMember( CBaseEntity *pBody )
{
	// Since ragdolls don't store squads, instead see if we can find ourself as a member
	StealthSquadMemberInfo_t *memberInfo = NULL;
	for ( int i = 0; i < m_SquadInfo.Count(); i++ )
	{
		for ( int j = 0; j < m_SquadInfo[i].m_Members.Count(); j++)
		{
			if ( m_SquadInfo[i].m_Members[j].hEntity == pBody )
			{
				memberInfo = &m_SquadInfo[i].m_Members[j];
				break;
			}
		}
	}

	if ( !memberInfo )
		return;

	UpdateDeadSquadMember( memberInfo, pBody );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::UpdateDeadSquadMember( StealthSquadMemberInfo_t *pSquadMemberInfo, CBaseEntity *pBody )
{
	// We don't track the location on ragdolls because that's handled by m_SeenObjects,
	// and also because information from when they were last alive may be useful
	pSquadMemberInfo->bKnownAlive = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthManager::GetDistanceFromSquad( const Vector &vecOrigin, StealthSquadInfo_t *pSquadInfo, CAI_BaseNPC *pExclude )
{
	float flBestDistSqr = FLT_MAX;
	for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
	{
		if ( !pSquadInfo->m_Members[i].bKnownAlive || pSquadInfo->m_Members[i].hEntity == pExclude )
			continue;

		float flDistSqr = (pSquadInfo->m_Members[i].vecLastKnownLocation - vecOrigin).LengthSqr();
		if (flDistSqr < flBestDistSqr)
			flBestDistSqr = flDistSqr;
	}

	return sqrt( flBestDistSqr );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthManager::GetKnownLivingSquadMembers( StealthSquadInfo_t *pSquadInfo )
{
	int iNumMembers = 0;
	for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
	{
		if ( pSquadInfo->m_Members[i].bKnownAlive )
			iNumMembers++;
	}

	return iNumMembers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthManager::GetSquadMemberLeastSeen( StealthSquadInfo_t *pSquadInfo )
{
	// Lowest as in relative to curtime
	int iLowestMember = pSquadInfo->m_Members.InvalidIndex();
	float flLowestTime = FLT_MAX;
	for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
	{
		if ( !pSquadInfo->m_Members[i].bKnownAlive )
			continue;

		if (pSquadInfo->m_Members[i].flLastKnownTime < flLowestTime)
		{
			iLowestMember = i;
			flLowestTime = pSquadInfo->m_Members[i].flLastKnownTime;
		}
	}

	return iLowestMember;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthManager::SquadHasUnknownDeadMember( StealthSquadInfo_t *pSquadInfo )
{
	for ( int i = 0; i < pSquadInfo->m_Members.Count(); i++ )
	{
		if ( !pSquadInfo->m_Members[i].hEntity )
		{
			//Msg( "-- %i (%s): Known Alive %d, [null]\n", i, STRING( pSquadInfo->m_Members[i].iszID ), pSquadInfo->m_Members[i].bKnownAlive );
			return i;	// TODO: Ensure removing ents doesn't mark them as dead
		}

		//Msg( "-- %i (%s): Known Alive %d, Actual Alive %d\n", i, STRING( pSquadInfo->m_Members[i].iszID ), pSquadInfo->m_Members[i].bKnownAlive, pSquadInfo->m_Members[i].hEntity->IsAlive() );

		if ( pSquadInfo->m_Members[i].bKnownAlive && !pSquadInfo->m_Members[i].hEntity->IsAlive() )
			return i;
	}

	return pSquadInfo->m_Members.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::AddStealthArea( CTriggerStealthArea *pArea )
{
	m_StealthAreas.AddToTail( pArea );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::RemoveStealthArea( CTriggerStealthArea *pArea )
{
	m_StealthAreas.FindAndRemove( pArea );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHandle<CTriggerStealthArea> &CAI_StealthManager::GetStealthArea( int i )
{
	return m_StealthAreas[i];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthManager::GetStealthAreaCount()
{
	return m_StealthAreas.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthManager::FindBestStealthArea( CAI_BaseNPC *pNPC, float flMaxDist, const CUtlVector<StealthInterestPoint_t> *vecInterestPoints, bool bInterestOnly )
{
	float flMinDist = 0.0f;
	if ( vecInterestPoints )
	{
		int iBestInterestPoint = GetBestInterestPoint( *vecInterestPoints );
		if ( iBestInterestPoint != vecInterestPoints->InvalidIndex() )
		{
			float flInterestDistToMe = (pNPC->GetAbsOrigin() - (*vecInterestPoints)[iBestInterestPoint].vecOrigin).Length();
			if ( flInterestDistToMe > flMaxDist )
			{
				// Place our search region with the most interesting position in the middle
				float flHalfMax = (flMaxDist / 2.0);
				flMinDist = flInterestDistToMe - flHalfMax;
				flMaxDist = flInterestDistToMe + flHalfMax;
			}
		}
	}

	CTriggerStealthArea *pBestArea = NULL;
	float flBestWeight = 0.1f; // Minimum of 0.1
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		float flWeight = GetStealthAreaWeight( pNPC, m_StealthAreas[i], flMinDist, flMaxDist, vecInterestPoints, bInterestOnly );
		if ( flWeight > flBestWeight )
		{
			pBestArea = m_StealthAreas[i];
			flBestWeight = flWeight;
		}
	}

	if ( g_debug_stealth_search_weight.GetBool() &&
		( g_debug_stealth_search_weight.GetInt() != 2 || ( pNPC->GetSquad() && pNPC->GetSquad()->IsLeader( pNPC ) ) ) )
	{
		NDebugOverlay::Circle( pNPC->GetAbsOrigin(), QAngle( 90, 0, 0 ), flMinDist, 255, 0, 0, 255, true, 5.0f );
		NDebugOverlay::Circle( pNPC->GetAbsOrigin(), QAngle( 90, 0, 0 ), flMaxDist, 255, 0, 0, 255, true, 5.0f );

		// Need to get the weights again so that we have best weight
		for ( int i = 0; i < m_StealthAreas.Count(); i++ )
		{
			char szTemp[128] = { 0 };
			V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s", i, m_StealthAreas[i]->GetDebugName() );
			NDebugOverlay::EntityTextAtPosition( m_StealthAreas[i]->GetAbsOrigin(), 0, szTemp, 5.0f, 255, 64, 255 );

			float flWeight = GetStealthAreaWeight( pNPC, m_StealthAreas[i], flMinDist, flMaxDist, vecInterestPoints );
			V_snprintf( szTemp, sizeof( szTemp ), "-- %.2f", flWeight );
			NDebugOverlay::EntityTextAtPosition( m_StealthAreas[i]->GetAbsOrigin(), 1, szTemp, 5.0f, 255, 192, 255 );

			V_snprintf( szTemp, sizeof( szTemp ), "-- Searchers: %i", m_StealthAreas[i]->GetSearcherCount() );
			NDebugOverlay::EntityTextAtPosition( m_StealthAreas[i]->GetAbsOrigin(), 2, szTemp, 5.0f, 255, 192, 255 );

			/*const Vector &vecTarget = vecEnemyLastKnown ? *vecEnemyLastKnown : pNPC->GetAbsOrigin();
			int nClr = (int)RemapVal( flWeight, 0.0f, flBestWeight, 0.0f, 255.0f );
			NDebugOverlay::Line( m_StealthAreas[i]->GetAbsOrigin(), vecTarget, (255 - nClr), nClr, 0, true, 10.0f );*/
		}

		if ( vecInterestPoints )
		{
			for ( int i = 0; i < vecInterestPoints->Count(); i++ )
			{
				const StealthInterestPoint_t &point = vecInterestPoints->Element( i );

				NDebugOverlay::Circle( point.vecOrigin, QAngle( 90, 0, 0 ), GetInterestTypeRadius( point.nType ), 0, 0, 255, 255, true, 5.0f );

				char szTemp[128] = { 0 };
				V_snprintf( szTemp, sizeof( szTemp ), "[%i] %s", i, GetInterestTypeName( point.nType ) );
				NDebugOverlay::EntityTextAtPosition( point.vecOrigin, 0, szTemp, 5.0f, 64, 64, 255 );

				V_snprintf( szTemp, sizeof( szTemp ), "-- Expire time: %f", point.flExpireTime - gpGlobals->curtime );
				NDebugOverlay::EntityTextAtPosition( point.vecOrigin, 1, szTemp, 5.0f, 64, 64, 255 );
			}
		}
	}

	return pBestArea;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthManager::GetStealthAreaForEntity( CBaseEntity *pEntity )
{
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		if ( m_StealthAreas[i]->IsTouching( pEntity ) )
			return m_StealthAreas[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthManager::GetStealthAreaForPoint( const Vector &vecOrigin )
{
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		if ( m_StealthAreas[i]->PointIsWithin( vecOrigin ) )
			return m_StealthAreas[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthManager::GetStealthAreaForHint( CAI_Hint *pHint )
{
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		if ( m_StealthAreas[i]->IsValidSearchPoint( pHint ) )
			return m_StealthAreas[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthManager::GetStealthAreaForDoor( CBasePropDoor *pDoor, int *iIndex, bool *bShouldBeOpen )
{
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		if ( m_StealthAreas[i]->IsValidDoor( pDoor, iIndex, bShouldBeOpen ) )
			return m_StealthAreas[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector *CAI_StealthManager::GetInteriorPositionThroughDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor )
{
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		if ( m_StealthAreas[i]->IsValidDoor( pDoor ) )
		{
			return &m_StealthAreas[i]->GetInteriorPosition( pNPC, pDoor );
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthManager::GetStealthAreaWeight( CAI_BaseNPC *pNPC, CTriggerStealthArea *pArea, float flMinDist, float flMaxDist, const CUtlVector<StealthInterestPoint_t> *vecInterestPoints, bool bInterestOnly )
{
	if ( pArea->m_bDisabled || !pArea->IsSearchable() || pArea->GetSearcherCount() >= pArea->GetMaxSearchers() )
		return 0.0f;

	float flWeight = pArea->GetSearchWeight();

	// Don't search areas that were just searched (if the enemy was in/near them, its time would've been reset)
	if ( pArea->GetTimeLastSearched() > 0.0f )
	{
		flWeight *= RemapValClamped( gpGlobals->curtime - pArea->GetTimeLastSearched(),
			ai_stealth_area_time_min.GetFloat(), ai_stealth_area_time_max.GetFloat(),
			0.0f, 1.0f );
	}

	if ( flWeight == 0.0f )
		return flWeight;

	// Prioritize search areas near the enemy
	if ( vecInterestPoints )
	{
		bool bNearInterestPoint = false;
		FOR_EACH_VEC( *vecInterestPoints, i )
		{
			Vector vecPointDir = (pArea->GetInteriorPosition(pNPC) - (*vecInterestPoints)[i].vecOrigin);

			// Consider higher areas to be farther away
			if (vecPointDir.z > 0.0f)
				vecPointDir.z *= 4.0f;

			float flPointDist = vecPointDir.Length();
			if ( flPointDist > GetInterestTypeRadius( (*vecInterestPoints)[i].nType ) || flPointDist > pArea->GetMaxInterestDistance() )
				continue;

			float flMult = 1.0f;
			switch ((*vecInterestPoints)[i].nType)
			{
				case STEALTH_INTEREST_ENEMY:
					flMult = ( 2.0f - RemapVal( flPointDist,
						ai_stealth_area_enemydist_min.GetFloat(), ai_stealth_area_enemydist_max.GetFloat(),
						0.0f, 1.5f ) );
					break;
				case STEALTH_INTEREST_SOUND:
					flMult = ( 1.5f - RemapVal( flPointDist,
						ai_stealth_area_sound_min.GetFloat(), ai_stealth_area_sound_max.GetFloat(),
						0.0f, 0.8f ) );
					break;
				case STEALTH_INTEREST_MISSING_ALLY:
					flMult = ( 1.5f - RemapVal( flPointDist,
						ai_stealth_area_sound_min.GetFloat(), ai_stealth_area_sound_max.GetFloat(),
						0.0f, 0.8f ) );
					break;
			}

			flWeight *= flMult;
			bNearInterestPoint = true;
		}

		if ( bInterestOnly )
		{
			// This area is invalid if not near an interest point
			if ( !bNearInterestPoint )
				return 0.0f;
		}
		else
		{
			// If this area has a max interest distance, early out anyway
			if ( !bNearInterestPoint && pArea->GetMaxInterestDistance() != FLT_MAX )
				return 0.0f;
		}
	}
	else
	{
		// If this area wants to only be near interest points, don't care about it
		if ( pArea->GetMaxInterestDistance() != FLT_MAX )
			return 0.0f;
	}

	// Now calculate our own distance
	float flMyDist = (pArea->GetAbsOrigin() - pNPC->GetAbsOrigin()).Length();
	if ( flMyDist < flMinDist )
		return 0.0f;

	flWeight *= ( 1.0f - RemapValClamped( flMyDist,
		flMinDist, flMaxDist,
		0.1f, 1.0f ) );

	return flWeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::ResetAreaSearches( const Vector &vecOrigin, float flRadius )
{
	const float flMaxDist = Square( flRadius );
	for ( int i = 0; i < m_StealthAreas.Count(); i++ )
	{
		float flDistSqr = (m_StealthAreas[i]->GetAbsOrigin() - vecOrigin).LengthSqr();
		if ( flDistSqr < flMaxDist )
		{
			m_StealthAreas[i]->ResetTimeSearched();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CAI_StealthManager::GetInterestTypeName( StealthInterestType_t nInterestType )
{
	switch (nInterestType)
	{
	case STEALTH_INTEREST_ENEMY:
		return "Enemy";
	case STEALTH_INTEREST_SOUND:
		return "Sound";
	case STEALTH_INTEREST_MISSING_ALLY:
		return "Missing Ally";
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthManager::GetInterestTypeRadius( StealthInterestType_t nInterestType )
{
	switch (nInterestType)
	{
		case STEALTH_INTEREST_ENEMY:
			return ai_stealth_area_enemydist_max.GetFloat();
		case STEALTH_INTEREST_SOUND:
			return ai_stealth_area_sound_max.GetFloat();
		case STEALTH_INTEREST_MISSING_ALLY:
			return ai_stealth_area_enemydist_max.GetFloat(); // For now
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthManager::GetInterestTypeDuration( StealthInterestType_t nInterestType )
{
	switch (nInterestType)
	{
		case STEALTH_INTEREST_ENEMY:
			return 300.0f; // 5 minutes
		case STEALTH_INTEREST_SOUND:
			return 120.0f; // 2 minutes
		case STEALTH_INTEREST_MISSING_ALLY:
			return 300.0f; // 5 minutes
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthManager::GetBestInterestPoint( const CUtlVector<StealthInterestPoint_t> &vecInterestPoints )
{
	int nBestIdx = -1;
	float flBestWeight = 0.0f;
	FOR_EACH_VEC( vecInterestPoints, i )
	{
		float flWeight = 1.0f;

		// Different weights for different types
		switch ( vecInterestPoints[i].nType )
		{
			case STEALTH_INTEREST_ENEMY:
				flWeight *= 3.0f;
				break;
			case STEALTH_INTEREST_MISSING_ALLY:
				flWeight *= 2.0f;
				break;
		}

		// Expire time
		float flStartTime = vecInterestPoints[i].flExpireTime - GetInterestTypeDuration( vecInterestPoints[i].nType );
		flWeight *= 1.0f - RemapValClamped( gpGlobals->curtime, flStartTime, vecInterestPoints[i].flExpireTime, 0.0f, 1.0f );

		if ( flWeight > flBestWeight )
		{
			flBestWeight = flWeight;
			nBestIdx = i;
		}
	}

	return nBestIdx;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::OnObjectEnteredArea( CTriggerStealthArea *pArea, CBaseEntity *pEntity )
{
	StealthObjectType_t nType = GetStealthObjectType( pEntity );
	if ( nType != STEALTH_OBJ_PROP )
		return;

	// Update area time if this was previously seen
	for ( int i = 0; i < m_SeenObjects.Count(); i++ )
	{
		if ( m_SeenObjects[i].hEntity == pEntity )
		{
			m_SeenObjects[i].flTimeEnteredArea = gpGlobals->curtime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputEnableMusic( inputdata_t &inputdata )
{
	m_bPlayMusic = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputDisableMusic( inputdata_t &inputdata )
{
	m_bPlayMusic = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputSetStealthMusic( inputdata_t &inputdata )
{
	m_hStealthMusic = inputdata.value.Entity();
	if ( m_hStealthMusic )
		m_iszStealthMusic = m_hAlertMusic->GetEntityName();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputSetAlertMusic( inputdata_t &inputdata )
{
	m_hAlertMusic = inputdata.value.Entity();
	if ( m_hAlertMusic )
		m_iszAlertMusic = m_hAlertMusic->GetEntityName();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputEnableResumeStealthMusic( inputdata_t &inputdata )
{
	m_bResumeStealthMusic = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::InputDisableResumeStealthMusic( inputdata_t &inputdata )
{
	m_bResumeStealthMusic = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::SendMusicInput( CBaseEntity *pEntity, const char *pszInputName, const char *pszParam, float flDelay, CBaseEntity *pActivator )
{
	if ( !pEntity )
		return;

	variant_t var;
	var.SetString( MAKE_STRING( pszParam ) );
	g_EventQueue.AddEvent( pEntity, pszInputName, var, flDelay, pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::ModifyOrAppendCriteria( CBaseEntity *pEntity, AI_CriteriaSet &set )
{
	set.AppendCriteria( "stealth_level", UTIL_VarArgs( "%i", GetStealthLevel() ) );

	CTriggerStealthArea *pArea = NULL;

	// Note that most other NPC stealth criteria is handled within CAI_StealthSenses::ModifyOrAppendCriteria().
	if ( pEntity->IsNPC() )
	{
		CAI_BaseNPC *pNPC = pEntity->MyNPCPointer();
		if ( pNPC->GetEnemy() )
		{
			if ( pNPC->HasCondition( COND_SEE_ENEMY ) )
			{
				// Use the enemy's area
				pArea = GetStealthAreaForEntity( pNPC->GetEnemy() );
			}
			else
			{
				// Use the enemy's *supposed* area
				pArea = GetStealthAreaForPoint( pNPC->GetEnemyLKP() );
			}
		}
		else
		{
			// Use our search area (which we may not be inside of)
			CAI_StealthSearchBehavior *pSearchBehavior;
			if ( pNPC->GetBehavior( &pSearchBehavior ) )
			{
				pArea = pSearchBehavior->GetSearchArea();
			}

			if ( !pArea )
			{
				// Use my area
				pArea = GetStealthAreaForEntity( pEntity );
			}
		}
	}
	else
	{
		// Use my area
		pArea = GetStealthAreaForEntity( pEntity );
	}

	if ( pArea )
	{
		set.AppendCriteria( "area_name", STRING( pArea->GetEntityName() ) );
		set.AppendCriteria( "area", pArea->GetAreaContext() );
		//set.AppendCriteria( "area_enclosed", pArea->IsEnclosed() );
		pArea->AppendContextToCriteria( set );

		float flMyTimeSpent = pArea->GetTimeSpentInArea( pEntity );
		set.AppendCriteria( "area_time_spent", flMyTimeSpent );

		if ( pEntity->GetEnemy() ) // Either NPC or player
		{
			// Check to see if our enemy's been in here for longer than us
			float flEnemyTimeSpent = pArea->GetTimeSpentInArea( pEntity->GetEnemy() );
			set.AppendCriteria( "enemy_area_time_spent", flEnemyTimeSpent );

			if ( flEnemyTimeSpent > flMyTimeSpent )
				set.AppendCriteria( "enemy_already_inside", "1" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void InsertStealthSound( int iType, const Vector &vecOrigin, int iVolume, float flDuration, CBaseEntity *pOwner, int soundChannelIndex, CBaseEntity *pSoundTarget )
{
	if ( g_hStealthManager && g_hStealthManager->GetStealthLevel() == STEALTH_LEVEL_NONE )
		return;

	// TODO: Print sounds here?

	CSoundEnt::InsertSound( iType, vecOrigin, iVolume, flDuration, pOwner, soundChannelIndex, pSoundTarget );
}

//=============================================================================
// Debugging
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthManager::StealthDebugPrintf( int iLine, Vector clr, const char *pMsg, ... )
{
	// Format the string.
	char str[256];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( str, sizeof( str ), pMsg, marker );
	va_end( marker );

	con_nprint_s info;
	info.index = iLine;
	info.time_to_live = STEALTH_MANAGER_THINK;
	info.color[0] = clr[0];
	info.color[1] = clr[1];
	info.color[2] = clr[2];
	info.fixed_width_font = true;

	// Show it with Con_NPrintf.
	engine->Con_NXPrintf( &info, "%s", str );
}

// -----------------------------------------------------------------------------
void CAI_StealthManager::DebugShowStealthState( int iStartLine )
{
	int iLine = iStartLine;

	//---------------------------------------------------
	// Stealth Level
	//---------------------------------------------------
	switch ( m_nStealthLevel )
	{
		case STEALTH_LEVEL_NONE:
			StealthDebugPrintf( iLine++, Vector( 1.0, 1.0, 1.0 ), "Stealth Level: None\n" );
			break;
		case STEALTH_LEVEL_QUIET:
			StealthDebugPrintf( iLine++, Vector( .25, 1.0, .25 ), "Stealth Level: Quiet\n" );
			break;
		case STEALTH_LEVEL_GUARD:
			StealthDebugPrintf( iLine++, Vector( .5, 1.0, .25 ), "Stealth Level: Guard\n" );
			break;
		case STEALTH_LEVEL_TENSE:
			StealthDebugPrintf( iLine++, Vector( 1.0, 1.0, .25 ), "Stealth Level: Tense\n" );
			break;
		case STEALTH_LEVEL_LOUD:
			StealthDebugPrintf( iLine++, Vector( 1.0, .25, .25 ), "Stealth Level: Loud\n" );
			break;
	}

	StealthDebugPrintf( iLine++, Vector( .9, .9, .9 ), "Tracking %i objects\n", m_SeenObjects.Count() );

	//---------------------------------------------------
	// Alarms
	//---------------------------------------------------
	if ( m_bAlarmsEnabled )
	{
		StealthDebugPrintf( iLine++, Vector( 1.0, .9, .9 ), "Alarms enabled\n" );
		StealthDebugPrintf( iLine++, Vector( 1.0, .9, .9 ), "Bodies needed: %i --\n", m_nBodiesToRaiseAlarm );
	}
	else
	{
		StealthDebugPrintf( iLine++, Vector( .75, .75, .75 ), "Alarms not enabled\n" );
	}

	//---------------------------------------------------
	// NPCs
	//---------------------------------------------------
	if ( m_nStealthLevel != STEALTH_LEVEL_NONE )
	{
		StealthDebugPrintf( iLine++, Vector( 1.0, 1.0, 1.0 ), "NPCs:\n" );

		CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
		for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
		{
			if ( ppAIs[i] == NULL || !ppAIs[i]->IsUsingStealthSenses() || !ppAIs[i]->IsAlive() )
				continue;

			if ( !ppAIs[i]->HasCondition( COND_IN_PVS ) )
			{
				StealthDebugPrintf( iLine++, Vector( .5, .5, .5 ), "[%i : %s]: Not in PVS --\n", ppAIs[i]->entindex(), ppAIs[i]->GetDebugName() );
				continue;
			}

			Vector vecClr = Vector( .9, .9, .9 );
			const char *pszState = "N/A";
	
			switch ( ppAIs[i]->GetState() )
			{
				case NPC_STATE_IDLE:
					vecClr = Vector( .5, 1.0, .5 );
					pszState = "Idle";
					break;
				case NPC_STATE_ALERT:
					vecClr = Vector( 1.0, 1.0, .5 );
					pszState = "Alert";
					break;
				case NPC_STATE_COMBAT:
					vecClr = Vector( 1.0, .5, .5 );
					pszState = "Combat";
					break;
			}

			CAI_StealthCuriousBehavior *pCuriousBehavior;
			if ( ppAIs[i]->GetBehavior( &pCuriousBehavior ) )
			{
				if ( pCuriousBehavior->IsInvestigatingSound() )
				{
					vecClr = Vector( 0, 1.0, 1.0 );
					pszState = "Investigating Sound";
				}
			}

			CAI_StealthAlarmBehavior *pAlarmBehavior;
			if ( ppAIs[i]->GetBehavior( &pAlarmBehavior ) )
			{
				if ( pAlarmBehavior->IsRaisingAlarm() )
				{
					vecClr = Vector( 1.0, .5, 0 );
					pszState = "Raising Alarm";
				}
			}

			StealthDebugPrintf( iLine++, vecClr, "[%i : %s]: %s --\n", ppAIs[i]->entindex(), ppAIs[i]->GetDebugName(), pszState );
		}
	}
}
