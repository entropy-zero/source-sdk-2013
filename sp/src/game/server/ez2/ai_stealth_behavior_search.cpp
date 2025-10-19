//=============================================================================//
//
// Purpose:		AI behavior for intelligently searching areas.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_behavior_search.h"
#include "ai_stealth_behavior_alarm.h"
#include "ai_stealth_manager.h"
#include "ai_stealth_area.h"
#include "ai_stealth_senses.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_playerally.h"
#include "mapbase_matchers_base.h"
#include "saverestore_utlvector.h"
#include "ai_interactions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

extern ISoundEmitterSystemBase *soundemitterbase;

ConVar	ai_stealth_search_always( "ai_stealth_search_always", "0" );
ConVar	ai_stealth_search_prioritize_areas( "ai_stealth_search_prioritize_areas", "0" );
ConVar	ai_stealth_search_point_max_dist( "ai_stealth_search_point_max_dist", "300" );
ConVar	ai_stealth_search_area_max_dist( "ai_stealth_search_area_max_dist", "600" );
ConVar	ai_stealth_search_point_unlock_min_time( "ai_stealth_search_point_unlock_min_time", "30" );
ConVar	ai_stealth_search_point_unlock_max_time( "ai_stealth_search_point_unlock_max_time", "45" );
ConVar	ai_stealth_search_point_default_wait_min( "ai_stealth_search_point_default_wait_min", "3.0" );
ConVar	ai_stealth_search_point_default_wait_max( "ai_stealth_search_point_default_wait_max", "5.0" );

ConVar	ai_stealth_regroup_max_dist( "ai_stealth_regroup_max_dist", "2500" );
ConVar	ai_stealth_regroup_max_wait( "ai_stealth_regroup_max_wait", "60" );
ConVar	ai_stealth_regroup_stop_dist( "ai_stealth_regroup_stop_dist", "64" );
ConVar	ai_stealth_sweep_max_dist( "ai_stealth_sweep_max_dist", "3000" );
ConVar	ai_stealth_sweep_min_enemy_time( "ai_stealth_sweep_min_enemy_time", "20" );
ConVar	ai_stealth_sweep_lone_cooldown( "ai_stealth_sweep_lone_cooldown", "30" );
ConVar	ai_stealth_sweep_squad_cooldown( "ai_stealth_sweep_squad_cooldown", "120" );
ConVar	ai_stealth_sitrep_cooldown( "ai_stealth_sitrep_cooldown", "30" );

ConVar	g_debug_stealth_search( "g_debug_stealth_search", "0" );

//-----------------------------------------------------------------------------

#define SearchDbgMsg( msg, ... )		if ( g_debug_stealth_search.GetBool() ) { ConColorMsg( DbgStealthColor, msg, __VA_ARGS__ ); }

//-----------------------------------------------------------------------------
// Interactions
//-----------------------------------------------------------------------------
int g_interactionStealthOrder = 0;
int g_interactionStealthRegroup = 0;

// Simple data carrier for the above interactions. Should not persist
struct StealthSearchOrderData_t
{
	union // If g_interactionStealthOrder, use nOrder. If g_interactionStealthRegroup, use bLeaderArrived.
	{
		StealthSquadOrder_t	nOrder;
		bool bLeaderArrived;
	};
	void *pOrderData;
};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_StealthSearchBehavior )

	DEFINE_FIELD( m_bForcedSearch, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_bLoneSweep, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextLoneSweepTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextSquadSweepTime, FIELD_TIME ),
	DEFINE_FIELD( m_flNextSitrepTime, FIELD_TIME ),

	DEFINE_FIELD( m_bWaitingAtRegroup, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_hRegroupPoint, FIELD_EHANDLE ),

	DEFINE_FIELD( m_iszTargetClass, FIELD_STRING ),
	DEFINE_FIELD( m_iTargetGender, FIELD_INTEGER ),

	DEFINE_FIELD( m_iSquadOrder, FIELD_INTEGER ),
	DEFINE_FIELD( m_bOrderCarriedOut, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bOrderQueued, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_hCurrentSearchArea, FIELD_EHANDLE ),

END_DATADESC()

CTriggerStealthArea *CAI_StealthSearchBehavior::m_pHintSearchArea = NULL;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_StealthSearchBehavior::CAI_StealthSearchBehavior()
{
	m_bForcedSearch = false;

	m_bLoneSweep = false;
	m_bWaitingAtRegroup = false;
	m_hRegroupPoint = NULL;
	m_flNextLoneSweepTime = 0.0f;
	m_flNextSquadSweepTime = 0.0f;
	m_flNextSitrepTime = 0.0f;

	m_iszTargetClass = NULL_STRING;
	m_iTargetGender = GENDER_NONE;

	m_hCurrentSearchArea = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsAtSearchPoint( void )
{
	return IsCurSchedule( SCHED_STEALTH_SEARCH_AREA, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea *CAI_StealthSearchBehavior::GetSearchArea( void )
{
	return m_hCurrentSearchArea;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ShouldSearch()
{
	switch ( GetNpcState() )
	{
		// Only in these states
		case NPC_STATE_IDLE:
			{
				if ( !IsSweeping() )
				{
					if ( !m_bForcedSearch && !ai_stealth_search_always.GetBool() )
						return false;

					if ( g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_QUIET ) )
						return false;
				}
			}
		case NPC_STATE_ALERT:
			break;
		default:
			return false;
	}

	// Patrol dumbly for a few seconds
	if ( GetOuter()->GetLastEnemyTime() != 0.0f && gpGlobals->curtime - GetOuter()->GetLastEnemyTime() < 5.0f )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ForceSearchHint( CAI_Hint *pHint )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ForceSearchArea( CTriggerStealthArea *pArea )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::FinishAreaSearch()
{
	SetCondition( COND_STEALTH_SEARCH_LEAVE_AREA );
	
	if ( m_hCurrentSearchArea )
	{
		// Only speak if we're the last one
		if ( m_hCurrentSearchArea->FinishSearch( GetOuter() ) )
			SpeakStealthConcept( TLK_SEARCH_AREA_FINISH );

		m_hCurrentSearchArea = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::CancelAreaSearch()
{
	if ( m_hCurrentSearchArea )
	{
		m_hCurrentSearchArea->CancelSearch( GetOuter() );
		m_hCurrentSearchArea = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnFindSearchArea( CTriggerStealthArea *pArea )
{
	// Only speak if we're the first one and not already inside
	if ( pArea->StartBeingSearched( GetOuter() ) && g_hStealthManager->GetStealthAreaForEntity( GetOuter() ) != pArea )
		SpeakStealthConcept( TLK_SEARCH_AREA_START );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnFindSearchPoint( CAI_Hint *pHint )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnArrivedAtSearchPoint( CAI_Hint *pHint )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnLeaveSearchPoint( CAI_Hint *pHint )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CAI_StealthSearchBehavior::GetAreaSearchDist()
{
	if ( IsSweeping() )
		return ai_stealth_sweep_max_dist.GetFloat();

	return ai_stealth_search_point_max_dist.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
	case HINT_STEALTH_SEARCH_POINT:
	case HINT_STEALTH_REGROUP_POINT:
		return true;
		break;

	default:
		break;
	}

	return BaseClass::FValidateHintType( pHint );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Hint *CAI_StealthSearchBehavior::FindSearchPointHintInArea( CTriggerStealthArea *pArea )
{
	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_STEALTH_SEARCH_POINT );

	int iBits = bits_HINT_NODE_USE_GROUP | bits_HINT_NODE_NEAREST | bits_HINT_NODE_CLEAR;
	if ( g_debug_stealth_search.GetInt() == 2 )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	//hintCriteria.AddExcludePosition( GetAbsOrigin(), flMinDist );
	hintCriteria.AddIncludePosition( pArea->GetAbsOrigin(), pArea->GetMaxSearchPointDist() );

	m_pHintSearchArea = pArea;
	hintCriteria.SetFilterFunc( SearchPointHintFilter_OnlyArea, GetOuter() );

	return CAI_HintManager::FindHint( GetOuter(), hintCriteria );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Hint *CAI_StealthSearchBehavior::FindSearchPointHint( float flMaxDist, CTriggerStealthArea **ppArea )
{
	// Try finding an area first
	if ( !ai_stealth_search_prioritize_areas.GetBool() && ppArea )
	{
		*ppArea = g_hStealthManager->FindBestStealthArea( GetOuter(), ai_stealth_search_area_max_dist.GetFloat(), &m_InterestPoints, IsOrderFindSubject() );
		if ( *ppArea )
		{
			CAI_Hint *pHint = FindSearchPointHintInArea( *ppArea );
			if ( pHint )
				return pHint;
			else
				*ppArea = NULL;
		}
	}

	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_STEALTH_SEARCH_POINT );

	int iBits = bits_HINT_NODE_USE_GROUP | bits_HINT_NODE_NEAREST | bits_HINT_NODE_CLEAR;
	if ( g_debug_stealth_search.GetInt() == 3 )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	//hintCriteria.AddExcludePosition( GetAbsOrigin(), flMinDist );
	hintCriteria.AddIncludePosition( GetAbsOrigin(), flMaxDist );

	if ( ai_stealth_search_prioritize_areas.GetBool() )
	{
		// Already tried searching for an area
		hintCriteria.SetFilterFunc( SearchPointHintFilter_NoArea, GetOuter() );
	}

	CAI_Hint *pHint = CAI_HintManager::FindHint( GetOuter(), hintCriteria );
	if ( pHint )
	{
		if ( !ai_stealth_search_prioritize_areas.GetBool() && ppArea )
		{
			*ppArea = g_hStealthManager->GetStealthAreaForHint( pHint );
		}
	}

	return pHint;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::SearchPointHintFilter_OnlyArea( void *pContext, CAI_Hint *pHint )
{
	if ( !m_pHintSearchArea )
		return true;

	if ( !m_pHintSearchArea->IsValidSearchPoint( pHint ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::SearchPointHintFilter_OnlyAreaDirect( void *pContext, CAI_Hint *pHint )
{
	if ( !m_pHintSearchArea )
		return true;

	if ( !m_pHintSearchArea->PointIsWithin( pHint->GetAbsOrigin() ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::SearchPointHintFilter_NoArea( void *pContext, CAI_Hint *pHint )
{
	if ( !g_hStealthManager || !g_hStealthManager->GetStealthAreaForHint( pHint ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
AI_EnemyInfo_t *CAI_StealthSearchBehavior::GetNewestEnemyMemory( float &flLastTimeSeen )
{
	// TODO: Is there no existing function for getting the newest enemy memory?
	AI_EnemyInfo_t *pBestMemory = NULL;
	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if ( pEMemory->timeLastSeen > flLastTimeSeen )
		{
			flLastTimeSeen = pEMemory->timeLastSeen;
			pBestMemory = pEMemory;
		}
	}

	return pBestMemory;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsSweeping()
{
	return m_bLoneSweep || IsOrderSquadSweeping();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsLoneSweeping()
{
	return m_bLoneSweep;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ShouldLoneSweep()
{
	// Only when tense
	if ( !g_hStealthManager || !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_TENSE ) )
		return false;

	if ( m_flNextLoneSweepTime > gpGlobals->curtime )
		return false;

	if ( gpGlobals->curtime - GetOuter()->GetLastEnemyTime() < ai_stealth_sweep_min_enemy_time.GetFloat() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::FinishLoneSweep()
{
	SearchDbgMsg( "%s [%i]: Finished lone sweep\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

	SpeakStealthConcept( TLK_SWEEP_FINISH );
	m_bLoneSweep = false;
	m_flNextLoneSweepTime = gpGlobals->curtime + ai_stealth_sweep_lone_cooldown.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::CancelLoneSweep()
{
	SearchDbgMsg( "%s [%i]: Canceled lone sweep\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

	m_bLoneSweep = false;
	m_flNextLoneSweepTime = gpGlobals->curtime + 30.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ShouldSquadSweep()
{
	// Only when tense
	if ( !g_hStealthManager || !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_TENSE ) )
		return false;

	if ( m_flNextSquadSweepTime > gpGlobals->curtime )
		return false;

	if ( GetOuter()->GetLastEnemyTime() != 0.0f && gpGlobals->curtime - GetOuter()->GetLastEnemyTime() < ai_stealth_sweep_min_enemy_time.GetFloat() )
		return false;

	// Only when we don't currently hear anything
	if ( GetOuter()->GetStealthSenses()->GetLastSoundLocationUpdateTime() != 0.0f && gpGlobals->curtime - GetOuter()->GetStealthSenses()->GetLastSoundLocationUpdateTime() < 5.0f )
		return false;

	// Make sure we have a valid area
	if ( !g_hStealthManager->FindBestStealthArea( GetOuter(), ai_stealth_sweep_max_dist.GetFloat(), &m_InterestPoints, IsOrderFindSubject() ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::CallToRegroup( CInfoStealthRegroup *pRegroupPoint )
{
	SearchDbgMsg( "%s [%i]: Calling to regroup point \"%s\"\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), pRegroupPoint->GetDebugName() );

	m_hRegroupPoint = pRegroupPoint;
	m_bLoneSweep = false;
	SetCondition( COND_STEALTH_REGROUP_START );
	CancelAreaSearch();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::FinishActiveOrder()
{
	SearchDbgMsg( "%s [%i]: Finished active order \"%s\"\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), GetStringForOrder( GetSquadOrder() ) );

	EndRegroup();

	SetSquadOrder( STEALTH_SQUAD_ORDER_NONE );
	SetCondition( COND_STEALTH_REGROUP_FINISH );
	m_flNextSquadSweepTime = gpGlobals->curtime + ai_stealth_sweep_squad_cooldown.GetFloat();
	m_flNextSitrepTime = gpGlobals->curtime + ai_stealth_sitrep_cooldown.GetFloat();
	m_flNextLoneSweepTime = gpGlobals->curtime + ai_stealth_sweep_lone_cooldown.GetFloat();

	ResetSearchTarget();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::CancelActiveOrder()
{
	SearchDbgMsg( "%s [%i]: Canceled active order (%i)\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), GetStringForOrder( GetSquadOrder() ) );

	EndRegroup();

	SetSquadOrder( STEALTH_SQUAD_ORDER_NONE );
	SetCondition( COND_STEALTH_REGROUP_CANCEL );
	m_flNextSquadSweepTime = gpGlobals->curtime + 30.0f;
	m_flNextSitrepTime = gpGlobals->curtime + 30.0f;
	m_flNextLoneSweepTime = gpGlobals->curtime + 15.0f;

	ResetSearchTarget();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsOrderSquadSweeping() const
{
	return ( m_iSquadOrder == STEALTH_SQUAD_ORDER_SWEEP || m_iSquadOrder == STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE
		|| m_iSquadOrder == STEALTH_SQUAD_ORDER_LOCATE_ENTITY );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsOrderFindSubject() const
{
	return ( m_iSquadOrder == STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE || m_iSquadOrder == STEALTH_SQUAD_ORDER_LOCATE_ENTITY );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsOrderSitrep() const
{
	return m_iSquadOrder == STEALTH_SQUAD_ORDER_SITREP;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ShouldSitrep()
{
	if ( !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_GUARD, STEALTH_LEVEL_TENSE ) )
		return false;

	if ( gpGlobals->curtime < m_flNextSitrepTime )
		return false;

	// Only when we don't currently hear anything
	if ( GetOuter()->GetStealthSenses()->GetLastSoundLocationUpdateTime() != 0.0f && gpGlobals->curtime - GetOuter()->GetStealthSenses()->GetLastSoundLocationUpdateTime() < 5.0f )
		return false;

	StealthSquadInfo_t *pSquadInfo = GetStealthSenses()->GetStealthSquadInfo();
	if ( !pSquadInfo )
		return false;

	int iSquadMemberLeastSeen = g_hStealthManager->GetSquadMemberLeastSeen( pSquadInfo );
	if ( iSquadMemberLeastSeen == pSquadInfo->m_Members.InvalidIndex() )
		return false;

	float flWantUpdateTime = 90.0f;
	if ( g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_TENSE ) )
		flWantUpdateTime = 45.0f;

	if ( gpGlobals->curtime - pSquadInfo->m_Members[iSquadMemberLeastSeen].flLastKnownTime < flWantUpdateTime )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::IsWaitingAtRegroup()
{
	return m_bWaitingAtRegroup;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::EndRegroup()
{
	SearchDbgMsg( "%s [%i]: Ending regroup\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

	if ( m_hRegroupPoint )
	{
		m_hRegroupPoint->ResetSquad();
		m_hRegroupPoint = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::HasRegroupPoint()
{
	return m_hRegroupPoint != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInfoStealthRegroup *CAI_StealthSearchBehavior::FindRegroupPoint( float flMaxDistSqr )
{
	for ( int i = 0; i < IStealthRegroupAutoList::AutoList().Count(); i++ )
	{
		CInfoStealthRegroup *pInfoRegroup = static_cast<CInfoStealthRegroup *>(IStealthRegroupAutoList::AutoList()[i]);
		if ( pInfoRegroup->IsEnabled() &&
			( !pInfoRegroup->HasSquadName() || Matcher_NamesMatch( pInfoRegroup->GetSquadName(), GetOuter()->GetSquad()->GetName() ) ) )
		{
			float flDistSqr = (GetAbsOrigin() - pInfoRegroup->GetAbsOrigin()).LengthSqr();
			if ( flDistSqr > flMaxDistSqr )
				continue;

			return pInfoRegroup;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Hint *CAI_StealthSearchBehavior::FindRegroupPointHint( CInfoStealthRegroup *pRegroupPoint )
{
	CHintCriteria hintCriteria;
	hintCriteria.SetHintType( HINT_STEALTH_REGROUP_POINT );

	int iBits = bits_HINT_NODE_USE_GROUP | bits_HINT_NODE_NEAREST | bits_HINT_NODE_CLEAR;
	if ( g_debug_stealth_search.GetInt() == 3 )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	//hintCriteria.AddExcludePosition( GetAbsOrigin(), flMinDist );
	//hintCriteria.AddIncludePosition( GetAbsOrigin(), flMaxDist );
	hintCriteria.AddIncludePosition( pRegroupPoint->GetAbsOrigin(), pRegroupPoint->GetRadius() );

	return CAI_HintManager::FindHint( GetOuter(), hintCriteria );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::SetSquadOrder( StealthSquadOrder_t iSquadOrder )
{
	SearchDbgMsg( "%s [%i]: New squad order: \"%s\"\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), GetStringForOrder( iSquadOrder ) );

	m_iPreviousSquadOrder = m_iSquadOrder;
	m_iSquadOrder = iSquadOrder;
	m_bOrderCarriedOut = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::HasActiveOrder() const
{
	if ( m_iSquadOrder == STEALTH_SQUAD_ORDER_NONE || m_iSquadOrder == STEALTH_SQUAD_ORDER_DISMISS )
		return false;

	if ( m_bOrderCarriedOut || m_bOrderQueued )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
StealthSquadOrder_t CAI_StealthSearchBehavior::SelectBestOrder( CUtlVector< CHandle<CAI_BaseNPC> > *vecSquadMembers, variant_t *pVarOrderData )
{
	// Check who heard a sound the most recently
	float flBestSoundTime = FLT_MAX;
	Vector vecBestSoundLocation = vec3_origin;
	for ( int i = 0; i < vecSquadMembers->Count(); i++ )
	{
		float flSoundTime = vecSquadMembers->Element( i )->GetStealthSenses()->GetLastSoundLocationUpdateTime();
		if ( flSoundTime < flBestSoundTime && vecSquadMembers->Element( i )->GetStealthSenses()->IsLastSoundRelevant() )
		{
			flBestSoundTime = flSoundTime;
			vecBestSoundLocation = vecSquadMembers->Element( i )->GetStealthSenses()->GetLastSoundLocation();
		}
	}

	if ( flBestSoundTime != FLT_MAX )
	{
		// Update everyone's last sound location and do a sweep
		if ( pVarOrderData )
			pVarOrderData->SetVector3D( vecBestSoundLocation );

		return STEALTH_SQUAD_ORDER_SWEEP;
	}

	return STEALTH_SQUAD_ORDER_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::GiveSquadOrder( CAI_BaseNPC *pLeader, CUtlVector< CHandle<CAI_BaseNPC> > *vecSquadMembers, StealthSquadOrder_t iSquadOrder, variant_t *pVarOrderData )
{
	CAI_StealthSearchBehavior *pBehavior = NULL;
	variant_t &var = pVarOrderData ? *pVarOrderData : variant_t();
	for ( int i = 1; i < vecSquadMembers->Count(); i++ )
	{
		if ( vecSquadMembers->Element( i )->GetBehavior( &pBehavior ) )
		{
			pBehavior->ReceiveSquadOrder( pLeader, iSquadOrder, var );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	// Behaviors lack bridges for interactions, so we have to handle this here.
	// (May also need to be handled whlie the behavior isn't running)
	if (interactionType == g_interactionStealthOrder)
	{
		StealthSearchOrderData_t *pOrderData = (StealthSearchOrderData_t *)data;
		ReceiveSquadOrder( sourceEnt->MyNPCPointer(), pOrderData->nOrder, *((variant_t*)pOrderData->pOrderData) );
		return true;
	}
	else if (interactionType == g_interactionStealthRegroup)
	{
		StealthSearchOrderData_t *pOrderData = (StealthSearchOrderData_t *)data;

		if (pOrderData->bLeaderArrived)
		{
			SetCondition( COND_STEALTH_REGROUP_LEADER_ARRIVES );
		}
		else
		{
			CallToRegroup( (CInfoStealthRegroup *)(pOrderData->pOrderData) );
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CAI_StealthSearchBehavior::GetStringForOrder( StealthSquadOrder_t iSquadOrder )
{
	switch ( iSquadOrder )
	{
		case STEALTH_SQUAD_ORDER_DISMISS:
			return "dismiss";
		case STEALTH_SQUAD_ORDER_SWEEP:
			return "sweep";
		case STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE:
			return "locate_squadmate";
		case STEALTH_SQUAD_ORDER_LOCATE_ENTITY:
			return "locate_entity";
		case STEALTH_SQUAD_ORDER_SITREP:
			return "sitrep";
	}

	return "none";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::ShoutSquadOrder( StealthSquadOrder_t iSquadOrder, AI_CriteriaSet &modifiers, variant_t &varOrderData )
{
	modifiers.AppendCriteria( "order", GetStringForOrder( iSquadOrder ) );

	if ( m_iPreviousSquadOrder != STEALTH_SQUAD_ORDER_NONE )
		modifiers.AppendCriteria( "order_prev", GetStringForOrder( m_iPreviousSquadOrder ) );

	bool bSpoke = SpeakStealthConcept( TLK_SQUAD_ORDER, &modifiers, true );

	/*if ( iSquadOrder == STEALTH_SQUAD_ORDER_SITREP )
	{
		if ( bSpoke )
		{
			// TODO: Add context think to regroup point that handles the queue of NPCs providing sitreps
			// It's procedural for responses, wait for speech task waits for regroup point to report that it's done
			m_hRegroupPoint;
		}
	}*/

	return bSpoke;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::ReceiveSquadOrder( CAI_BaseNPC *pLeader, StealthSquadOrder_t iSquadOrder, variant_t &varOrderData )
{
	SearchDbgMsg( "%s [%i]: Receiving squad order: \"%s\"\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), GetStringForOrder( iSquadOrder ) );

	switch ( iSquadOrder )
	{
		case STEALTH_SQUAD_ORDER_DISMISS:
			FinishActiveOrder();
			TaskComplete();
			return;

		case STEALTH_SQUAD_ORDER_SWEEP:
			ResetSearchTarget();

			if ( GetOuter()->GetLastEnemyTime() != 0.0 )
			{
				// Target our latest enemy
				float flLastTimeSeen = 0.0f;
				AI_EnemyInfo_t *pBestMemory = GetNewestEnemyMemory( flLastTimeSeen );
				if ( pBestMemory && pBestMemory->hEnemy )
				{
					SetSearchTarget( pBestMemory->hEnemy->m_iClassname, soundemitterbase->GetActorGender( STRING( pBestMemory->hEnemy->GetModelName() ) ) );
				}
			}

			if ( varOrderData.FieldType() == FIELD_VECTOR )
			{
				Vector vecInterest;
				varOrderData.Vector3D( vecInterest );
				AddInterestPoint( STEALTH_INTEREST_SOUND, vecInterest );

				// Last sound no longer relevant now that it's been reported
				GetStealthSenses()->ResetLastSound();
			}

			break;

		case STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE:
			{
				if ( varOrderData.FieldType() == FIELD_INTEGER )
				{
					const StealthSquadMemberInfo_t &squadMemberInfo = GetStealthSenses()->GetStealthSquadInfo()->m_Members[varOrderData.Int()];
					AddInterestPoint( STEALTH_INTEREST_MISSING_ALLY, squadMemberInfo.vecLastKnownLocation );
					SetSearchTarget( squadMemberInfo.iszID, squadMemberInfo.nGender );
				}
			}
			break;

		case STEALTH_SQUAD_ORDER_LOCATE_ENTITY:
			{
				if ( varOrderData.FieldType() == FIELD_EHANDLE )
				{
					// TODO: Implement interest point?
					//AddInterestPoint( STEALTH_INTEREST_MISSING_ALLY, squadMemberInfo.vecLastKnownLocation );
					SetSearchTarget( varOrderData.Entity()->m_iClassname, soundemitterbase->GetActorGender( STRING( varOrderData.Entity()->GetModelName() ) ) );
				}
			}
			break;

		case STEALTH_SQUAD_ORDER_SITREP:
			break;
	}

	SetSquadOrder( iSquadOrder );

	TaskComplete();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnLeaveRegroup()
{
	SearchDbgMsg( "%s [%i]: Leaving regroup point\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

	m_bWaitingAtRegroup = false;

	if ( IsOrderSitrep() || GetSquadOrder() == STEALTH_SQUAD_ORDER_DISMISS )
	{
		// All done
		FinishActiveOrder();
	}

	if ( GetHintNode() )
	{
		GetHintNode()->Unlock();
		SetHintNode( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::ResetSearchTarget()
{
	m_iszTargetClass = NULL_STRING;
	m_iTargetGender = GENDER_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::SetSearchTarget( string_t iszClass, gender_t nGender )
{
	m_iszTargetClass = iszClass;
	m_iTargetGender = nGender;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::AddInterestPoint( StealthInterestType_t nType, const Vector &vecOrigin )
{
	if ( !g_hStealthManager )
		return;

	int i = -1;
	if ( nType == STEALTH_INTEREST_ENEMY )
	{
		// Find and replace any interest points of an existing type
		FOR_EACH_VEC( m_InterestPoints, j )
		{
			if ( m_InterestPoints[j].nType == nType )
			{
				i = j;
				break;
			}
		}
	}

	if ( i == -1 )
		i = m_InterestPoints.AddToTail();

	m_InterestPoints[i].nType = nType;
	m_InterestPoints[i].vecOrigin = vecOrigin;
	m_InterestPoints[i].flExpireTime = gpGlobals->curtime + g_hStealthManager->GetInterestTypeDuration( nType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::ReplaceInterestPoints( const CUtlVector<StealthInterestPoint_t> &vecInterestPoints )
{
	m_InterestPoints.RemoveAll();
	m_InterestPoints.AddVectorToTail( vecInterestPoints );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::MaintainInterestPoints()
{
	FOR_EACH_VEC_BACK( m_InterestPoints, i )
	{
		if ( m_InterestPoints[i].flExpireTime < gpGlobals->curtime )
		{
			m_InterestPoints.Remove( i );
		}
	}

	if ( GetStealthSenses()->IsLastSoundRelevant() )
	{
		// Add an interest point for our last sound if we haven't added it already
		const float flLastSoundTime = GetStealthSenses()->GetLastSoundLocationUpdateTime();
		const float flLastSoundTimeEnd = flLastSoundTime + g_hStealthManager->GetInterestTypeDuration( STEALTH_INTEREST_SOUND );
		if ( gpGlobals->curtime - flLastSoundTime < g_hStealthManager->GetInterestTypeDuration( STEALTH_INTEREST_SOUND ) )
		{
			int i = 0;
			for ( ; i < m_InterestPoints.Count(); i++ )
			{
				if ( m_InterestPoints[i].nType == STEALTH_INTEREST_SOUND )
				{
					if ( m_InterestPoints[i].flExpireTime == flLastSoundTimeEnd )
						break;
				}
			}

			if ( i == m_InterestPoints.Count() )
			{
				// Add a new one
				const Vector &vecLastSoundLocation = GetStealthSenses()->GetLastSoundLocation();
				AddInterestPoint( STEALTH_INTEREST_SOUND, vecLastSoundLocation );
			}
		}
	}

	if ( GetOuter()->GetLastEnemyTime() != 0.0f )
	{
		// Add an interest point for our last enemy if we haven't added it already
		float flLastTimeSeen = 0.0f;
		AI_EnemyInfo_t *pEMemory = GetNewestEnemyMemory( flLastTimeSeen );
		const float flLastEnemyTimeEnd = flLastTimeSeen + g_hStealthManager->GetInterestTypeDuration( STEALTH_INTEREST_ENEMY );
		if ( pEMemory )
		{
			int i = 0;
			for ( ; i < m_InterestPoints.Count(); i++ )
			{
				if ( m_InterestPoints[i].nType == STEALTH_INTEREST_SOUND )
				{
					if ( m_InterestPoints[i].flExpireTime == flLastEnemyTimeEnd )
						break;
				}
			}

			if ( i == m_InterestPoints.Count() )
			{
				// Add a new one
				AddInterestPoint( STEALTH_INTEREST_ENEMY, pEMemory->vLastKnownLocation );
			}

			if ( m_iszTargetClass == NULL_STRING )
			{
				// Make this our default search target
				if ( pEMemory->hEnemy )
				{
					SetSearchTarget( pEMemory->hEnemy->m_iClassname, soundemitterbase->GetActorGender( STRING( pEMemory->hEnemy->GetModelName() ) ) );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	if ( IsWaitingAtRegroup() )
	{
		criteriaSet.AppendCriteria( "at_regroup", "1" );
	}

	if ( m_hRegroupPoint != NULL )
	{
		m_hRegroupPoint->AppendContextToCriteria( criteriaSet );
	}

	if ( m_iszTargetClass != NULL_STRING )
	{
		criteriaSet.AppendCriteria( "search_target", STRING( m_iszTargetClass ) );
		criteriaSet.AppendCriteria( "search_target_gender", UTIL_VarArgs( "%i", m_iTargetGender ) );
	}

	// Search areas already covered by stealth manager
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSearchBehavior::DrawDebugTextOverlays( int text_offset )
{
	char			tempstr[ 128 ];
	int				offset;

	offset = BaseClass::DrawDebugTextOverlays( text_offset );
	if ( GetOuter()->m_debugOverlays & OVERLAY_TEXT_BIT )
	{
		if ( GetOuter()->GetSquad() )
		{
			// Redundant
			/*if ( GetOuter()->GetSquad()->IsLeader( GetOuter() ) )
			{
				V_strncpy( tempstr, "LEADER", sizeof( tempstr ) );
				GetOuter()->EntityText( offset, tempstr, 0, 128, 255, 128 );
				offset++;
			}*/

			const char *pszOrderSuffix = "";
			if ( IsOrderCarriedOut() )
				pszOrderSuffix = "(done)";
			else if ( m_bOrderQueued )
				pszOrderSuffix = "(queued)";

			V_snprintf( tempstr, sizeof( tempstr ), "Current Order: [%i] %s %s", GetSquadOrder(), GetStringForOrder( GetSquadOrder() ), pszOrderSuffix );
			GetOuter()->EntityText( offset, tempstr, 0, 224, 255, 224 );
			offset++;

			if ( m_hRegroupPoint )
			{
				V_snprintf( tempstr, sizeof( tempstr ), "Regroup Point: %s (%s)", m_hRegroupPoint->GetDebugName(), IsWaitingAtRegroup() ? "waiting" : "not waiting" );
				GetOuter()->EntityText( offset, tempstr, 0, 224, 255, 224 );
				offset++;
			}
		}
	}

	return offset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSearchBehavior::SelectSchedule()
{
	if ( !CanSelectSchedule() )
	{
		if ( GetHintNode() )
		{
			GetHintNode()->Unlock();
			SetHintNode( NULL );
		}
		return BaseClass::SelectSchedule();
	}

	MaintainInterestPoints();

	if ( m_hRegroupPoint && !HasActiveOrder() )
	{
		bool bShouldBeAtRegroup = false;
		if ( GetOuter()->GetSquad() )
		{
			CAI_BaseNPC *pLeader = GetOuter()->GetSquad()->GetLeader();
			if ( pLeader != GetOuter() )
			{
				CAI_StealthSearchBehavior *pBehavior = NULL;
				if ( pLeader->GetBehavior( &pBehavior ) )
				{
					// Leader has arrived
					if (pBehavior->IsWaitingAtRegroup())
					{
						bShouldBeAtRegroup = true;
					}
				}
			}
			else
			{
				// Leader always goes straight to the regroup point
				bShouldBeAtRegroup = true;
			}
		}

		if ( IsWaitingAtRegroup() )
		{
			if ( bShouldBeAtRegroup )
			{
				if ( GetHintNode() )
				{
					SearchDbgMsg( "%s [%i]::SelectSchedule: Should be on regroup point and already on it, just waiting\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
					return SCHED_STEALTH_WAIT_AT_REGROUP;
				}

				SearchDbgMsg( "%s [%i]::SelectSchedule: Should be on regroup point\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
				SetHintNode( FindRegroupPointHint( m_hRegroupPoint ) );
				if ( !GetHintNode() )
				{
					SearchDbgMsg( "%s [%i]::SelectSchedule: No hint node, waiting within regroup area\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
					return SCHED_STEALTH_WAIT_AT_REGROUP;
				}

				return SCHED_STEALTH_GO_TO_REGROUP;
			}
			else
			{
				if ( GetHintNode() )
				{
					GetHintNode()->Unlock();
					SetHintNode( NULL );
				}

				// Patrol around until the leader arrives
				SearchDbgMsg( "%s [%i]::SelectSchedule: Patrolling regroup point until leader arrives\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
				return SCHED_STEALTH_PATROL_REGROUP;
			}
		}

		// We've been called to regroup
		SetHintNode( FindRegroupPointHint( m_hRegroupPoint ) );
		SearchDbgMsg( "%s [%i]::SelectSchedule: Not at regroup point but have one with no active order, moving to regroup\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
		return SCHED_STEALTH_GO_TO_REGROUP;
	}

	if ( GetHintNode() )
	{
		switch ( GetHintNode()->HintType() )
		{
			case HINT_STEALTH_SEARCH_POINT:
			{
				if ( m_hRegroupPoint && !HasActiveOrder() )
				{
					GetHintNode()->Unlock();

					// We've been called to regroup
					SetHintNode( FindRegroupPointHint( m_hRegroupPoint ) );
					SearchDbgMsg( "%s [%i]::SelectSchedule: Leaving search for regroup point\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );
					return SCHED_STEALTH_GO_TO_REGROUP;
				}

				return SCHED_STEALTH_SEARCH_ENTER_AREA;
			}
			break;
			case HINT_STEALTH_REGROUP_POINT:
			{
				// Shouldn't be here...
				Assert( 0 );
				GetHintNode()->Unlock();
				SetHintNode( NULL );
			}
			break;
		}
	}

	CAI_Hint *pHint = NULL;
	
	if ( IsSweeping() )
	{
		// See if we can find a new area
		if ( m_hCurrentSearchArea )
			m_hCurrentSearchArea->FinishSearch( GetOuter() );
		m_hCurrentSearchArea = g_hStealthManager->FindBestStealthArea( GetOuter(), ai_stealth_sweep_max_dist.GetFloat(), &m_InterestPoints, IsOrderFindSubject() );
		if ( m_hCurrentSearchArea )
		{
			OnFindSearchArea( m_hCurrentSearchArea );
			pHint = FindSearchPointHintInArea( m_hCurrentSearchArea );
		}
		else
		{
			// Done with our sweep
			if ( IsOrderSquadSweeping() )
			{
				MarkOrderCarriedOut();

				// Return to the regroup point
				if ( m_hRegroupPoint )
				{
					SetHintNode( FindRegroupPointHint( m_hRegroupPoint ) );
					return SCHED_STEALTH_GO_TO_REGROUP;
				}
				else
				{
					CancelActiveOrder();
				}
			}
			else
			{
				FinishLoneSweep();
				return SCHED_STEALTH_SEARCH_EXIT_AREA;
			}
		}
	}
	else if ( !HasActiveOrder() )
	{
		if ( GetOuter()->GetSquad() && GetOuter()->GetSquad()->IsLeader( GetOuter() ) )
		{
			// See if we *should* sweep
			bool bSweep = ShouldSquadSweep();
			bool bSitrep = ShouldSitrep();

			if ( bSweep || bSitrep )
			{
				CInfoStealthRegroup *pRegroupPoint = FindRegroupPoint( Square( ai_stealth_regroup_max_dist.GetFloat() ) );
				if ( pRegroupPoint )
				{
					AI_CriteriaSet modifiers;
					modifiers.AppendCriteria( "order", bSweep ? "sweep" : "sitrep" );

					SpeakStealthConcept( TLK_SQUAD_CALL, &modifiers );

					pHint = dynamic_cast<CAI_Hint *>(gEntList.FindEntityByName( NULL, pRegroupPoint->GetLeaderHintName(), GetOuter() ));
					if (!pHint)
						pHint = FindRegroupPointHint( pRegroupPoint );

					if ( pHint )
					{
						SetHintNode( pHint );
					}

					CallToRegroup( pRegroupPoint );
				
					// Tell everyone else to go there
					StealthSearchOrderData_t orderData;
					orderData.bLeaderArrived = false;
					orderData.pOrderData = pRegroupPoint;
					GetOuter()->GetSquad()->BroadcastInteraction( g_interactionStealthRegroup, &orderData, GetOuter() );

					m_bOrderQueued = true;

					if ( bSweep )
					{
						SetSquadOrder( STEALTH_SQUAD_ORDER_SWEEP );
					}
					else
					{
						SetSquadOrder( STEALTH_SQUAD_ORDER_SITREP );
					}

					return SCHED_STEALTH_GO_TO_REGROUP;
				}
			}
		}
		else
		{
			// See if we *should* sweep
			if ( ShouldLoneSweep() )
			{
				if ( m_hCurrentSearchArea )
					m_hCurrentSearchArea->FinishSearch( GetOuter() );
				m_hCurrentSearchArea = g_hStealthManager->FindBestStealthArea( GetOuter(), ai_stealth_sweep_max_dist.GetFloat(), &m_InterestPoints, IsOrderFindSubject() );
				if ( m_hCurrentSearchArea )
				{
					OnFindSearchArea( m_hCurrentSearchArea );
					pHint = FindSearchPointHintInArea( m_hCurrentSearchArea );
					m_bLoneSweep = true;
					SpeakStealthConcept( TLK_SWEEP_START );
				}
			}
		}
	}

	if ( !pHint )
	{
		if ( m_hCurrentSearchArea )
		{
			pHint = FindSearchPointHintInArea( m_hCurrentSearchArea );
		}
		else if ( ai_stealth_search_always.GetBool() && !IsSweeping() )
		{
			CTriggerStealthArea *pArea = NULL;
			pHint = FindSearchPointHint( GetAreaSearchDist(), &pArea );

			// Adopt the search area
			if ( pArea )
			{
				m_hCurrentSearchArea = pArea;
				OnFindSearchArea( pArea );
			}
		}
	}

	if ( pHint )
	{
		SetHintNode( pHint );
		return SCHED_STEALTH_SEARCH_ENTER_AREA;
	}
	else if ( HasCondition( COND_STEALTH_SEARCH_LEAVE_AREA ) )
	{
		return SCHED_STEALTH_SEARCH_EXIT_AREA;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::GatherConditions()
{
	BaseClass::GatherConditions();

	ClearCondition( COND_STEALTH_REGROUP_FINISH );
	ClearCondition( COND_STEALTH_REGROUP_CANCEL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSearchBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if ( IsCurSchedule( SCHED_STEALTH_SEARCH_ENTER_AREA, false ) || IsCurSchedule( TASK_STEALTH_GET_PATH_OUTSIDE_AREA, false ) )
	{
		return SCHED_PATROL_WALK;
	}
	else if ( IsCurSchedule( SCHED_STEALTH_GO_TO_REGROUP, false ) )
	{
		return SCHED_MOVE_AWAY;
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_StealthSearchBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	switch ( nBase )
	{
		case SCHED_PATROL_WALK:
			{
				if ( m_hCurrentSearchArea )
				{
					CAI_Hint *pHint = FindSearchPointHintInArea( m_hCurrentSearchArea );
					if (pHint)
					{
						// Have a specific area we can search, rather than patrolling
						SetHintNode( pHint );
						OnFindSearchPoint( pHint );

						return SCHED_STEALTH_SEARCH_ENTER_AREA;
					}
				}

				if ( m_hRegroupPoint && !HasActiveOrder() )
				{
					// We've been called to regroup
					SetHintNode( FindRegroupPointHint( m_hRegroupPoint ) );
					return SCHED_STEALTH_GO_TO_REGROUP;
				}

				CTriggerStealthArea *pArea = NULL;
				CAI_Hint *pHint = FindSearchPointHint( GetAreaSearchDist(), &pArea );
				if (pHint)
				{
					// Have a specific area we can search, rather than patrolling
					SetHintNode( pHint );
					OnFindSearchPoint( pHint );

					// Adopt the search area
					if ( pArea )
					{
						// Why weren't we able to find it before?
						Assert( pArea != m_hCurrentSearchArea );

						if ( m_hCurrentSearchArea )
							m_hCurrentSearchArea->FinishSearch( GetOuter() );

						m_hCurrentSearchArea = pArea;
						OnFindSearchArea( pArea );
					}

					return SCHED_STEALTH_SEARCH_ENTER_AREA;
				}
			}
			break;
	}

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::BuildScheduleTestBits( void )
{
	GetOuter()->SetCustomInterruptCondition( GetClassScheduleIdSpace()->ConditionLocalToGlobal( COND_STEALTH_SEARCH_FORCE ) );

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_StealthSearchBehavior::CanSelectSchedule( void )
{
	if ( !GetOuter()->IsUsingStealthSenses() )
		return false;

	if ( !ShouldSearch() )
		return false;

	if ( HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_HEAR_PLAYER ) || HasCondition( COND_HEAR_WORLD )
		|| HasCondition( COND_HEAR_DANGER ) || HasCondition( COND_HEAR_BULLET_IMPACT ) )
		return false;

	if ( !g_hStealthManager )
		return false;

	CAI_StealthAlarmBehavior *pBehavior;
	if ( GetOuter()->GetBehavior( &pBehavior ) )
	{
		// Defer to alarm behavior if there's an alarm we should raise
		if ( pBehavior->ShouldRaiseAlarm() )
			return false;
	}

	return BaseClass::CanSelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();

	if ( IsCurSchedule( SCHED_STEALTH_WAIT_AT_REGROUP, false ) )
	{
		if ( m_bWaitingAtRegroup )
			m_bWaitingAtRegroup = false;

		if ( GetHintNode() )
		{
			GetHintNode()->Unlock();
			SetHintNode( NULL );
		}

		if ( GetOuter()->GetSquad() && GetOuter()->GetSquad()->IsLeader( GetOuter() ) )
		{
			SearchDbgMsg( "%s [%i]: Leader no longer waiting at regroup\n", GetOuter()->GetDebugName(), GetOuter()->entindex() );

			// Leader no longer running schedule, make sure no one is stuck at the regroup point waiting for orders
			StealthSquadInfo_t *squadInfo = GetStealthSenses()->GetStealthSquadInfo();
			if ( squadInfo )
			{
				variant_t var;
				for ( int i = 0; i < squadInfo->m_Members.Count(); i++ )
				{
					if ( !squadInfo->m_Members[i].hEntity || !squadInfo->m_Members[i].hEntity->IsNPC() )
						continue;

					CAI_BaseNPC *pNPC = squadInfo->m_Members[i].hEntity->MyNPCPointer();
					CAI_StealthSearchBehavior *pBehavior = NULL;
					if ( pNPC->GetBehavior( &pBehavior ) )
					{
						if ( pBehavior->HasRegroupPoint() && !pBehavior->HasActiveOrder() && !pBehavior->IsCurSchedule( SCHED_STEALTH_WAIT_AT_REGROUP, false ) )
						{
							SearchDbgMsg( "-- Dismissing %s [%i]\n", pNPC->GetDebugName(), pNPC->entindex() );
							pBehavior->ReceiveSquadOrder( GetOuter(), STEALTH_SQUAD_ORDER_DISMISS, var );
							pBehavior->OnLeaveRegroup();
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::EndScheduleSelection( void )
{
	//if ( !ShouldSearch() )
	{
		// We probably won't be returning to this
		if ( m_hCurrentSearchArea )
		{
			CancelAreaSearch();
		}

		if ( IsLoneSweeping() )
		{
			CancelLoneSweep();
		}

		if ( GetEnemy() && HasActiveOrder() )
		{
			CancelActiveOrder();
		}

		if ( IsWaitingAtRegroup() )
		{
			OnLeaveRegroup();
		}
	}

	BaseClass::EndScheduleSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_GET_PATH_TO_SEARCH_POINT:
			{
				ChainStartTask( TASK_GET_PATH_TO_HINTNODE );

				if ( !HasCondition(COND_TASK_FAILED) && GetHintNode() )
				{
					if (GetNavigator()->IsGoalSet())
					{
						GetHintNode()->Lock( GetOuter() );
						TaskComplete();
						break;
					}
				}

				TaskFail( FAIL_NO_ROUTE );
			}
			break;

		case TASK_STEALTH_GET_PATH_TO_REGROUP_POINT:
			{
				if ( GetHintNode() )
				{
					ChainStartTask( TASK_GET_PATH_TO_HINTNODE, 1.0f );

					if ( !HasCondition(COND_TASK_FAILED) )
					{
						if (GetNavigator()->IsGoalSet())
						{
							GetHintNode()->Lock( GetOuter() );
							TaskComplete();
							break;
						}
					}

					TaskFail( FAIL_NO_ROUTE );
				}
				else if ( !m_hRegroupPoint || !GetNavigator()->SetGoal( m_hRegroupPoint->GetAbsOrigin() ) )
				{
					TaskFail( FAIL_NO_ROUTE );
				}
			}
			break;

		case TASK_STEALTH_GET_PATH_TO_NODE_NEAR_REGROUP:
			{
				if ( m_hRegroupPoint == NULL )
				{
					TaskFail( FAIL_NO_GOAL );
					break;
				}

				if ( GetNavigator()->SetRandomGoal( m_hRegroupPoint->GetAbsOrigin(), pTask->flTaskData ) )
					TaskComplete();
				else
					TaskFail(FAIL_NO_REACHABLE_NODE);
		
			}
			break;

		case TASK_STEALTH_GET_PATH_OUTSIDE_AREA:
			{
				CTriggerStealthArea *pArea = g_hStealthManager ? g_hStealthManager->GetStealthAreaForEntity( GetOuter() ) : NULL;
				if ( !pArea )
				{
					ChainStartTask( TASK_GET_PATH_TO_RANDOM_NODE, pTask->flTaskData );
					break;
				}

				float flDistToArea = (GetAbsOrigin() - pArea->GetAbsOrigin()).Length();
				const Vector *vecExitPosition = pArea->GetExitPosition( GetAbsOrigin(), flDistToArea );
				const Vector vecDirToExit = vecExitPosition ? ( GetAbsOrigin() - *vecExitPosition ) : vec3_origin;

				float flMoveDist = pTask->flTaskData;
				if ( vecExitPosition )
				{
					flMoveDist += flDistToArea;
				}

				if (GetNavigator()->SetRandomGoal( flMoveDist, vecDirToExit ))
					TaskComplete();
				else
					TaskFail( FAIL_NO_REACHABLE_NODE );
			}
			break;

		case TASK_STEALTH_WAIT_AT_SEARCH_POINT:
			{
				if ( GetHintNode() )
				{
					OnArrivedAtSearchPoint( GetHintNode() );
				}

				ChainStartTask( TASK_PLAY_HINT_ACTIVITY );

				if ( m_hCurrentSearchArea )
				{
					float flWait = m_hCurrentSearchArea->GetRandomSearchInterval();
					GetOuter()->SetWait( flWait );

					// If we're not actually in the area, look at it instead
					if ( !m_hCurrentSearchArea->IsTouching( GetOuter() ) )
						GetOuter()->AddLookTarget( m_hCurrentSearchArea->GetAbsOrigin(), 1.0f, 3.0f, 0.5f );
				}
				else
				{
					GetOuter()->SetWait( ai_stealth_search_point_default_wait_min.GetFloat(), ai_stealth_search_point_default_wait_max.GetFloat() );
				}
			}
			break;

		case TASK_STEALTH_FINISH_SEARCH_POINT:
			{
				// For anything specific to individual points.
				// Bit of a hack since there's no dedicated output on the hint itself
				if ( GetHintNode() )
				{
					variant_t var;
					GetHintNode()->FireNamedOutput( "OnUser4", var, GetOuter(), GetHintNode() );
				}

				CAI_Hint *pNextSearchPoint = NULL;

				if ( m_hRegroupPoint && !HasActiveOrder() )
				{
					// Don't do anything, we've been called to regroup
				}
				else if ( m_hCurrentSearchArea )
				{
					pNextSearchPoint = FindSearchPointHintInArea( m_hCurrentSearchArea );
					if ( !pNextSearchPoint )
					{
						// Done searching
						FinishAreaSearch();
					}
				}
				else
				{
					CTriggerStealthArea *pArea = NULL;
					pNextSearchPoint = FindSearchPointHint( GetAreaSearchDist(), &pArea );

					// Adopt the search area
					if ( pArea )
					{
						m_hCurrentSearchArea = pArea;
						OnFindSearchArea( pArea );
					}
				}

				if ( pNextSearchPoint )
				{
					// Set the next area to search
					SetHintNode( pNextSearchPoint );
				}
				else if ( GetHintNode() )
				{
					// All done
					GetHintNode()->Unlock( RandomFloat( ai_stealth_search_point_unlock_min_time.GetFloat(), ai_stealth_search_point_unlock_max_time.GetFloat() ) );
					SetHintNode( NULL );
				}

				TaskComplete();

			} break;

		case TASK_STEALTH_WAIT_UNTIL_AT_REGROUP:
			{
				ChainStartTask( TASK_WAIT_FOR_MOVEMENT );
			}
			break;

		case TASK_STEALTH_ARRIVE_AT_REGROUP:
			{
				m_bWaitingAtRegroup = true;

				if ( GetOuter()->GetSquad() && GetOuter()->GetSquad()->IsLeader( GetOuter() ) )
				{
					// Tell everyone I'm here
					StealthSearchOrderData_t orderData;
					orderData.bLeaderArrived = true;
					orderData.pOrderData = NULL;
					GetOuter()->GetSquad()->BroadcastInteraction( g_interactionStealthRegroup, &orderData, GetOuter() );
				}

				TaskComplete();
			}
			break;

		case TASK_STEALTH_WAIT_AT_REGROUP:
			{
				if ( GetOuter()->GetSquad() && GetOuter()->GetSquad()->IsLeader( GetOuter() ) )
				{
					GetOuter()->SetWait( ai_stealth_regroup_max_wait.GetFloat() );
				}
			}
			break;

		case TASK_STEALTH_WAIT_FOR_SPEECH:
			{
				if ( !m_hRegroupPoint )
				{
					if ( GetOuter()->GetSquad() )
					{
						// Primitive version
						CAI_BaseNPC *pLeader = GetOuter()->GetSquad()->GetLeader();
						if ( pLeader && pLeader->GetExpresser() && pLeader->GetExpresser()->IsSpeaking() )
						{
							float flSpeakTime = pLeader->GetExpresser()->GetTimeSpeechCompleteWithoutDelay() - gpGlobals->curtime;
							GetOuter()->SetWait( flSpeakTime, flSpeakTime + 1.25f );

							if ( GetOuter() != pLeader )
								GetOuter()->AddLookTarget( pLeader, 0.8f, flSpeakTime, 0.5f );
						}
						else
							TaskComplete();
					}
					else
						TaskComplete();
				}

				if ( TaskIsComplete() )
				{
					OnLeaveRegroup();
				}
			}
			break;

	default:
		BaseClass::StartTask( pTask );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_StealthSearchBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_STEALTH_WAIT_AT_SEARCH_POINT:
			{
				if ( !GetHintNode() )
				{
					TaskFail( FAIL_NO_HINT_NODE );
				}
				else if ( GetOuter()->IsWaitFinished() )
				{
					TaskComplete();

					if ( m_hCurrentSearchArea )
					{
						// TODO: Different unlock times that prevent us from being caught in a loop
						GetHintNode()->Unlock( RandomFloat( ai_stealth_search_point_unlock_min_time.GetFloat(), ai_stealth_search_point_unlock_max_time.GetFloat() ) );
					}
					else
					{
						GetHintNode()->Unlock( RandomFloat( ai_stealth_search_point_unlock_min_time.GetFloat(), ai_stealth_search_point_unlock_max_time.GetFloat() ) );
					}
				}
				else
				{
					if ( GetHintNode()->GetIgnoreFacing() != HIF_YES )
					{
						GetMotor()->SetIdealYawAndUpdate( GetHintNode()->Yaw() );
						//GetMotor()->SetIdealYaw( GetOuter()->CalcReasonableFacing( true ) );
					}
				}
			}
			break;

		case TASK_STEALTH_WAIT_UNTIL_AT_REGROUP:
			{
				ChainRunTask( TASK_WAIT_FOR_MOVEMENT );

				if ( GetHintNode() )
				{
					if ( TaskIsComplete() )
						TaskComplete();
				}
				else if ( GetNavigator()->BuildAndGetPathDistToGoal() <= ai_stealth_regroup_stop_dist.GetFloat() /*&& GetOuter()->FVisible( GetNavigator()->GetGoalPos() )*/ )
				{
					TaskComplete();
					GetNavigator()->ClearGoal();
					GetOuter()->SetIdealActivity( GetOuter()->GetStoppedActivity() );
					break;
				}
			}
			break;

		case TASK_STEALTH_WAIT_AT_REGROUP:
			{
				Assert( IsWaitingAtRegroup() );

				if ( GetOuter()->GetSquad() && GetOuter()->GetSquad()->IsLeader( GetOuter() ) && g_hStealthManager )
				{
					// Is everyone present?
					StealthSquadInfo_t *squadInfo = GetStealthSenses()->GetStealthSquadInfo();
					if ( !squadInfo )
					{
						TaskComplete();
						break;
					}

					int nNumExpectedMembers = 0;
					int nNumPresentMembers = 0;
					int nNumPossibleMembers = 0;

					// Pointers to the search behavior of each NPC that is at the regroup point.
					// Easier than doing loops around m_Members over and over again
					CUtlVector< CAI_StealthSearchBehavior * >	vecSquadSearchBehaviors;
					vecSquadSearchBehaviors.AddToTail( this );

					for ( int i = 0; i < squadInfo->m_Members.Count(); i++ )
					{
						if ( !squadInfo->m_Members[i].bKnownAlive )
							continue;

						nNumExpectedMembers++;

						bool bAccountedFor = false;
						if ( squadInfo->m_Members[i].hEntity && squadInfo->m_Members[i].hEntity->IsNPC() )
						{
							CAI_BaseNPC *pNPC = squadInfo->m_Members[i].hEntity->MyNPCPointer();
							CAI_StealthSearchBehavior *pBehavior = NULL;
							if ( pNPC->GetBehavior( &pBehavior ) )
							{
								nNumPossibleMembers++;
								if ( pBehavior->IsWaitingAtRegroup() && pNPC->GetTask()->iTask == TASK_STEALTH_WAIT_AT_REGROUP )
								{
									bAccountedFor = true;

									if ( pNPC != GetOuter() )
										vecSquadSearchBehaviors.AddToTail( pBehavior );
								}
								else if ( !pBehavior->HasRegroupPoint() )
								{
									// Every squad member is supposed to know about the regroup point, but they might not have
									// if they spawned later.
									// Just discount them rather than trying to force them into this at this stage
									nNumPossibleMembers--;
								}
							}
							else
							{
								// Pretend they're here since they won't be participating
								nNumPossibleMembers++;
								bAccountedFor = true;
							}
						}

						if ( bAccountedFor )
						{
							nNumPresentMembers++;
						}
					}

					StealthSquadOrder_t iOrder = STEALTH_SQUAD_ORDER_NONE;
					variant_t varOrderData;

					if ( nNumPresentMembers == nNumExpectedMembers )
					{
						// Everyone is here
						if ( IsOrderSquadSweeping() )
						{
							// We're done sweeping, ask for update
							iOrder = STEALTH_SQUAD_ORDER_SITREP;
						}
						else
						{
							// Give the order
							iOrder = m_iSquadOrder;

							if ( iOrder == STEALTH_SQUAD_ORDER_NONE )
								iOrder = STEALTH_SQUAD_ORDER_DISMISS; // No order to give
						}
					}
					else if ( nNumPresentMembers == nNumPossibleMembers )
					{
						// We have all of the members we can have. Someone's dead and isn't coming
						if ( GetSquadOrder() == STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE )
						{
							// If this is someone who went null, mark them as dead since we'll never find them
							int iSomeoneDied = g_hStealthManager->SquadHasUnknownDeadMember( squadInfo );
							if ( iSomeoneDied != squadInfo->m_Members.InvalidIndex() )
							{
								if ( !squadInfo->m_Members[iSomeoneDied].hEntity )
									squadInfo->m_Members[iSomeoneDied].bKnownAlive = false;
							}

							// Ask for update
							iOrder = STEALTH_SQUAD_ORDER_SITREP;
						}
						else
						{
							// To make it less awkward, reduce the wait time if possible
							if ( GetOuter()->GetWaitFinishTime() - gpGlobals->curtime > 7.5f )
								GetOuter()->SetWait( 7.5f );
						}
					}

					if ( GetOuter()->IsWaitFinished() && iOrder == STEALTH_SQUAD_ORDER_NONE )
					{
						// Ran out of time and not everyone is here. Cheat and see if someone's actually dead or if they just got stuck
						int iSomeoneDied = g_hStealthManager->SquadHasUnknownDeadMember( squadInfo );
						if ( iSomeoneDied != squadInfo->m_Members.InvalidIndex() )
						{
							g_hStealthManager->ResetAreaSearches( squadInfo->m_Members[iSomeoneDied].vecLastKnownLocation, 1500.0f );

							iOrder = STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE;
							varOrderData.SetInt( iSomeoneDied );
						}
						else
						{
							iOrder = STEALTH_SQUAD_ORDER_DISMISS;
						}
					}

					if ( GetHintNode() )
					{
						if ( GetHintNode()->GetIgnoreFacing() != HIF_YES )
							GetMotor()->SetIdealYawAndUpdate( GetHintNode()->Yaw() );
					}

					if ( iOrder != STEALTH_SQUAD_ORDER_NONE )
					{
						SearchDbgMsg( "%s [%i]: Starting regroup order \"%s\" (%i)\n", GetOuter()->GetDebugName(), GetOuter()->entindex(), GetStringForOrder( GetSquadOrder() ), GetSquadOrder() );

						ReceiveSquadOrder( GetOuter(), iOrder, varOrderData );

						AI_CriteriaSet modifiers;
						if ( m_hRegroupPoint && ShoutSquadOrder( iOrder, modifiers, varOrderData ) )
						{
							SearchDbgMsg( "-- Shouted order, setting up regroup point logic\n" );
							m_hRegroupPoint->SetupSquad( GetOuter(), vecSquadSearchBehaviors, iOrder );
						}
						else
						{
							// Cancel the order
							SearchDbgMsg( "-- Failed to shout order, dismissing squad\n" );
							iOrder = STEALTH_SQUAD_ORDER_DISMISS;
							ReceiveSquadOrder( GetOuter(), iOrder, varOrderData );
						}

						for ( int i = 0; i < vecSquadSearchBehaviors.Count(); i++ )
						{
							vecSquadSearchBehaviors[i]->ReceiveSquadOrder( GetOuter(), iOrder, varOrderData );
						}

						m_bOrderQueued = false;
					}
				}
				else
				{
					// Do nothing, wait for everyone else to arrive
					//ChainRunTask( TASK_FACE_REASONABLE );
					//GetMotor()->UpdateYaw();
					/*if ( GetHintNode()->GetIgnoreFacing() != HIF_YES )
					{
						GetMotor()->SetIdealYawAndUpdate( GetHintNode()->Yaw() );
					}*/

					if ( !m_hRegroupPoint || m_hRegroupPoint->IsSquadSpeaking() )
						TaskComplete();
				}
			}
			break;

		case TASK_STEALTH_WAIT_FOR_SPEECH:
			{
				if ( m_hRegroupPoint )
				{
					if ( !m_hRegroupPoint->IsSquadSpeaking() )
						TaskComplete();
					else
					{
						if ( GetHintNode() && GetHintNode()->GetIgnoreFacing() != HIF_YES )
						{
							GetMotor()->SetIdealYawAndUpdate( GetHintNode()->Yaw() );
						}
					}
				}
				else if ( GetOuter()->IsWaitFinished() || !IsWaitingAtRegroup() )
				{
					TaskComplete();
				}

				if ( TaskIsComplete() )
				{
					OnLeaveRegroup();
				}
			}
			break;

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_StealthSearchBehavior )

	DECLARE_CONDITION( COND_STEALTH_SEARCH_FORCE )
	DECLARE_CONDITION( COND_STEALTH_SEARCH_LEAVE_AREA )
	DECLARE_CONDITION( COND_STEALTH_REGROUP_START )
	DECLARE_CONDITION( COND_STEALTH_REGROUP_FINISH )
	DECLARE_CONDITION( COND_STEALTH_REGROUP_CANCEL )
	DECLARE_CONDITION( COND_STEALTH_REGROUP_LEADER_ARRIVES )

	DECLARE_TASK( TASK_STEALTH_GET_PATH_TO_SEARCH_POINT )
	DECLARE_TASK( TASK_STEALTH_GET_PATH_OUTSIDE_AREA )
	DECLARE_TASK( TASK_STEALTH_WAIT_AT_SEARCH_POINT )
	DECLARE_TASK( TASK_STEALTH_FINISH_SEARCH_POINT )
	DECLARE_TASK( TASK_STEALTH_GET_PATH_TO_REGROUP_POINT )
	DECLARE_TASK( TASK_STEALTH_WAIT_UNTIL_AT_REGROUP )
	DECLARE_TASK( TASK_STEALTH_ARRIVE_AT_REGROUP )
	DECLARE_TASK( TASK_STEALTH_GET_PATH_TO_NODE_NEAR_REGROUP )
	DECLARE_TASK( TASK_STEALTH_WAIT_AT_REGROUP )
	DECLARE_TASK( TASK_STEALTH_WAIT_FOR_SPEECH )

	DECLARE_INTERACTION( g_interactionStealthOrder )
	DECLARE_INTERACTION( g_interactionStealthRegroup )

	//---------------------------------

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_SEARCH_ENTER_AREA,

		"	Tasks"
		//"		TASK_STOP_MOVING						0"
		"		TASK_STEALTH_GET_PATH_TO_SEARCH_POINT	0"
		"		TASK_WALK_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_STOP_MOVING						0"
		"		TASK_SET_SCHEDULE						SCHEDULE:SCHED_STEALTH_SEARCH_AREA"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_PROVOKED"
		"		COND_STEALTH_REGROUP_START"
	)

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_SEARCH_AREA,

		"	Tasks"
		"		TASK_STEALTH_WAIT_AT_SEARCH_POINT		3"
		"		TASK_STEALTH_FINISH_SEARCH_POINT		0"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_SEARCH_EXIT_AREA,

		"	Tasks"
		//"		TASK_STOP_MOVING						0"
		"		TASK_STEALTH_GET_PATH_OUTSIDE_AREA		0"
		"		TASK_WALK_PATH							0"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		TASK_STOP_MOVING						0"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_PROVOKED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_GO_TO_REGROUP,

		"	Tasks"
		//"		TASK_STOP_MOVING						0"
		"		TASK_STEALTH_GET_PATH_TO_REGROUP_POINT	0"
		"		TASK_WALK_PATH							0"
		"		TASK_STEALTH_WAIT_UNTIL_AT_REGROUP		0"
		"		TASK_STOP_MOVING						0"
		"		TASK_STEALTH_ARRIVE_AT_REGROUP			0"
		""
		"	Interrupts"
		"		COND_STEALTH_REGROUP_FINISH"
		"		COND_STEALTH_REGROUP_CANCEL"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_PATROL_REGROUP,

		"	Tasks"
	//	"		TASK_SET_TOLERANCE_DISTANCE		48"
		"		TASK_SET_ROUTE_SEARCH_TIME		5"	// Spend 5 seconds trying to build a path if stuck
		"		TASK_STEALTH_GET_PATH_TO_NODE_NEAR_REGROUP	200"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		""
		"	Interrupts"
		"		COND_STEALTH_REGROUP_LEADER_ARRIVES"
		"		COND_STEALTH_REGROUP_FINISH"
		"		COND_STEALTH_REGROUP_CANCEL"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_PROVOKED"
		"		COND_GIVE_WAY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_STEALTH_WAIT_AT_REGROUP,

		"	Tasks"
		"		TASK_STEALTH_WAIT_AT_REGROUP			0"
		"		TASK_STEALTH_WAIT_FOR_SPEECH		0"
		"		TASK_WAIT_RANDOM						1"
		""
		"	Interrupts"
		"		COND_STEALTH_REGROUP_FINISH"
		"		COND_STEALTH_REGROUP_CANCEL"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_HEAR_WORLD"
		"		COND_HEAR_PLAYER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()


//-------------------------------------


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CInfoStealthRegroup )

	DEFINE_UTLVECTOR( m_hSquadMembers, FIELD_EHANDLE ),
	DEFINE_FIELD( m_nCurrentSquadMember, FIELD_INTEGER ),
	DEFINE_FIELD( m_bLeaderSpeaking, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iSquadOrder, FIELD_INTEGER ),

	DEFINE_KEYFIELD( m_bStartDisabled, FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_iszSquadName, FIELD_STRING, "squadname" ),

	DEFINE_KEYFIELD( m_iszLeaderHint, FIELD_STRING, "LeaderHint" ),

	DEFINE_THINKFUNC( OrderSpeechQueueThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( info_stealth_regroup, CInfoStealthRegroup );

IMPLEMENT_AUTO_LIST( IStealthRegroupAutoList );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInfoStealthRegroup::CInfoStealthRegroup()
{
	m_nCurrentSquadMember = 0;
	m_bLeaderSpeaking = false;
	m_iSquadOrder = STEALTH_SQUAD_ORDER_NONE;

	m_bStartDisabled = false;
	m_flRadius = 64.0f;
	m_iszSquadName = NULL_STRING;
	m_iszLeaderHint = NULL_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoStealthRegroup::OrderSpeechQueueThink()
{
	CAI_Expresser *pExpresser = NULL;

	switch (m_iSquadOrder)
	{
		case STEALTH_SQUAD_ORDER_SITREP:
			{
				if ( m_nCurrentSquadMember + 1 < m_hSquadMembers.Count() || m_bLeaderSpeaking )
				{
					if ( m_bLeaderSpeaking )
					{
						// Have the next squad member start speaking

						CAI_StealthSearchBehavior *pBehavior = NULL;
						if ( m_hSquadMembers[m_nCurrentSquadMember]->GetBehavior( &pBehavior ) )
						{
							pBehavior->SetSpeechTarget( m_hSquadMembers[0] );
							pBehavior->SpeakStealthConcept( TLK_SQUAD_REPORT, NULL, true );
							pExpresser = m_hSquadMembers[m_nCurrentSquadMember]->GetExpresser();
						}
					}
					else
					{
						// Have the leader speak to the next one
						m_nCurrentSquadMember++;

						CAI_StealthSearchBehavior *pBehavior = NULL;
						if ( m_hSquadMembers[0]->GetBehavior( &pBehavior ) )
						{
							pBehavior->SetSpeechTarget( m_hSquadMembers[m_nCurrentSquadMember] );
							pBehavior->SpeakStealthConcept( TLK_SQUAD_CHECK, NULL, true );
							pExpresser = m_hSquadMembers[0]->GetExpresser();
						}
					}

					if ( pExpresser )
					{
						float flLookTime = pExpresser->GetTimeSpeechCompleteWithoutDelay() - gpGlobals->curtime + 1.0f;
						m_hSquadMembers[0]->AddLookTarget( m_hSquadMembers[m_nCurrentSquadMember], 1.0f, flLookTime );
						m_hSquadMembers[m_nCurrentSquadMember]->AddLookTarget( m_hSquadMembers[0], 1.0f, flLookTime );
					}

					m_bLeaderSpeaking = !m_bLeaderSpeaking;
				}
				else
				{
					// All done
					if ( !m_bLeaderSpeaking )
					{
						// Determine if a new order should be given
						CAI_StealthSearchBehavior *pBehavior = NULL;
						if ( m_hSquadMembers[0]->GetBehavior( &pBehavior ) )
						{
							variant_t var;
							StealthSquadOrder_t iNewOrder = pBehavior->SelectBestOrder( &m_hSquadMembers, &var );

							if ( iNewOrder != STEALTH_SQUAD_ORDER_NONE )
							{
								AI_CriteriaSet modifiers;
								modifiers.AppendCriteria( "from_sitrep", "1" );

								if ( iNewOrder != STEALTH_SQUAD_ORDER_NONE )
								{
									if ( pBehavior->ShoutSquadOrder( iNewOrder, modifiers, var ) )
									{
										m_iSquadOrder = iNewOrder;
										pExpresser = m_hSquadMembers[0]->GetExpresser();
										pBehavior->ReceiveSquadOrder( m_hSquadMembers[0], iNewOrder, var );

										for ( int i = 1; i < m_hSquadMembers.Count(); i++ )
										{
											if ( m_hSquadMembers[i]->GetBehavior( &pBehavior ) )
											{
												pBehavior->ReceiveSquadOrder( m_hSquadMembers[0], iNewOrder, var );
											}
										}
									}
								}
								else
								{
									// Not doing anything
									pBehavior->ShoutSquadOrder( STEALTH_SQUAD_ORDER_DISMISS, modifiers, var );
								}
							}
						}

						m_bLeaderSpeaking = true;
					}
				}
			}
			break;
	}

	if ( pExpresser )
	{
		SetContextThink( &CInfoStealthRegroup::OrderSpeechQueueThink, pExpresser->GetTimeSpeechComplete(), "OrderSpeechQueueThink" );
	}
	else
	{
		ResetSquad();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoStealthRegroup::SetupSquad( CAI_BaseNPC *pLeader, const CUtlVector< CAI_StealthSearchBehavior* > &vecSquad, StealthSquadOrder_t iOrder )
{
	if (!pLeader->GetExpresser())
		return;

	// Leader is always first
	m_hSquadMembers.AddToTail( pLeader );

	FOR_EACH_VEC( vecSquad, i )
	{
		if ( vecSquad[i]->GetOuter() != pLeader )
			m_hSquadMembers.AddToTail( vecSquad[i]->GetOuter() );
	}

	m_nCurrentSquadMember = 0;
	m_iSquadOrder = iOrder;
	m_bLeaderSpeaking = false;

	SetContextThink( &CInfoStealthRegroup::OrderSpeechQueueThink, pLeader->GetExpresser()->GetTimeSpeechComplete(), "OrderSpeechQueueThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInfoStealthRegroup::ResetSquad()
{
	m_hSquadMembers.RemoveAll();
	m_nCurrentSquadMember = 0;
	m_iSquadOrder = STEALTH_SQUAD_ORDER_NONE;
	SetContextThink( NULL, TICK_NEVER_THINK, "OrderSpeechQueueThink" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInfoStealthRegroup::IsSquadSpeaking()
{
	return (m_hSquadMembers.Count() > 0);
}
