//=============================================================================//
//
// Purpose:		AI behavior
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_behavior_tripmine_place.h"
#include "npc_playercompanion.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_playerally.h"
#include "ai_network.h"
#include "ai_link.h"
#include "hl2mp/grenade_tripmine.h"
#include "saverestore_utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	ai_tripmine_place_stable_only( "ai_tripmine_place_stable_only", "1" );

ConVar	ai_debug_tripmine_place( "ai_debug_tripmine_place", "0" );

extern ConVar sk_npc_dmg_tripmine;

#define DEFAULT_TRIPMINE_HEIGHT				48.0f
#define DEFAULT_TRIPMINE_CROUCH_HEIGHT		16.0f
#define DEFAULT_TRIPMINE_DISTANCE			32.0f	 // GetOuter()->GetHullWidth()
#define MAX_TRIPMINE_HEIGHT					72.0f

#define TRIPMINE_ANIM_CROUCH_HEIGHT			32.0f
#define TRIPMINE_LASER_MAX_DIST				1024.0f
#define TRIPMINE_LASER_MIN_DIST				48.0f

int AE_SLAM_TRIPMINE_PLACE;

// ACT_RANGE_ATTACK_TRIPWIRE is in the default activity list, while ACT_RANGE_ATTACK_TRIPWIRE_LOW is not
// This needs to be changed if it is ever properly added
int ACT_RANGE_ATTACK_TRIPWIRE_LOW;

static int g_iMaxContextTripmines[TRIPMINE_CONTEXT_COUNT] = {
	0,		// TRIPMINE_CONTEXT_NONE
	3,		// TRIPMINE_CONTEXT_LAST_KNOWN
	2,		// TRIPMINE_CONTEXT_COMBAT
	4,		// TRIPMINE_CONTEXT_FORTIFY
	3,		// TRIPMINE_CONTEXT_MOVING
};

static int g_flTripmineExcludeRadius[TRIPMINE_CONTEXT_COUNT] = {
	96.0f,		// TRIPMINE_CONTEXT_NONE
	96.0f,		// TRIPMINE_CONTEXT_LAST_KNOWN
	256.0f,		// TRIPMINE_CONTEXT_COMBAT
	96.0f,		// TRIPMINE_CONTEXT_FORTIFY
	128.0f,		// TRIPMINE_CONTEXT_MOVING
};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CAI_TripminePlaceBehavior )

	DEFINE_FIELD( m_bTripmineCapable, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bForcePlaceTripmine, FIELD_BOOLEAN ),

	DEFINE_EMBEDDED_ARRAY( m_TripmineContexts, TRIPMINE_CONTEXT_COUNT ),
	DEFINE_FIELD( m_nTripmineContext, FIELD_INTEGER ),

	DEFINE_FIELD( m_vecCurrentTripmineLocation, FIELD_POSITION_VECTOR ),

	DEFINE_UTLVECTOR( m_TripmineCandidates, FIELD_EMBEDDED ),

END_DATADESC()

BEGIN_SIMPLE_DATADESC( TripmineCandidate_t )

	DEFINE_FIELD( vecOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( flWeight, FIELD_FLOAT ),
	DEFINE_FIELD( hAttachParent, FIELD_EHANDLE ),

END_DATADESC()

BEGIN_SIMPLE_DATADESC( TripmineContextData_t )

	DEFINE_FIELD( vecOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_UTLVECTOR( hTripmines, FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_TripminePlaceBehavior::CAI_TripminePlaceBehavior()
{
	m_bTripmineCapable = false;
	m_bForcePlaceTripmine = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::IsPlacingTripmine( void )
{
	return IsCurSchedule( SCHED_TRIPMINE_PLACE, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::ShouldPlaceTripmine()
{
	if ( m_bForcePlaceTripmine )
	{
		m_nTripmineContext = TRIPMINE_CONTEXT_NONE;
		return true;
	}

	if ( !m_bTripmineCapable || !GetOuter()->HasGrenades() )
		return false;

	if ( !GetOuter()->HasStrategySlot( GetOuter()->GetEngineerSlot() ) && GetOuter()->IsEngineerSlotOccupied() )
		return false;

	if ( GetNpcState() == NPC_STATE_COMBAT )
	{
		if ( !ShouldPlaceTripminesWhileMoving() )
		{
			// If we've lost sight of our enemy or aren't giving chase, then try setting a tripmine next to us
			if ( ( !HasCondition( COND_SEE_ENEMY ) || !GetOuter()->HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) ) && !GetOuter()->FVisible( GetEnemyLKP() ) )
			{
				if ( TryFindTripmineLocations( GetAbsOrigin() + Vector(0,0,DEFAULT_TRIPMINE_HEIGHT), 128.0f, TRIPMINE_CONTEXT_COMBAT, true ) )
					return true;
			}
		}
	}

	// If we have lost an enemy, then we should place around their last position
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if ( pEMemory->bEludedMe )
		{
			if ( TryFindTripmineLocations( pEMemory->vLastKnownLocation + Vector(0,0,DEFAULT_TRIPMINE_HEIGHT), 512.0f, TRIPMINE_CONTEXT_LAST_KNOWN, false ) )
				return true;
		}
	}

	// TODO: If fortifying

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::ShouldPlaceTripminesWhileMoving()
{
	// Currently only overridden by derived classes
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::ForcePlaceTripmineOnTarget( CBaseEntity *pTarget )
{
	// Try sticking to a nearby surface
	Vector vecForward, vecRight, vecLocation, vecDir;
	float flWeight = 1.0f;
	pTarget->GetVectors( &vecForward, &vecRight, NULL );
	if ( !ProbeAllAngles( pTarget->GetAbsOrigin(), vecForward, vecRight, 4.0f, vecLocation, vecDir, flWeight ) )
	{
		// Fall back to direct entity position
		vecLocation = pTarget->GetAbsOrigin();
		vecDir = vecForward;
	}

	// Find a plant position if the default doesn't work
	/*
	Vector vecPlantPos = vecLocation + (vecDir * DEFAULT_TRIPMINE_DISTANCE);
	trace_t entTr;
	UTIL_TraceEntity( GetOuter(), vecPlantPos, vecPlantPos - Vector(0,0,MAX_TRIPMINE_HEIGHT), MASK_NPCSOLID, &entTr );
	if ( entTr.startsolid || entTr.fraction == 1.0f )
	{
		// Try probing with default coords instead of entity direction
		vecDir = Vector( 1, 0, 0 );
		vecRight = Vector( 0, -1, 0 );
		vecPlantPos = vecLocation + (vecDir * DEFAULT_TRIPMINE_DISTANCE);
	}
	*/

	// Clear all candidates for this
	m_TripmineCandidates.RemoveAll();
	m_nTripmineContext = TRIPMINE_CONTEXT_NONE;
	m_TripmineContexts[TRIPMINE_CONTEXT_NONE].vecOrigin = vecLocation;

	int i = m_TripmineCandidates.AddToTail();
	m_TripmineCandidates[i].vecOrigin = vecLocation;
	m_TripmineCandidates[i].vecDir = vecDir;
	m_TripmineCandidates[i].flWeight = MAX( flWeight, 1.0f );

	// Don't bother parenting if it's an invisible entity with no parent of itself
	if ( pTarget->IsViewable() || pTarget->GetParent() )
		m_TripmineCandidates[i].hAttachParent = pTarget;
	else
		m_TripmineCandidates[i].hAttachParent = NULL;

	m_bForcePlaceTripmine = true;

	SetCondition( COND_PROVOKED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::TakePossessionOfTripmine( CTripmineGrenade *pMine )
{
	TripmineContext_t nContext = GetTripmineContext( pMine );
	if ( nContext == TRIPMINE_CONTEXT_INVALID )
		nContext = TRIPMINE_CONTEXT_NONE;

	m_TripmineContexts[nContext].hTripmines.AddToTail( pMine );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::TakePossessionOfTripmine( const char *pszName, CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	if ( !pszName || !*pszName )
		return;

	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, pszName, GetOuter(), pActivator, pCaller );

	for ( ; pEntity; pEntity = gEntList.FindEntityByName( pEntity, pszName, GetOuter(), pActivator, pCaller ) )
	{
		CTripmineGrenade *pMine = dynamic_cast<CTripmineGrenade*>(pEntity);
		if ( !pMine )
			continue;

		TakePossessionOfTripmine( pMine );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::ProbeSurface( const Vector &vecOrigin, const Vector &vecNormal, float &flWeight, bool bCheckCandidates )
{
	// Before testing anything, make sure it's not too close to any existing tripmines or candidates
	{
		float flExcludeRadiusSqr = Square( g_flTripmineExcludeRadius[m_nTripmineContext] );

		CBaseEntity *pTripmine = gEntList.FindEntityByClassname( NULL, "npc_tripmine" );
		while ( pTripmine )
		{
			if ( (vecOrigin - pTripmine->GetAbsOrigin()).LengthSqr() < flExcludeRadiusSqr )
				return false;

			pTripmine = gEntList.FindEntityByClassname( pTripmine, "npc_tripmine" );
		}

		if ( bCheckCandidates )
		{
			for ( int i = 0; i < m_TripmineCandidates.Count(); i++ )
			{
				if ( (vecOrigin - m_TripmineCandidates[i].vecOrigin).LengthSqr() < flExcludeRadiusSqr )
					return false;
			}
		}
	}

	// Just use a simple box for now
	// True dimensions would be 5.0f, 1.5f, 4.0f
	const Vector vecTripmineMaxs = Vector( 4.0f, 4.0f, 4.0f );
	const Vector vecTripmineMins = -vecTripmineMaxs;

	// Check if the tripmine would fit
	trace_t trHull;
	Vector vecTestPos = vecOrigin + (vecNormal * (vecTripmineMaxs.Length() + 0.1f));
	UTIL_TraceHull( vecTestPos, vecTestPos, vecTripmineMins, vecTripmineMaxs, MASK_SOLID, GetOuter(), COLLISION_GROUP_NONE, &trHull );

	if ( trHull.startsolid )
	{
		if ( ai_debug_tripmine_place.GetBool() )
			NDebugOverlay::Box( vecOrigin, vecTripmineMins, vecTripmineMaxs, 255, 0, 0, 128, 3.0f );
		return false;
	}

	// The tripmine would fit here
	// Now see if the laser would be stable
	if ( ai_tripmine_place_stable_only.GetBool() )
	{
		// 2048 is the npc_tripmine laser distance, but that's not always desirable, so do something lower
		// NOTE: Test pos may not be exact to laser attachment

		// Exclude NPCs and player
		// Friendly NPCs are allowed to move in front of the tripmine and invisible players can be in the tripmine's path
		CTraceFilterNoNPCsOrPlayer traceFilter( GetOuter(), COLLISION_GROUP_NONE );
		trace_t trLaser;
		UTIL_TraceLine( vecTestPos, vecTestPos + (vecNormal * TRIPMINE_LASER_MAX_DIST), MASK_SHOT, &traceFilter, &trLaser );
		
		if ( !trLaser.DidHitWorld() )
			return false;

		// Lasers should be within a reasonable distance
		if ( trLaser.fraction == 1.0f || trLaser.fraction < (TRIPMINE_LASER_MIN_DIST / TRIPMINE_LASER_MAX_DIST) )
			return false;

		// Make sure the laser isn't touching any existing tripmines either
		{
			float flExcludeRadiusSqr = Square( g_flTripmineExcludeRadius[m_nTripmineContext] );

			CBaseEntity *pTripmine = gEntList.FindEntityByClassname( NULL, "npc_tripmine" );
			while ( pTripmine )
			{
				if ( (trLaser.endpos - pTripmine->GetAbsOrigin()).LengthSqr() < flExcludeRadiusSqr )
					return false;

				pTripmine = gEntList.FindEntityByClassname( pTripmine, "npc_tripmine" );
			}
			
			if ( bCheckCandidates )
			{
				for ( int i = 0; i < m_TripmineCandidates.Count(); i++ )
				{
					if ( (trLaser.endpos - m_TripmineCandidates[i].vecOrigin).LengthSqr() < flExcludeRadiusSqr )
						return false;
				}
			}
		}

		// Consider tighter locations to be more favorable
		flWeight *= RemapValClamped( trLaser.fraction, 1.0f, (TRIPMINE_LASER_MIN_DIST / TRIPMINE_LASER_MAX_DIST), 0.0f, 1.0f );
	}

	if ( ai_debug_tripmine_place.GetBool() )
	{
		NDebugOverlay::Box( vecOrigin, vecTripmineMins, vecTripmineMaxs, 0, 255, 0, 128, 3.0f );

		char szText[8];
		V_snprintf( szText, sizeof( szText ), "%.2f", flWeight );
		NDebugOverlay::EntityTextAtPosition( vecOrigin, 0, szText, 3.0f );
	}

	// Then this is a valid surface
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::ProbeDirection( const Vector &vecOrigin, const Vector &vecDir, float flMaxDist, Vector &vecOutOrigin, Vector &vecOutNormal, float &flWeight, CBaseEntity **ppAttachParent )
{
	// First, trace a line in this direction
	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + vecDir * flMaxDist, MASK_SOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );

	if ( tr.startsolid )
	{
		if ( ai_debug_tripmine_place.GetBool() )
			NDebugOverlay::HorzArrow( tr.startpos, tr.endpos, 4.0f, 255, 0, 0, 128, true, 3.0f );
		return false;
	}

	// For now, only place tripmines on the world
	// (unless we're in combat, in which case this isn't meant to be a long-term trap)
	if ( !tr.DidHitWorld() )
	{
		if ( GetNpcState() != NPC_STATE_COMBAT || !tr.m_pEnt || tr.m_pEnt->IsAlive() )
		{
			if ( ai_debug_tripmine_place.GetBool() )
				NDebugOverlay::HorzArrow( tr.startpos, tr.endpos, 4.0f, 255, 0, 0, 128, true, 3.0f );
			return false;
		}
		else
		{
			// Not on moving entities
			if ( tr.m_pEnt->IsMoving() )
			{
				if ( ai_debug_tripmine_place.GetBool() )
					NDebugOverlay::HorzArrow( tr.startpos, tr.endpos, 4.0f, 255, 0, 0, 128, true, 3.0f );
				return false;
			}
		}
	}

	// Too slanted
	if ( tr.plane.normal.z > 0.3f || tr.plane.normal.z < -0.3f )
		return false;

	// Make sure that we can stand here (doesn't start solid, but also not bottomless)
	Vector vecPlantPos = tr.endpos + (tr.plane.normal * DEFAULT_TRIPMINE_DISTANCE);
	trace_t entTr;
	UTIL_TraceEntity( GetOuter(), vecPlantPos, vecPlantPos - Vector(0,0,MAX_TRIPMINE_HEIGHT), MASK_NPCSOLID, &entTr );
	if ( entTr.startsolid || entTr.fraction == 1.0f )
		return false;

	if ( ProbeSurface( tr.endpos, tr.plane.normal, flWeight ) )
	{
		if ( ai_debug_tripmine_place.GetBool() )
			NDebugOverlay::HorzArrow( tr.startpos, tr.endpos, 4.0f, 0, 255, 0, 128, true, 3.0f );

		vecOutOrigin = tr.endpos;
		vecOutNormal = tr.plane.normal;
		if ( ppAttachParent && !tr.DidHitWorld() )
			*ppAttachParent = tr.m_pEnt;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::ProbeAllAngles( const Vector &vecOrigin, const Vector &vecForward, const Vector &vecRight, float flMaxDist, Vector &vecOutOrigin, Vector &vecOutNormal, float &flWeight, CBaseEntity **ppAttachParent )
{
	Vector vecTestOrigin = vecOrigin;
	Vector vecDir = vecForward;

	if ( RandomInt(0,2) == 0 && m_nTripmineContext != TRIPMINE_CONTEXT_MOVING )
	{
		// Crouch instead
		vecTestOrigin.z += DEFAULT_TRIPMINE_CROUCH_HEIGHT + RandomFloat( -2.0f, 2.0f );
	}
	else
	{
		vecTestOrigin.z += DEFAULT_TRIPMINE_HEIGHT + RandomFloat( -4.0f, 2.0f );
	}

	if ( ProbeDirection( vecTestOrigin, vecDir, flMaxDist, vecOutOrigin, vecOutNormal, flWeight, ppAttachParent ) )
		return true;

	vecDir *= -1.0f;
	if ( ProbeDirection( vecTestOrigin, vecDir, flMaxDist, vecOutOrigin, vecOutNormal, flWeight, ppAttachParent ) )
		return true;

	vecDir = vecRight;
	if ( ProbeDirection( vecTestOrigin, vecDir, flMaxDist, vecOutOrigin, vecOutNormal, flWeight, ppAttachParent ) )
		return true;

	vecDir *= -1.0f;
	if ( ProbeDirection( vecTestOrigin, vecDir, flMaxDist, vecOutOrigin, vecOutNormal, flWeight, ppAttachParent ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::FValidateHintType( CAI_Hint *pHint )
{
	switch( pHint->HintType() )
	{
	case HINT_TRIPMINE_PLACE:
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
int CAI_TripminePlaceBehavior::FindTripmineHints( const Vector &vecOrigin, float flRadius, TripmineContext_t nContext, CUtlVector<TripmineCandidate_t> &tripmineCandidates, bool bCheckVis )
{
	CHintCriteria hintCriteria;
	hintCriteria.AddHintType( HINT_TRIPMINE_PLACE );
	hintCriteria.AddHintType( HINT_TACTICAL_PINCH );

	int iBits = bits_HINT_NODE_USE_GROUP;
	if ( ai_debug_tripmine_place.GetBool() )
	{
		iBits |= bits_HINT_NODE_REPORT_FAILURES;
	}

	hintCriteria.SetFlag( iBits );
	hintCriteria.AddIncludePosition( vecOrigin, flRadius );

	CUtlVector<CAI_Hint *> vecHints;
	int nNumHints = CAI_HintManager::FindAllHints( GetOuter(), hintCriteria, &vecHints );

	int nNumCandidates = 0;
	const float flMaxDistSqr = Square( flRadius );

	Assert( flMaxDistSqr > 0.0f );
	
	for ( int i = 0; i < nNumHints; i++ )
	{
		Vector vecTargetOrigin = vec3_origin;
		Vector vecTargetNormal = vec3_origin;
		float flHintDistSqr = (vecOrigin - vecHints[i]->GetAbsOrigin()).LengthSqr();

		if ( flHintDistSqr > flMaxDistSqr )
			continue;

		trace_t tr;
		UTIL_TraceLine( vecOrigin, vecHints[i]->GetAbsOrigin(), MASK_BLOCKLOS, GetOuter(), COLLISION_GROUP_NONE, &tr );
		
		if ( tr.fraction != 1.0f )
		{
			// If visibility is needed, discard
			if ( bCheckVis )
				continue;
		}
		else
		{
			// Otherwise, just artificially decrease distance if it's visible
			flHintDistSqr *= 0.5f;
		}

		float flWeight = 1.0f - (flHintDistSqr / flMaxDistSqr);
		if (vecHints[i]->HintType() == HINT_TRIPMINE_PLACE)
		{
			Vector vecForward;
			vecHints[i]->GetVectors( &vecForward, NULL, NULL );
			if ( !ProbeSurface( vecHints[i]->GetAbsOrigin(), vecForward, flWeight ) )
				continue;

			// Higher weight if it's a dedicated hint
			flWeight *= 2.0f;
		}
		else if (vecHints[i]->HintType() == HINT_TACTICAL_PINCH)
		{
			// Probe nearby surfaces
			Vector vecForward, vecRight;
			vecHints[i]->GetVectors( &vecForward, &vecRight, NULL );
			if ( !ProbeAllAngles( vecHints[i]->GetAbsOrigin(), vecForward, vecRight, 64.0f, vecTargetOrigin, vecTargetNormal, flWeight ) )
				continue;

			// Prefer pinches over regular nodes
			flWeight *= 1.5f;
		}

		int j = tripmineCandidates.AddToTail();
		tripmineCandidates[j].vecOrigin = vecTargetOrigin;
		tripmineCandidates[j].vecDir = vecTargetNormal;
		tripmineCandidates[j].flWeight = flWeight;
		tripmineCandidates[j].hAttachParent = NULL;

		nNumCandidates++;
	}

	return nNumCandidates;
}

//-----------------------------------------------------------------------------
// Purpose: Finds tripmine candidates in an area
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::TryFindTripmineLocations( const Vector &vecOrigin, float flRadius, TripmineContext_t nContext, bool bCheckVis )
{
	if ( m_TripmineContexts[nContext].hTripmines.Count() > 0 )
	{
		// Remove invalid handles before checking count
		FOR_EACH_VEC_BACK( m_TripmineContexts[nContext].hTripmines, i )
		{
			if (m_TripmineContexts[nContext].hTripmines[i] == NULL)
				m_TripmineContexts[nContext].hTripmines.Remove( i );
		}

		if ( m_TripmineContexts[nContext].hTripmines.Count() >= GetMaxTripminesForContext( nContext ) )
			return false;
	}

	// UNDONE: If we already have tripmines for this context, and the origin hasn't changed, then early out
	//if ( m_TripmineCandidates.Count() > 0 && m_TripmineContexts[nContext].vecOrigin == vecOrigin )
	//	return false;

	if ( m_TripmineCandidates.Count() > 0 )
		return true;

	TripmineContext_t nRestoreContext = TRIPMINE_CONTEXT_INVALID;
	if ( m_nTripmineContext != nContext )
	{
		// Change the context for the functions below, and switch it back if we fail to find a location
		nRestoreContext = m_nTripmineContext;
		m_nTripmineContext = nContext;
	}

	CUtlVector<TripmineCandidate_t>	tripmineCandidates;

	if ( ai_debug_tripmine_place.GetBool() )
		NDebugOverlay::Circle( vecOrigin, QAngle( -90, 0, 0 ), flRadius, 0, 0, 255, 32, true, 3.0f );

	if ( FindTripmineHints( vecOrigin, flRadius, nContext, tripmineCandidates ) )
	{
		if ( nContext != m_nTripmineContext )
		{
			m_TripmineCandidates.RemoveAll();
			m_TripmineContexts[nContext].vecOrigin = vecOrigin;
		}

		m_TripmineCandidates.AddVectorToTail( tripmineCandidates );

		// Sort them by weight
		m_TripmineCandidates.Sort( TripmineCandidate_t::Sort );
		return true;
	}

	// Try probing around nearby nodes instead
	const float flMaxDistSqr = Square( flRadius );

	Assert( flMaxDistSqr > 0.0f );

	CAI_Network *pNetwork = GetNavigator()->GetNetwork();
	if ( !pNetwork )
		return false;

	for ( int node = 0; node < pNetwork->NumNodes(); node++ )
	{
		CAI_Node *pNode = pNetwork->GetNode( node );
		Vector vecToNode = (vecOrigin - pNode->GetOrigin());

		if ( vecToNode.LengthSqr() > flMaxDistSqr )
			continue;

		// We've confirmed that it's in direct radius
		// Now artificially increase vertical distance and check again because we mainly want to stick to our level
		// This is more efficient if we assume that the majority of nodes will not be in range
		vecToNode.z *= 2.0f;

		float flDistSqr = vecToNode.LengthSqr();

		if ( flDistSqr > flMaxDistSqr )
			continue;

		// Only use ground nodes
		if ( pNode->GetType() != NODE_GROUND )
			continue;

		trace_t tr;
		UTIL_TraceLine( vecOrigin, pNode->GetOrigin(), MASK_BLOCKLOS, GetOuter(), COLLISION_GROUP_NONE, &tr );
		
		if ( tr.fraction != 1.0f )
		{
			// If visibility is needed, discard
			if ( bCheckVis )
				continue;
		}
		else
		{
			// Otherwise, just artificially decrease distance if it's visible
			flDistSqr *= 0.5f;
		}

		// Make sure it has at least one link we can use
		CAI_Link *pLink = NULL;
		int i = 0;
		for ( ; i < pNode->NumLinks(); i++ )
		{
			pLink = pNode->GetLinkByIndex( i );
			if (pLink)
			{
				if (pLink->m_iAcceptedMoveTypes[GetOuter()->GetHullType()] & bits_CAP_MOVE_GROUND)
					break;
			}
		}

		if ( i == pNode->NumLinks() )
			continue;

		// Now probe this surface
		Vector vecForward = Vector( 1, 0, 0 );
		Vector vecRight = Vector( 0, -1, 0 );
		Vector vecLocation, vecDir;
		CBaseEntity *pAttachParent = NULL;
		float flWeight = 1.0f - (flDistSqr / flMaxDistSqr);
		if ( ProbeAllAngles( pNode->GetOrigin(), vecForward, vecRight, 256.0f, vecLocation, vecDir, flWeight, &pAttachParent ) )
		{
			if ( flWeight == 0.0f )
				continue;

			// Add the number of links to its weight so that we prefer nodes in more connected locations
			flWeight *= (1.0f + (((float)pNode->NumLinks()) * 0.1f));

			int j = tripmineCandidates.AddToTail();
			tripmineCandidates[j].vecOrigin = vecLocation;
			tripmineCandidates[j].vecDir = vecDir;
			tripmineCandidates[j].flWeight = flWeight;
			tripmineCandidates[j].hAttachParent = pAttachParent;
		}
	}
	
	if ( tripmineCandidates.Count() > 0 )
	{
		if ( nContext != m_nTripmineContext )
		{
			m_TripmineCandidates.RemoveAll();
			m_TripmineContexts[nContext].vecOrigin = vecOrigin;
		}

		m_TripmineCandidates.AddVectorToTail( tripmineCandidates );

		// Sort them by weight
		m_TripmineCandidates.Sort( TripmineCandidate_t::Sort );
		return true;
	}

	// Failed to find a candidate, revert context
	if ( nRestoreContext != TRIPMINE_CONTEXT_INVALID )
	{
		m_nTripmineContext = nRestoreContext;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Finds tripmine surfaces around a particular origin only, rather than searching nodes
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::TryFindTripmineSurfaces( const Vector &vecOrigin, float flRadius, TripmineContext_t nContext )
{
	if ( m_TripmineContexts[nContext].hTripmines.Count() > 0 )
	{
		// Remove invalid handles before checking count
		FOR_EACH_VEC_BACK( m_TripmineContexts[nContext].hTripmines, i )
		{
			if (m_TripmineContexts[nContext].hTripmines[i] == NULL)
				m_TripmineContexts[nContext].hTripmines.Remove( i );
		}

		if ( m_TripmineContexts[nContext].hTripmines.Count() >= GetMaxTripminesForContext( nContext ) )
			return false;
	}

	// If we already have tripmines for this context, and the origin hasn't changed, then early out
	if ( m_TripmineCandidates.Count() > 0 && m_TripmineContexts[nContext].vecOrigin == vecOrigin )
		return false;

	TripmineContext_t nRestoreContext = TRIPMINE_CONTEXT_INVALID;
	if ( m_nTripmineContext != nContext )
	{
		// Change the context for the functions below, and switch it back if we fail to find a location
		nRestoreContext = m_nTripmineContext;
		m_nTripmineContext = nContext;
	}

	CUtlVector<TripmineCandidate_t>	tripmineCandidates;

	if ( ai_debug_tripmine_place.GetBool() )
		NDebugOverlay::Circle( vecOrigin, QAngle( -90, 0, 0 ), flRadius, 0, 0, 255, 32, true, 3.0f );

	if ( FindTripmineHints( vecOrigin, flRadius, nContext, tripmineCandidates ) )
	{
		if ( nContext != m_nTripmineContext )
		{
			m_TripmineCandidates.RemoveAll();
			m_nTripmineContext = nContext;
			m_TripmineContexts[nContext].vecOrigin = vecOrigin;
		}

		m_TripmineCandidates.AddVectorToTail( tripmineCandidates );

		// Sort them by weight
		m_TripmineCandidates.Sort( TripmineCandidate_t::Sort );
		return true;
	}

	// Check in worldspace first
	Vector vecForward = Vector( 1, 0, 0 );
	Vector vecRight = Vector( 0, -1, 0 );
	Vector vecLocation, vecDir;
	CBaseEntity *pAttachParent = NULL;
	float flWeight = 1.0f;
	if ( ProbeAllAngles( vecOrigin, vecForward, vecRight, 128.0f, vecLocation, vecDir, flWeight, &pAttachParent ) )
	{
		if ( flWeight != 0.0f )
		{
			int j = tripmineCandidates.AddToTail();
			tripmineCandidates[j].vecOrigin = vecLocation;
			tripmineCandidates[j].vecDir = vecDir;
			tripmineCandidates[j].flWeight = flWeight;
			tripmineCandidates[j].hAttachParent = pAttachParent;
		}
	}

	// Now check with our own vectors
	GetOuter()->GetVectors( &vecForward, &vecRight, NULL );
	flWeight = 1.0f;
	if ( ProbeAllAngles( vecOrigin, vecForward, vecRight, 128.0f, vecLocation, vecDir, flWeight, &pAttachParent ) )
	{
		if ( flWeight != 0.0f )
		{
			int j = tripmineCandidates.AddToTail();
			tripmineCandidates[j].vecOrigin = vecLocation;
			tripmineCandidates[j].vecDir = vecDir;
			tripmineCandidates[j].flWeight = flWeight;
			tripmineCandidates[j].hAttachParent = pAttachParent;
		}
	}
	
	if ( tripmineCandidates.Count() > 0 )
	{
		if ( nContext != m_nTripmineContext )
		{
			m_TripmineCandidates.RemoveAll();
			m_nTripmineContext = nContext;
			m_TripmineContexts[nContext].vecOrigin = vecOrigin;
		}

		m_TripmineCandidates.AddVectorToTail( tripmineCandidates );

		// Sort them by weight
		m_TripmineCandidates.Sort( TripmineCandidate_t::Sort );
		return true;
	}

	// Failed to find a candidate, revert context
	if ( nRestoreContext != TRIPMINE_CONTEXT_INVALID )
	{
		m_nTripmineContext = nRestoreContext;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TripminePlaceBehavior::GetMaxTripminesForContext( TripmineContext_t nContext )
{
	return g_iMaxContextTripmines[nContext];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::ClearTripmineCandidates()
{
	m_TripmineCandidates.RemoveAll();
	m_nTripmineContext = TRIPMINE_CONTEXT_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::MoveCandidateToFront( int nIndex )
{
	// Just copy it to a new element
	TripmineCandidate_t candidate = m_TripmineCandidates[nIndex];
	m_TripmineCandidates.Remove( nIndex );
	m_TripmineCandidates.AddToHead( candidate );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::SetTripmineContext( CTripmineGrenade *pMine, TripmineContext_t nContext )
{
	// Consider a dedicated field in CTripmineGrenade if this becomes more important
	pMine->AddContext( "placement_context", CNumStr( nContext ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TripmineContext_t CAI_TripminePlaceBehavior::GetTripmineContext( CTripmineGrenade *pMine )
{
	const char *pszContext = pMine->GetContextValue( "placement_context" );
	if ( pszContext && *pszContext )
	{
		TripmineContext_t nContext = (TripmineContext_t)atoi( pszContext );
		if ( nContext < TRIPMINE_CONTEXT_INVALID || nContext >= TRIPMINE_CONTEXT_COUNT )
			return TRIPMINE_CONTEXT_INVALID;

		return nContext;
	}

	return TRIPMINE_CONTEXT_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::Precache()
{
	BaseClass::Precache();

	GetOuter()->PrecacheScriptSound( "TripmineGrenade.ChargeUp" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "CanUseTripmines" ) )
	{
		SetTripmineCapable( (atoi( szValue ) != 0) );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::ModifyOrAppendCriteria( AI_CriteriaSet& criteriaSet )
{
	//criteriaSet.AppendCriteria( "raising_alarm", IsRaisingAlarm() ? "1" : "0" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TripminePlaceBehavior::SelectSchedule()
{
	if ( m_TripmineCandidates.Count() > 0 && GetOuter()->OccupyStrategySlot( GetOuter()->GetEngineerSlot() ) ) // m_nTripmineContext != TRIPMINE_CONTEXT_NONE
	{
		m_TripmineCandidates.Sort( TripmineCandidate_t::Sort );
		
		// Clean up invalid candidates
		FOR_EACH_VEC_BACK( m_TripmineCandidates, i )
		{
			if ( m_TripmineCandidates[i].flWeight == 0.0f )
				m_TripmineCandidates.Remove( i );
		}

		if ( GetEnemy() )
		{
			return SCHED_TRIPMINE_RUN_TO_PLACE;
		}
		else
		{
			return SCHED_TRIPMINE_WALK_TO_PLACE;
		}
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TripminePlaceBehavior::TranslateSchedule( int scheduleType )
{
	int nBase = BaseClass::TranslateSchedule( scheduleType );

	return nBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_TripminePlaceBehavior::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	// TODO: Walk to place can fail if target area is unreachable (on catwalk, deactivated nodes, etc.)
	// Handle that case here (add exclusion area?)
	if ( IsCurSchedule( SCHED_TRIPMINE_RUN_TO_PLACE, false ) || IsCurSchedule( SCHED_TRIPMINE_WALK_TO_PLACE, false ) )
	{
		if ( m_TripmineCandidates.Count() > 0 )
		{
			// This one isn't valid
			m_TripmineCandidates.Remove( 0 );

			if ( m_TripmineCandidates.Count() > 0 )
			{
				// Try finding the next one
				return IsCurSchedule( SCHED_TRIPMINE_RUN_TO_PLACE, false ) ? SCHED_TRIPMINE_RUN_TO_PLACE : SCHED_TRIPMINE_WALK_TO_PLACE;
			}
		}
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::GatherConditions( void )
{
	BaseClass::GatherConditions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::BuildScheduleTestBits( void )
{
	if ( IsCurSchedule( SCHED_TRIPMINE_RUN_TO_PLACE, false ) )
	{
		// Only interrupt if not already in combat
		if ( GetNpcState() != NPC_STATE_COMBAT )
		{
			GetOuter()->SetCustomInterruptCondition( COND_HEAR_COMBAT );
			GetOuter()->SetCustomInterruptCondition( COND_HEAR_DANGER );
			GetOuter()->SetCustomInterruptCondition( COND_NEW_ENEMY );
			GetOuter()->SetCustomInterruptCondition( COND_SEE_ENEMY );
			GetOuter()->SetCustomInterruptCondition( COND_LIGHT_DAMAGE );
		}
	}

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_TripminePlaceBehavior::CanSelectSchedule( void )
{
	if ( !ShouldPlaceTripmine() )
		return false;

	return BaseClass::CanSelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::EndScheduleSelection( void )
{
	if ( GetOuter()->HasStrategySlot( GetOuter()->GetEngineerSlot() ) )
		GetOuter()->VacateStrategySlot();

	BaseClass::EndScheduleSelection();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::HandleAnimEvent( animevent_t *pEvent )
{
	if ( pEvent->event == AE_SLAM_TRIPMINE_PLACE )
	{
		Vector vecTripmineOrigin, vecTripmineDir;
		QAngle angTripmineAngles;
		CBaseEntity *pAttachParent = NULL;

		if ( m_TripmineCandidates.Count() > 0 )
		{
			vecTripmineOrigin = m_TripmineCandidates[0].vecOrigin;
			vecTripmineDir = m_TripmineCandidates[0].vecDir;
			pAttachParent = m_TripmineCandidates[0].hAttachParent;

			m_TripmineCandidates.Remove( 0 );
		}
		else
		{
			// TODO: Just do a trace outward immediately?
			Warning( "Tried to place tripmine without candidate!!!\n" );
			return;
		}

		VectorAngles( vecTripmineDir, angTripmineAngles );
		angTripmineAngles.x += 90;

		CTripmineGrenade *pMine = (CTripmineGrenade *)CBaseEntity::CreateNoSpawn( "npc_tripmine", vecTripmineOrigin + (vecTripmineDir * 2), angTripmineAngles, NULL);
		pMine->m_hOwner = GetOuter();

		switch ( GetOuter()->Classify() )
		{
			case CLASS_PLAYER_ALLY:
				pMine->KeyValue( "TripmineColor", "255 192 0 64" );
				break;
			case CLASS_CONSCRIPT:
				// HL1 color
				pMine->KeyValue( "TripmineColor", "0 255 236 64" );
				break;
			case CLASS_COMBINE_NEMESIS:
				pMine->KeyValue( "TripmineColor", "0 255 255 64" );
				break;
		}

		// If the SLAM is attached to a non-world entity, parent it!
		if ( pAttachParent )
		{
			pMine->SetParent( pAttachParent );
		}

		pMine->SetOwnerEntity( GetOuter() );
		pMine->SetDamage( sk_npc_dmg_tripmine.GetFloat() );
		pMine->SetVisibleToNPCs( true );
		SetTripmineContext( pMine, m_nTripmineContext );
		DispatchSpawn( pMine );
		pMine->Activate();

		//pMine->SetSolidFlags( FSOLID_COLLIDE_WITH_OWNER );

		GetOuter()->AddGrenades( -1, pMine );
		pMine->EmitSound( "Weapon_SLAM.TripMineAttach" );

		pMine->EmitSound( "TripmineGrenade.ChargeUp" );

		m_TripmineContexts[m_nTripmineContext].hTripmines.AddToTail( pMine );

		GetOuter()->m_OutTripmine.Set( pMine, pMine, GetOuter() );

		if ( m_bForcePlaceTripmine )
			m_bForcePlaceTripmine = false;

		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_TripminePlaceBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TRIPMINE_PLACE:
		{
			float flZDiff;
			if ( m_TripmineCandidates.Count() > 0 )
			{
				m_vecCurrentTripmineLocation = m_TripmineCandidates[0].vecOrigin;
				flZDiff = m_vecCurrentTripmineLocation.z - GetAbsOrigin().z;
			}
			else
			{
				Warning( "Tried to place tripmine without candidate!!!\n" );
				TaskFail( FAIL_NO_TARGET );
				break;
			}

			if ( !m_bForcePlaceTripmine )
			{
				// Verify that this candidate is still valid first
				float flDummyWeight = 1.0f;
				if ( !ProbeSurface( m_vecCurrentTripmineLocation, m_TripmineCandidates[0].vecDir, flDummyWeight, false ) )
				{
					// Failed this candidate, zero its weight
					m_TripmineCandidates[0].flWeight = 0.0f;
					TaskFail( FAIL_BAD_POSITION );
					break;
				}
			}

			if ( flZDiff > TRIPMINE_ANIM_CROUCH_HEIGHT )
			{
				GetOuter()->SetIdealActivity( ACT_RANGE_ATTACK_TRIPWIRE );
			}
			else
			{
				GetOuter()->SetIdealActivity( (Activity)ACT_RANGE_ATTACK_TRIPWIRE_LOW );
			}

			GetOuter()->SetAimTarget( NULL );
			GetOuter()->AddLookTarget( m_vecCurrentTripmineLocation, 1.0f, 1.0f );
		}
		break;

	case TASK_TRIPMINE_GET_PATH_TO_PLACE:
		{
			// Find the first valid candidate among our tripmine locations
			for ( int i = 0; i < m_TripmineCandidates.Count(); i++ )
			{
				if ( !m_bForcePlaceTripmine )
				{
					// Verify that this candidate is still valid first
					float flDummyWeight = 1.0f;
					if ( !ProbeSurface( m_TripmineCandidates[i].vecOrigin, m_TripmineCandidates[i].vecDir, flDummyWeight, false ) )
					{
						// Failed this candidate, zero its weight
						m_TripmineCandidates[i].flWeight = 0.0f;
						continue;
					}
				}

				Vector vecTestPos = m_TripmineCandidates[i].vecOrigin + (m_TripmineCandidates[i].vecDir * DEFAULT_TRIPMINE_DISTANCE);
				trace_t tr;
				UTIL_TraceLine( vecTestPos, vecTestPos - Vector( 0, 0, 96 ), MASK_NPCSOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );

				if ( tr.startsolid || ( m_TripmineCandidates[i].vecOrigin - tr.endpos ).LengthSqr() < (DEFAULT_TRIPMINE_DISTANCE * 0.5f) )
				{
					// Tripmine is either facing an angle we can't place it at or we ended up too close to it
					// This can happen if the tripmine angle isn't facing a valid spot (e.g. directly up)
					// Try some generic surrounding direction
					const int NUM_TESTS = 5;
					Vector vecTests[NUM_TESTS] = {
						Vector( DEFAULT_TRIPMINE_DISTANCE, 0, 0 ),	// Forward
						Vector( -DEFAULT_TRIPMINE_DISTANCE, 0, 0 ),	// Backward
						Vector( 0, -DEFAULT_TRIPMINE_DISTANCE, 0 ),	// Right
						Vector( 0, DEFAULT_TRIPMINE_DISTANCE, 0 ),	// Left
						Vector( 0, 0, 0 ),							// Directly on top
					};

					for ( int i = 0; i < NUM_TESTS; i++ )
					{
						vecTestPos = m_TripmineCandidates[i].vecOrigin + vecTests[i];
						UTIL_TraceLine( vecTestPos, vecTestPos - Vector( 0, 0, 96 ), MASK_NPCSOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );
						if ( !tr.startsolid )
							break;
					}
				}

				AI_NavGoal_t goal;
				goal.type = GOALTYPE_LOCATION;
				goal.dest = tr.endpos;

				if ( GetNavigator()->SetGoal( goal ) )
				{
					GetNavigator()->SetArrivalDirection( -m_TripmineCandidates[i].vecDir );

					float flZDiff = m_TripmineCandidates[i].vecOrigin.z - GetNavigator()->GetGoalPos().z;
					if ( flZDiff > TRIPMINE_ANIM_CROUCH_HEIGHT )
					{
						GetNavigator()->SetArrivalActivity( ACT_RANGE_ATTACK_TRIPWIRE );
					}
					else
					{
						GetNavigator()->SetArrivalActivity( (Activity)ACT_RANGE_ATTACK_TRIPWIRE_LOW );
					}

					TaskComplete();
					break;
				}
				else
				{
					// Failed this candidate, zero its weight
					m_TripmineCandidates[i].flWeight = 0.0f;
				}
			}

			if ( !GetNavigator()->IsGoalSet() )
			{
				// If we're here, we didn't find a valid candidate
				TaskFail( FAIL_NO_REACHABLE_NODE );
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
void CAI_TripminePlaceBehavior::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_TRIPMINE_PLACE:
		{
			GetOuter()->SetAim( m_vecCurrentTripmineLocation - GetOuter()->EyePosition() );
			GetMotor()->SetIdealYawToTargetAndUpdate( m_vecCurrentTripmineLocation );

			GetOuter()->AutoMovement( );
			if ( GetOuter()->IsActivityFinished() )
			{
				TaskComplete();
			}
		} break;

	default:
		BaseClass::RunTask( pTask );
	}
}

//-------------------------------------

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_TripminePlaceBehavior )

	DECLARE_ANIMEVENT( AE_SLAM_TRIPMINE_PLACE )

	DECLARE_ACTIVITY( ACT_RANGE_ATTACK_TRIPWIRE_LOW )

	DECLARE_TASK( TASK_TRIPMINE_PLACE )
	DECLARE_TASK( TASK_TRIPMINE_GET_PATH_TO_PLACE )

	//---------------------------------

	DEFINE_SCHEDULE
	(
		SCHED_TRIPMINE_PLACE,

		"	Tasks"
		"		TASK_TRIPMINE_PLACE		0"
		""
		"	Interrupts"
		//"		COND_HEAR_COMBAT"
		//"		COND_HEAR_DANGER"
		//"		COND_NEW_ENEMY"
		//"		COND_SEE_ENEMY"
		//"		COND_SEE_FEAR"
		//"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TRIPMINE_RUN_TO_PLACE,

		"	Tasks"
		"		TASK_TRIPMINE_GET_PATH_TO_PLACE		0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING		1"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_TRIPMINE_PLACE"
		""
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TRIPMINE_WALK_TO_PLACE,

		"	Tasks"
		"		TASK_TRIPMINE_GET_PATH_TO_PLACE		0"
		"		TASK_WALK_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_STOP_MOVING		1"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_TRIPMINE_PLACE"
		""
		"	Interrupts"
		"		COND_HEAR_COMBAT"
		"		COND_HEAR_DANGER"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()
