//=============================================================================//
//
// Purpose:		Utility functions for stealth AI.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_manager.h"
#include "ai_stealth_area.h"
#include "ai_stealth_utils.h"
#include "ai_node.h"
#include "ai_memory.h"
#include "ai_network.h"
#include "BasePropDoor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

ConVar	g_debug_stealth_predictpos( "g_debug_stealth_predictpos", "0" );

#define PREDICTED_ENEMY_POS_COOLDOWN			2.0
#define PREDICTED_ENEMY_POS_MIN_DIST			4.0
#define PREDICTED_ENEMY_POS_MAX_TIME			4.0
#define PREDICTED_ENEMY_POS_PROJECT_DIST		128.0
#define PREDICTED_ENEMY_POS_NODE_DOT_TOLERANCE	-0.1
#define PREDICTED_ENEMY_POS_NODE_DIST			320.0
#define PREDICTED_ENEMY_POS_NEAR_ME_NODE_DOT_TOLERANCE	0.3
#define PREDICTED_ENEMY_POS_NEAR_ME_NODE_DIST			50.0

//---------------------------------------------------------
// Node filter to check for enemy prediction path
//---------------------------------------------------------
class CStealthPredictNodeFilter : public INearestNodeFilter
{
public:
	CStealthPredictNodeFilter( CAI_BaseNPC *pOuter, const Vector &vecOrigin, const Vector &vecAvoidDir, int nLastNode, int nIteration, CUtlVector<CTriggerStealthArea*> *pNodeAreas )
	{
		m_pOuter = pOuter;
		m_vecOrigin = vecOrigin;
		m_vecAvoidDir = vecAvoidDir;
		m_nLastNode = nLastNode;
		m_nIteration = nIteration;
		m_bFoundValidNode = false;
		
		if ( pNodeAreas && pNodeAreas->Count() > 0 )
			m_pNodeAreas = pNodeAreas;
		else
			m_pNodeAreas = NULL;
	}

	void RemoveAreaList()
	{
		m_pNodeAreas = NULL;
		m_bFoundValidNode = false;
	}

	bool IsValid( CAI_Node *pNode )
	{
		if ( pNode->GetId() == m_nLastNode )
			return false;

		// Must be close enough and not facing the previous direction
		Vector vecDir = (pNode->GetOrigin() - m_vecOrigin);
		float flDist = VectorNormalize( vecDir );
		if ( flDist > PREDICTED_ENEMY_POS_NODE_DIST || DotProduct( vecDir, m_vecAvoidDir ) > PREDICTED_ENEMY_POS_NODE_DOT_TOLERANCE )
		{
			if ( g_debug_stealth_predictpos.GetInt() == 2 )
			{
				if ( flDist < PREDICTED_ENEMY_POS_NODE_DIST )
				{
					NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 255, 0, 255, true, 5.0f );

					char szTemp[128] = { 0 };
					V_snprintf( szTemp, sizeof( szTemp ), "[%i: %f]", m_nIteration, DotProduct( vecDir, m_vecAvoidDir ) );
					NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 255, 64, 255 );

					NDebugOverlay::EntityTextAtPosition( m_vecOrigin, 0, "*", 5.0f, 255, 64, 255 );
				}
				else
				{
					NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 128, 128, 128, true, 5.0f );

					char szTemp[128] = { 0 };
					V_snprintf( szTemp, sizeof( szTemp ), "[%i: %.2f]", m_nIteration, flDist );
					NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 128, 128, 128 );
				}
			}

			return false;
		}
		
		// And also not facing us
		Vector vecDirToMe = (pNode->GetOrigin() - m_pOuter->GetAbsOrigin());
		float flDistToMe = VectorNormalize( vecDirToMe );
		if ( flDistToMe < PREDICTED_ENEMY_POS_NEAR_ME_NODE_DIST || DotProduct( vecDir, vecDirToMe ) < PREDICTED_ENEMY_POS_NEAR_ME_NODE_DOT_TOLERANCE)
		{
			if ( g_debug_stealth_predictpos.GetInt() == 3 )
			{
				if ( flDist > PREDICTED_ENEMY_POS_NEAR_ME_NODE_DIST )
				{
					NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 255, 0, 255, true, 5.0f );

					char szTemp[128] = { 0 };
					V_snprintf( szTemp, sizeof( szTemp ), "[%i: %f]", m_nIteration, DotProduct( vecDir, vecDirToMe ) );
					NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 255, 64, 255 );

					NDebugOverlay::EntityTextAtPosition( m_vecOrigin, 0, "*", 5.0f, 255, 64, 255 );
				}
				else
				{
					NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 128, 128, 128, true, 5.0f );

					char szTemp[128] = { 0 };
					V_snprintf( szTemp, sizeof( szTemp ), "[%i: %.2f]", m_nIteration, flDistToMe );
					NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 128, 128, 128 );
				}
			}

			return false;
		}

		// And not in a place we could see
		if ( m_pOuter->FVisible( pNode->GetOrigin() ) )
			return false;

		if ( m_pNodeAreas )
		{
			// If we have an area to use, only allow nodes inside of it
			bool bSuccess = false;
			for ( int i = 0; i < m_pNodeAreas->Count(); i++ )
			{
				if ( m_pNodeAreas->Element(i)->PointIsWithin( pNode->GetOrigin() ) )
				{
					bSuccess = true;
					break;
				}
			}

			if ( !bSuccess )
			{
				if ( g_debug_stealth_predictpos.GetInt() == 2 )
				{
					NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 255, 0, 0, true, 5.0f );

					//char szTemp[128] = { 0 };
					//V_snprintf( szTemp, sizeof( szTemp ), "[Area: %s]", m_pArea->GetAreaContext() );
					//NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 128, 128, 128 );
				}

				return false;
			}
		}
		
		if ( g_debug_stealth_predictpos.GetInt() == 2 )
		{
			NDebugOverlay::Line( m_vecOrigin, pNode->GetOrigin(), 0, 255, 0, true, 5.0f );

			NDebugOverlay::Line( m_vecOrigin, m_vecOrigin + ( vecDir * 32.0 ), 0, 0, 255, true, 5.0f );
			NDebugOverlay::Line( m_vecOrigin, m_vecOrigin + (m_vecAvoidDir * 32.0 ), 255, 0, 0, true, 5.0f );

			char szTemp[128] = { 0 };
			V_snprintf( szTemp, sizeof( szTemp ), "[%i: %f]", m_nIteration, DotProduct( vecDir, m_vecAvoidDir ) );
			NDebugOverlay::EntityTextAtPosition( pNode->GetOrigin(), 0, szTemp, 5.0f, 255, 64, 255 );
		}

		m_bFoundValidNode = true;
		return true;
	}

	bool ShouldContinue()
	{
		return !m_bFoundValidNode;
	}

private:
	CAI_BaseNPC *m_pOuter;
	Vector m_vecOrigin;
	Vector m_vecAvoidDir;
	int m_nLastNode;
	int m_nIteration;
	CUtlVector<CTriggerStealthArea *> *m_pNodeAreas;
	bool m_bFoundValidNode;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool DoFindPredictedEnemyPos( CAI_BaseNPC *pNPC, CBaseEntity *pEnemy, Vector &vecPos, const Vector &vecDelta, int &nNode, int i )
{
	Vector vecDir = vecDelta;
	float flDist = VectorNormalize( vecDir );
	if ( flDist < PREDICTED_ENEMY_POS_MIN_DIST )
	{
		// Just do the direction from us to the position instead
		vecDir = ( vecPos - pNPC->GetAbsOrigin() );
		VectorNormalize( vecDir );
	}

	CUtlVector<CTriggerStealthArea *> vecNodeAreas;
	if ( g_hStealthManager )
	{
		for ( int i = 0; i < g_hStealthManager->GetStealthAreaCount(); i++ )
		{
			CTriggerStealthArea *pArea = g_hStealthManager->GetStealthArea( i );
			if ( !pArea->IsEnclosed() )
				continue;

			// Get the nearest door, if possible
			CBasePropDoor *pBestDoor = NULL;
			float flBestDoorDistSqr = FLT_MAX;
			for ( int j = 0; j < pArea->GetDoorCount(); j++ )
			{
				float flDoorDistSqr = (vecPos - pArea->GetDoor( j )->EyePosition()).Length2DSqr();
				if (flDoorDistSqr < flBestDoorDistSqr)
				{
					pBestDoor = pArea->GetDoor( j );
					flBestDoorDistSqr = flDoorDistSqr;
				}
			}

			Vector vecAreaTestPos;
			if (pBestDoor)
				vecAreaTestPos = pBestDoor->EyePosition();
			else
				vecAreaTestPos = pArea->GetInteriorPosition( pNPC, NULL );

			Vector vecDirToArea = (vecAreaTestPos - vecPos);
			float flDistToArea = VectorNormalize( vecDirToArea );

			if ( flDistToArea > 256.0f )
				continue;

			// Reduce distance based on dot product
			float flDot = DotProduct2D( vecDirToArea.AsVector2D(), vecDir.AsVector2D() );
			if (flDot < -0.7f)
				continue;

			// This area may be near where our target is going. Only use nodes inside of it
			vecNodeAreas.AddToTail( pArea );

			if ( g_debug_stealth_predictpos.GetBool() )
				Msg("Area: %s\n", pArea->GetAreaContext());

			break;
		}
	}

	CStealthPredictNodeFilter nodeFilter( pNPC, vecPos, -vecDir, nNode, i, &vecNodeAreas );
	nNode = g_pBigAINet->NearestNodeToPoint( pNPC, vecPos, vecNodeAreas.Count() == 0, &nodeFilter );
	if ( nNode == NO_NODE )
	{
		if ( vecNodeAreas.Count() > 0 )
		{
			// Try chasing without areas
			nodeFilter.RemoveAreaList();
			nNode = g_pBigAINet->NearestNodeToPoint( pNPC, vecPos, true, &nodeFilter );
		}
		
		if ( nNode == NO_NODE )
			return false;
	}

	if ( g_debug_stealth_predictpos.GetBool() )
		NDebugOverlay::Cross3D( vecPos, 5.0f, 255, 0, 0, true, 5.0f );

	vecPos = g_pBigAINet->GetNodePosition( pNPC->GetHullType(), nNode );

	return true;
}

bool FindPredictedEnemyPos( CAI_BaseNPC *pNPC, CBaseEntity *pEnemy, Vector &vecOutPos )
{
	AI_EnemyInfo_t *pMemory = pNPC->GetEnemies()->Find( pEnemy );
	if ( !pMemory || gpGlobals->curtime - pMemory->timeLastSeen > PREDICTED_ENEMY_POS_MAX_TIME )
		return false;

	// Try to ascertain which direction the enemy was going in
	Vector vecGuessDir = pMemory->vLastKnownLocation - pMemory->vLastSeenLocation;
	Vector vecLastPos = pMemory->vLastKnownLocation;

	trace_t tr;
	Vector vecGuessOrigin = vecLastPos + (vecGuessDir.Normalized() * PREDICTED_ENEMY_POS_PROJECT_DIST);
	UTIL_TraceLine( vecLastPos + Vector(0,0,1), vecGuessOrigin, MASK_NPCSOLID, pEnemy, COLLISION_GROUP_NONE, &tr );
	if ( tr.startsolid )
		return false;
	
	if ( g_debug_stealth_predictpos.GetInt() == 2 )
		NDebugOverlay::HorzArrow( vecLastPos, vecGuessOrigin, 4.0f, 0, 255, 0, 128, true, 1.0f );

	vecOutPos = tr.endpos;

	int nLastNode = -1;
	if ( !DoFindPredictedEnemyPos( pNPC, pEnemy, vecOutPos, vecGuessDir, nLastNode, 0 ) )
		return false;
	
	// Subsequent iterations
	int i = 1;
	const int MAX_ITERATIONS = 8;
	for ( ; i < MAX_ITERATIONS; i++ )
	{
		if ( g_debug_stealth_predictpos.GetInt() == 2 )
			NDebugOverlay::HorzArrow( vecLastPos, vecOutPos, 4.0f, 0, 0, 255, 128, true, 1.0f );

		vecGuessDir = vecOutPos - vecLastPos;
		vecGuessDir.z = 0.0f; // Ignore Z for subsequent iterations
		vecLastPos = vecOutPos;

		if ( !DoFindPredictedEnemyPos( pNPC, pEnemy, vecOutPos, vecGuessDir, nLastNode, i ) )
		{
			if ( g_debug_stealth_predictpos.GetBool() )
			{
				NDebugOverlay::Line( vecLastPos, vecOutPos, 255, 0, 0, true, 5.0f );

				char szTemp[128] = { 0 };
				V_snprintf( szTemp, sizeof( szTemp ), "Failed at %i", i );
				NDebugOverlay::EntityTextAtPosition( vecOutPos, 0, szTemp, 5.0f, 255, 0, 0 );
			}
			break;
		}

		if ( g_debug_stealth_predictpos.GetBool() )
			NDebugOverlay::Line( vecLastPos, vecOutPos, 255, 255 / i, 255 / i, true, 5.0f );
	}

	return true;
}

//-----------------------------------------------------------------------------
