//=============================================================================//
//
// Purpose:		Progenitor grappling hook functions
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_progenitor.h"
#include "particle_parse.h"
#include "rope.h"
#include "rope_shared.h"
#include "ai_moveprobe.h"
#include "ai_navigator.h"
#include "ai_route.h"
#include "ai_network.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------

extern ConVar	sk_progenitor_grapple_speed;
extern ConVar	sk_progenitor_grapple_decel_dist;
extern ConVar	sk_progenitor_grapple_accel_dist;
extern ConVar	sk_progenitor_grapple_hook_speed;
extern ConVar	sk_progenitor_grapple_hook_dmg;
extern ConVar	sk_progenitor_grapple_min_dist;
extern ConVar	sk_progenitor_grapple_max_dist;

extern Activity ACT_IDLE_ANGRY_GRAPPLE;
extern Activity ACT_RANGE_ATTACK_GRAPPLE;
extern Activity ACT_RANGE_ATTACK_GRAPPLE_PULL;
extern Activity ACT_GESTURE_RANGE_ATTACK_GRAPPLE;
extern Activity ACT_GESTURE_RANGE_ATTACK_GRAPPLE_PULL;
extern Activity ACT_GRAPPLE_FLY;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::CanUseGrapple() const
{
	if ( !m_bGrappleAllowed )
		return false;

	if ( IsPropShieldEquipped() )
		return false;

	if ( m_iGrapplePhase < GRAPPLE_PHASE_HOOKED && !const_cast<CNPC_Progenitor*>(this)->IsInAScript() )
	{
		// Don't grapple if our enemy might exploit it
		if ( m_flCounterTacticWeights[COUNTER_TACTIC_BRUTE] > 0.9f )
			return false;

		if ( const_cast<CNPC_Progenitor*>(this)->HasCondition( COND_SEE_ENEMY ) && GetEnemy() && (GetEnemyLKP() - GetAbsOrigin()).LengthSqr() < Square( 200.0f ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Hits nothing except one entity. See below for explanation
//-----------------------------------------------------------------------------
class CTraceFilterGrappleTarget : public CTraceFilter
{
public:
	CTraceFilterGrappleTarget( const IHandleEntity *hitent, int collisionGroup )
	{
		m_pHitEnt = hitent;
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_ENTITIES_ONLY;
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !PassServerEntityFilter( pHandleEntity, m_pHitEnt ) )
			return true;

		return false;
	}

private:
	const IHandleEntity *m_pHitEnt;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::GrappleHookMove()
{
	Vector vecHookToDest = (m_vecGrappleDest - m_hGrapplingHookProjectile->GetAbsOrigin());
	float flDistToHookSqr = vecHookToDest.LengthSqr();

	VectorNormalize( vecHookToDest );
	Vector vecMoveDir = m_hGrapplingHookProjectile->GetAbsVelocity();
	float flSpeed = VectorNormalize( vecMoveDir );

	bool bHook = false;
	if ( flDistToHookSqr < Square( 32.0f ) )
		bHook = true;

	else if ( DotProduct( vecMoveDir, vecHookToDest ) < 0.0f )
	{
		// Hook is moving away from our target, suggesting it went past it
		switch ( m_iGrappleType )
		{
			case GRAPPLE_TYPE_NAV:
				// Don't miss nav shots
				bHook = true;
				break;

			default:
			case GRAPPLE_TYPE_PULL:
			case GRAPPLE_TYPE_PULL_TO:
				{
					// Could we be plausibly close enough anyway?
					Vector vecTraceStartPos = m_hGrapplingHookProjectile->GetAbsOrigin() - ( vecMoveDir * (flSpeed * 0.1f) );
					if ( flDistToHookSqr < Square( flSpeed * 0.3f ) && (!m_hGrappleDest || !m_hGrappleDest->IsPlayer()) )
					{
						bHook = true;
					}
					// Would we be close enough if it were on a line?
					else if ( CalcDistanceSqrToLine( m_vecGrappleDest, vecTraceStartPos, m_hGrapplingHookProjectile->GetAbsOrigin() ) < Square( 32.0f ) )
					{
						bHook = true;
					}
					else if ( m_iGrappleType == GRAPPLE_TYPE_PULL_TO )
					{
						// If we're pulling to a target, then see if we can hook onto something else (e.g. a wall behind them)
						Vector vecTraceEndPos = vecTraceStartPos + ( vecMoveDir * (flSpeed * 0.2f) );
						trace_t tr;
						UTIL_TraceLine( vecTraceStartPos, vecTraceEndPos, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
						if ( tr.fraction != 1.0f )
						{
							if ( tr.DidHitNonWorldEntity() )
							{
								m_hGrappleDest = tr.m_pEnt;

								// Let the trace below handle everything else
								m_hGrapplingHookProjectile->SetAbsOrigin( tr.endpos );
							}
							else
							{
								m_hGrappleDest = NULL;

								VectorAngles( -tr.plane.normal, m_vecGrappleAngle );
								m_vecGrappleDest = tr.endpos;

								// If it hit the world, it may have hit a surface we'll need to hang from
								m_iGrappleType = GRAPPLE_TYPE_FORCED;
							}

							bHook = true;
						}

						if ( !bHook && flDistToHookSqr > Square( 500.0f ) )
						{
							// Didn't find anything
							SetCondition( COND_COMBINE_GRAPPLE_FAILED );
						}
					}
					else
					{
						// Then we missed our target
						SetCondition( COND_COMBINE_GRAPPLE_FAILED );
					}
				}
				break;
		}
	}
	else if ( m_iGrappleType == GRAPPLE_TYPE_PULL_TO && (m_hGrapplingHookProjectile->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() > Square(flSpeed * 0.15f) )
	{
		// Make sure we didn't hit anything else
		// We do this because we're MOVETYPE_NOCLIP... maybe consider using MOVETYPE_FLY again for this type of grapple and using collisions instead
		Vector vecTraceStartPos = m_hGrapplingHookProjectile->GetAbsOrigin() - (vecMoveDir * (flSpeed * 0.15f));
		trace_t tr;
		UTIL_TraceLine( vecTraceStartPos, m_hGrapplingHookProjectile->GetAbsOrigin(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0f )
		{
			if ( tr.DidHitNonWorldEntity() )
			{
				m_hGrappleDest = tr.m_pEnt;

				// Let the trace below handle everything else
				m_hGrapplingHookProjectile->SetAbsOrigin( tr.endpos );
			}
			else
			{
				m_hGrappleDest = NULL;

				VectorAngles( -tr.plane.normal, m_vecGrappleAngle );
				m_vecGrappleDest = tr.endpos;

				// If it hit the world, it may have hit a surface we'll need to hang from
				m_iGrappleType = GRAPPLE_TYPE_FORCED;
			}

			bHook = true;
		}
	}

	if ( bHook )
	{
		m_hGrapplingHookProjectile->SetMoveType( MOVETYPE_NONE );

		if ( m_hGrappleDest )
		{
			if ( m_hGrappleDest->IsViewable() )
			{
				// Trace from slightly behind in case we whizzed past it
				Vector vecTraceOrigin = m_hGrapplingHookProjectile->GetAbsOrigin() - m_hGrapplingHookProjectile->GetAbsVelocity();

				// Make sure we properly latch onto it if it moved, etc.
				trace_t tr;
				CTraceFilterGrappleTarget traceFilter( m_hGrappleDest, COLLISION_GROUP_NONE );
				UTIL_TraceLine( vecTraceOrigin, m_hGrappleDest->WorldSpaceCenter(), MASK_SOLID, &traceFilter, &tr );

				VectorAngles( -tr.plane.normal, m_vecGrappleAngle );
				m_vecGrappleDest = tr.endpos;

				// Take damage (assuming non-viewable entities wouldn't need to)
				Vector vecTraceDir = (tr.endpos - tr.startpos);
				VectorNormalize( vecTraceDir );
				CTakeDamageInfo info( this, this, sk_progenitor_grapple_hook_dmg.GetFloat(), DMG_SLASH );
				ClearMultiDamage();
				m_hGrappleDest->DispatchTraceAttack( info, vecTraceDir, &tr );
				ApplyMultiDamage();
			}

			// Have to set these before parenting
			m_hGrapplingHookProjectile->SetAbsAngles( m_vecGrappleAngle );
			m_hGrapplingHookProjectile->SetAbsOrigin( m_vecGrappleDest );
			m_hGrapplingHookProjectile->SetAbsVelocity( vec3_origin );

			m_hGrapplingHookProjectile->SetParent( m_hGrappleDest );

			// Now handled by trace attack above
			/*
			if ( m_hGrappleDest->GetMoveType() != MOVETYPE_NONE && m_hGrappleDest->VPhysicsGetObject() )
			{
				Vector vecImpulse;
				AngularImpulse vecAngImpulse;
				CalculateGrappleImpulse( m_hGrappleDest->VPhysicsGetObject(), 100.0f, vecImpulse, vecAngImpulse );
				vecImpulse *= -1.0f;
				ApplyGrappleImpulse( m_hGrappleDest, vecImpulse, vecAngImpulse );
			}
			*/
		}
		else
		{
			m_hGrapplingHookProjectile->SetAbsAngles( m_vecGrappleAngle );
			m_hGrapplingHookProjectile->SetAbsOrigin( m_vecGrappleDest );
			m_hGrapplingHookProjectile->SetAbsVelocity( vec3_origin );
		}
	}

	if ( bHook )
	{
		m_hGrapplingHookProjectile->EmitSound( "Weapon_GrapplingHook.Hook" );
		StopParticleEffects( m_hGrapplingHookProjectile );

		if ( m_hGrapplingHookCable )
		{
			// Allow motion on the rope now that it's hooked
			m_hGrapplingHookCable->m_RopeFlags |= (ROPE_USE_WIND | ROPE_RESIZE);
			m_hGrapplingHookCable->m_Slack = 10;
		}

		m_iGrapplePhase = GRAPPLE_PHASE_HOOKED;
		return true;
	}
	else if ( IsForcedGrapple() && m_hGrappleDest )
	{
		// If we're following an entity, then make sure we stay on course
		VectorNormalize( vecHookToDest );
		vecHookToDest *= sk_progenitor_grapple_hook_speed.GetFloat();
		m_hGrapplingHookProjectile->SetAbsVelocity( vecHookToDest );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::GrappleMove()
{
	if ( GetAbsOrigin() == m_vecGrappleLastOrigin )
	{
		// We must be stuck
		return true;
	}

	Vector vecVelocity = (m_vecGrappleDest - GetAbsOrigin());
	float flDistSqr = vecVelocity.LengthSqr();
	float fl2DDistSqr = vecVelocity.Length2DSqr();

	// Find a reasonable hang distance
	// (consider caching this before grappling)
	trace_t tr;
	Vector vecDest = m_vecGrappleDest;

	bool bHanging = false;
	float flHangDist = 0.0f;
	float flMinDistToHangSqr = 0.0f;

	if ( m_vecGrappleDropDest != vec3_invalid )
	{
		Vector vecDropDest = m_vecGrappleDropDest;
		vecDropDest.z += 8;
		vecDest.z -= (GetHullHeight() + 8);

		UTIL_TraceEntity( this, vecDest, vecDropDest, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
		flHangDist = ( vecDest - tr.endpos ).Length();
		flMinDistToHangSqr = Square( flHangDist * 2.0f );

		if ( (m_flGrappleStartDistSqr <= flMinDistToHangSqr)
			? flDistSqr < flMinDistToHangSqr
			: fl2DDistSqr < flMinDistToHangSqr )
		{
			// Make sure we can actually go to the custom position
			UTIL_TraceEntity( this, GetAbsOrigin(), vecDropDest, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction == 1.0f )
			{
				fl2DDistSqr = (vecDropDest - GetAbsOrigin()).Length2DSqr();
				vecDest = VectorLerp( vecDest, vecDropDest, SmoothCurve_Tweak( 1.0f - (fl2DDistSqr / flMinDistToHangSqr) ) );

				// Recalculate 3D and 2D position
				vecVelocity = (vecDest - GetAbsOrigin());
				flDistSqr = vecVelocity.LengthSqr();
				fl2DDistSqr = vecVelocity.Length2DSqr();
				bHanging = true;
			}
		}
	}
	else if ( !IsToObjGrapple() )
	{
		vecDest.z -= (GetHullHeight() + 8);

		UTIL_TraceEntity( this, vecDest, vecDest - Vector( 0, 0, 2048 ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
		//float flHangDist = (2048 * 0.5f * tr.fraction) + GetHullHeight();
		flHangDist = ( vecDest.z - tr.endpos.z );
		flMinDistToHangSqr = Square( flHangDist * 2.0f );

		if ( (m_flGrappleStartDistSqr <= flMinDistToHangSqr)
			? flDistSqr < flMinDistToHangSqr
			: fl2DDistSqr < flMinDistToHangSqr )
		{
			// Make sure we could actually go to the hang position
			vec_t vHangFactor = ( flHangDist * ( 1.0f - (fl2DDistSqr / flMinDistToHangSqr) ) );

			UTIL_TraceEntity( this, GetAbsOrigin(), vecDest - Vector(0,0,vHangFactor), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
			if ( tr.fraction == 1.0f )
			{
				vecDest.z -= vHangFactor;

				// Recalculate 3D position
				vecVelocity = (vecDest - GetAbsOrigin());
				flDistSqr = vecVelocity.LengthSqr();
				bHanging = true;
			}
		}
	}
	
	// Are we at our destination?
	if ( m_hGrappleDest && m_vecGrappleDropDest == vec3_invalid )
	{
		if ( flDistSqr < Square( m_hGrappleDest->BoundingRadius() + 8.0f ) )
			return true;
	}
	else
	{
		if ( flDistSqr < Square( 16.0f ) )
			return true;
	}

	VectorNormalize( vecVelocity );

	const float flDecelDistSqr = Square( sk_progenitor_grapple_decel_dist.GetFloat() );
	const float flAccelDistSqr = m_flGrappleStartDistSqr - Square( sk_progenitor_grapple_accel_dist.GetFloat() );

	if ( flDistSqr < flDecelDistSqr)
	{
		// Decelerate on approach
		vecVelocity *= RemapValClamped( flDistSqr, 0.0f, flDecelDistSqr, 0.2f, 1.0f );
	}
	/*else if ( flDistSqr > flAccelDistSqr )
	{
		// Accelerate on start
		vecVelocity *= RemapValClamped( flDistSqr, flAccelDistSqr, m_flGrappleStartDistSqr, 1.0f, 0.2f );
	}*/

	// Trace before we apply our speed
	UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin() + ( Vector(64,64,-72) * vecVelocity ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	// Are we stuck, but close enough to detach?
	if ( tr.fraction == 0.0f && flDistSqr < Square( 64.0f ) )
		return true;

	vecVelocity *= sk_progenitor_grapple_speed.GetFloat();

	if ( flDistSqr > flAccelDistSqr )
	{
		// Accelerate on start
		vecVelocity = VectorLerp( GetAbsVelocity(), vecVelocity, RemapValClamped( flDistSqr, flAccelDistSqr, m_flGrappleStartDistSqr, 1.0f, 0.1f ) );
	}

	if ( tr.fraction > 0.1f )
	{
		// Allow gravity to take hold if we aren't too close to the ground
		float flGravFactor = RemapValClamped( fl2DDistSqr, 0.0f, Square( 1000.0f ), 0.0f, 0.5f * tr.fraction );

		if ( bHanging )
		{
			// Add more gravity so that we hang slightly below
			flGravFactor += RemapValClamped( vecDest.z - GetAbsOrigin().z, flHangDist, 0.0f, 1.0f - (fl2DDistSqr / flMinDistToHangSqr), 0.0f );
		}

		extern float GetCurrentGravity( void );
		vecVelocity.z -= ( GetCurrentGravity() * flGravFactor );
	}

	SetAbsVelocity( vecVelocity );

	m_vecGrappleLastOrigin = GetAbsOrigin();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::StartGrappling( int nType )
{
	if ( IsGrappling() )
		return;

	m_iGrapplePhase = GRAPPLE_PHASE_GRAPPLING;
	m_iGrappleType = nType;

	m_vecGrappleLastOrigin = vec3_origin;

	SetNavType( NAV_JUMP );
	SetMoveType( MOVETYPE_FLY );
	SetGroundEntity( NULL );

	Vector vecVelocity = (m_vecGrappleDest - GetAbsOrigin());
	m_flGrappleStartDistSqr = vecVelocity.LengthSqr();
	//VectorNormalize( vecVelocity );

	GetMotor()->SetIdealYawToTarget( m_vecGrappleDest );

	EmitSound( "NPC_Combine.Zipline_Mid" );

	if ( m_hGrapplingHookCable )
	{
		// Technically shouldn't be any slack, but add just enough to shake the rope
		m_hGrapplingHookCable->m_Slack = 2;
	}

	if ( m_nGrappleLayer != -1 )
	{
		SetLayerAutokill( m_nGrappleLayer, true );
		RemoveLayer( m_nGrappleLayer );
		m_nGrappleLayer = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::StopGrappling( bool bCancel )
{
	RemoveGrapplingEntities();

	if ( m_iGrapplePhase == GRAPPLE_PHASE_NONE || m_iGrapplePhase == GRAPPLE_PHASE_LANDING )
		return;

	if ( m_iGrappleType != GRAPPLE_TYPE_PULL )
	{
		SetNavType( NAV_GROUND );
		SetMoveType( MOVETYPE_STEP );
		SetAbsVelocity( vec3_origin );

		float flTime = GetGroundChangeTime();
		AddStepDiscontinuity( flTime, GetAbsOrigin(), GetAbsAngles() );

		SetIdealActivity( ACT_GLIDE ); // ACT_IDLE
		ResetActivity();
	}

	m_vecGrappleDest = vec3_invalid;
	m_vecGrappleDropDest = vec3_invalid;
	m_hGrappleDest = NULL;

	if ( m_nGrappleLayer != -1 )
	{
		SetLayerAutokill( m_nGrappleLayer, true );
		RemoveLayer( m_nGrappleLayer );
		m_nGrappleLayer = -1;
	}

	// UNDONE: Botched nav jump
	/*
		// Try jumping from where we are
		int nNode = GetNavigator()->GetNetwork()->NearestNodeToPoint( this, GetAbsOrigin() );
		if ( nNode == NO_NODE )
			return;

		AIMoveTrace_t moveTrace;
		GetMoveProbe()->MoveLimit( NAV_JUMP, GetLocalOrigin(), GetNavigator()->GetNetwork()->GetNodePosition( this, nNode ),
			MASK_NPCSOLID, GetNavTargetEntity(), &moveTrace );
		if ( !IsMoveBlocked( moveTrace ) )
		{
			SetNavType( NAV_JUMP );
			GetMotor()->MoveJumpStart( moveTrace.vJumpVelocity );
		}
	*/

	if ( !bCancel )
	{
		m_iGrapplePhase = GRAPPLE_PHASE_LANDING;
	}
	else
	{
		if ( IsGrappling() )
			EmitSound( "NPC_Combine.Zipline_End" );

		m_iGrapplePhase = GRAPPLE_PHASE_NONE;

		m_bNavEvaluatedJump = false;
		m_bNavTrueJump = false;
	}

	m_iGrappleType = GRAPPLE_TYPE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::RemoveGrapplingEntities()
{
	if ( m_hGrapplingHook )
	{
		UTIL_Remove( m_hGrapplingHook );
		m_hGrapplingHook = NULL;
	}

	if ( m_hGrapplingHookProjectile )
	{
		m_hGrapplingHookProjectile->StopSound( "Weapon_GrapplingHook.Wire" );
		UTIL_Remove( m_hGrapplingHookProjectile );
		m_hGrapplingHookProjectile = NULL;
	}

	if ( m_hGrapplingHookCable )
	{
		UTIL_Remove( m_hGrapplingHookCable );
		m_hGrapplingHookCable = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static bool GrappleHintFilter( void *pContext, CAI_Hint *pCandidate )
{
	const Vector &vecOrigin = *(Vector *)pContext;
	const Vector &vecHintOrigin = pCandidate->GetAbsOrigin();

	if ( pCandidate->GetTargetNode() != NO_NODE )
	{
		// If this hint has a target node, ignore distance and just check if
		// the node's origin is roughly at our search origin
		CAI_Node *pNode = g_pBigAINet->GetNode( pCandidate->GetTargetNode() );
		if ( pNode )
		{
			if ( (pNode->GetOrigin() - vecOrigin).LengthSqr() < Square( 32.0f ) )
				return true;

			return false;
		}
	}

	// Check height
	if ( abs( vecHintOrigin.z - vecOrigin.z ) > 2000.0f )
		return false;

	// Check 2D distance
	Vector vecDelta = (pCandidate->GetAbsOrigin() - vecOrigin);
	if ( vecDelta.Length2DSqr() > Square( 500.0f ) )
		return false;

	// Superseded by node FOV check below
	/*VectorNormalize( vecDelta );

	// Check angle
	if ( vecDelta.z < DOT_45DEGREE && pCandidate->GetIgnoreFacing() != HIF_YES )
		return false;*/

	// Check FOV
	if ( !pCandidate->IsInNodeFOV( vecOrigin ) )
		return false;

	// See if this point is visible to the hint
	if ( !pCandidate->FVisible( vecOrigin, MASK_BLOCKLOS ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ProbeGrappleTarget( const Vector &vecOrigin, bool bSetGrappleDest )
{
	// Find a hint with low 2D distance
	CHintCriteria hintCriteria;
	hintCriteria.SetGroup( GetHintGroup() );
	hintCriteria.SetHintType( HINT_GRAPPLE_POINT );
	hintCriteria.SetFlag( bits_HINT_NODE_NEAREST /*| bits_HINT_NODE_VISIBLE | bits_HINT_NPC_IN_NODE_FOV*/ ); // NPC may not be in position

	// Filter func handles distance check
	hintCriteria.SetFilterFunc( GrappleHintFilter, &const_cast<Vector&>(vecOrigin) );
	//hintCriteria.AddIncludePosition( vecOrigin, FLT_MAX );

	CAI_Hint *pHint = CAI_HintManager::FindHint( this, vecOrigin, hintCriteria );
	if ( pHint )
	{
		if ( bSetGrappleDest )
		{
			// Do not set hint node in case we're already navigating to one.
			// Just use its origin and angles
			m_vecGrappleDest = pHint->GetAbsOrigin();
			m_vecGrappleAngle = pHint->GetAbsAngles();

			// Only set drop dest if we're not directly below
			if ( (m_vecGrappleDest.AsVector2D() - vecOrigin.AsVector2D()).LengthSqr() > Square( 4.0f ) )
				m_vecGrappleDropDest = vecOrigin;
		}
		return true;
	}

	// If no hint, then find grapple location procedurally
	// Trace directly up first
	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + Vector(0,0,2000), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction != 1.0f && !(tr.surface.flags & SURF_SKY) )
	{
		// Make sure it's close enough
		if ( (GetAbsOrigin() - tr.endpos).LengthSqr() < Square( sk_progenitor_grapple_max_dist.GetFloat() ) )
		{
			if ( bSetGrappleDest )
			{
				m_vecGrappleDest = tr.endpos;
				VectorAngles( tr.plane.normal, m_vecGrappleAngle );
			}
			return true;
		}
	}

	// If it got at least 200 units up, try protruding in other directions
	if ( tr.fraction >= 0.1f )
	{
		Vector vecDirs[4] = {
			Vector( 1, 0, 0 ), // 0
			Vector( 0, 1, 0 ), // 90
			Vector( -1, 0, 0 ), // 180
			Vector( 0, -1, 0 ), // 270
		};

		for ( int i = 0; i < 4; i++ )
		{
			UTIL_TraceLine( vecOrigin, vecOrigin + Vector(0,0,200) + (vecDirs[i] * 200.0f), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

			if ( tr.fraction != 1.0f && !(tr.surface.flags & SURF_SKY) )
			{
				// Make sure it's close enough and visible
				if ( (GetAbsOrigin() - tr.endpos).LengthSqr() < Square( sk_progenitor_grapple_max_dist.GetFloat() ) && FVisible( tr.endpos - vecDirs[i] ) )
				{
					if ( bSetGrappleDest )
					{
						m_vecGrappleDest = tr.endpos;
						VectorAngles( tr.plane.normal, m_vecGrappleAngle );
						m_vecGrappleDropDest = vecOrigin;
					}
					return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::GrappleTraceTest( CBaseEntity *pTarget, float flMaxDistSqr )
{
	float flDistSqr = ( pTarget->GetAbsOrigin() - GetAbsOrigin() ).LengthSqr();
	if ( flDistSqr > Square( 200.0f ) && flDistSqr < flMaxDistSqr )
	{
		// Would a small box from our eyes make it through?
		// (we don't do a full hull because we can slide against obstacles... making a lot of assumptions here, though)
		trace_t tr;
		Vector bounds( 12, 12, 12 );
		UTIL_TraceHull( EyePosition(), pTarget->EyePosition(), -bounds, bounds, MASK_SHOT_HULL, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction == 1.0f || tr.m_pEnt == pTarget )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CalculateGrappleImpulse( IPhysicsObject *pPhys, float flMagnitude, Vector &vecImpulse, AngularImpulse &vecAngImpulse )
{
	Assert( m_hGrapplingHookProjectile );

	Vector vecToProj = (WorldSpaceCenter() - m_hGrapplingHookProjectile->GetAbsOrigin());
	VectorNormalize( vecToProj );

	pPhys->CalculateForceOffset( vecToProj * flMagnitude, m_hGrapplingHookProjectile->GetAbsOrigin(), &vecImpulse, &vecAngImpulse );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::ApplyGrappleImpulse( CBaseEntity *pEntity, Vector &vecImpulse, AngularImpulse &vecAngImpulse )
{
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		pEntity->VPhysicsGetObject()->AddVelocity( &vecImpulse, &vecAngImpulse );
	}
	else
	{
		Vector vecResult;
		VectorAdd( pEntity->GetAbsVelocity(), vecImpulse, vecResult );
		pEntity->SetAbsVelocity( vecResult );

		if ( !pEntity->IsCombatCharacter() )
		{
			QAngle angResult;
			AngularImpulseToQAngle( vecAngImpulse, angResult );
			VectorAdd( pEntity->GetLocalAngularVelocity(), angResult, angResult );
			pEntity->SetLocalAngularVelocity( angResult );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::GetGrappleDestForEntity( CBaseEntity *pEnt, Vector &vecOrigin, QAngle &angAngles )
{
	if ( !pEnt->IsViewable() )
	{
		// Don't need to complicate it
		vecOrigin = pEnt->GetAbsOrigin();
		angAngles = pEnt->GetAbsAngles();
		return;
	}
	
	if ( m_iGrapplePhase < GRAPPLE_PHASE_HOOKED )
	{
		// Move the destination to be outside of the target, since LOS checks won't ignore this
		// It'd be really good to use fractionleftsolid instead of this trace filter, but I couldn't get it working for this
		CTraceFilterGrappleTarget traceFilter( pEnt, COLLISION_GROUP_NONE );
		trace_t tr;
		UTIL_TraceLine( EyePosition(), pEnt->WorldSpaceCenter(), MASK_SOLID, &traceFilter, &tr );

		vecOrigin = tr.endpos;
		VectorAngles( tr.plane.normal, angAngles );

		Vector vecVelocity = pEnt->GetSmoothedVelocity();
		float flEntSpeed = VectorNormalize( vecVelocity );
		if ( pEnt->IsCombatCharacter() && flEntSpeed > 0.0f )
		{
			// Lead it based on how far away it is
			Vector vecToDest = (vecOrigin - GetAbsOrigin());
			float flTimeToReach = 0.0f;
			if ( sk_progenitor_grapple_hook_speed.GetFloat() > 0.0f )
				flTimeToReach = ( vecToDest.LengthSqr() / Square( sk_progenitor_grapple_hook_speed.GetFloat() ) );

			vecOrigin += (vecVelocity * flTimeToReach);
		}
	}
	else
	{
		// We're already hooked to the target, so just follow their center
		vecOrigin = pEnt->WorldSpaceCenter();

		/*if ( pEnt->IsCombatCharacter() )
		{
			// Follow half-way to the origin, since the grapple dest will be our origin and we don't want to come in above them
			// (This is mostly important for when we're grappling to the player)
			Vector vecCenterToOrigin = ( pEnt->GetAbsOrigin() - vecOrigin );
			vecOrigin += (vecCenterToOrigin * 0.5f);
		}*/
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::InputGrappleToTargetForced( inputdata_t &inputdata )
{
	char szParam[128];
	V_strncpy( szParam, inputdata.value.String(), sizeof( szParam ) );

	m_vecGrappleDropDest = vec3_invalid;

	char *pszSpace = V_strstr( szParam, " " );
	if ( pszSpace )
	{
		// Drop target specified
		CBaseEntity *pDropTarget = gEntList.FindEntityByName( NULL, pszSpace + 1, this, inputdata.pActivator, inputdata.pCaller );
		if ( pDropTarget )
		{
			m_vecGrappleDropDest = pDropTarget->GetAbsOrigin();
			*pszSpace = '\0';
		}
	}

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, szParam, this, inputdata.pActivator, inputdata.pCaller );
	if ( !pTarget )
		return;

	m_hGrappleDest = pTarget;
	m_vecGrappleDest = pTarget->GetAbsOrigin();
	m_vecGrappleAngle = pTarget->GetAbsAngles();

	m_iGrappleType = GRAPPLE_TYPE_FORCED;

	ClearSchedule( "Told to grapple via input" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::InputGrappleToTarget( inputdata_t &inputdata )
{
	CBaseEntity *pTarget = inputdata.value.Entity();
	if ( !pTarget )
		return;

	m_hGrappleDest = pTarget;
	m_vecGrappleDest = pTarget->GetAbsOrigin();
	m_vecGrappleAngle = pTarget->GetAbsAngles();

	m_iGrappleType = GRAPPLE_TYPE_PULL_TO;

	ClearSchedule( "Told to grapple via input" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::InputGrapplePullTarget( inputdata_t &inputdata )
{
	CBaseEntity *pTarget = inputdata.value.Entity();
	if ( !pTarget )
		return;

	m_hGrappleDest = pTarget;
	GetGrappleDestForEntity( m_hGrappleDest, m_vecGrappleDest, m_vecGrappleAngle );

	m_iGrappleType = GRAPPLE_TYPE_PULL;

	ClearSchedule( "Told to grapple via input" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::AimGun()
{
	if ( m_iGrapplePhase != GRAPPLE_PHASE_NONE && m_vecGrappleDest != vec3_invalid )
	{
		Vector vecToDest;
		if ( IsPullObjGrapple() && IsGrappleHooked() && m_hGrapplingHookProjectile )
		{
			// Aim at our grappling hook
			vecToDest = (m_hGrapplingHookProjectile->GetAbsOrigin() - EyePosition());
		}
		else
		{
			// Aim at our grappling destination
			vecToDest = (m_vecGrappleDest - GetAbsOrigin());
		}

		VectorNormalize( vecToDest );
		SetAim( vecToDest );
		return;
	}

	BaseClass::AimGun();
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	if ( !CanUseGrapple() )
		return IsTrueJumpLegal( startPos, apex, endPos );

	// We use jump navigation to hook into grappling AI, so our values are more extreme here
	// Use IsTrueJumpLegal() for actual jumping
	const float MAX_JUMP_RISE			= sk_progenitor_grapple_max_dist.GetFloat();		// 64
	const float MAX_JUMP_DISTANCE		= sk_progenitor_grapple_max_dist.GetFloat();		// 224
	const float MAX_JUMP_DROP			= sk_progenitor_grapple_max_dist.GetFloat();		// 384

	if ( !CNPC_PlayerCompanion::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE) )
		return false;

	// Make sure our grappling hook can actually be used here
	if ( !const_cast<CNPC_Progenitor*>(this)->ProbeGrappleTarget( endPos, false ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a reasonable jumping distance
// Input  :
// Output :
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::IsTrueJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const
{
	// Use smaller values than Clone Cop
	const float MAX_JUMP_RISE = 64.0f;
	const float MAX_JUMP_DISTANCE = 224.0f;
	const float MAX_JUMP_DROP = 384.0f;

	return CNPC_PlayerCompanion::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::OnMovementFailed()
{
	BaseClass::OnMovementFailed();

	if ( m_iGrapplePhase != GRAPPLE_PHASE_NONE )
	{
		StopGrappling();
	}
	else
	{
		m_bNavEvaluatedJump = false;
		m_bNavTrueJump = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::IsInterruptable()
{
	// Technically we should only truly be uninterruptable when grappling, but usually the conditions
	// that would interrupt us when we're shooting will become irrelevant after we start grappling
	// So if we've already gotten far enough to shoot, then consider us uninterruptable
	if ( m_iGrapplePhase >= GRAPPLE_PHASE_SHOT && ( IsNavGrapple() || IsForcedGrapple() ) )
		return false;

	return BaseClass::IsInterruptable();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CNPC_Progenitor::GetHealthItemRange( bool bFollowing )
{
	if ( CanUseGrapple() )
	{
		// Can pull them to us
		return BaseClass::GetHealthItemRange() * 4.0f;
	}

	return BaseClass::GetHealthItemRange();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
AIMoveResult_t CNPC_Progenitor::CNavigator::MoveJump()
{
	if ( !GetOuter()->CanUseGrapple() || ( GetOuter()->m_iGrappleType != GRAPPLE_TYPE_NONE && !GetOuter()->IsNavGrapple() ) )
		return BaseClass::MoveJump();

	if ( !GetOuter()->m_bNavEvaluatedJump )
	{
		if ( GetOuter()->m_iGrapplePhase != GRAPPLE_PHASE_NONE )
			GetOuter()->StopGrappling();

		// Determine if this is a grapple or a true jump
		if (GetOuter()->IsTrueJumpLegal( GetLocalOrigin(),
			GetPath()->CurWaypointPos(),
			GetPath()->CurWaypointPos() ))
		{
			GetOuter()->m_bNavTrueJump = true;
		}

		GetOuter()->m_bNavEvaluatedJump = true;
	}

	if ( GetOuter()->m_bNavTrueJump )
	{
		if (GetNavType() == NAV_JUMP && (GetEntFlags() & FL_ONGROUND))
		{
			// Jump is finished
			GetOuter()->m_bNavEvaluatedJump = false;
			GetOuter()->m_bNavTrueJump = false;
		}

		return BaseClass::MoveJump();
	}

	GetMotor()->SetMoveInterval( 0 );

	switch ( GetOuter()->m_iGrapplePhase )
	{
		case GRAPPLE_PHASE_NONE:
			{
				if ( !GetOuter()->ProbeGrappleTarget( GetPath()->CurWaypointPos() ) )
				{
					// It shouldn't be possible for this to fail since we probed it in IsJumpLegal(),
					// but just continue on the path instead of suddenly cancelling
					GetOuter()->m_vecGrappleDest = GetPath()->CurWaypointPos() /*+ Vector(0,0,64)*/;
					GetOuter()->m_vecGrappleAngle = QAngle(-90,0,0);
				}

				// This makes sure we don't have any lingering blend sequences in CAI_BlendedMotor
				GetMotor()->MoveStop();

				GetOuter()->SetIdealActivity( ACT_RANGE_ATTACK_GRAPPLE );
				GetOuter()->m_iGrapplePhase = GRAPPLE_PHASE_PRESHOT;
				GetOuter()->m_iGrappleType = GRAPPLE_TYPE_NAV;
			}
			//break;
		case GRAPPLE_PHASE_PRESHOT:
		case GRAPPLE_PHASE_SHOT:
			{
				if ( GetOuter()->IsActivityFinished() && GetActivity() == ACT_RANGE_ATTACK_GRAPPLE )
				{
					SetActivity( ACT_IDLE_ANGRY_GRAPPLE );
				}
				else if ( GetOuter()->m_nGrappleLayer != -1 )
				{
					if ( GetOuter()->IsLayerFinished( GetOuter()->m_nGrappleLayer ) )
					{
						// If we're doing the jump execution, we don't need the gesture anymore
						GetOuter()->SetLayerAutokill( GetOuter()->m_nGrappleLayer, true );
						GetOuter()->RemoveLayer( GetOuter()->m_nGrappleLayer );
						GetOuter()->m_nGrappleLayer = -1;
					}

					if ( GetActivity() != ACT_IDLE_ANGRY_GRAPPLE )
						SetActivity( ACT_IDLE_ANGRY_GRAPPLE );
				}

				GetMotor()->SetIdealYawToTargetAndUpdate( GetPath()->CurWaypointPos() );
			}
			break;
		case GRAPPLE_PHASE_HOOKED:
			{
				GetOuter()->StartGrappling( GRAPPLE_TYPE_NAV );
				GetOuter()->SetIdealActivity( ACT_GRAPPLE_FLY );

				if ( GetOuter()->m_nGrappleLayer != -1 )
				{
					GetOuter()->SetLayerAutokill( GetOuter()->m_nGrappleLayer, true );
					GetOuter()->RemoveLayer( GetOuter()->m_nGrappleLayer );
					GetOuter()->m_nGrappleLayer = -1;
				}
			}
			break;
		case GRAPPLE_PHASE_GRAPPLING:
			{
				Vector vecYawDir = GetOuter()->GetAbsVelocity();
				VectorNormalize( vecYawDir );

				if ( GetEnemy() )
				{
					// Face enemy if they are in front of us
					Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
					VectorNormalize( vecToEnemy );

					if ( DotProduct( vecYawDir, vecToEnemy ) > 0.0f )
						vecYawDir = vecToEnemy;
				}

				GetMotor()->SetIdealYawAndUpdate( UTIL_VecToYaw( vecYawDir ) );
				GetOuter()->AimGun();

				if ( GetOuter()->GrappleMove() )
				{
					GetOuter()->StopGrappling( false );
				}
			}
			break;
		case GRAPPLE_PHASE_LANDING:
			{
				if ( GetEntFlags() & FL_ONGROUND )
				{
					SetActivity( ACT_LAND );
					GetOuter()->EmitSound( "NPC_Combine.Zipline_End" );

					GetOuter()->m_iGrapplePhase = GRAPPLE_PHASE_NONE;

					GetOuter()->m_bNavEvaluatedJump = false;
					GetOuter()->m_bNavTrueJump = false;

					if ( CurWaypointIsGoal() )
					{
						OnNavComplete();
						return AIMR_OK;
					}
					else 
					{
						AdvancePath();
						return AIMR_CHANGE_TYPE;
					}
				}
			}
			break;
	}

	return AIMR_OK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CNavigator::MoveCalcBaseGoal( AILocalMoveGoal_t *pMoveGoal )
{
	BaseClass::MoveCalcBaseGoal( pMoveGoal );

	if ( pMoveGoal->flags & AILMG_TARGET_IS_TRANSITION )
	{
		AI_Waypoint_t *pCurWaypoint = GetPath()->GetCurWaypoint();
		if ( pCurWaypoint->GetNext() && pCurWaypoint->GetNext()->NavType() == NAV_JUMP && GetOuter()->m_nGrappleLayer == -1 )
		{
			AI_Waypoint_t *pJumpStartPoint	= pCurWaypoint;
			AI_Waypoint_t *pJumpEndPoint	= pCurWaypoint->GetNext();

			if ( !pJumpEndPoint )
				return;

			// See if we should try the grappling gesture
			if ( !GetOuter()->CanUseGrapple() || GetOuter()->m_iGrappleType != GRAPPLE_TYPE_NONE )
				return;

			// Determine if this is a grapple or a true jump
			if ( GetOuter()->IsTrueJumpLegal( pJumpStartPoint->GetPos(),
				pJumpEndPoint->GetPos(),
				pJumpEndPoint->GetPos() ) )
			{
				return;
			}

			// Grapple now
			GetOuter()->m_bNavEvaluatedJump = true;
			GetOuter()->m_bNavTrueJump = false;

			if ( !GetOuter()->ProbeGrappleTarget( pJumpEndPoint->GetPos() ) )
			{
				// hack...
				GetOuter()->m_vecGrappleDest = pJumpEndPoint->GetPos() + Vector(0,0,64);
				GetOuter()->m_vecGrappleAngle = vec3_angle;
			}

			GetOuter()->m_nGrappleLayer = GetOuter()->AddGesture( ACT_GESTURE_RANGE_ATTACK_GRAPPLE );
			GetOuter()->SetLayerAutokill( GetOuter()->m_nGrappleLayer, false );
			GetOuter()->m_iGrapplePhase = GRAPPLE_PHASE_PRESHOT;
			GetOuter()->ResetActivity();

			// Face the target
			float flDuration = GetOuter()->GetLayerDuration( GetOuter()->m_nGrappleLayer );
			GetOuter()->AddFacingTarget( GetOuter()->m_vecGrappleDest, 1.0f, flDuration );

			if ( GetEnemy() )
			{
				// Overwrite facing target given by move shoot
				// (shot regulator in default action gesture portion should prevent shooting during this gesture)
				GetOuter()->AddFacingTarget( GetEnemy(), GetEnemyLKP(), 0.1f, 0.1f );

				// NOTE: This could get overwritten by CNPC_Combine::PrescheduleThink() if we're close enough to our goal
				GetOuter()->m_MoveAndShootOverlay.SuspendMoveAndShoot( flDuration );
			}
		}
	}
	else if ( pMoveGoal->flags & AILMG_TARGET_IS_GOAL )
	{
		if ( GetOuter()->ShouldSlideToGoal( pMoveGoal ) )
			GetOuter()->StartSlidingToGoal( pMoveGoal );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CNavigator::OnNavComplete()
{
	BaseClass::OnNavComplete();

	GetOuter()->m_bNavEvaluatedJump = false;
	GetOuter()->m_bNavTrueJump = false;

	if ( GetOuter()->m_bSliding )
		GetOuter()->StopSliding( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CNavigator::OnNewGoal()
{
	BaseClass::OnNewGoal();

	GetOuter()->m_bNavEvaluatedJump = false;
	GetOuter()->m_bNavTrueJump = false;

	if ( GetOuter()->IsNavGrapple() && GetOuter()->m_iGrapplePhase > GRAPPLE_PHASE_NONE && GetOuter()->m_iGrapplePhase < GRAPPLE_PHASE_GRAPPLING )
	{
		GetOuter()->StopGrappling();
	}

	if ( GetOuter()->m_bSliding )
		GetOuter()->StopSliding();
}
