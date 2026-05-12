//=============================================================================//
//
// Purpose:		AI behavior for advanced curiosity features.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_SENSES_CURIOUS_H
#define AI_STEALTH_SENSES_CURIOUS_H

#include "ai_stealth_senses.h"

#if defined( _WIN32 )
#pragma once
#endif

class CAI_Squad;
class CTakeDamageInfo;
class CSound;
class CTriggerStealthArea;

//-----------------------------------------------------------------------------

// Used to track whether we'd recognize changes to props, doors, etc.
struct StealthAreaMemory_t
{
	EHANDLE	hArea;
	float	flLastTimeEntered;
	int		iDoorState;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Variant of stealth senses with increased investigation capabilities.
//-----------------------------------------------------------------------------
class CAI_CuriousStealthSenses : public CAI_StealthSenses
{
public:
	DECLARE_CLASS( CAI_CuriousStealthSenses, CAI_StealthSenses );

	CAI_CuriousStealthSenses( CAI_BaseNPC *pOuter );

	void			InitStealthSenses();
	void			InitSquad( CAI_Squad *pSquad );

	void			OnDamagedByAttacker( const CTakeDamageInfo &info );
	int				GetLastDamageType() { return m_nLastDamageType; }

	//-----------------------------------------------

	virtual void	OnListened();
	virtual bool	ShouldSeeInArea( CBaseEntity *pEntity, CTriggerStealthArea *pArea );

	CSound			*GetInvestigatingSound();
	virtual void	StartInvestigatingSound( CSound *pSound );
	virtual bool	IsInvestigatingSound();

	virtual bool	IsCuriousObject( CBaseEntity *pEntity );
	virtual bool	IsCuriousObjectMoving( CBaseEntity *pEntity );

	virtual void	ResetLastSound();
	virtual bool	IsLastSoundRelevant();
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &set );

	virtual bool	EvalAlertLevel( CBaseEntity *pTarget, const Vector &vecDelta, float flDot );
	virtual void	OnSpotEnemyFromAlert( CBaseEntity *pTarget, int i );

	virtual bool	ShouldEmitSawSuspicious( CBaseEntity *pTarget, int i );

	int		GetNumBodiesFound() const { return m_nBodiesFound; }
	void	IncrementBodiesFound() { m_nBodiesFound++; }
	const Vector	&GetLastSoundLocation() const { return m_vecLastSoundLocation; }
	const float		GetLastSoundTime() const { return m_flLastSoundTime; }
	const int		GetLastSoundChannel() const { return m_nLastSoundChannel; }
	CBaseEntity		*GetLastSoundOwner() { return m_hLastSoundOwner; }
	void	UpdateLastSound( CSound *pSound );

	//-----------------------------------------------

	StealthAreaMemory_t *GetAreaMemory( CTriggerStealthArea *pArea );
	void UpdateAreaMemory( CTriggerStealthArea *pArea, bool bLeaving );

	StealthSquadInfo_t *GetStealthSquadInfo();
	StealthSquadMemberInfo_t *GetStealthSquadMemberInfo();

private:

	int				m_nBodiesFound;

	// Used to distinguish between investigating stealth and investigating actual combat
	bool			m_bInvestigatingStealth;
	int				m_nInvestigatingSoundIdx;

	Vector			m_vecLastSoundLocation;
	int				m_nLastSoundType;
	int				m_nLastSoundChannel;
	EHANDLE			m_hLastSoundOwner;
	string_t		m_iszLastSoundModel;
	float			m_flLastSoundTime;

	int				m_nLastDamageType;

	CUtlVector<StealthAreaMemory_t>		m_AreaMemories;

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------

#endif
