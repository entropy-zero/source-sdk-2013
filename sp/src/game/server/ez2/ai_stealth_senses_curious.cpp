//=============================================================================//
//
// Purpose:		AI behavior for advanced curiosity features.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_senses_curious.h"
#include "ai_stealth_behavior_curious.h"
#include "ai_stealth_behavior_alarm.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_area.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_senses.h"
#include "ai_tacticalservices.h"
#include "ai_behavior_actbusy.h"
#include "ai_playerally.h"
#include "npcevent.h"
#include "physics_prop_ragdoll.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CAI_CuriousStealthSenses )

	DEFINE_FIELD( m_nBodiesFound, FIELD_INTEGER ),

	DEFINE_FIELD( m_bInvestigatingStealth, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nInvestigatingSoundIdx, FIELD_INTEGER ),

	DEFINE_FIELD( m_vecLastSoundLocation, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_nLastSoundType, FIELD_INTEGER ),
	DEFINE_FIELD( m_nLastSoundChannel, FIELD_INTEGER ),
	DEFINE_FIELD( m_hLastSoundOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flLastSoundTime, FIELD_TIME ),

	DEFINE_FIELD( m_nLastDamageType, FIELD_INTEGER ),

END_DATADESC();

//-----------------------------------------------------------------------------

CAI_CuriousStealthSenses::CAI_CuriousStealthSenses( CAI_BaseNPC *pOuter )
	: CAI_StealthSenses( pOuter )
{
	m_nBodiesFound = 0;

	m_bInvestigatingStealth = false;
	m_nInvestigatingSoundIdx = SOUNDLIST_EMPTY;

	m_vecLastSoundLocation = vec3_origin;
	m_nLastSoundType = 0;
	m_nLastSoundChannel = 0;
	m_hLastSoundOwner = NULL;
	m_flLastSoundTime = 0.0f;

	m_nLastDamageType = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::InitStealthSenses()
{
	BaseClass::InitStealthSenses();

	if ( GetOuter()->IsInSquad() )
	{
		InitSquad( GetOuter()->GetSquad() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::InitSquad( CAI_Squad *pSquad )
{
	BaseClass::InitSquad( pSquad );

	GetStealthSquadInfo();
	GetStealthSquadMemberInfo();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::OnDamagedByAttacker( const CTakeDamageInfo &info )
{
	m_nLastDamageType = info.GetDamageType();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::OnListened()
{
	if (GetOuter()->HasCondition(COND_HEAR_COMBAT) || GetOuter()->HasCondition(COND_HEAR_DANGER))
	{
		CSound *pBestSound = GetOuter()->GetBestSound( SOUND_COMBAT | SOUND_DANGER );
		if (IsStealthSound(pBestSound))
		{
			m_bInvestigatingStealth = true;
		}
		else if (m_bInvestigatingStealth)
		{
			m_bInvestigatingStealth = false;
			if (IsInvestigatingSound() || GetOuter()->IsCurSchedule( SCHED_ALERT_FACE_BESTSOUND ))
			{
				//printl("Interrupting stealth")
				GetOuter()->SetCondition( COND_HEAR_BULLET_IMPACT );
			}
		}
			
		if (GetOuter()->GetState() == NPC_STATE_ALERT)
		{
			if ((IsInvestigatingSound() || GetOuter()->IsCurSchedule( SCHED_ALERT_FACE_BESTSOUND )))
			{
				CSound *pInvestigatingSound = GetInvestigatingSound();
				if ( pInvestigatingSound )
				{
					AISoundIter_t iter;
					CSound *pCurrentSound = GetOuter()->GetSenses()->GetFirstHeardSound( &iter );
					while ( pCurrentSound )
					{
						if (!pCurrentSound->IsSoundType(SOUND_COMBAT | SOUND_DANGER) && pCurrentSound->SoundChannel() != SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS)
						{
							pCurrentSound = GetOuter()->GetSenses()->GetNextHeardSound( &iter );
							continue;
						}
					
						if (pCurrentSound->SoundExpirationTime() > pInvestigatingSound->SoundExpirationTime())
						{
							//printl("Interrupting stealth to hear another best sound")
							pInvestigatingSound->ScriptFreeSound();
							StartInvestigatingSound( pCurrentSound );
							GetOuter()->SetCondition( COND_HEAR_BULLET_IMPACT );
							break;
						}

						pCurrentSound = GetOuter()->GetSenses()->GetNextHeardSound( &iter );
					}
				}
			}
			else if (IsRunningStartActBusy())
			{
				StopRunningActBusy();
			}
			
			if ( pBestSound->SoundChannel() != SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS )
			{
				if ( pBestSound->SoundContext() & SOUND_CONTEXT_REACT_TO_SOURCE && pBestSound->m_hOwner )
				{
					GetOuter()->AddLookTarget( pBestSound->m_hOwner, 0.8f, 5.0f, 0.0f );
				}
				else
				{
					GetOuter()->AddLookTarget( pBestSound->GetSoundReactOrigin(), 0.8f, 5.0f, 0.0f );
				}
			}
			
			//RaiseAlertLevelTo(bestSound.GetOwner(), 0.5)
		}

		CAI_StealthCuriousBehavior *pBehavior;
		if ( GetOuter()->GetBehavior( &pBehavior ) && pBehavior->IsInvestigatingSound() )
			pBehavior->OnHearNewSound( pBestSound );
		
		UpdateLastSound( pBestSound );

		if ( g_hStealthManager && pBestSound->SoundChannel() != SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS )
			g_hStealthManager->NPCHeardSuspiciousSound( GetOuter(), pBestSound );
	}
	else if (GetOuter()->GetState() != NPC_STATE_COMBAT && (GetOuter()->HasCondition( COND_HEAR_PLAYER ) || GetOuter()->HasCondition( COND_HEAR_WORLD )))
	{
		CSound *pBestSound = GetOuter()->GetBestSound( SOUND_PLAYER | SOUND_WORLD );
		if (pBestSound)
		{
			if ( pBestSound->SoundContext() & SOUND_CONTEXT_REACT_TO_SOURCE && pBestSound->m_hOwner )
			{
				GetOuter()->AddLookTarget( pBestSound->m_hOwner, 0.8f, 5.0f, 2.0f );
			}
			else
			{
				GetOuter()->AddLookTarget( pBestSound->GetSoundReactOrigin(), 0.8f, 5.0f, 2.0f );
			}

			CAI_StealthCuriousBehavior *pBehavior;
			if ( GetOuter()->GetBehavior( &pBehavior ) && pBehavior->IsInvestigatingSound() )
				pBehavior->OnHearNewSound( pBestSound );

			// TODO: Dedicated function for world sounds we don't want to interrupt (e.g. IsNonPrioritySound)
			//if ( GetOuter()->GetState() == NPC_STATE_IDLE && IsCalmStealthSound( pBestSound ) )
				//GetOuter()->ClearCondition( COND_HEAR_WORLD );
		
			UpdateLastSound( pBestSound );

			if ( g_hStealthManager )
				g_hStealthManager->NPCHeardSuspiciousSound( GetOuter(), pBestSound );
		}

		// Do not use COND_HEAR_WORLD for world stealth sounds, as it may interrupt some schedules
		// TODO: Dedicated function for world sounds we don't want to interrupt (e.g. IsNonPrioritySound)
		/*CSound *pBestSound = GetOuter()->GetBestSound( SOUND_WORLD );
		if ( IsCalmStealthSound( pBestSound ) )
		{
			GetOuter()->ClearCondition( COND_HEAR_WORLD );
		}*/
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSound *CAI_CuriousStealthSenses::GetInvestigatingSound()
{
	CSound *pSound = NULL;
	if (m_nInvestigatingSoundIdx != SOUNDLIST_EMPTY)
	{
		pSound = CSoundEnt::SoundPointerForIndex( m_nInvestigatingSoundIdx );
		if (!pSound)
			m_nInvestigatingSoundIdx = SOUNDLIST_EMPTY;
	}

	return pSound;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::StartInvestigatingSound( CSound *pSound )
{
	// Save the sound's index
	int nSound = CSoundEnt::ActiveList();
	while (nSound != SOUNDLIST_EMPTY)
	{
		CSound *pCurrentSound = CSoundEnt::SoundPointerForIndex( nSound );
		if ( pCurrentSound == pSound )
		{
			m_nInvestigatingSoundIdx = nSound;
			return;
		}

		nSound = pCurrentSound->NextSound();
	}

	m_nInvestigatingSoundIdx = SOUNDLIST_EMPTY;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_CuriousStealthSenses::IsInvestigatingSound()
{
	CAI_StealthCuriousBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) && pBehavior->IsInvestigatingSound() )
		return true;

	return BaseClass::IsInvestigatingSound();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_CuriousStealthSenses::IsCuriousObject( CBaseEntity *pEntity )
{
	if (!(pEntity->GetFlags() & FL_OBJECT) || pEntity->IsCombatCharacter())
		return false;

	if (!g_hStealthManager || !g_hStealthManager->ShouldSeeObject( pEntity ))
		return false;

	int nObjType = g_hStealthManager->GetStealthObjectType( pEntity );
	if ( nObjType == STEALTH_OBJ_NONE )
		return false;

	if ( nObjType == STEALTH_OBJ_PROP )
	{
		// Ignore props when investigating (also stops from getting caught in loop when investigating prop sounds)
		if ( IsInvestigatingSound() )
			return false;

		if ( !g_hStealthManager->ShouldPropBePerceivable( pEntity ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::ResetLastSound()
{
	m_flLastSoundTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_CuriousStealthSenses::IsLastSoundRelevant()
{
	if ( m_flLastSoundTime == 0.0f )
		return false;

	// Not from when we were idle
	if ( m_flLastSoundTime < ( GetOuter()->GetLastEnemyTime() + 10.0f ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	// Only use last sound criteria if we heard it while idle
	if ( IsLastSoundRelevant() )
	{
		if ( g_hStealthManager )
		{
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthAreaForPoint( m_vecLastSoundLocation );
			if ( pArea )
				set.AppendCriteria( "last_sound_area", pArea->GetAreaContext() );
		}

		set.AppendCriteria( "last_sound_type", UTIL_VarArgs( "%i", m_nLastSoundType ) );
		set.AppendCriteria( "last_sound_channel", UTIL_VarArgs( "%i", m_nLastSoundChannel ) );
		set.AppendCriteria( "last_sound_time", gpGlobals->curtime - m_flLastSoundTime );

		if ( m_hLastSoundOwner )
		{
			set.AppendCriteria( "last_sound_owner", m_hLastSoundOwner->GetClassname() );
			m_hLastSoundOwner->AppendContextToCriteria( set, "last_sound_owner_" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_CuriousStealthSenses::EvalAlertLevel( CBaseEntity *pTarget, const Vector &vecDelta, float flDot )
{
	if ( IsCuriousObject( pTarget ) )
	{
		if (pTarget->ClassMatches( "prop_physics" ))
		{
			// Only find curious props within a certain distance
			float flDistSqr = vecDelta.LengthSqr();
			if (flDistSqr > Square(512.0f))
				return false;
		}
	}

	return BaseClass::EvalAlertLevel( pTarget, vecDelta, flDot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::OnSpotEnemyFromAlert( CBaseEntity *pTarget, int i )
{
	CAI_StealthCuriousBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		if ( GetOuter()->GetExpresser()
			&& !CompareConcepts( GetOuter()->GetExpresser()->GetLastSpokeConcept(), TLK_STARTCOMBAT )
			&& !CompareConcepts( GetOuter()->GetExpresser()->GetLastSpokeConcept(), TLK_REFINDENEMY ) )
		{
			AI_CriteriaSet modifiers;
			modifiers.AppendCriteria( "stealth_spotted", "1" );
			modifiers.AppendCriteria( "stealth_alert_duration", gpGlobals->curtime - GetAlertLevel( i ).flStartTime );

			// This is done here so that we *always* call out enemies, even if we're already speaking
			pBehavior->SpeakStealthConcept( TLK_STARTCOMBAT, &modifiers, true );
		}
	}

	BaseClass::OnSpotEnemyFromAlert( pTarget, i );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_CuriousStealthSenses::ShouldEmitSawSuspicious( CBaseEntity *pTarget, int i )
{
	if ( pTarget->GetFlags() & FL_OBJECT && g_hStealthManager )
	{
		int nObjType = g_hStealthManager->GetStealthObjectType( pTarget );

		// We have unique handling for this that could result in not noticing
		if ( nObjType == STEALTH_OBJ_DOOR )
			return false;
	}

	return BaseClass::ShouldEmitSawSuspicious( pTarget, i );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::UpdateLastSound( CSound *pSound )
{
	m_vecLastSoundLocation = pSound->GetSoundReactOrigin();
	m_nLastSoundType = pSound->SoundType();
	m_nLastSoundChannel = pSound->SoundChannel();
	m_hLastSoundOwner = pSound->m_hOwner;
	m_flLastSoundTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthAreaMemory_t *CAI_CuriousStealthSenses::GetAreaMemory( CTriggerStealthArea *pArea )
{
	FOR_EACH_VEC( m_AreaMemories, i )
	{
		if ( m_AreaMemories[i].hArea == pArea )
			return &m_AreaMemories[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_CuriousStealthSenses::UpdateAreaMemory( CTriggerStealthArea *pArea, bool bLeaving )
{
	int i = -1;
	FOR_EACH_VEC( m_AreaMemories, j )
	{
		if ( m_AreaMemories[j].hArea == pArea )
		{
			i = j;
		}
	}

	if ( i == -1 )
	{
		i = m_AreaMemories.AddToTail();
		m_AreaMemories[i].hArea = pArea;
	}

	m_AreaMemories[i].flLastTimeEntered = gpGlobals->curtime;

	// UNDONE: Only update door state when leaving
	//if ( bLeaving )
	//	m_AreaMemories[i].iDoorState = pArea->GetDoorStateMask();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthSquadInfo_t *CAI_CuriousStealthSenses::GetStealthSquadInfo()
{
	if ( !g_hStealthManager || !GetOuter()->GetSquad() )
		return NULL;

	return g_hStealthManager->FindSquadInfo( GetOuter()->GetSquad() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthSquadMemberInfo_t *CAI_CuriousStealthSenses::GetStealthSquadMemberInfo()
{
	StealthSquadInfo_t *squadInfo = GetStealthSquadInfo();
	if ( squadInfo )
	{
		return g_hStealthManager->FindSquadMemberInfo( squadInfo, GetOuter() );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
