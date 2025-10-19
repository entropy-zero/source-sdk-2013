//=============================================================================//
//
// Purpose:		AI behavior for intelligently searching areas.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_BEHAVIOR_SEARCH_H
#define AI_STEALTH_BEHAVIOR_SEARCH_H

#include "ai_stealth_behavior.h"
#include "ai_stealth_manager.h"
#include "ai_goalentity.h"

#if defined( _WIN32 )
#pragma once
#endif

class CTriggerStealthArea;
class CInfoStealthRegroup;
struct AI_EnemyInfo_t;

extern int g_interactionStealthOrder;
extern int g_interactionStealthRegroup;

//-----------------------------------------------------------------------------

enum StealthSquadOrder_t
{
	STEALTH_SQUAD_ORDER_NONE,
	STEALTH_SQUAD_ORDER_DISMISS,				// Return to posts
	STEALTH_SQUAD_ORDER_SWEEP,					// Sweep all accessible areas
	STEALTH_SQUAD_ORDER_LOCATE_SQUADMATE,		// Locate a missing squadmate
	STEALTH_SQUAD_ORDER_LOCATE_ENTITY,			// Locate some other entity
	STEALTH_SQUAD_ORDER_SITREP,					// People are reporting what they saw; leader will speak at the end of it and say what to do
	STEALTH_SQUAD_ORDER_ALARM,					// Formally decide to raise the alarm
	STEALTH_SQUAD_ORDER_SCRIPTED,				// Order is driven by map logic
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_StealthSearchBehavior : public CAI_StealthBehavior<>
{
	DECLARE_CLASS( CAI_StealthSearchBehavior, CAI_StealthBehavior<> );
public:
	DECLARE_DATADESC();
	CAI_StealthSearchBehavior();

	enum
	{
		// Schedules
		SCHED_STEALTH_SEARCH_ENTER_AREA = BaseClass::NEXT_SCHEDULE,
		SCHED_STEALTH_SEARCH_AREA,
		SCHED_STEALTH_SEARCH_EXIT_AREA,
		SCHED_STEALTH_GO_TO_REGROUP,
		SCHED_STEALTH_WAIT_AT_REGROUP,
		SCHED_STEALTH_PATROL_REGROUP,
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_STEALTH_GET_PATH_TO_SEARCH_POINT = BaseClass::NEXT_TASK,
		TASK_STEALTH_GET_PATH_OUTSIDE_AREA,
		TASK_STEALTH_WAIT_AT_SEARCH_POINT,
		TASK_STEALTH_FINISH_SEARCH_POINT,
		TASK_STEALTH_GET_PATH_TO_REGROUP_POINT,
		TASK_STEALTH_WAIT_UNTIL_AT_REGROUP,
		TASK_STEALTH_ARRIVE_AT_REGROUP,
		TASK_STEALTH_GET_PATH_TO_NODE_NEAR_REGROUP,
		TASK_STEALTH_WAIT_AT_REGROUP,
		TASK_STEALTH_WAIT_FOR_SPEECH,
		NEXT_TASK,
		
		// Conditions
		COND_STEALTH_SEARCH_FORCE = BaseClass::NEXT_CONDITION,
		COND_STEALTH_SEARCH_LEAVE_AREA,
		COND_STEALTH_REGROUP_START,
		COND_STEALTH_REGROUP_FINISH,
		COND_STEALTH_REGROUP_CANCEL,
		COND_STEALTH_REGROUP_LEADER_ARRIVES,
		NEXT_CONDITION,
	};

	//-----------------------------------------------

	bool					IsAtSearchPoint();
	CTriggerStealthArea		*GetSearchArea();	// This refers to trigger_stealth_area
	bool					ShouldSearch();
	bool					ForceSearchHint( CAI_Hint *pHint );
	bool					ForceSearchArea( CTriggerStealthArea *pArea );

	void		FinishAreaSearch();
	void		CancelAreaSearch();
	virtual void	OnFindSearchArea( CTriggerStealthArea *pArea );
	virtual void	OnFindSearchPoint( CAI_Hint *pHint );
	virtual void	OnArrivedAtSearchPoint( CAI_Hint *pHint );
	virtual void	OnLeaveSearchPoint( CAI_Hint *pHint );
	float		GetAreaSearchDist();

	bool		FValidateHintType( CAI_Hint *pHint );
	CAI_Hint	*FindSearchPointHintInArea( CTriggerStealthArea *pArea );
	CAI_Hint	*FindSearchPointHint( float flMaxDist, CTriggerStealthArea **ppArea );

	AI_EnemyInfo_t	*GetNewestEnemyMemory( float &flLastTimeSeen );

	//-----------------------------------------------

	bool		IsSweeping();
	bool		IsLoneSweeping();
	bool		ShouldLoneSweep();
	void		FinishLoneSweep();
	void		CancelLoneSweep();

	bool		ShouldSquadSweep();
	void		CallToRegroup( CInfoStealthRegroup *pRegroupPoint );

	bool		ShouldSitrep();

	bool		IsWaitingAtRegroup();
	void		EndRegroup();
	bool		HasRegroupPoint();
	
	CInfoStealthRegroup		*FindRegroupPoint( float flMaxDistSqr );
	CAI_Hint				*FindRegroupPointHint( CInfoStealthRegroup *pRegroupPoint );

	//-----------------------------------------------

	StealthSquadOrder_t		GetSquadOrder() const { return m_iSquadOrder; }
	void					SetSquadOrder( StealthSquadOrder_t iSquadOrder );
	bool					HasActiveOrder() const;
	StealthSquadOrder_t		GetPreviousSquadOrder() const { return m_iPreviousSquadOrder; }

	StealthSquadOrder_t		SelectBestOrder( CUtlVector< CHandle<CAI_BaseNPC> > *vecSquadMembers, variant_t *pVarOrderData = NULL );
	void					GiveSquadOrder( CAI_BaseNPC *pLeader, CUtlVector< CHandle<CAI_BaseNPC> > *vecSquadMembers, StealthSquadOrder_t iSquadOrder, variant_t *pVarOrderData = NULL );

	bool					IsOrderCarriedOut() const { return m_bOrderCarriedOut; }
	void					MarkOrderCarriedOut() { m_bOrderCarriedOut = true; }
	void					FinishActiveOrder();
	void					CancelActiveOrder();

	bool		IsOrderSquadSweeping() const;
	bool		IsOrderFindSubject() const;
	bool		IsOrderSitrep() const;

	// Used for squad orders, must be passed in by outer
	bool		HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sourceEnt );

	const char *GetStringForOrder( StealthSquadOrder_t iSquadOrder );
	bool		ShoutSquadOrder( StealthSquadOrder_t iSquadOrder, AI_CriteriaSet &modifiers, variant_t &varOrderData );
	void		ReceiveSquadOrder( CAI_BaseNPC *pLeader, StealthSquadOrder_t iSquadOrder, variant_t &varOrderData );
	void		OnLeaveRegroup();

	//-----------------------------------------------

	void		ResetSearchTarget();
	void		SetSearchTarget( string_t iszClass, gender_t nGender );

	//-----------------------------------------------

	void		AddInterestPoint( StealthInterestType_t nType, const Vector &vecOrigin );
	void		ReplaceInterestPoints( const CUtlVector<StealthInterestPoint_t> &vecInterestPoints );
	void		MaintainInterestPoints();

	//-----------------------------------------------

	virtual const char *GetName() { return "Stealth Search"; }

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &criteriaSet );

	int		DrawDebugTextOverlays( int text_offset );

	int		SelectSchedule();
	void	GatherConditions();
	int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int		TranslateSchedule( int scheduleType );
	void	BuildScheduleTestBits( void );
	bool	CanSelectSchedule( void );
	void	EndScheduleSelection( void );
	void	OnScheduleChange( void );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	static bool SearchPointHintFilter_OnlyArea( void *pContext, CAI_Hint *pHint );
	static bool SearchPointHintFilter_OnlyAreaDirect( void *pContext, CAI_Hint *pHint );	// Uses direct pos instead (for non-search points)
	static bool SearchPointHintFilter_NoArea( void *pContext, CAI_Hint *pHint );
	static CTriggerStealthArea *m_pHintSearchArea;	// Temporary pointer used while searching for hints

private:

	bool	m_bForcedSearch;

	// Doing a methodical sweep of every area we can access
	bool	m_bLoneSweep;
	float	m_flNextLoneSweepTime;
	float	m_flNextSquadSweepTime;
	float	m_flNextSitrepTime;

	bool							m_bWaitingAtRegroup;
	CHandle<CInfoStealthRegroup>	m_hRegroupPoint;

	// The person or thing we're searching for (could be an enemy or a missing squadmate; entity not available)
	string_t	m_iszTargetClass;
	gender_t	m_iTargetGender;	// "He's not here", "She's not here", etc.

	StealthSquadOrder_t		m_iSquadOrder;
	StealthSquadOrder_t		m_iPreviousSquadOrder;
	bool					m_bOrderCarriedOut;
	bool					m_bOrderQueued;

	CHandle< CTriggerStealthArea >	m_hCurrentSearchArea;

	CUtlVector<StealthInterestPoint_t>		m_InterestPoints;

public:
	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
DECLARE_AUTO_LIST( IStealthRegroupAutoList );
class CInfoStealthRegroup : public CLogicalEntity, public IStealthRegroupAutoList
{
	DECLARE_CLASS( CInfoStealthRegroup, CLogicalEntity );
public:
	DECLARE_DATADESC();

	CInfoStealthRegroup();

	void		OrderSpeechQueueThink();
	void		SetupSquad( CAI_BaseNPC *pLeader, const CUtlVector< CAI_StealthSearchBehavior* > &vecSquad, StealthSquadOrder_t iOrder );
	void		ResetSquad();
	bool		IsSquadSpeaking();

	//-----------------------------------------------

	bool		IsEnabled() const { return !m_bStartDisabled; }

	float		GetRadius() const { return m_flRadius; }

	bool		HasSquadName() const { return (m_iszSquadName != NULL_STRING); }
	const char	*GetSquadName() const { return STRING( m_iszSquadName ); }

	bool		HasLeaderHintName() const { return (m_iszLeaderHint != NULL_STRING); }
	const char	*GetLeaderHintName() const { return STRING( m_iszLeaderHint ); }

private:

	// The squad at this point. Leader is always first
	CUtlVector< CHandle<CAI_BaseNPC> >		m_hSquadMembers;
	int						m_nCurrentSquadMember;
	bool					m_bLeaderSpeaking;
	StealthSquadOrder_t		m_iSquadOrder;

	//-----------------------------------------------

	bool		m_bStartDisabled;

	float		m_flRadius;
	string_t	m_iszSquadName;

	string_t	m_iszLeaderHint;
};

#endif
