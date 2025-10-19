//=============================================================================//
//
// Purpose:		AI component dedicated to stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_AREA_H
#define AI_STEALTH_AREA_H

#include "ai_stealth_manager.h"
#include "triggers.h"

#if defined( _WIN32 )
#pragma once
#endif

class CAI_Hint;
class CBasePropDoor;

//-----------------------------------------------------------------------------
// Purpose: An area which can be searched
//-----------------------------------------------------------------------------
class CTriggerStealthArea : public CTriggerMultiple
{
	DECLARE_CLASS( CTriggerStealthArea, CTriggerMultiple );
public:
	CTriggerStealthArea();
	~CTriggerStealthArea();

	void Spawn( void );
	void OnRestore( void );
	void UpdateOnRemove( void );

	void StartTouch( CBaseEntity *pOther );
	void EndTouch( CBaseEntity *pOther );

	bool		IsHiddenTo( CAI_BaseNPC *pNPC ) const;
	float		GetAlertLevelMultiplier() const;
	float		GetMaxInterestDistance() const;

	bool		IsEnclosed() const { return m_bEnclosed; }
	const char	*GetAreaContext() const { return STRING( m_iszAreaContext ); }

	void		PopulateSearchPoints( void );
	inline CAI_Hint		*GetSearchPoint( int i ) const { return m_SearchPoints[i]; }
	inline int			GetSearchPointCount() const { return m_SearchPoints.Count(); }
	inline bool			IsSearchable() const { return m_SearchPoints.Count() > 0; }
	bool		IsValidSearchPoint( CAI_Hint *pHint ) const;
	bool		IsValidInterestHint( CAI_Hint *pHint ) const;
	float		GetMaxSearchPointDist() const { return m_flMaxSearchPointDist; }

	float				GetSearchWeight() const { return m_flSearchWeight; }
	const interval_t	&GetSearchInterval() const { return m_SearchInterval; }
	float				GetRandomSearchInterval();
	float				GetTimeLastSearched() const { return m_flTimeLastSearched; }
	bool				StartBeingSearched( CAI_BaseNPC *pNPC );
	CAI_BaseNPC			*GetSearcher( int i ) { return m_hSearchers[i]; }
	int					GetSearcherCount() { return m_hSearchers.Count(); }
	int					GetMaxSearchers() { return m_nMaxSearchers; }
	void				CancelSearch( CAI_BaseNPC *pNPC );
	bool				FinishSearch( CAI_BaseNPC *pNPC );
	void				ResetTimeSearched() { m_flTimeLastSearched = -1.0f; }
	
	void					PopulateDoors( void );
	inline CBasePropDoor	*GetDoor( int i ) const { return m_hDoors[i]; }
	inline int				GetDoorCount() const { return m_hDoors.Count(); }
	bool					IsValidDoor( CBasePropDoor *pDoor, int *iIndex = NULL, bool *bShouldBeOpen = NULL ) const;
	bool					IsDoorMeantToBeOpen( int i ) const;
	inline int				GetDoorStateMask() const { return m_iDoorStates; }
	void					UpdateDoorState( CBasePropDoor *pDoor, ThreeState_t iForceState = TRS_NONE );

	void					PopulateInteriorPositions( void );

	const Vector	&GetInteriorPosition( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor = NULL );
	const Vector	*GetExitPosition( const Vector &vecOrigin, float flRadius );

	float			GetTimeSpentInArea( CBaseEntity *pEntity );

	bool		ShouldManage( CBaseEntity *pManager ) { return ( pManager->NameMatches( m_iszTargetManager ) ); }

	void		InputSetStealthManager( inputdata_t &inputdata );
	void		InputSetSearchWeight( inputdata_t &inputdata );

private:

	string_t	m_iszTargetManager;

	bool		m_bHidden;
	float		m_flMaxInterestDist;
	float		m_flAlertLevelMultiplier;

	bool		m_bEnclosed;
	string_t	m_iszAreaContext;

	string_t	m_iszSearchPoints;
	CUtlVector< CHandle<CAI_Hint> >	m_SearchPoints;
	float		m_flMaxSearchPointDist;
	float		m_flSearchWeight;
	interval_t	m_SearchInterval;

	CUtlVector< CHandle<CAI_BaseNPC> >	m_hSearchers;
	int						m_nMaxSearchers;
	float					m_flTimeLastSearched;

	string_t	m_iszAreaDoors;
	CUtlVector< CHandle<CBasePropDoor> >	m_hDoors;
	int			m_iDoorStates; // One bit for each door's expected state (1 for open, 0 for closed)

	Vector		m_vecInteriorPos; // Closest node to center. Consider tracking more node positions?

	COutputEvent	m_OnNPCStartSearch;
	COutputEvent	m_OnNPCFinishSearch;
	COutputEvent	m_OnNPCCancelSearch;

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------

#endif
