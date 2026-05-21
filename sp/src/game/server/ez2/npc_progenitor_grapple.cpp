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
	VectorNormalize( vecMoveDir );

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

				// Could we be plausibly close enough anyway?
				if ( flDistToHookSqr < Square( 200.0f ) )
					bHook = true;
				else
				{
					// Then we missed our target
					SetCondition( COND_COMBINE_GRAPPLE_FAILED );
				}
				break;
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

				VectorAngles( tr.plane.normal, m_vecGrappleAngle );
				m_vecGrappleDest = tr.endpos;

				// Take damage (assuming non-viewable entities wouldn't need to)
				Vector vecTraceDir = (tr.endpos - tr.startpos);
				VectorNormalize( vecTraceDir );
				CTakeDamageInfo info( this, this, sk_progenitor_grapple_hook_dmg.GetFloat(), DMG_SLASH );
				ClearMultiDamage();
				m_hGrappleDest->DispatchTraceAttack( info, vecTraceDir, &tr );
				ApplyMultiDamage();
			}

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
	// We use MOVETYPE_NOCLIP now, so we don't have a need for this
	/*
	else
	{
		// See if there's something near it to latch on to
		Vector vecStartPos = m_hGrapplingHookProjectile->GetAbsOrigin();
		Vector vecEndPos = vecStartPos + ( m_hGrapplingHookProjectile->GetAbsVelocity() * 0.2f );
		trace_t tr;
		UTIL_TraceLine( vecStartPos, vecEndPos, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0f )
		{
			m_hGrapplingHookProjectile->SetMoveType( MOVETYPE_NONE );
			m_hGrapplingHookProjectile->SetAbsVelocity( vec3_origin );

			QAngle angles;
			VectorAngles( tr.plane.normal, angles );
			m_hGrapplingHookProjectile->SetAbsAngles( angles );

			m_hGrapplingHookProjectile->SetAbsOrigin( tr.endpos );
		}
	}
	*/

	if ( bHook )
	{
		m_hGrapplingHookProjectile->EmitSound( "Weapon_GrapplingHook.Hook" );
		StopParticleEffects( m_hGrapplingHookProjectile );

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
	vecDest.z -= (GetHullHeight() + 8);

	UTIL_TraceEntity( this, vecDest, vecDest - Vector( 0, 0, 2048 ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
	//float flHangDist = (2048 * 0.5f * tr.fraction) + GetHullHeight();
	float flHangDist = ( vecDest.z - tr.endpos.z );
	float flMinDistToHangSqr = Square( flHangDist * 2.0f );
	bool bHanging = false;

	if ( (m_flGrappleStartDistSqr <= flMinDistToHangSqr) ? flDistSqr < flMinDistToHangSqr : fl2DDistSqr < flMinDistToHangSqr )
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
	
	// Are we at our destination?
	if ( flDistSqr < Square( 16.0f ) )
		return true;

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
		vecVelocity = VectorLerp( GetAbsVelocity(), vecVelocity, RemapValClamped( flDistSqr, m_flGrappleStartDistSqr, flAccelDistSqr, 0.1f, 1.0f ) );
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

	SetNavType( NAV_GROUND );
	SetMoveType( MOVETYPE_STEP );
	SetAbsVelocity( vec3_origin );

	float flTime = GetGroundChangeTime();
	AddStepDiscontinuity( flTime, GetAbsOrigin(), GetAbsAngles() );

	m_vecGrappleDest = vec3_invalid;
	m_hGrappleDest = NULL;

	m_iGrappleType = GRAPPLE_TYPE_NONE;

	SetIdealActivity( ACT_GLIDE ); // ACT_IDLE
	ResetActivity();

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
		m_iGrapplePhase = GRAPPLE_PHASE_NONE;

		if ( m_iGrappleType != GRAPPLE_TYPE_PULL )
			EmitSound( "NPC_Combine.Zipline_End" );

		m_bNavEvaluatedJump = false;
		m_bNavTrueJump = false;
	}
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
bool CNPC_Progenitor::ProbeGrappleTarget( const Vector &vecOrigin )
{
	// Trace directly up first
	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + Vector(0,0,2000), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction != 1.0f && !(tr.surface.flags & SURF_SKY) )
	{
		// Make sure it's close enough
		if ( (GetAbsOrigin() - tr.endpos).LengthSqr() < Square( sk_progenitor_grapple_max_dist.GetFloat() ) )
		{
			m_vecGrappleDest = tr.endpos;
			VectorAngles( tr.plane.normal, m_vecGrappleAngle );
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
			UTIL_TraceLine( vecOrigin, vecOrigin + Vector(0,0,200) + (vecDirs[i] * 96.0f), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

			if ( tr.fraction != 1.0f && !(tr.surface.flags & SURF_SKY) )
			{
				// Make sure it's close enough
				if ( (GetAbsOrigin() - tr.endpos).LengthSqr() < Square( sk_progenitor_grapple_max_dist.GetFloat() ) )
				{
					m_vecGrappleDest = tr.endpos;
					VectorAngles( tr.plane.normal, m_vecGrappleAngle );
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

	// Move the destination to be outside of the target, since LOS checks won't ignore this
	// It'd be really good to use fractionleftsolid instead of this trace filter, but I couldn't get it working for this
	CTraceFilterGrappleTarget traceFilter( pEnt, COLLISION_GROUP_NONE );
	trace_t tr;
	UTIL_TraceLine( EyePosition(), pEnt->WorldSpaceCenter(), MASK_SOLID, &traceFilter, &tr );

	vecOrigin = tr.endpos;
	VectorAngles( tr.plane.normal, angAngles );

	Vector vecVelocity = pEnt->GetAbsVelocity();
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

	m_iGrappleType = GRAPPLE_TYPE_FORCED;

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

	return CNPC_PlayerCompanion::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE);
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
	if ( IsGrappling() )
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

	switch ( GetOuter()->m_iGrapplePhase )
	{
		case GRAPPLE_PHASE_NONE:
			{
				if ( !GetOuter()->ProbeGrappleTarget( GetPath()->CurWaypointPos() ) )
				{
					// hack...
					GetOuter()->m_vecGrappleDest = GetPath()->CurWaypointPos() + Vector(0,0,64);
					GetOuter()->m_vecGrappleAngle = vec3_angle;
				}

				GetOuter()->SetIdealActivity( ACT_RANGE_ATTACK_GRAPPLE );
				GetOuter()->m_iGrapplePhase = GRAPPLE_PHASE_PRESHOT;
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

	GetMotor()->SetMoveInterval( 0 );

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
	/*else if ( pMoveGoal->flags & AILMG_TARGET_IS_GOAL )
	{
		if ( GetOuter()->ShouldSlideToGoal( pMoveGoal ) )
			GetOuter()->StartSlidingToGoal( pMoveGoal );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CNavigator::OnNavComplete()
{
	BaseClass::OnNavComplete();

	GetOuter()->m_bNavEvaluatedJump = false;
	GetOuter()->m_bNavTrueJump = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Progenitor::CNavigator::OnNewGoal()
{
	BaseClass::OnNewGoal();

	GetOuter()->m_bNavEvaluatedJump = false;
	GetOuter()->m_bNavTrueJump = false;
}
