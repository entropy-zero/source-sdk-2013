//=============================================================================//
//
// Purpose:		AI component dedicated to stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_SENSES_H
#define AI_STEALTH_SENSES_H

#include "ai_component.h"
#include "ez2/ai_stealth_shared.h"
#include "triggers.h"

#if defined( _WIN32 )
#pragma once
#endif

class CTriggerStealthArea;
class CAI_Hint;
class CAI_Squad;

struct StealthSquadInfo_t;
struct StealthSquadMemberInfo_t;
struct StealthAreaMemory_t;

//-----------------------------------------------------------------------------

enum StealthFlags_t
{
	STEALTH_F_CIVILIAN = ( 1 << 0 ),		// Busy/not on guard, won't be as curious
};

//-----------------------------------------------------------------------------

// Custom soundent channels. Since SOUND_COMBAT is used for stealth sounds, we use this
// to categorize them.
// This starts at 1000 to avoid colliding with existing soundent channels or related logic.
enum
{
	SOUNDENT_CHANNEL_STEALTH_FIRST = 1000,

	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_BODY = SOUNDENT_CHANNEL_STEALTH_FIRST,
	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_OPEN_DOOR,
	SOUNDENT_CHANNEL_STEALTH_DISCOVERED_ENEMY,		// Transcends squad information

	SOUNDENT_CHANNEL_STEALTH_PROP_BREAK,
	SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK,
	SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT,
	SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING,	// Was moved, etc.
	SOUNDENT_CHANNEL_STEALTH_PROP_MOVING,

	SOUNDENT_CHANNEL_STEALTH_HIT_BY_OBJECT,
	SOUNDENT_CHANNEL_STEALTH_SPEECH_INTERRUPTED,	// Someone died mid-sentence
	SOUNDENT_CHANNEL_STEALTH_ANNOUNCE_ATTACKED,		// I was/am being attacked

	SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS,

	// Keep this at the bottom
	SOUNDENT_CHANNEL_STEALTH_LAST,
};

//-----------------------------------------------------------------------------

#define STEALTH_SENSE_DEBUG_LINE_SIGHT			0
#define STEALTH_SENSE_DEBUG_LINE_SOUND			2
#define STEALTH_SENSE_DEBUG_LINE_ALERTNESS		4
#define STEALTH_SENSE_DEBUG_LINE_INVESTIGATE	6

// TODO: Make into dedicated console group? See mapbase_con_groups.cpp
extern const Color DbgStealthColor;

//-----------------------------------------------------------------------------

#define AI_SQUAD_ALERT_DELAY 1.5

struct AlertLevel_t
{
	EHANDLE		hTarget;		// Note that this can be a prop as well as a player/NPC.

	float		flLevel;
	float		flPrevLevel;
	float		flTotalLevel;	// How much level has been gained in total, including decayed level

	float		flStartTime;
	float		flLastUpdate;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Base stealth senses component.
//-----------------------------------------------------------------------------
class CAI_StealthSenses : public CAI_Component
{
public:
	DECLARE_DATADESC();

	CAI_StealthSenses( CAI_BaseNPC *pOuter );

	virtual void	InitStealthSenses();
	void	StopStealthSenses();
	void	RunStealthSenses();

	virtual void	InitSquad( CAI_Squad *pSquad );

	//-----------------------------------------------

	virtual bool	QueryHearSound( CSound *pSound );
	virtual void	OnListened() {}
	virtual bool	QuerySeeEntity( CBaseEntity *pEntity );
	bool	UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer );
	void	OnStateChange( int eNPCState );

	bool	IsRunningActBusy();
	bool	IsRunningStartActBusy();
	void	StopRunningActBusy();

	static bool			IsStealthSound( CSound *pSound );
	static bool			IsStealthSound( int nSoundChannel );
	static bool			IsCalmStealthSound( CSound *pSound );				// Don't go alert when hearing this
	static bool			IsCalmStealthSound( int nSoundChannel );			// Don't go alert when hearing this
	static bool			IsExclusiveStealthSound( int nSoundChannel );		// Only the sound's target hears this
	static bool			IsExclusiveStealthSound( CSound *pSound );			// Only the sound's target hears this
	static bool			IsPotentialEnemyStealthSound( int nSoundChannel );	// This sound could've been made by an enemy
	static bool			ShouldStayAtSound( CSound *pSound );				// Stay at the sound's location when investigating
	static float		GetSoundStopDistance( CSound *pSound );
	static const char	*GetStealthSoundChannelName( int nSoundChannel );

	virtual bool	IsCuriousObject( CBaseEntity *pEntity ) { return false; }

	bool	HasStealthFlags( int iFlags );

	virtual void	ResetLastSound() {}
	virtual bool	IsLastSoundRelevant() { return false; }
	virtual void	ModifyOrAppendCriteria( AI_CriteriaSet &set );

	//-----------------------------------------------

	// Implemented by CAI_CuriousStealthSenses
	virtual int		GetNumBodiesFound() const { return 0; }
	virtual void	IncrementBodiesFound() {}
	virtual const Vector	&GetLastSoundLocation() const { return vec3_origin; }
	virtual const float		GetLastSoundLocationUpdateTime() const { return 0.0f; }
	virtual const int		GetLastSoundChannel() const { return 0; }

	virtual StealthAreaMemory_t *GetAreaMemory( CTriggerStealthArea *pArea ) { return NULL; }
	virtual void UpdateAreaMemory( CTriggerStealthArea *pArea, bool bLeaving ) {}

	virtual StealthSquadInfo_t *GetStealthSquadInfo() { return NULL; }
	virtual StealthSquadMemberInfo_t *GetStealthSquadMemberInfo() { return NULL; }

	void	OnDamagedByAttacker( const CTakeDamageInfo &info ) {}
	int		GetLastDamageType() { return DMG_GENERIC; }

	//-----------------------------------------------

	virtual void GetStealthLookVectors( Vector &vecPos, Vector &vecDir );
	float	GetDotToSee( int eNPCState );

	bool	ShouldSeeInArea( CBaseEntity *pEntity, CTriggerStealthArea *pArea );
	bool	ShouldSeeInDark( CBaseEntity *pEntity );
	bool	ShouldSeeInWater( CBaseEntity *pEntity, int nWaterLevel );

	virtual bool	ShouldInvestigateSounds();
	virtual bool	IsInvestigatingSound();

	virtual CAI_Hint	*FindSearchPointHint( float flMinDist, float flMaxDist ) { return NULL; }

	//-----------------------------------------------

	void	BuildScheduleTestBits();

	//-----------------------------------------------

	void			MaintainAlertLevels();
	virtual bool	EvalAlertLevel( CBaseEntity *pTarget, const Vector &vecDelta, float flDot );
	virtual void	OnSpotEnemyFromAlert( CBaseEntity *pTarget, int i ) {}

	const AlertLevel_t	&GetAlertLevel( int i ) const;
	const int			GetAlertLevelCount() const;
	const float			GetAlertLevelForTarget( CBaseEntity *pTarget ) const;
	float				GetHighestAlertLevel() const;

	virtual int		GetAlertSourceType() const { return ALERT_SOURCE_TYPE_NONE; }

	virtual float	GetAlertFaceThreshold( CBaseEntity *pTarget, int i ) const;
	virtual bool	ShouldEmitSawSuspicious( CBaseEntity *pTarget, int i );

	// Debugging
	void				EntityPrint( int iLine, Color clr, float flDuration, const char *pMsg, ... );
	int					GetOffsetForDebugType( int iStartLine );
	void				DebugPrintAlertLevels();

private:
	
	CUtlVector< AlertLevel_t >	m_AlertLevels;
	float			m_flNextAlertLevelThink;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline bool CAI_StealthSenses::IsStealthSound( int nSoundChannel )
{
	return (nSoundChannel >= SOUNDENT_CHANNEL_STEALTH_FIRST && nSoundChannel < SOUNDENT_CHANNEL_STEALTH_LAST);
}

inline bool CAI_StealthSenses::IsCalmStealthSound( int nSoundChannel )
{
	switch (nSoundChannel)
	{
		case SOUNDENT_CHANNEL_STEALTH_DISCOVERED_OPEN_DOOR:
		case SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK:
		case SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT:
		case SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING:
		case SOUNDENT_CHANNEL_STEALTH_HIT_BY_OBJECT:
		case SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS:
			return true;
	}

	return false;
}

inline bool CAI_StealthSenses::IsExclusiveStealthSound( int nSoundChannel )
{
	switch (nSoundChannel)
	{
		case SOUNDENT_CHANNEL_STEALTH_DISCOVERED_OPEN_DOOR:
		case SOUNDENT_CHANNEL_STEALTH_HIT_BY_OBJECT:
		case SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS:
		case SOUNDENT_CHANNEL_STEALTH_PROP_INTERESTING:
			return true;
	}

	return false;
}

inline bool CAI_StealthSenses::IsPotentialEnemyStealthSound( int nSoundChannel )
{
	switch (nSoundChannel)
	{
		case SOUNDENT_CHANNEL_STEALTH_PROP_SMALL_BREAK:
		case SOUNDENT_CHANNEL_STEALTH_PROP_IMPACT:
		case SOUNDENT_CHANNEL_STEALTH_PROP_MOVING:
		case SOUNDENT_CHANNEL_STEALTH_HIT_BY_OBJECT:
		case SOUNDENT_CHANNEL_STEALTH_SAW_SUSPICIOUS:
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------

#endif
