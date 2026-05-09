//=============================================================================//
//
// Purpose:		AI behavior for advanced curiosity features.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_behavior_curious.h"
#include "ai_stealth_behavior_alarm.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_senses.h"
#include "ai_stealth_area.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_tacticalservices.h"
#include "ai_behavior_actbusy.h"
#include "ai_playerally.h"
#include "npcevent.h"
#include "physics_prop_ragdoll.h"
#include "BasePropDoor.h"
#include "ai_behavior_tripmine_place.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	ai_stealth_investigate_wait( "ai_stealth_investigate_wait", "3.5" );
ConVar	ai_stealth_investigate_wait_cooldown( "ai_stealth_investigate_wait_cooldown", "45" );
ConVar	ai_stealth_investigate_repeat_stay_time( "ai_stealth_investigate_repeat_stay_time", "30" );
ConVar	ai_stealth_investigate_repeat_goto_threshold( "ai_stealth_investigate_repeat_goto_threshold", "1" );
ConVar	ai_stealth_investigate_repeat_run_threshold( "ai_stealth_investigate_repeat_run_threshold", "3" );

ConVar	g_debug_stealth_investigate( "g_debug_stealth_investigate", "0" );

//-----------------------------------------------------------------------------

#define CuriousDbgMsg( msg, ... )		if ( g_debug_stealth_investigate.GetBool() ) { ConColorMsg( DbgStealthColor, msg, __VA_ARGS__ ); }

//-----------------------------------------------------------------------------

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_StealthCuriousBehavior )

	DEFINE_FIELD( m_bCalmSound, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nSoundChannel, FIELD_INTEGER ),
	DEFINE_FIELD( m_flSoundExpireTime, FIELD_TIME ),

	DEFINE_FIELD( m_flLastTimeHeardSound, FIELD_TIME ),
	DEFINE_FIELD( m_nNumTimesInvestigatedSound, FIELD_INTEGER ),

	DEFINE_FIELD( m_hSuspiciousTarget, FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_StealthCuriousBehavior::CAI_StealthCuriousBehavior()
{
	m_bCalmSound = false;
	m_nSoundChannel = 0;
	m_flSoundExpireTime = FLT_MAX;
	m_flLastTimeHeardSound = -1.0f;
	m_nNumTimesInvestigatedSound = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_CuriousStealthSenses *CAI_StealthCuriousBehavior::GetStealthSenses()
{
	return static_cast<CAI_CuriousStealthSenses*>( GetOuter()->GetStealthSenses() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthCuriousBehavior::IsInvestigatingSound()
{
	if ( IsCurSchedule( SCHED_INVESTIGATE_SOUND ) ||
		IsCurSchedule( SCHED_STEALTH_INVESTIGATE_SOUND, false ) ||
		IsCurSchedule( SCHED_STEALTH_INVESTIGATE_SOUND_STAY, false ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthCuriousBehavior::ShouldStayAtSound( CSound *pSound )
{
	if ( CAI_StealthSenses::ShouldStayAtSound( pSound ) )
		return true;

	if ( m_flLastTimeHeardSound == -1.0f )
		return false;

	// If we just heard another sound, stick around
	return (gpGlobals->curtime - m_flLastTimeHeardSound) < ai_stealth_investigate_repeat_stay_time.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthCuriousBehavior::ShouldGoToSoundSource( CSound *pSound )
{
	// TODO: More specific behavior for sticking together?
	if ( g_hStealthManager && !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET ) && GetOuter()->GetSquad() )
	{
		if ( g_hStealthManager->GetKnownLivingSquadMembers( GetStealthSenses()->GetStealthSquadInfo() ) <= 2 )
			return false;

		if ( GetStealthSenses()->GetNumBodiesFound() >= 2 )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnHearNewSound( CSound *pSound )
{
	if ( pSound->SoundChannel() != SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS && pSound->SoundExpirationTime() > m_flSoundExpireTime
		&& GetCurTask() && ( GetCurTask()->iTask == TASK_WAIT || GetCurTask()->iTask == TASK_WAIT_FOR_MOVEMENT ) )
	{
		SetCondition( COND_STEALTH_NEW_SOUND );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	BaseClass::ModifyOrAppendCriteria( criteriaSet );

	criteriaSet.AppendCriteria( "bodies_found", GetStealthSenses()->GetNumBodiesFound() );

	if ( m_hSuspiciousTarget )
	{
		// Add contexts from whatever we're inspecting
		m_hSuspiciousTarget->AppendContextToCriteria( criteriaSet );

		if ( g_hStealthManager )
		{
			StealthObjectState_t *pObjState = g_hStealthManager->GetStealthObjectState( m_hSuspiciousTarget );
			if ( pObjState )
			{
				criteriaSet.AppendCriteria( "times_found", pObjState->nTimesFound );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::MarkAsSeen( CBaseEntity *pEntity )
{
	if ( g_hStealthManager )
		g_hStealthManager->AddSeenObject( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnSeeEntity( CBaseEntity *pEntity )
{
	if ( g_hStealthManager )
	{
		if (pEntity->GetFlags() & FL_OBJECT)
		{
			switch ( g_hStealthManager->GetStealthObjectType( pEntity ) )
			{
				case STEALTH_OBJ_RAGDOLL:
					OnSeeRagdoll( pEntity );
					break;

				case STEALTH_OBJ_DOOR:
					OnSeeDoor( pEntity );
					break;

				case STEALTH_OBJ_PROP:
					OnSeeProp( pEntity );
					break;

				case STEALTH_OBJ_PROP_PICKUP:
					OnSeeProp( pEntity, true );
					break;

				case STEALTH_OBJ_LASER_DOT:
					OnSeeLaserDot( pEntity );
					break;
			}
		}
		else if (pEntity->IsNPC() && pEntity->MyNPCPointer()->GetSquad() == GetOuter()->GetSquad() && GetOuter()->GetSquad())
		{
			g_hStealthManager->UpdateSeenSquadMember( pEntity->MyNPCPointer(), GetOuter() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnSeeRagdoll( CBaseEntity *pEntity )
{
	// TODO: This currently won't work with peacekeeper elites due to them being CLASS_COMBINE, while regular
	// peacekeepers are CLASS_PLAYER_ALLY.
	// That can be amended if they're ever moved to code
	CRagdollProp *pRagdoll = assert_cast<CRagdollProp *>(pEntity);
	if ( pRagdoll && GetOuter()->GetDefaultRelationshipDisposition( pRagdoll->GetSourceClassification() ) )
	{
		if (g_hStealthManager)
			g_hStealthManager->NPCFoundBody( GetOuter(), pEntity );

		AI_CriteriaSet modifiers;
		
		// Check if the body is moving. If it is, we should be VERY concerned
		bool bIsMoving = GetStealthSenses()->IsCuriousObjectMoving( pEntity );
		if (bIsMoving)
		{
			// TODO: Should instead alert immediately if it's close enough
			GetStealthSenses()->IncrementBodiesFound();	// Add another to the count to make them alarm faster (for now)
			modifiers.AppendCriteria( "obj_moving", "1" );
		}

		m_hSuspiciousTarget = pEntity;
		
		SetSpeechTarget( pEntity );
		SpeakStealthConcept( TLK_FOUND_BODY, &modifiers );

		if (GetOuter()->GetState() == NPC_STATE_IDLE || GetOuter()->GetState() == NPC_STATE_ALERT)
		{
			// A body!?!?!? WTF!?!?!!?!?
			GetOuter()->SetIdealState( NPC_STATE_ALERT );
			InsertStealthSound( SOUND_COMBAT | SOUND_CONTEXT_REACT_TO_SOURCE, pEntity->GetAbsOrigin(), 1024, 2.0f, pEntity, SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY, GetOuter() );
		
			CAI_StealthCuriousBehavior *pBehavior;
			if ( GetOuter()->GetBehavior( &pBehavior ) )
				pBehavior->m_bCalmSound = false;	// Start running to the sound if we're already walking
		}
				
		GetStealthSenses()->IncrementBodiesFound();
	
		// Apply to all pSquadmates within alert radius of the body
		if (GetOuter()->GetSquad())
		{
			CAI_Squad *pSquad = GetOuter()->GetSquad();
			AISquadIter_t iter;
			for ( CAI_BaseNPC *pSquadmate = pSquad->GetFirstMember(&iter); pSquadmate; pSquadmate = pSquad->GetNextMember(&iter) )
			{
				if (pSquadmate == GetOuter() || !pSquadmate->IsAlive())
					continue;
			
				if (!pSquadmate->IsUsingStealthSenses())
					continue;
				
				if ((pSquadmate->GetAbsOrigin() - pEntity->GetAbsOrigin()).LengthSqr() > Square(1024.0f))
					continue;
				
				pSquadmate->GetStealthSenses()->IncrementBodiesFound();

				//printl("Incrementing pSquadmate body found")
			}
		}
	}
	
	MarkAsSeen( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnSeeDoor( CBaseEntity *pEntity )
{
	//	// Door that's moving without anyone being visible
	//	if (GetOuter()->GetState() == NPC_STATE_IDLE || GetOuter()->GetState() == NPC_STATE_ALERT)
	//	{
	//		printl("Investigating door")
	//		GetOuter()->AcceptInput("SpeakResponseConcept", TLK_FOUND_DOOR, GetOuter(), GetOuter())
	//		InsertAISound(SOUND_COMBAT, pEntity->GetOrigin(), 1024, 2.0, pEntity, SOUNDENT_CHANNEL_DISCOVERED_OPEN_DOOR, null)
	//	}

	CuriousDbgMsg( "OnSeeDoor\n" );

	if ( GetStealthSenses()->HasStealthFlags( STEALTH_F_DONT_NOTICE_DOOR ) )
	{
		CuriousDbgMsg( "- Set to ignore doors\n" );
		return;
	}

	if ( g_hStealthManager )
	{
		CBasePropDoor *pDoor = assert_cast<CBasePropDoor *>(pEntity);

		if ( pDoor->IsDoorOpening() || pDoor->IsDoorClosing() )
		{
			CuriousDbgMsg( "- Door is opening or closing\n" );

			// See if it's associated with an area with an interior position
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForDoor( pDoor );
			if (pArea)
			{
				Vector vecTargetPosition;
				if ( !pArea->IsTouching( GetOuter() ) )
				{
					CuriousDbgMsg( "-- Has interior position\n" );
					vecTargetPosition = pArea->GetInteriorPosition( GetOuter(), pDoor );
				}
				else
				{
					// TODO: Proper implementation of GetExitPosition()
					CuriousDbgMsg( "-- Has exterior position\n" );
					vecTargetPosition = pDoor->WorldSpaceCenter(); // pArea->GetExitPosition( pDoor->WorldSpaceCenter() );
				}


				if ( GetEnemy() && !HasCondition( COND_SEE_ENEMY ) )
				{
					CuriousDbgMsg( "--- Has enemy and can't see it\n" );

					// Aha! Our enemy must've gone inside!
					GetOuter()->UpdateEnemyMemory( GetEnemy(), vecTargetPosition, pDoor );

					AI_CriteriaSet modifiers;
					SetSpeechTarget( pEntity );

					// Even though we didn't fully lose the enemy, this calls out that they've been seen again
					// and that the door was responsible for that
					modifiers.AppendCriteria( "spotted", "door_moving" );
					SpeakStealthConcept( TLK_REFINDENEMY, &modifiers );
				}
				else
				{
					CuriousDbgMsg( "--- Just emitting combat sound\n" );

					// Emit a combat sound in the area
					InsertStealthSound( SOUND_COMBAT, vecTargetPosition, 2048, 2.0f, pEntity, SOUNDENT_CHANNEL_STEALTH_PROP_MOVING, GetOuter() );
				}
			}
		}
		else
		{
			int iIndex = 0;
			bool bShouldBeOpen = false;
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForDoor( pDoor, &iIndex, &bShouldBeOpen );
			if (pArea)
			{
				CuriousDbgMsg( "- Door is meant to be %s\n", bShouldBeOpen ? "Open" : "Closed" );

				// If we have a memory of what this door is meant to be, use that instead
				StealthAreaMemory_t *pMemory = GetStealthSenses()->GetAreaMemory( pArea );
				if ( pMemory )
				{
					if ( pMemory->iDoorState & (1 << iIndex) )
						bShouldBeOpen = true;
					else
						bShouldBeOpen = false;

					CuriousDbgMsg( "-- I've been here before, and the door was %s\n", bShouldBeOpen ? "Open" : "Closed" );
				}

				if ( ( !bShouldBeOpen && pDoor->IsDoorOpen() ) || ( bShouldBeOpen && !pDoor->IsDoorOpen() ) )
				{
					CuriousDbgMsg( "--- Door is not %s\n", bShouldBeOpen ? "Open" : "Closed" );

					Vector vecSoundPosition = pEntity->WorldSpaceCenter();
					const Vector *pInteriorPosition = g_hStealthManager->GetInteriorPositionThroughDoor( GetOuter(), pDoor );
					if (pInteriorPosition)
						vecSoundPosition = *pInteriorPosition;

					AI_CriteriaSet modifiers;
					modifiers.AppendCriteria( "door_expected_state", bShouldBeOpen ? "open" : "close" );

					SetSpeechTarget( pEntity );

					// Door is not in the state it's meant to be in
					if ( GetEnemy() && !HasCondition( COND_SEE_ENEMY ) )
					{
						CuriousDbgMsg( "---- Chasing enemy through door\n" );

						// Aha! Our enemy must've gone inside!
						GetOuter()->UpdateEnemyMemory( GetEnemy(), vecSoundPosition, pDoor );

						// Even though we didn't fully lose the enemy, this calls out that they've been seen again
						// and that the door was responsible for that
						modifiers.AppendCriteria( "spotted", "door_different" );
						SpeakStealthConcept( TLK_REFINDENEMY, &modifiers );
					}
					else
					{
						CuriousDbgMsg( "---- Just emitting world sound\n" );

						// Emit a world sound in the area
						InsertStealthSound( SOUND_WORLD, vecSoundPosition, 2048, 2.0f, pEntity, SOUNDENT_CHANNEL_STEALTH_DISCOVERED_OPEN_DOOR, GetOuter() );

						SpeakStealthConcept( TLK_FOUND_DOOR, &modifiers, true );
					}
				}
				else
					CuriousDbgMsg( "--- Door is indeed %s\n", bShouldBeOpen ? "Open" : "Closed" );
			}
		}

		g_hStealthManager->NPCFindOpenDoor( GetOuter(), pDoor );
	}

	MarkAsSeen( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnSeeProp( CBaseEntity *pEntity, bool bPickup )
{
	CuriousDbgMsg( "OnSeeProp\n" );

	if ( GetStealthSenses()->HasStealthFlags( STEALTH_F_DONT_NOTICE_PROP ) )
	{
		CuriousDbgMsg( "- Set to ignore props\n" );
		return;
	}

	if (GetOuter()->GetState() != NPC_STATE_COMBAT)
	{
		bool bIsMoving = GetStealthSenses()->IsCuriousObjectMoving( pEntity );

		if (bIsMoving)
		{
			AI_CriteriaSet modifiers;
			modifiers.AppendCriteria( "obj_moving", "1" );

			m_hSuspiciousTarget = pEntity;

			CuriousDbgMsg( "- Commenting on moving prop\n" );

			SetSpeechTarget( pEntity );
			SpeakStealthConcept( TLK_FOUND_PROP, &modifiers );
			
			// Very suspicious
			InsertStealthSound( SOUND_COMBAT | SOUND_CONTEXT_REACT_TO_SOURCE, pEntity->GetAbsOrigin(), 1024, 2.0f, pEntity, SOUNDENT_CHANNEL_STEALTH_PROP_MOVING, GetOuter() );

			MarkAsSeen( pEntity );
		}
		else
		{
			m_hSuspiciousTarget = pEntity;

			CuriousDbgMsg( "- Commenting on prop\n" );

			SetSpeechTarget( pEntity );
			SpeakStealthConcept( TLK_FOUND_PROP );

			if (bPickup)
			{
				if (GetOuter()->GetState() == NPC_STATE_IDLE)
				{
					// HandleAnimEvent() will take care of this for us
					SetTarget( pEntity );
					GetOuter()->SetSchedule( SCHED_GET_HEALTHKIT );
				}
			}
			else if (g_hStealthManager && g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET, STEALTH_LEVEL_GUARD ))
			{
				InsertStealthSound( SOUND_WORLD | SOUND_CONTEXT_REACT_TO_SOURCE, pEntity->GetAbsOrigin(), 512, 2.0f, pEntity, SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING, GetOuter() );
			}

			MarkAsSeen( pEntity );
		}
	}
	else
	{
		MarkAsSeen( pEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnSeeLaserDot( CBaseEntity *pEntity )
{
	// Only see laser dots that are on
	if ( pEntity->GetEffects() & EF_NODRAW )
		return;

	CBaseEntity *pOwner = pEntity->GetOwnerEntity();
	if ( pOwner && GetOuter()->IRelationType( pOwner ) == D_LI )
		return;

	CuriousDbgMsg( "OnSeeLaserDot\n" );

	if ( g_hStealthManager )
	{
		/*if ( g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET ) )
		{
			// TODO: Something quiet to do
		}
		else*/ if ( GetOuter()->GetState() != NPC_STATE_COMBAT )
		{
			m_hSuspiciousTarget = pEntity;

			CuriousDbgMsg( "- Calling out laser sight danger\n" );

			if ( pOwner )
			{
				// Create a temporary target for two reasons:
				// 1. The player could move away quickly
				// 2. Companions look to sounds from origin, which can be obstructed by a ledge
				// TODO: Also make them acquire it as an enemy briefly?
				CBaseEntity *pTarget = GetOuter()->CreateCustomTarget( pOwner->EyePosition(), 5.1f );
				if (pTarget)
				{
					pTarget->SetOwnerEntity( pOwner );
					pOwner = pTarget;
				}
			}

			// SOUND_CONTEXT_REACT_TO_SOURCE makes us take cover from the weapon
			InsertStealthSound( SOUND_DANGER | SOUND_CONTEXT_REACT_TO_SOURCE, pEntity->GetAbsOrigin(), 2000, 5.0f, pOwner );
		}
		else
		{
			// Notice the source
			if ( pOwner )
			{
				GetOuter()->UpdateEnemyMemory( pOwner, pOwner->GetAbsOrigin() );
			}
		}
	}

	MarkAsSeen( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::HandleAnimEvent( animevent_t *pEvent )
{
	switch (pEvent->event)
	{
		case NPC_EVENT_ITEM_PICKUP:
			{
				if ( IsCurSchedule( SCHED_GET_HEALTHKIT, false ) && m_hSuspiciousTarget )
				{
					// "Remove" the suspicious target
					UTIL_Remove( m_hSuspiciousTarget );
					m_hSuspiciousTarget = NULL;
					TaskComplete();
					return;
				}
				break;
			}
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthCuriousBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	switch ( nBase )
	{
		case SCHED_ALERT_FACE_BESTSOUND:
			{
				// In lieu of a ShouldInvestigateSounds() override for behaviors
				if ( HasCondition( COND_STEALTH_SOUND_UNREACHABLE ) )
				{
					if ( g_debug_stealth_investigate.GetInt() == 2 )
						GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 3.0f,
							"Sound unreachable" );

					return SCHED_ALERT_FACE_BESTSOUND;
				}
			}
			break;
		case SCHED_INVESTIGATE_SOUND:
			{
				if ( HasCondition( COND_STEALTH_SOUND_UNREACHABLE ) )
				{
					if ( g_debug_stealth_investigate.GetInt() == 2 )
						GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 3.0f,
							"Sound unreachable" );

					return SCHED_ALERT_FACE_BESTSOUND;
				}

				CSound *pSound = GetOuter()->GetBestSound();
				if (pSound /*&& CAI_StealthSenses::IsStealthSound(pSound)*/)
				{
					if ( CAI_StealthSenses::IsCalmStealthSound( pSound ) && !GetOuter()->OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
					{
						if ( g_debug_stealth_investigate.GetInt() == 2 )
							GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 3.0f,
								"Don't have investigate slot" );

						return SCHED_ALERT_FACE_BESTSOUND;
					}

					if ( ShouldStayAtSound( pSound ) )
						return SCHED_STEALTH_INVESTIGATE_SOUND_STAY;

					return SCHED_STEALTH_INVESTIGATE_SOUND;
				}
			}
			break;
		case SCHED_ALERT_STAND:
			{
				// Always patrol when alert (fall through into next to see if we have a search point)
				nBase = SCHED_PATROL_WALK;
			}
			break;
	}

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthCuriousBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( IsInvestigatingSound() )
	{
		SetCondition( COND_STEALTH_SOUND_UNREACHABLE );
		return SCHED_ALERT_FACE_BESTSOUND;
	}
	else if ( failedSchedule == SCHED_PATROL_WALK )
	{
		return SCHED_IDLE_WANDER;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if ( IsInvestigatingSound() )
	{
		GetOuter()->SetCustomInterruptCondition( GetClassScheduleIdSpace()->ConditionLocalToGlobal( COND_STEALTH_NEW_SOUND ) );
		GetOuter()->SetCustomInterruptCondition( COND_HEAR_DANGER );
	}
	else if ( IsCurSchedule( SCHED_GET_HEALTHKIT, false ) )
	{
		// Normally has no interrupts. Give it the standard set for investigating a sound
		GetOuter()->SetCustomInterruptCondition( GetClassScheduleIdSpace()->ConditionLocalToGlobal( COND_STEALTH_NEW_SOUND ) );
		GetOuter()->SetCustomInterruptCondition( COND_NEW_ENEMY );
		GetOuter()->SetCustomInterruptCondition( COND_SEE_ENEMY );
		GetOuter()->SetCustomInterruptCondition( COND_LIGHT_DAMAGE );
		GetOuter()->SetCustomInterruptCondition( COND_HEAVY_DAMAGE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	if ( g_debug_stealth_investigate.GetInt() == 2 &&
		( GetOuter()->HasCondition( COND_HEAR_COMBAT ) || GetOuter()->HasCondition( COND_HEAR_PLAYER ) || GetOuter()->HasCondition( COND_HEAR_WORLD ) ) )
	{
		CSound *pSound = GetOuter()->GetBestSound();
		if ( pSound )
		{
			const char *pszSoundType = "Unknown";
			const char *pszSoundName = "[Not Stealth]";
			switch (pSound->SoundTypeNoContext())
			{
				case SOUND_COMBAT:
					pszSoundType = "Combat";
					break;
				case SOUND_PLAYER:
					pszSoundType = "Player";
					break;
				case SOUND_WORLD:
					pszSoundType = "World";
					break;
			}

			if ( CAI_StealthSenses::IsStealthSound( pSound ) )
				pszSoundName = CAI_StealthSenses::GetStealthSoundChannelName( pSound->SoundChannel() );

			GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ), Color( 255, 255, 128 ), 0.3f,
				"Running %s (Hear %s sound, %s)", GetOuter()->GetCurSchedule() ? GetOuter()->GetCurSchedule()->GetName() : "N/A", pszSoundType, pszSoundName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthCuriousBehavior::CanSelectSchedule( void )
{
	if ( !GetOuter()->IsUsingStealthSenses() )
		return false;

	if ( GetOuter()->GetState() != NPC_STATE_IDLE && GetOuter()->GetState() != NPC_STATE_ALERT )
		return false;

	CAI_StealthAlarmBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		// Defer to alarm behavior if there's an alarm we should raise
		if ( pBehavior->ShouldRaiseAlarm() )
			return false;
	}

	if ( !HasCondition( COND_HEAR_COMBAT ) && !HasCondition( COND_HEAR_BULLET_IMPACT ) )
	{
		CAI_ActBusyBehavior *pBehavior;
		if ( GetOuter()->GetBehavior( &pBehavior ) )
		{
			if ( pBehavior->NeedsToResume() )
				return false;
		}

		CAI_TripminePlaceBehavior *pTripmineBehavior;
		if ( GetOuter()->GetBehavior( &pTripmineBehavior ) )
		{
			// TODO: Need to have something more specific than "is tripmine capable"
			if ( pTripmineBehavior->IsTripmineCapable() )
				return false;
		}
	}

	return BaseClass::CanSelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	m_bCalmSound = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::OnStartSchedule( int scheduleType )
{
	BaseClass::OnStartSchedule( scheduleType );

	if ( scheduleType == SCHED_INVESTIGATE_SOUND ||
		scheduleType == SCHED_STEALTH_INVESTIGATE_SOUND ||
		scheduleType == SCHED_STEALTH_INVESTIGATE_SOUND_STAY )
	{
		GetStealthSenses()->StartInvestigatingSound( GetOuter()->GetBestSound() );
		OnStartInvestigatingSound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::EndScheduleSelection( void )
{
	BaseClass::EndScheduleSelection();

	m_bCalmSound = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_GET_PATH_TO_BESTSOUND:
			{
				CSound *pSound = GetOuter()->GetBestSound();
				if ( !pSound )
				{
					TaskFail( FAIL_NO_SOUND );
				}
				else
				{
					bool bGoToSource = ShouldGoToSoundSource( pSound );
					AI_NavGoal_t goal( pSound->GetSoundReactOrigin() );

					if ( CAI_StealthSenses::IsStealthSound( pSound ) )
					{
						goal.tolerance = CAI_StealthSenses::GetSoundStopDistance( pSound );
						if (goal.tolerance == 0.0f)
							goal.tolerance = AIN_HULL_TOLERANCE;

						m_bCalmSound = CAI_StealthSenses::IsCalmStealthSound( pSound );
						m_nSoundChannel = pSound->SoundChannel();
						m_flSoundExpireTime = pSound->SoundExpirationTime();

						if ( m_bCalmSound )
						{
							if ( m_nNumTimesInvestigatedSound > ai_stealth_investigate_repeat_run_threshold.GetInt() &&
								(gpGlobals->curtime - m_flLastTimeHeardSound) < ai_stealth_investigate_repeat_stay_time.GetFloat() )
							{
								// Too many "calm" moments
								m_bCalmSound = false;
							}
							else if ( pSound->SoundChannel() != SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS &&
								m_nNumTimesInvestigatedSound < ai_stealth_investigate_repeat_goto_threshold.GetInt() )
							{
								bGoToSource = false;
							}
						}

						m_nNumTimesInvestigatedSound++;

						if ( pSound->SoundContext() & SOUND_CONTEXT_REACT_TO_SOURCE && pSound->m_hOwner )
						{
							if ( pSound->m_hTarget == GetOuter() )
							{
								goal.type = GOALTYPE_TARGETENT;
								goal.pTarget = pSound->m_hOwner;
								goal.dest = AIN_NO_DEST;
								
								if ( g_debug_stealth_investigate.GetInt() == 2 )
									GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 5.0f,
										"Moving to sound source (stop dist %.2f)", goal.tolerance );
							}
							else
							{
								// If we're not the target, we're a bystander
								bGoToSource = false;
							}

							SetTarget( pSound->m_hOwner );
						}
						else if ( !GetOuter()->OccupyStrategySlot( SQUAD_SLOT_INVESTIGATE_SOUND ) )
						{
							// Don't go directly to the source if we're not the main investigator
							bGoToSource = false;
						}
						else if ( g_debug_stealth_investigate.GetInt() == 2 )
							GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 5.0f,
								"Moving to sound react origin (stop dist %.2f)", goal.tolerance );
					}
					else
					{
						m_bCalmSound = pSound->IsSoundType( SOUND_PLAYER );
						m_nSoundChannel = pSound->SoundChannel();
						m_flSoundExpireTime = pSound->SoundExpirationTime();
					}

					if ( goal.type != GOALTYPE_INVALID )
					{
						if ( !bGoToSource && goal.dest != AIN_NO_DEST )
						{
							// Find LOS instead of going directly to the sound
							if ( !GetOuter()->FVisible( goal.dest ) )
							{
								Vector vecPosLOS = vec3_origin;
								if ( /*GetOuter()->GetTacticalServices()->FindLateralLos( goal.dest, &vecPosLOS ) ||*/
									GetTacticalServices()->FindLos( goal.dest, goal.dest + Vector(0,0,32), 0.0f, pSound->Volume(), 1.0f, &vecPosLOS ))
								{
									Vector vecDir = (goal.dest - vecPosLOS);
									VectorNormalize( vecDir );
									GetNavigator()->SetArrivalDirection( vecDir );

									goal.dest = vecPosLOS;
									goal.tolerance = 16.0f;
								
									if ( g_debug_stealth_investigate.GetInt() == 2 )
										GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 10.0f,
											"Not going to sound source - Finding LOS to sound" );
								}
							}
							else
							{
								// Already have LOS, don't move
								if ( g_debug_stealth_investigate.GetInt() == 2 )
									GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 10.0f,
										"Not going to sound source - Deciding not to move" );

								TaskFail( FAIL_NO_ROUTE_GOAL );
								break;
							}
						}

						if ( !GetNavigator()->SetGoal( goal ) && bGoToSource )
						{
							// If the direct sound origin is different, try going to that instead
							// e.g. going to the bullet impact instead of going to who caused it)
							if ( pSound->GetSoundOrigin() != goal.dest )
							{
								Vector vecTemp = goal.dest;
								goal.dest = pSound->GetSoundOrigin();
								if ( GetNavigator()->SetGoal( goal ) )
								{
									// HACKHACK: SetGoal() fails the task if it fails, so we have to override that for our fallback goal
									// This is based on a hack in CAI_AssaultBehavior for the same problem
									ClearCondition( COND_TASK_FAILED );
									TaskComplete();
								}
								else
								{
									goal.dest = vecTemp;
								}
							}

							// If that didn't work, then try finding LOS to the sound instead
							if ( !TaskIsComplete() && !GetOuter()->FVisible( goal.dest ) )
							{
								Vector vecPosLOS = vec3_origin;
								if ( /*GetOuter()->GetTacticalServices()->FindLateralLos( goal.dest, &vecPosLOS ) ||*/
									GetTacticalServices()->FindLos( goal.dest, goal.dest + Vector(0,0,48), 0.0f, pSound->Volume(), 1.0f, &vecPosLOS ))
								{
									Vector vecDir = (goal.dest - vecPosLOS);
									VectorNormalize( vecDir );
									GetNavigator()->SetArrivalDirection( vecDir );

									goal.dest = vecPosLOS;
									goal.tolerance = 16.0f;
								
									if ( g_debug_stealth_investigate.GetInt() == 2 )
										GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 1, Color( 255, 255, 128 ), 10.0f,
											"Couldn't get to sound source - Finding LOS to sound" );

									if ( GetNavigator()->SetGoal( goal ) )
									{
										// HACKHACK: SetGoal() fails the task if it fails, so we have to override that for our fallback goal
										// This is based on a hack in CAI_AssaultBehavior for the same problem
										ClearCondition( COND_TASK_FAILED );
										TaskComplete();
									}
								}
							}
						}

						if ( TaskIsComplete() )
						{
							// Wait for TASK_STEALTH_BESTSOUND_PAUSE to get a proper value
							GetOuter()->DelayMoveStart( 0.3f );
						}
					}
					else
						TaskComplete();
				}
			}
			break;

		case TASK_STEALTH_BESTSOUND_PAUSE:
			{
				float flWaitTime = 0.0f;

				if ( m_nSoundChannel != SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY ) // m_bCalmSound
				{
					flWaitTime = ai_stealth_investigate_wait.GetFloat();

					if ( m_flLastTimeHeardSound != -1.0f )
					{
						// Account for rapid succession
						float flTimePassed = (gpGlobals->curtime - m_flLastTimeHeardSound);
						if (flTimePassed > 0.5f)
							flWaitTime *= flTimePassed / ai_stealth_investigate_wait_cooldown.GetFloat();
					}

					m_flLastTimeHeardSound = gpGlobals->curtime;

					if ( GetOuter()->GetState() == NPC_STATE_ALERT )
					{
						flWaitTime *= 0.5f;
					}
				}

				if (flWaitTime > 0.3f)
				{
					// Clamp the wait time
					if (flWaitTime > 5.0f)
						flWaitTime = 5.0f;

					GetOuter()->SetWait( flWaitTime );
					GetOuter()->DelayMoveStart( flWaitTime );
					
					if ( g_debug_stealth_investigate.GetInt() == 2 )
						GetStealthSenses()->EntityPrint( GetStealthSenses()->GetOffsetForDebugType( STEALTH_SENSE_DEBUG_LINE_INVESTIGATE ) + 2, Color( 255, 255, 128 ), flWaitTime + 2.0f,
							"Pausing for %.2f", flWaitTime );
				}
				else
				{
					TaskComplete();
				}
			}
			break;

		case TASK_STEALTH_MOVE_TO_BESTSOUND:
			{
				if ( m_bCalmSound )
				{
					ChainStartTask( TASK_WALK_PATH );
				}
				else
				{
					ChainStartTask( TASK_RUN_PATH );
				}
			}
			break;

		case TASK_STEALTH_SUGGEST_STATE_FOR_SOUND:
			{
				// Become idle when returning if we're quiet and it wasn't an important sound
				if ( g_hStealthManager && g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET ) && GetNpcState() == NPC_STATE_ALERT )
				{
					switch ( GetStealthSenses()->GetLastSoundChannel() )
					{
						case SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS:
						case SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT:
						case SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK:
							{
								GetOuter()->SetIdealState( NPC_STATE_IDLE );
								break;
							}
					}
				}

				TaskComplete();
			}
			break;

		default:
			BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthCuriousBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_BESTSOUND_PAUSE:
			{
				if ( GetTarget() )
				{
					if ( GetTarget()->VPhysicsGetObject() )
					{
						Vector vecVelocity;
						GetTarget()->VPhysicsGetObject()->GetVelocity( &vecVelocity, NULL );

						// Wait until it's not moving
						bool bIsMoving = vecVelocity.LengthSqr() > Square( 16.0f );
						if ( bIsMoving )
							break;
						else
						{
							// Ensure that its position has been updated
							MarkAsSeen( GetTarget() );
						}
					}

					// Face the target
					GetMotor()->SetIdealYawToTargetAndUpdate( GetTarget()->GetAbsOrigin() );
				}
				else if ( GetNavigator()->IsGoalSet() )
				{
					GetMotor()->SetIdealYawToTargetAndUpdate( GetNavigator()->GetGoalPos() );
				}

				if ( GetOuter()->IsWaitFinished() )
				{
					TaskComplete();
				}
			}
			break;

		case TASK_WAIT_FOR_MOVEMENT:
			{
				if ( IsCurSchedule( SCHED_STEALTH_INVESTIGATE_SOUND, false ) || IsCurSchedule( SCHED_STEALTH_INVESTIGATE_SOUND_STAY, false ) )
				{
					if ( GetNavigator()->BuildAndGetPathDistToGoal() <= GetNavigator()->GetGoalTolerance() && GetOuter()->FVisible( GetNavigator()->GetGoalPos() ) )
					{
						TaskComplete();
						GetNavigator()->ClearGoal();

						AI_CriteriaSet modifiers;

						if ( GetTarget() )
						{
							SetSpeechTarget( GetTarget() );

							// UNDONE: Get a simplified model name
							// (we now use m_iszLastSoundModel for this)
							//modifiers.AppendCriteria( "speechtarget_modelname", V_GetFileName( STRING( GetTarget()->GetModelName() ) ) );
						}
						else
						{
							SetSpeechTarget( NULL );
						}

						SpeakStealthConcept( TLK_INSPECT_OBJECT, &modifiers );
						break;
					}
				}
				BaseClass::RunTask( pTask );
			}
			break;

		default:
			BaseClass::RunTask( pTask );
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_StealthCuriousBehavior )

	DECLARE_CONDITION( COND_STEALTH_SOUND_UNREACHABLE )
	DECLARE_CONDITION( COND_STEALTH_NEW_SOUND )

	DECLARE_TASK( TASK_STEALTH_GET_PATH_TO_BESTSOUND )
	DECLARE_TASK( TASK_STEALTH_BESTSOUND_PAUSE )
	DECLARE_TASK( TASK_STEALTH_MOVE_TO_BESTSOUND )
	DECLARE_TASK( TASK_STEALTH_SUGGEST_STATE_FOR_SOUND )

	//---------------------------------
	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_INVESTIGATE_SOUND,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_STORE_LASTPOSITION				0"
		"		TASK_STEALTH_GET_PATH_TO_BESTSOUND	0"
		"		TASK_FACE_IDEAL						0"
		"		TASK_STEALTH_BESTSOUND_PAUSE		0"
		"		TASK_STEALTH_MOVE_TO_BESTSOUND		0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_REASONABLE				0"
		"		TASK_WAIT							5"
		"		TASK_STEALTH_SUGGEST_STATE_FOR_SOUND	0"
		"		TASK_GET_PATH_TO_LASTPOSITION		0"
		"		TASK_WALK_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_STOP_MOVING					0"
		"		TASK_CLEAR_LASTPOSITION				0"
		"		TASK_FACE_REASONABLE				0"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	);
	
	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_INVESTIGATE_SOUND_STAY,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_STEALTH_GET_PATH_TO_BESTSOUND	0"
		"		TASK_FACE_IDEAL						0"
		"		TASK_STEALTH_BESTSOUND_PAUSE		0"
		"		TASK_STEALTH_MOVE_TO_BESTSOUND		0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_REASONABLE				0"
		"		TASK_WAIT							5"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
	);

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_WANDER,

		"	Tasks"
	//	"		TASK_SET_TOLERANCE_DISTANCE		48"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1 "
		"		COND_CAN_RANGE_ATTACK2 "
		"		COND_CAN_MELEE_ATTACK1 "
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_GIVE_WAY"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_SMELL"
		"		COND_PROVOKED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()
