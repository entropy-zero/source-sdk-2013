//=============================================================================//
//
// Purpose:		Special navigator/motor override for generic advanced movement techniques
//				(mainly sliding and shooting while jumping)
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_acrobatic_movement.h"
#include "ai_route.h"
#include "ai_moveprobe.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//ConVar	ai_acrobatic_slide_decel_dist( "ai_acrobatic_slide_decel_dist", "100" );
ConVar	ai_acrobatic_jump_gesture_grav( "ai_acrobatic_jump_gesture_grav", "0.95" );
ConVar	ai_acrobatic_jump_gesture_speed( "ai_acrobatic_jump_gesture_speed", "250" );

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( CAI_AcrobaticNavigator )

	DEFINE_FIELD( m_bSliding, FIELD_BOOLEAN ),

END_DATADESC()

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_AcrobaticNavigator::StartSlidingToGoal( AILocalMoveGoal_t *pMoveGoal )
{
	m_bSliding = true;
	SetArrivalActivity( ACT_RANGE_AIM_LOW );
	GetOuter()->ResetActivity();

	GetOuter()->EmitSound( "AI_BaseNPC.Slide" ); // TODO: Allow override?
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_AcrobaticNavigator::StopSliding( bool bIntoCrouch )
{
	m_bSliding = false;

	if ( bIntoCrouch )
		GetOuter()->DispatchCrouch();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_AcrobaticNavigator::MoveCalcBaseGoal( AILocalMoveGoal_t *pMoveGoal )
{
	BaseClass::MoveCalcBaseGoal( pMoveGoal );

	if ( pMoveGoal->flags & AILMG_TARGET_IS_GOAL )
	{
		if ( !m_bSliding && GetSink()->ShouldSlideToGoal( pMoveGoal ) )
			StartSlidingToGoal( pMoveGoal );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_AcrobaticNavigator::OnNavComplete()
{
	BaseClass::OnNavComplete();

	if ( m_bSliding )
		StopSliding( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_AcrobaticNavigator::OnNewGoal()
{
	BaseClass::OnNewGoal();

	if ( m_bSliding )
		StopSliding();
}

//-----------------------------------------------------------------------------

BEGIN_SIMPLE_DATADESC( CAI_AcrobaticMotor )

	DEFINE_FIELD( m_nGlideLayer, FIELD_INTEGER ),
	DEFINE_FIELD( m_nJumpLayer, FIELD_INTEGER ),

END_DATADESC()

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_AcrobaticMotor::StartGlideLayer()
{
	m_nGlideLayer = GetOuter()->AddGesture( GetOuter()->Weapon_TranslateActivity( ACT_GESTURE_GLIDE ), false );
	if ( GetOuter()->IsValidLayer( m_nGlideLayer ) )
	{
		GetOuter()->SetLayerLooping( m_nGlideLayer, true );
		GetSink()->OnStartGestureJump();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_AcrobaticMotor::StopGlideLayer()
{
	Assert( IsUsingGlideLayer() );

	GetOuter()->SetLayerAutokill( m_nGlideLayer, true );
	GetOuter()->RemoveLayer( m_nGlideLayer );
	m_nGlideLayer = -1;

	if ( GetOuter()->IsValidLayer( m_nJumpLayer ) )
	{
		GetOuter()->SetLayerAutokill( m_nJumpLayer, true );
		GetOuter()->RemoveLayer( m_nJumpLayer );
		m_nJumpLayer = -1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_AcrobaticMotor::MoveJumpStart( const Vector &velocity )
{
	if ( GetSink()->ShouldUseJumpGesture() )
	{
		// Use the gesture layer instead of jumping
		if ( StartGlideLayer() )
		{
			SetSmoothedVelocity( velocity );
			SetGravity( GetOuter()->GetJumpGravity() );
			SetGroundEntity( NULL );

			m_nJumpLayer = GetOuter()->AddGesture( GetOuter()->Weapon_TranslateActivity( ACT_GESTURE_JUMP ) );
			if ( GetOuter()->IsValidLayer( m_nJumpLayer ) )
			{
				GetOuter()->SetLayerAutokill( m_nJumpLayer, false ); // Take ownership so that the slot doesn't get reassigned while we're tracking it
				//GetOuter()->SetLayerPriority( nJumpLayer, 1 );

				// Blend the glide in to about half of the jump layer duration
				float flDuration = GetOuter()->GetLayerDuration( m_nJumpLayer );
				GetOuter()->SetLayerBlendIn( m_nGlideLayer, flDuration * 0.5f );
				GetOuter()->SetLayerWeight( m_nGlideLayer, 0.0f );

				if ( GetSink()->ShouldJumpGestureDelayShoot() )
				{
					// Don't shoot until the jump gesture is over
					GetOuter()->GetShotRegulator()->FireNoEarlierThan( gpGlobals->curtime + flDuration );
				}
			}

			GetOuter()->SetIdealActivity( ACT_IDLE );
			GetOuter()->ResetActivity();
			return;
		}
	}

	BaseClass::MoveJumpStart( velocity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_AcrobaticMotor::MoveJumpExecute()
{
	if ( IsUsingGlideLayer() )
	{
		// Make sure the glide layer doesn't keep blending after it loops
		if ( GetOuter()->GetLayerCycle( m_nGlideLayer ) > 0.5f )
			GetOuter()->SetLayerBlendIn( m_nGlideLayer, 0.0f );

		if ( GetOuter()->IsValidLayer( m_nJumpLayer ) && GetOuter()->IsLayerFinished( m_nJumpLayer ) )
		{
			GetOuter()->SetLayerAutokill( m_nJumpLayer, true );
			GetOuter()->RemoveLayer( m_nJumpLayer );
			m_nJumpLayer = -1;
		}

		// We want to continue running facing if the glide is a layer
		
		// HACKHACK: It seems that moveshoot enemy facing still isn't convinced for whatever reason.
		// So we need to set it ourselves
		/*if ( GetOuter()->HasCondition( COND_SEE_ENEMY ) )
		{
			//Vector vecToEnemy = (GetEnemyLKP() - GetAbsOrigin());
			//VectorNormalize( vecToEnemy );
			//SetIdealYawAndUpdate( UTIL_VecToYaw( vecToEnemy ) );

			AddFacingTarget( GetEnemy(), GetEnemyLKP(), 1.0, 0.8 );
		}*/
		//else
		{
			// Note that this only contains information MoveFacing() uses, not all of the information normally provided
			AILocalMoveGoal_t move;
			move.navType = GetOuter()->GetNavType();
			move.target = GetNavigator()->GetPath()->CurWaypointPos();
			move.dir = GetSmoothedVelocity();
			move.facing = move.dir;

			MoveFacing( move );
		}

		SetMoveInterval( 0 );

		return AIMR_OK;
	}

	return BaseClass::MoveJumpExecute();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
AIMoveResult_t CAI_AcrobaticMotor::MoveJumpStop()
{
	if ( IsUsingGlideLayer() )
	{
		StopGlideLayer();

		if ( GetOuter()->GetActivity() != ACT_GLIDE )
		{
			// Act as if our activity was indeed ACT_GLIDE and stick the landing
			float flTime = GetOuter()->GetGroundChangeTime();
			GetOuter()->AddStepDiscontinuity( flTime, GetAbsOrigin(), GetAbsAngles() );

			if ( SelectWeightedSequence( ACT_LAND ) == ACT_INVALID )
			{
				SetSmoothedVelocity( Vector(0,0,0) );
				return AIMR_CHANGE_TYPE;
			}

			SetActivity( ACT_LAND );
		}
	}

	return BaseClass::MoveJumpStop();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_AcrobaticMotor::PopulatePoseParameters()
{
	m_poseMove_X = GetOuter()->LookupPoseParameter( "move_x" );
	m_poseMove_Y = GetOuter()->LookupPoseParameter( "move_y" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAI_AcrobaticMotor::MoveFacing( const AILocalMoveGoal_t &move )
{
	// Check arrival activity before checking sliding directly
	if ( GetOuter()->GetNavigator()->GetArrivalActivity() == ACT_RANGE_AIM_LOW && GetSink()->IsSliding() )
	{
		//float flDistRemaining = (move.target - GetLocalOrigin()).LengthSqr() / Square( ai_acrobatic_slide_decel_dist.GetFloat() );
		if ( (move.target - GetLocalOrigin()).LengthSqr() > 0.0f ) // flDistRemaining > 0.0f
		{
			SetMoveInterval( 0 );

			// Update yaw using code from CAI_Motor::MoveFacing()
			Vector dir;
			if (IsDeceleratingToGoal() && (GetOuter()->GetHintNode() /*|| GetOuter()->m_hOpeningDoor*/))
			{
				dir = move.facing;
				VectorNormalize( dir );
			}
			else
			{
				float flInfluence = GetFacingDirection( dir );
				dir = move.facing * (1 - flInfluence) + dir * flInfluence;
				VectorNormalize( dir );
			}

			float idealYaw = UTIL_AngleMod( UTIL_VecToYaw( dir ) );
			SetIdealYawAndUpdate( idealYaw );	

			// Determine move yaw, then convert it to the X/Y anim type
			float flMoveYaw = UTIL_VecToYaw( move.dir );
			float flDiff = DEG2RAD( UTIL_AngleDiff( flMoveYaw, GetLocalAngles().y ) );

			// Approach 0.5 instead of 0 so that we don't become too slow
			//float flTimeDelta = ( SmoothCurve( flDistRemaining ) * 0.5f ) + 0.5f;
			float flTimeDelta = 1.0f;

			//Msg( "diff: %.2f (%.2f, %.2f, %.2f), Move X: %.2f, Move Y: %.2f (t: %.2f)\n", flDiff, flMoveYaw, GetLocalAngles().y, RAD2DEG( flDiff ), cos( flDiff ), sin( flDiff ), flTimeDelta );

			SetPoseParameter( m_poseMove_X, cos( flDiff ) * flTimeDelta );
			SetPoseParameter( m_poseMove_Y, sin( flDiff ) * flTimeDelta );

			return;
		}
		else
		{
			SetPoseParameter( m_poseMove_X, 0.0f );
			SetPoseParameter( m_poseMove_Y, 0.0f );
		}
	}
	else if ( IsUsingGlideLayer() )
	{
		// We want to obey facing direction even if our current sequence doesn't use move_yaw
		// Most of this is copied from CAI_Motor::MoveFacing()
		if ( GetOuter()->OverrideMoveFacing( move, GetMoveInterval() ) )
			return;

		Vector dir;
		float flInfluence = GetFacingDirection( dir );
		dir = move.facing * (1 - flInfluence) + dir * flInfluence;
		VectorNormalize( dir );

		// ideal facing direction
		float idealYaw = UTIL_AngleMod( UTIL_VecToYaw( dir ) );
		
		// FIXME: facing has important max velocity issues
		SetIdealYawAndUpdate( idealYaw );	

		//if ( HasPoseParameter( GetOuter()->GetLayerSequence( m_nGlideLayer ), GetOuter()->LookupPoseMoveYaw() ) )
		{
			// find movement direction to compensate for not being turned far enough
			float flMoveYaw = UTIL_VecToYaw( move.dir );
			float flDiff = UTIL_AngleDiff( flMoveYaw, GetLocalAngles().y );
			SetPoseParameter( GetOuter()->LookupPoseMoveYaw(), flDiff );
		}

		return;
	}

	BaseClass::MoveFacing( move );
}

//-----------------------------------------------------------------------------
