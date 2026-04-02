//=============================================================================//
//
// Purpose:		AI behavior
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_BEHAVIOR_TRIPMINE_PLACE_H
#define AI_BEHAVIOR_TRIPMINE_PLACE_H

#include "ai_behavior.h"
#include "npc_playercompanion.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------------------------------

// Saved while identifying spots and then used during schedule
struct	TripmineCandidate_t
{
	Vector				vecOrigin;
	Vector				vecDir;
	float				flWeight;

	static int __cdecl Sort( const TripmineCandidate_t *a, const TripmineCandidate_t *b )
	{
		return (a->flWeight < b->flWeight);
	}

	DECLARE_SIMPLE_DATADESC();
};

struct	TripmineContextData_t
{
	Vector					vecOrigin;
	CUtlVector<EHANDLE>		hTripmines;

	DECLARE_SIMPLE_DATADESC();
};

enum TripmineContext_t
{
	TRIPMINE_CONTEXT_NONE,
	TRIPMINE_CONTEXT_LAST_KNOWN,	// Placing around last known location
	TRIPMINE_CONTEXT_COMBAT,		// Placing in combat
	TRIPMINE_CONTEXT_FORTIFY,		// Fortifying a location

	TRIPMINE_CONTEXT_COUNT,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CAI_TripminePlaceBehavior : public CAI_Behavior<CNPC_PlayerCompanion>
{
	DECLARE_CLASS( CAI_TripminePlaceBehavior, CAI_Behavior<CNPC_PlayerCompanion> );
public:
	DECLARE_DATADESC();
	CAI_TripminePlaceBehavior();

	enum
	{
		// Schedules
		SCHED_TRIPMINE_PLACE = BaseClass::NEXT_SCHEDULE,
		SCHED_TRIPMINE_RUN_TO_PLACE,
		SCHED_TRIPMINE_WALK_TO_PLACE,
		NEXT_SCHEDULE,
		
		// Tasks
		TASK_TRIPMINE_PLACE = BaseClass::NEXT_TASK,
		TASK_TRIPMINE_GET_PATH_TO_PLACE,
		NEXT_TASK,
		
		// Conditions
		//COND_STEALTH_ALARM_INVALID = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,
	};

	//-----------------------------------------------

	bool	IsTripmineCapable() { return m_bTripmineCapable; }
	bool	IsPlacingTripmine();
	bool	ShouldPlaceTripmine();
	void	ForcePlaceTripmineOnTarget( CBaseEntity *pTarget );

	bool	ProbeSurface( const Vector &vecOrigin, const Vector &vecNormal, float &flWeight, bool bCheckCandidates = true );
	bool	ProbeDirection( const Vector &vecOrigin, const Vector &vecDir, float flMaxDist, Vector &vecOutOrigin, Vector &vecOutNormal, float &flWeight );
	bool	ProbeAllAngles( const Vector &vecOrigin, const Vector &vecForward, const Vector &vecRight, float flMaxDist, Vector &vecOutOrigin, Vector &vecOutNormal, float &flWeight );

	bool		FValidateHintType( CAI_Hint *pHint );
	int			FindTripmineHints( const Vector &vecOrigin, float flRadius, TripmineContext_t nContext, CUtlVector<TripmineCandidate_t> &tripmineCandidates, bool bCheckVis = false );
	bool		TryFindTripmineLocations( const Vector &vecOrigin, float flRadius, TripmineContext_t nContext, bool bCheckVis = false );

	//-----------------------------------------------

	virtual const char *GetName() { return "Tripmine Place"; }

	void	Precache();
	bool	KeyValue( const char *szKeyName, const char *szValue );

	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &criteriaSet );

	int		SelectSchedule();
	int		TranslateSchedule( int scheduleType );
	int		SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	void	GatherConditions( void );
	void	BuildScheduleTestBits( void );
	bool	CanSelectSchedule( void );
	void	EndScheduleSelection( void );
	void	OnScheduleChange( void );

	void	HandleAnimEvent( animevent_t *pEvent );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

private:

	bool	m_bTripmineCapable;
	bool	m_bForcePlaceTripmine;

	TripmineContextData_t	m_TripmineContexts[TRIPMINE_CONTEXT_COUNT];
	TripmineContext_t		m_nTripmineContext;

	Vector	m_vecCurrentTripmineLocation;	// Used during placing schedule

	CUtlVector<TripmineCandidate_t>	m_TripmineCandidates;

	//------------------------------------

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;
};

//-----------------------------------------------------------------------------

#endif
