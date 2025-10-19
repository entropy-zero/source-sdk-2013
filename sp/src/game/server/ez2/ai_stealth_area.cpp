//=============================================================================//
//
// Purpose:		AI component dedicated to stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_area.h"
#include "ai_stealth_senses.h"
#include "ai_stealth_senses_curious.h"
#include "saverestore_utlvector.h"
#include "ai_network.h"
#include "ai_hint.h"
#include "ai_senses.h"
#include "BasePropDoor.h"
#include "ez2_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CTriggerStealthArea )

	DEFINE_KEYFIELD( m_iszTargetManager, FIELD_STRING, "TargetManager" ),

	DEFINE_KEYFIELD( m_bHidden, FIELD_BOOLEAN, "Hidden" ),
	DEFINE_KEYFIELD( m_flMaxInterestDist, FIELD_FLOAT, "MaxInterestDist" ),
	DEFINE_KEYFIELD( m_flAlertLevelMultiplier, FIELD_FLOAT, "AlertLevelMultiplier" ),

	DEFINE_KEYFIELD( m_bEnclosed, FIELD_BOOLEAN, "Enclosed" ),
	DEFINE_KEYFIELD( m_iszAreaContext, FIELD_STRING, "AreaContext" ),

	DEFINE_KEYFIELD( m_iszSearchPoints, FIELD_STRING, "SearchPoints" ),
	//DEFINE_UTLVECTOR( m_SearchPoints, FIELD_EHANDLE ),
	//DEFINE_FIELD( m_flMaxSearchPointDist, FIELD_FLOAT ),
	DEFINE_KEYFIELD( m_flSearchWeight, FIELD_FLOAT, "SearchWeight" ),
	DEFINE_KEYFIELD( m_SearchInterval, FIELD_INTERVAL, "SearchInterval" ),

	DEFINE_UTLVECTOR( m_hSearchers, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_nMaxSearchers, FIELD_INTEGER, "MaxSearchers" ),
	DEFINE_FIELD( m_flTimeLastSearched, FIELD_TIME ),

	DEFINE_KEYFIELD( m_iszAreaDoors, FIELD_STRING, "AreaDoor" ),
	//DEFINE_UTLVECTOR( m_hDoors, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iDoorStates, FIELD_INTEGER ),

	DEFINE_FIELD( m_vecInteriorPos, FIELD_POSITION_VECTOR ),

	DEFINE_INPUTFUNC( FIELD_STRING, "SetStealthManager", InputSetStealthManager ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetSearchWeight", InputSetSearchWeight ),

	DEFINE_OUTPUT( m_OnNPCStartSearch, "OnNPCStartSearch" ),
	DEFINE_OUTPUT( m_OnNPCFinishSearch, "OnNPCFinishSearch" ),
	DEFINE_OUTPUT( m_OnNPCCancelSearch, "OnNPCCancelSearch" ),

END_DATADESC();

LINK_ENTITY_TO_CLASS( trigger_stealth_area, CTriggerStealthArea );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea::CTriggerStealthArea( void )
{
	m_bEnclosed = true;
	m_iszAreaContext = MAKE_STRING( "" );

	m_flAlertLevelMultiplier = 0.0f;
	m_flMaxInterestDist = 0.0f;

	m_iszSearchPoints = NULL_STRING;
	m_flMaxSearchPointDist = 0.0f;
	m_flSearchWeight = 1.0f;
	m_SearchInterval.start = 3.0f;
	m_SearchInterval.range = 2.0f; // 5.0f

	m_nMaxSearchers = 1;
	m_flTimeLastSearched = -1.0f;

	m_iszAreaDoors = NULL_STRING;
	m_iDoorStates = 0;

	m_iszTargetManager = NULL_STRING;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTriggerStealthArea::~CTriggerStealthArea( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when spawning, after keyvalues have been handled.
//-----------------------------------------------------------------------------
void CTriggerStealthArea::Spawn( void )
{
	BaseClass::Spawn();

	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS | SF_TRIGGER_ALLOW_NPCS | SF_TRIGGER_ALLOW_PHYSICS | SF_TRIG_TOUCH_DEBRIS );

	InitTrigger();

	if ( m_iszTargetManager != NULL_STRING )
	{
		CAI_StealthManager *pManager = dynamic_cast<CAI_StealthManager *>( gEntList.FindEntityByName( NULL, m_iszTargetManager, this ) );
		if ( pManager )
		{
			pManager->AddStealthArea( this );
		}
	}

	PopulateSearchPoints();

	if ( m_iszAreaDoors != NULL_STRING )
	{
		PopulateDoors();
	}

	PopulateInteriorPositions();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::OnRestore( void )
{
	BaseClass::OnRestore();

	PopulateSearchPoints();

	if ( m_iszAreaDoors != NULL_STRING )
	{
		PopulateDoors();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::UpdateOnRemove( void )
{
	CAI_StealthManager *pManager = dynamic_cast<CAI_StealthManager *>( gEntList.FindEntityByName( NULL, m_iszTargetManager, this ) );
	if ( pManager )
	{
		pManager->RemoveStealthArea( this );
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: Called when an entity starts touching us.
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
void CTriggerStealthArea::StartTouch(CBaseEntity *pOther)
{
	BaseClass::StartTouch( pOther );

	if ( pOther->GetFlags() & FL_NPC && pOther->MyNPCPointer()->IsUsingStealthSenses() && IsTouching( pOther ) )
	{
		pOther->MyNPCPointer()->GetStealthSenses()->UpdateAreaMemory( this, false );
	}
	else if ( pOther->IsPlayer() && IsTouching( pOther ) )
	{
		static_cast<CEZ2_Player *>(pOther)->OnEnterStealthArea( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when an entity stops touching us.
// Input  : pOther - The entity that was touching us.
//-----------------------------------------------------------------------------
void CTriggerStealthArea::EndTouch(CBaseEntity *pOther)
{
	if ( !pOther->IsMarkedForDeletion() && IsTouching( pOther ) )
	{
		if ( pOther->GetFlags() & FL_NPC && pOther->MyNPCPointer()->IsUsingStealthSenses() )
		{
			pOther->MyNPCPointer()->GetStealthSenses()->UpdateAreaMemory( this, true );
		}
		else if ( pOther->IsPlayer() )
		{
			static_cast<CEZ2_Player *>(pOther)->OnExitStealthArea( this );
		}
	}

	BaseClass::EndTouch( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::IsHiddenTo( CAI_BaseNPC *pNPC ) const
{
	if ( m_bHidden )
	{
		// If they're searching here, then it's not hidden
		if ( m_hSearchers.Find( pNPC ) != m_hSearchers.InvalidIndex() )
			return false;

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTriggerStealthArea::GetAlertLevelMultiplier( void ) const
{
	if ( m_flAlertLevelMultiplier > 0.0f )
		return m_flAlertLevelMultiplier;

	if ( m_bHidden )
		return 0.325f;

	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTriggerStealthArea::GetMaxInterestDistance( void ) const
{
	if ( m_flMaxInterestDist > 0.0f )
		return m_flMaxInterestDist;

	if ( m_bHidden )
		return BoundingRadius();

	return FLT_MAX;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::PopulateSearchPoints( void )
{
	if ( m_iszSearchPoints != NULL_STRING )
	{
		// Explicit search points
		CAI_Hint *pHint = NULL;
		CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_iszSearchPoints, this );
		for ( ; pEnt != NULL; pEnt = gEntList.FindEntityByName( pEnt, m_iszSearchPoints, this ) )
		{
			pHint = dynamic_cast<CAI_Hint *>(pEnt);
			if ( !pHint || pHint->HintType() != HINT_STEALTH_SEARCH_POINT )
				continue;

			float flDistSqr = ( pHint->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
			if ( flDistSqr > m_flMaxSearchPointDist )
				m_flMaxSearchPointDist = flDistSqr;

			m_SearchPoints.AddToTail( pHint );
		}

		// Can't use the code below because the hint list is only initialized in Activate()
		/*
		AIHintIter_t iter;
		CAI_Hint *pHint = CAI_HintManager::GetFirstHint( &iter );
		for ( ; pHint != NULL; pHint = CAI_HintManager::GetNextHint( &iter ) )
		{
			if ( !pHint->NameMatches( m_iszSearchPoints ) || pHint->HintType() != HINT_STEALTH_SEARCH_POINT )
				continue;

			float flDistSqr = ( pHint->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
			if ( flDistSqr > m_flMaxSearchPointDist )
				m_flMaxSearchPointDist = flDistSqr;

			m_SearchPoints.AddToTail( pHint );
		}
		*/
	}
	else
	{
		// Just find search points within our bounds
		CAI_Hint *pHint = NULL;
		CBaseEntity *pEnt = gEntList.FindEntityByClassname( NULL, "ai_hint" );
		const float flMaxDistSqr = Square( BoundingRadius() );
		for ( ; pEnt != NULL; pEnt = gEntList.FindEntityByClassname( pEnt, "ai_hint" ) )
		{
			pHint = static_cast<CAI_Hint *>(pEnt);
			if ( pHint->HintType() != HINT_STEALTH_SEARCH_POINT )
				continue;

			// Check distance to bounds before doing full check
			float flDistSqr = ( pHint->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
			if ( flDistSqr > flMaxDistSqr )
				continue;

			if ( !PointIsWithin( pHint->GetAbsOrigin() ) )
				continue;

			if ( flDistSqr > m_flMaxSearchPointDist )
				m_flMaxSearchPointDist = flDistSqr;

			m_SearchPoints.AddToTail( pHint );
		}
	}

	// No longer using squared distances
	if ( m_flMaxSearchPointDist > 0.0f )
		m_flMaxSearchPointDist = sqrt( m_flMaxSearchPointDist );

	// Hint searching code will not accept it if it's exact
	m_flMaxSearchPointDist += 4.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::IsValidSearchPoint( CAI_Hint *pHint ) const
{
	for ( int i = 0; i < m_SearchPoints.Count(); i++ )
	{
		if ( m_SearchPoints[i] == pHint )
		{
			// Put any other conditions here
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::IsValidInterestHint( CAI_Hint *pHint ) const
{
	for ( int i = 0; i < m_SearchPoints.Count(); i++ )
	{
		if ( m_SearchPoints[i] == pHint )
		{
			// Put any other conditions here
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::StartBeingSearched( CAI_BaseNPC *pNPC )
{
	if ( m_hSearchers.Find(pNPC) != m_hSearchers.InvalidIndex() )
	{
		Assert( 0 );
		return false;
	}

	m_hSearchers.AddToTail( pNPC );
	m_OnNPCStartSearch.FireOutput( pNPC, this );

	// Only if we're the first searcher
	if ( m_hSearchers.Count() == 1 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTriggerStealthArea::GetRandomSearchInterval()
{
	return RandomInterval( m_SearchInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::CancelSearch( CAI_BaseNPC *pNPC )
{
	m_hSearchers.FindAndRemove( pNPC );
	m_OnNPCCancelSearch.FireOutput( pNPC, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::FinishSearch( CAI_BaseNPC *pNPC )
{
	m_hSearchers.FindAndRemove( pNPC );

	if ( m_hSearchers.Count() == 0 )
	{
		m_flTimeLastSearched = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::PopulateDoors( void )
{
	CBasePropDoor *pDoor = NULL;
	CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_iszAreaDoors, this );
	for (; pEnt != NULL; pEnt = gEntList.FindEntityByName( pEnt, m_iszAreaDoors, this ))
	{
		pDoor = dynamic_cast<CBasePropDoor *>(pEnt);
		if (!pDoor)
			continue;

		// m_iDoorStates stores a bit for each door that's meant to be open, starting at 0 for the first
		// (ajar is considered closed for simplicity)
		if ( !pDoor->IsDoorClosed() && !pDoor->IsDoorAjar() )
			m_iDoorStates |= (1 << m_hDoors.Count());

		m_hDoors.AddToTail( pDoor );

		// Doors need to be visible by AI sensing, but they're normally viewed directly from their origins,
		// which is on the door's hinge and can be obscured by the frame.
		// So we center their eye position on the door itself
		pDoor->SetViewOffset( pDoor->WorldSpaceCenter() - pDoor->GetAbsOrigin() );

		if ( m_hDoors.Count() >= 31 )
		{
			// Maximum doors reached
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::IsValidDoor( CBasePropDoor *pDoor, int *iIndex, bool *bShouldBeOpen ) const
{
	for ( int i = 0; i < m_hDoors.Count(); i++ )
	{
		if ( m_hDoors[i] == pDoor )
		{
			// Put any other conditions here
			if ( bShouldBeOpen )
				*bShouldBeOpen = IsDoorMeantToBeOpen(i);

			if ( iIndex )
				*iIndex = i;

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTriggerStealthArea::IsDoorMeantToBeOpen( int i ) const
{
	return m_iDoorStates & (1 << i);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::UpdateDoorState( CBasePropDoor *pDoor, ThreeState_t iForceState )
{
	for ( int i = 0; i < m_hDoors.Count(); i++ )
	{
		if ( m_hDoors[i] == pDoor )
		{
			if (iForceState != TRS_NONE)
			{
				if (iForceState == TRS_TRUE)
					m_iDoorStates |= (1 << i);
				else
					m_iDoorStates &= ~(1 << i);
			}
			else
			{
				if (!pDoor->IsDoorClosed() && !pDoor->IsDoorAjar())
					m_iDoorStates |= (1 << i);
				else
					m_iDoorStates &= ~(1 << i);
			}
			return;
		}
	}
}

//---------------------------------------------------------
// Node filter for what's within the specified area
//---------------------------------------------------------
class CStealthAreaNodeFilter : public INearestNodeFilter
{
public:
	CStealthAreaNodeFilter( CTriggerStealthArea *pArea, const Vector &vecOrigin )
	{
		m_pArea = pArea;
		m_vecOrigin = vecOrigin;
		m_flRadiusSqr = Square( pArea->BoundingRadius() );
		m_bFoundValidNode = false;
	}

	bool IsValid( CAI_Node *pNode )
	{
		// Must be within radius
		if ((pNode->GetOrigin() - m_vecOrigin).Length2DSqr() < m_flRadiusSqr)
			return false;

		// Must be inside
		if (!m_pArea->PointIsWithin( pNode->GetOrigin() ))
			return false;

		m_bFoundValidNode = true;
		return true;
	}

	bool ShouldContinue()
	{
		return !m_bFoundValidNode;
	}

private:
	CTriggerStealthArea *m_pArea;
	Vector m_vecOrigin;
	float m_flRadiusSqr;
	bool m_bFoundValidNode;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTriggerStealthArea::PopulateInteriorPositions( void )
{
	// For now, populating an interior position is only necessary if we have a door.
	if ( m_hDoors.Count() == 0 )
	{
		m_vecInteriorPos = GetAbsOrigin();
		return;
	}

	// First off, check to see if we can get a position to the ground at our origin
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, BoundingRadius() ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
	if ( !tr.startsolid )
	{
		// Next, do a hull trace at this ground position to see if a standard human NPC can fit at it
		trace_t htr;
		UTIL_TraceHull( tr.endpos, tr.endpos + Vector( 0, 0, 1 ), NAI_Hull::Mins( HULL_HUMAN ), NAI_Hull::Maxs( HULL_HUMAN ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &htr );
		if ( htr.fraction == 1.0f )
		{
			// This position is acceptable
			m_vecInteriorPos = htr.endpos;
			return;
		}
	}

	// They won't fit at the origin. Try finding a node near it instead
	CStealthAreaNodeFilter nodeFilter( this, GetAbsOrigin() );
	int nNode = g_pBigAINet->NearestNodeToPoint( NULL, GetAbsOrigin(), false, &nodeFilter );
	if ( nNode != NO_NODE )
	{
		// Found a node
		m_vecInteriorPos = g_pBigAINet->GetNodePosition( HULL_HUMAN, nNode );
		return;
	}

	// Search points?
	if ( m_SearchPoints.Count() > 0 )
	{
		for ( int i = 0; i < m_SearchPoints.Count(); i++ )
		{
			if ( PointIsWithin( m_SearchPoints[i]->GetAbsOrigin() ) )
			{
				m_vecInteriorPos = m_SearchPoints[0]->GetAbsOrigin();
				return;
			}
		}
	}

	// We can't find a position for this area. Warn if we may need one
	Warning( "Area %s (%s) has a door, but has no reachable interior position (change the origin or put more nodes inside of it)\n", GetDebugName(), GetAreaContext() );

	m_vecInteriorPos = GetAbsOrigin();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector &CTriggerStealthArea::GetInteriorPosition( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor )
{
	// TODO: Multiple positions?
	return m_vecInteriorPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const Vector *CTriggerStealthArea::GetExitPosition( const Vector &vecOrigin, float flRadius )
{
	if ( m_iszAreaDoors != NULL_STRING )
	{
		CBaseEntity *pNearest = gEntList.FindEntityByNameNearest( STRING( m_iszAreaDoors ), vecOrigin, flRadius, this );
		if ( pNearest )
		{
			return &pNearest->GetAbsOrigin();
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Called when an entity starts touching us.
// Input  : pOther - The entity that is touching us.
//-----------------------------------------------------------------------------
float CTriggerStealthArea::GetTimeSpentInArea( CBaseEntity *pEntity )
{
	if ( pEntity->IsNPC() && pEntity->MyNPCPointer()->IsUsingStealthSenses() )
	{
		StealthAreaMemory_t *pMemory = pEntity->MyNPCPointer()->GetStealthSenses()->GetAreaMemory( this );
		if ( pMemory )
			return gpGlobals->curtime - pMemory->flLastTimeEntered;
	}
	else if ( pEntity->IsPlayer() )
	{
		return gpGlobals->curtime - static_cast<CEZ2_Player *>(pEntity)->GetTimeEnteredStealthArea();
	}

	return -1.0f;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CTriggerStealthArea::InputSetStealthManager( inputdata_t &inputdata )
{
	CAI_StealthManager *pManager = dynamic_cast<CAI_StealthManager *>( gEntList.FindEntityByName( NULL, m_iszTargetManager, this ) );
	if ( pManager )
	{
		pManager->RemoveStealthArea( this );
	}

	m_iszTargetManager = inputdata.value.StringID();

	pManager = dynamic_cast<CAI_StealthManager *>( gEntList.FindEntityByName( NULL, m_iszTargetManager, this ) );
	if ( pManager )
	{
		pManager->AddStealthArea( this );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CTriggerStealthArea::InputSetSearchWeight( inputdata_t &inputdata )
{
	m_flSearchWeight = inputdata.value.Float();
}
