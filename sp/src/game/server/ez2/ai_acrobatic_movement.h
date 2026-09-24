//=============================================================================//
//
// Purpose:		Special navigator/motor override for generic advanced movement techniques
//				(mainly sliding and shooting while jumping)
//
// Author:		Blixibon
//
//=============================================================================//
#ifndef AI_ACROBATIC_MOVEMENT_H
#define AI_ACROBATIC_MOVEMENT_H

#include "ai_blended_movement.h"

struct AI_Waypoint_t;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class CAI_AcrobaticSink
{
public:
	virtual ~CAI_AcrobaticSink() {}

	virtual bool	IsSliding() const = 0;
	virtual bool	ShouldSlideToGoal( AILocalMoveGoal_t *pMoveGoal ) = 0;

	virtual bool	ShouldUseJumpGesture() = 0;
	virtual bool	ShouldJumpGestureDelayShoot() = 0;
	virtual void	OnStartGestureJump() = 0;
};

class CAI_AcrobaticNavigator : public CAI_Navigator
{
	typedef CAI_Navigator BaseClass;
public:
	CAI_AcrobaticNavigator( CAI_BaseNPC *pOuter )
	 :	BaseClass( pOuter )
	{
		m_pSink = dynamic_cast<CAI_AcrobaticSink*>(pOuter);
		m_bSliding = false;
	}

	inline bool	IsSliding() const { return m_bSliding; }

	void	StartSlidingToGoal( AILocalMoveGoal_t *pMoveGoal );
	void	StopSliding( bool bIntoCrouch = false );

	void 	MoveCalcBaseGoal( AILocalMoveGoal_t *pMoveGoal );

	void	OnNewGoal();
	void	OnNavComplete();

	CAI_AcrobaticSink	*GetSink() { return m_pSink; }

private:

	bool	m_bSliding;

	CAI_AcrobaticSink *m_pSink;

	DECLARE_SIMPLE_DATADESC();
};

class CAI_AcrobaticMotor : public CAI_BlendedMotor
{
	typedef CAI_BlendedMotor BaseClass;
public:
	CAI_AcrobaticMotor( CAI_BaseNPC *pOuter )
	 :	BaseClass( pOuter )
	{
		m_pSink = dynamic_cast<CAI_AcrobaticSink *>(pOuter);
		m_nGlideLayer = -1;
		m_nJumpLayer = -1;

		m_poseMove_X = 0;
		m_poseMove_Y = 0;
	}

	//-----------------------------------------------
	// Glide Gesture
	//-----------------------------------------------

	inline int	GetGlideLayer() const { return m_nGlideLayer; }
	inline bool	IsUsingGlideLayer() { return GetOuter()->IsValidLayer( m_nGlideLayer ); }

	// For the initial jump. Use GetGlideLayer/IsUsingGlideLayer for if we're using the gesture jump in general
	inline int	GetInitialJumpLayer() const { return m_nJumpLayer; }
	inline bool	IsInitialJumpLayerActive() { return GetOuter()->IsValidLayer( m_nJumpLayer ); }

	bool		StartGlideLayer();
	void		StopGlideLayer();

	void 		MoveJumpStart( const Vector &velocity );
	int			MoveJumpExecute();
	AIMoveResult_t MoveJumpStop();

	//-----------------------------------------------
	// Sliding
	//-----------------------------------------------

	void		PopulatePoseParameters( void );

	void		MoveFacing( const AILocalMoveGoal_t &move );

	CAI_AcrobaticSink	*GetSink() { return m_pSink; }

private:

	int		m_nGlideLayer;
	int		m_nJumpLayer;	// Initial jump

	// Used for sliding
	int		m_poseMove_X;
	int		m_poseMove_Y;

	CAI_AcrobaticSink *m_pSink;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

template <class BASE_NPC>
class CAI_AcrobaticHost : public BASE_NPC, public CAI_AcrobaticSink
{
	DECLARE_CLASS_NOFRIEND( CAI_AcrobaticHost, BASE_NPC );
public:
	const CAI_AcrobaticMotor *GetAcrobaticMotor() const { return assert_cast<const CAI_AcrobaticMotor *>(this->GetMotor()); }
	CAI_AcrobaticMotor *		GetAcrobaticMotor()		{ return assert_cast<CAI_AcrobaticMotor *>(this->GetMotor()); }

	CAI_BlendedMotor *CreateMotor()
	{
		MEM_ALLOC_CREDIT();
		return new CAI_AcrobaticMotor( this );
	}

	const CAI_AcrobaticNavigator *GetAcrobaticNavigator() const { return assert_cast<const CAI_AcrobaticNavigator *>(this->GetNavigator()); }
	CAI_AcrobaticNavigator *		GetAcrobaticNavigator()		{ return assert_cast<CAI_AcrobaticNavigator *>(this->GetNavigator()); }

	CAI_Navigator *CreateNavigator()
	{
		CAI_Navigator *pNavigator = new CAI_AcrobaticNavigator( this );
		pNavigator->SetValidateActivitySpeed( false );
		return pNavigator;
	}

public:

	virtual bool	ShouldUseJumpGesture() { return false; }
	virtual bool	ShouldJumpGestureDelayShoot() { return false; }
	virtual bool	ShouldStopJumpGesture() { return (this->GetFlags() & FL_ONGROUND); } // (this->GetNavType() != NAV_JUMP)
	virtual void	OnStartGestureJump() {}

	bool			IsSliding() const { return GetAcrobaticNavigator()->IsSliding(); }
	virtual bool	ShouldSlideToGoal( AILocalMoveGoal_t *pMoveGoal );
	virtual float	GetSlideMinSpeedSqr() const { return Square( 150.0f ); }

	//-------------------------------------------------

	void		Precache();

	void		OnScheduleChange( void );
	void		PrescheduleThink( void );
	bool		FCanCheckAttacks( void );

	Activity	NPC_TranslateActivity( Activity eNewActivity );
	float		GetMaxJumpSpeed() const;
	float		GetJumpGravity() const;
	virtual float		GetGestureMaxJumpSpeed() const;
	virtual float		GetGestureJumpGravity() const;

	void		PopulatePoseParameters( void );
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_AcrobaticHost<BASE_NPC>::ShouldSlideToGoal( AILocalMoveGoal_t *pMoveGoal )
{
	// Not in combat
	if ( this->GetState() != NPC_STATE_COMBAT || !this->GetEnemy() || this->IsInAScript() )
		return false;

	// Not going fast enough
	if ( this->GetNavType() != NAV_GROUND || this->GetMotor()->GetCurVel().LengthSqr() < GetSlideMinSpeedSqr() )
		return false;

	// Not enough or too much distance
	float flDistSqr = ( this->GetAbsOrigin() - pMoveGoal->target ).LengthSqr();
	if ( flDistSqr < Square( 120.0f ) || flDistSqr > Square( 350.0f ) )
		return false;

	// No sequence
	if ( !this->HaveSequenceForActivity( ACT_HL2MP_SLIDE ) )
		return false;

	// Would we see the enemy if crouched at our goal?
	trace_t tr;
	Vector vecEyeAtGoal = pMoveGoal->target + this->GetCrouchEyeOffset();
	Vector vecEnemyOffset = this->GetEnemy()->HeadTarget( vecEyeAtGoal ) - this->GetEnemy()->GetAbsOrigin();
	Vector vecEnemyPos = vecEnemyOffset + this->GetEnemyLKP();
	AI_TraceLOS( vecEyeAtGoal, vecEnemyPos, this, &tr );
	if ( tr.fraction != 1.0f && tr.m_pEnt != this->GetEnemy() )
		return false;

	/*if ( IsCurSchedule( SCHED_TAKE_COVER_FROM_BEST_SOUND )
		|| IsCurSchedule( SCHED_HIDE_AND_RELOAD )
		|| IsCurSchedule( SCHED_COMBINE_TAKE_COVER_FROM_BEST_SOUND, false ) )
		return true;*/

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_AcrobaticHost<BASE_NPC>::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( "AI_BaseNPC.Slide" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_AcrobaticHost<BASE_NPC>::OnScheduleChange( void )
{
	BaseClass::OnScheduleChange();
	
	if ( GetAcrobaticNavigator()->IsSliding() )
		GetAcrobaticNavigator()->StopSliding();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_AcrobaticHost<BASE_NPC>::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
	
	if ( GetAcrobaticMotor()->IsUsingGlideLayer() && ShouldStopJumpGesture() )
	{
		GetAcrobaticMotor()->StopGlideLayer();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
bool CAI_AcrobaticHost<BASE_NPC>::FCanCheckAttacks( void )
{
	if ( this->GetNavType() == NAV_JUMP && GetAcrobaticMotor()->IsUsingGlideLayer() )
	{
		// Get around the nav type check in CAI_BaseNPC::FCanCheckAttacks()
		this->SetNavType( NAV_GROUND );

		if ( BaseClass::FCanCheckAttacks() )
		{
			this->SetNavType( NAV_JUMP );
			return true;
		}

		this->SetNavType( NAV_JUMP );
		return false;
	}

	return BaseClass::FCanCheckAttacks();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template <class BASE_NPC>
Activity CAI_AcrobaticHost<BASE_NPC>::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( GetAcrobaticNavigator()->IsSliding() )
	{
		switch ( eNewActivity )
		{
			case ACT_IDLE:
			case ACT_IDLE_ANGRY:
			case ACT_WALK:
			case ACT_WALK_AIM:
			case ACT_RUN:
			case ACT_RUN_AIM:
				eNewActivity = ACT_HL2MP_SLIDE;
				break;
		}
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_AcrobaticHost<BASE_NPC>::GetMaxJumpSpeed() const
{
	// Slower when using gesture glide
	// (We need to evaluate this before we actually begin the gesture, so just check if we're going to use it)
	if ( const_cast<CAI_AcrobaticHost<BASE_NPC> *>(this)->ShouldUseJumpGesture() )
		return GetGestureMaxJumpSpeed();

	return BaseClass::GetMaxJumpSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_AcrobaticHost<BASE_NPC>::GetJumpGravity() const
{
	// Slower when using gesture glide
	// (We need to evaluate this before we actually begin the gesture, so just check if we're going to use it)
	if ( const_cast<CAI_AcrobaticHost<BASE_NPC> *>(this)->ShouldUseJumpGesture() )
		return GetGestureJumpGravity();

	return BaseClass::GetJumpGravity();
}

extern ConVar	ai_acrobatic_jump_gesture_grav;
extern ConVar	ai_acrobatic_jump_gesture_speed;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_AcrobaticHost<BASE_NPC>::GetGestureMaxJumpSpeed() const
{
	return ai_acrobatic_jump_gesture_speed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
float CAI_AcrobaticHost<BASE_NPC>::GetGestureJumpGravity() const
{
	return ai_acrobatic_jump_gesture_grav.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
template <class BASE_NPC>
void CAI_AcrobaticHost<BASE_NPC>::PopulatePoseParameters( void )
{
	BaseClass::PopulatePoseParameters();
	GetAcrobaticMotor()->PopulatePoseParameters();
}

//-----------------------------------------------------------------------------	

#endif
