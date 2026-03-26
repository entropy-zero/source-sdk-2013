//=============================================================================//
//
// Purpose:		Manager for stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef AI_STEALTH_MANAGER_H
#define AI_STEALTH_MANAGER_H

#include "baseentity.h"

#if defined( _WIN32 )
#pragma once
#endif

class CAI_Squad;
class CAI_Hint;
class CBasePropDoor;
class CSound;
class CTriggerStealthArea;

//-----------------------------------------------------------------------------

enum StealthLevel_t
{
	STEALTH_LEVEL_NONE,			// No stealth sensing NPCs in PVS
	STEALTH_LEVEL_QUIET,		// Stealth sensing NPCs are idle
	STEALTH_LEVEL_GUARD,		// Stealth sensing NPCs are idle, but expecting enemies (less naive)
	STEALTH_LEVEL_TENSE,		// Stealth sensing NPCs know danger is nearby/the area isn't safe
	STEALTH_LEVEL_LOUD,			// Stealth sensing NPCs are in combat

	NUM_STEALTH_LEVELS,
};

enum StealthObjectType_t
{
	STEALTH_OBJ_NONE,
	STEALTH_OBJ_RAGDOLL,		// The body of a stealth sensing NPC
	STEALTH_OBJ_DOOR,			// Door that's open/closed when it shouldn't be
	STEALTH_OBJ_PROP,			// Props that shouldn't be where they are
	STEALTH_OBJ_PROP_PICKUP,	// Props that should be collected
	STEALTH_OBJ_BLOODSTAIN,
	STEALTH_OBJ_SLAM,
	STEALTH_OBJ_ITEM,
	STEALTH_OBJ_WEAPON,
	STEALTH_OBJ_LASER_DOT,		// Laser dot from an enemy's weapon

	NUM_STEALTH_OBJS,
};

enum StealthInterestType_t
{
	STEALTH_INTEREST_NONE,
	STEALTH_INTEREST_ENEMY,			// The enemy was last known to be in this area
	STEALTH_INTEREST_SOUND,			// Sound was heard
	STEALTH_INTEREST_MISSING_ALLY,	// Missing squadmate was last seen here

	NUM_STEALTH_INTEREST_TYPES,
};

//-----------------------------------------------------------------------------
// Purpose: Stealth objects found by stealth sensing NPCs.
// 
//			This information is primarily used to check if an object has moved
//			from where it was initially spotted.
//-----------------------------------------------------------------------------
struct	StealthObjectState_t
{
	EHANDLE		hEntity;
	Vector		vecLastPosition;

	float		flLastTimeChecked;
	float		flTimeEnteredArea;
	float		flNoticeRadius;		// How far away the prop has to be from its last position for it to be noticed

	short		nTimesFound;
	bool		bResult;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: Areas where curious NPCs would be interested in searching.
//-----------------------------------------------------------------------------
struct	StealthInterestPoint_t
{
	Vector		vecOrigin;
	float		flExpireTime;
	StealthInterestType_t	nType;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: The squad's available information for each of its members. This is
//			similar to the enemy memory info NPCs use, except shared between
//			the whole squad and capable of persisting after death.
//-----------------------------------------------------------------------------
struct	StealthSquadMemberInfo_t
{
	EHANDLE		hEntity;		// If alive, this is the NPC, but can be the ragdoll if dead
	bool		bKnownAlive;	// Whether this squadmate was last seen alive. Use hEntity->IsAlive() to check if they actually are

	// These fields are updated when another squadmate is watching
	Vector					vecLastKnownLocation;		// Last location this squad member was seen at
	float					flLastKnownTime;			// Last time this squadmate was seen
	CHandle<CAI_BaseNPC>	hLastInformer;				// Last squadmate to see this member, if valid

	// ID information used when searching since the entity may not be available
	string_t	iszID;		// Usually a codename (or real name). Counts up from 0 by default, but can be explicitly set via response context.
	gender_t	nGender;	// Used to determine pronouns in callouts.

	// Used when checking a body
	int			nDamageType;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: The squad's available information for each of its members. This is
//			similar to the enemy memory info NPCs use, except shared between
//			the whole squad and capable of persisting after death.
//-----------------------------------------------------------------------------
struct	StealthSquadInfo_t
{
	CAI_Squad	*pSquad;
	string_t	iszSquadName;	// For save/restore

	bool		bAlerted;		// This squad is aware of the player and searching for them

	// Tracked information for each member. Note that this counts dead members.
	CUtlVector<StealthSquadMemberInfo_t>	m_Members;

	// Used during sweeps to indicate areas the squad has already covered.
	CUtlVector<CTriggerStealthArea>	m_SweepAreas;

	DECLARE_SIMPLE_DATADESC();
};

//-----------------------------------------------------------------------------

class CAI_StealthManager : public CLogicalEntity
{
public:
	DECLARE_CLASS( CAI_StealthManager, CLogicalEntity );

	CAI_StealthManager();
	~CAI_StealthManager();

	void	Spawn();
	void	OnRestore();
	void	UpdateOnRemove();

	void	StealthManagerThink();

	void	CheckStealthManagerState();
	void	Cleanup();
	void	Enable() { m_bDisabled = false; CheckStealthManagerState(); }
	void	Disable() { m_bDisabled = true; CheckStealthManagerState(); }
	void	InputEnable( inputdata_t &inputdata ) { Enable(); }
	void	InputDisable( inputdata_t &inputdata ) { Disable(); }

	//-----------------------------------------------

	StealthLevel_t	GetStealthLevel() const { return m_nStealthLevel; }
	StealthLevel_t	GetMinStealthLevel() const { return m_nMinStealthLevel; }
	void			SetMinStealthLevel( StealthLevel_t nStealthLevel );
	bool			IsStealthLevel( int nStealthLevel ) const { return m_nStealthLevel == nStealthLevel; }
	bool			IsStealthLevel( int nMinStealthLevel, int nMaxStealthLevel ) const { return m_nStealthLevel >= nMinStealthLevel && m_nStealthLevel <= nMaxStealthLevel; }

	void	CheckStillAlert( CAI_BaseNPC *pNPC );

	void	NPCFoundBody( CAI_BaseNPC *pNPC, CBaseEntity *pBody );
	void	NPCSawPlayer( CAI_BaseNPC *pNPC );
	void	NPCKilled( const CTakeDamageInfo &info, CAI_BaseNPC *pNPC, CBaseEntity *pRagdoll, StealthSquadInfo_t *pSquadInfo, StealthSquadMemberInfo_t *pSquadMemberInfo );
	void	NPCSilencedAfterSeeingPlayer( CAI_BaseNPC *pNPC );
	void	NPCRaisingAlarm( CAI_BaseNPC *pNPC );
	void	NPCStoppedRaisingAlarm( CAI_BaseNPC *pNPC, bool bFailed = false );
	void	NPCStartedSpeaking( CAI_BaseNPC *pNPC, const char *concept, AI_Response *response );
	void	NPCHeardSuspiciousSound( CAI_BaseNPC *pNPC, CSound *pSound );
	void	NPCOpenDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor );
	void	NPCFindOpenDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor );

	void	SquadSawPlayer( CAI_BaseNPC *pNPC, CAI_Squad *pSquad );
	void	SquadLostPlayer( CAI_BaseNPC *pNPC, CAI_Squad *pSquad );
	void	SquadQuieted( CAI_BaseNPC *pNPC, CAI_Squad *pSquad );

	void	PlayerMovedObject( CBasePlayer *pPlayer, CBaseEntity *pEntity );

	// Internal inputs
	void	InputNPCStartedSpeaking( inputdata_t &inputdata );
	void	InputNPCHeardSuspiciousSound( inputdata_t &inputdata );

	void	InputSetMinStealthLevelQuiet( inputdata_t &inputdata ) { SetMinStealthLevel( STEALTH_LEVEL_QUIET ); }
	void	InputSetMinStealthLevelGuard( inputdata_t &inputdata ) { SetMinStealthLevel( STEALTH_LEVEL_GUARD ); }
	void	InputSetMinStealthLevelTense( inputdata_t &inputdata ) { SetMinStealthLevel( STEALTH_LEVEL_TENSE ); }

	//-----------------------------------------------

	bool	GetAlarmsEnabled() const { return m_bAlarmsEnabled; }
	bool	IsAlarmRaised() const { return m_bAlarmRaised; }
	bool	IsAlarmDisabled( CBaseEntity *pAlarm );
	void	RaiseAlarm( CBaseEntity *pActivator, CBaseEntity *pCaller );
	void	ResetAlarm( CBaseEntity *pActivator );

	int		GetNumBodiesToRaiseAlarm() const { return m_nBodiesToRaiseAlarm; }

	void	InputEnableAlarms( inputdata_t &inputdata ) { m_bAlarmsEnabled = true; }
	void	InputDisableAlarms( inputdata_t &inputdata ) { m_bAlarmsEnabled = false; }
	void	InputRaiseAlarm( inputdata_t &inputdata ) { RaiseAlarm( inputdata.pActivator, inputdata.pCaller ); }
	void	InputResetAlarm( inputdata_t &inputdata ) { ResetAlarm( inputdata.pActivator ); }
	void	InputForceThisNPCToRaiseAlarm( inputdata_t &inputdata );

	//-----------------------------------------------

	void	AddSeenObject( CBaseEntity *pEntity );
	bool	ShouldSeeObject( CBaseEntity *pEntity );
	void	MakePropPerceivable( CBaseEntity *pEntity );
	void	RemovePropPerceivable( CBaseEntity *pEntity );
	bool	ShouldPropBePerceivable( CBaseEntity *pEntity ) const;
	StealthObjectState_t *GetStealthObjectState( CBaseEntity *pEntity );
	StealthObjectType_t	GetStealthObjectType( CBaseEntity *pEntity ) const;

	//-----------------------------------------------

	StealthSquadInfo_t		*FindSquadInfo( CAI_Squad *pSquad, bool bCreate = true );
	StealthSquadInfo_t		*GetSquadInfo( int i ) { return &m_SquadInfo[i]; }
	int						GetSquadInfoCount() { return m_SquadInfo.Count(); }
	StealthSquadMemberInfo_t	*FindSquadMemberInfo( StealthSquadInfo_t *pSquadInfo, CBaseEntity *pSquadmate, bool bCreate = true );

	void		RemoveSquadMemberInfo( CAI_Squad *pSquad, CAI_BaseNPC *pSquadmate );
	string_t	GenerateSquadMemberID( StealthSquadInfo_t *pSquadInfo, CBaseEntity *pSquadmate );

	void		UpdateSeenSquadMember( CAI_BaseNPC *pSquadmate, CAI_BaseNPC *pInformer = NULL );
	void		UpdateSeenSquadMember( StealthSquadMemberInfo_t *pSquadMemberInfo, CAI_BaseNPC *pInformer = NULL );
	void		UpdateDeadSquadMember( CBaseEntity *pBody );
	void		UpdateDeadSquadMember( StealthSquadMemberInfo_t *pSquadMemberInfo, CBaseEntity *pBody );

	float		GetDistanceFromSquad( const Vector &vecOrigin, StealthSquadInfo_t *pSquadInfo, CAI_BaseNPC *pExclude = NULL );
	int			GetKnownLivingSquadMembers( StealthSquadInfo_t *pSquadInfo );
	int			GetSquadMemberLeastSeen( StealthSquadInfo_t *pSquadInfo );
	int			SquadHasUnknownDeadMember( StealthSquadInfo_t *pSquadInfo );

	//-----------------------------------------------

	void	AddStealthArea( CTriggerStealthArea *pArea );
	void	RemoveStealthArea( CTriggerStealthArea *pArea );
	CHandle<CTriggerStealthArea> &GetStealthArea( int i );
	int						GetStealthAreaCount();
	CTriggerStealthArea		*FindBestStealthArea( CAI_BaseNPC *pNPC, float flMaxDist, const CUtlVector<StealthInterestPoint_t> *vecInterestPoints = NULL, bool bInterestOnly = false );
	CTriggerStealthArea		*GetStealthAreaForEntity( CBaseEntity *pEntity );
	CTriggerStealthArea		*GetStealthAreaForPoint( const Vector &vecOrigin );
	CTriggerStealthArea		*GetStealthAreaInBox( const Vector &vecOrigin, const Vector &vecMins, const Vector &vecMaxs );
	CTriggerStealthArea		*GetStealthAreaForHint( CAI_Hint *pHint );
	CTriggerStealthArea		*GetStealthAreaForDoor( CBasePropDoor *pDoor, int *iIndex = NULL, bool *bShouldBeOpen = NULL );
	const Vector			*GetInteriorPositionThroughDoor( CAI_BaseNPC *pNPC, CBasePropDoor *pDoor );
	static float			GetStealthAreaWeight( CAI_BaseNPC *pNPC, CTriggerStealthArea *pArea, float flMinDist, float flMaxDist, const CUtlVector<StealthInterestPoint_t> *vecInterestPoints = NULL, bool bInterestOnly = false );
	void	ResetAreaSearches( const Vector &vecOrigin, float flRadius );

	static const char	*GetInterestTypeName( StealthInterestType_t nInterestType );
	static float		GetInterestTypeRadius( StealthInterestType_t nInterestType );
	static float		GetInterestTypeDuration( StealthInterestType_t nInterestType );
	int		GetBestInterestPoint( const CUtlVector<StealthInterestPoint_t> &vecInterestPoints );	// Note that this does not factor distance

	void	OnObjectEnteredArea( CTriggerStealthArea *pArea, CBaseEntity *pEntity );

	//-----------------------------------------------

	void	SendMusicInput( CBaseEntity *pEntity, const char *pszInputName, const char *pszParam = "", float flDelay = 0.0f, CBaseEntity *pActivator = NULL );

	CBaseEntity *GetStealthMusic() const { return m_hStealthMusic; }
	CBaseEntity *GetAlertMusic() const { return m_hAlertMusic; }

	void	InputEnableMusic( inputdata_t &inputdata );
	void	InputDisableMusic( inputdata_t &inputdata );
	void	InputSetStealthMusic( inputdata_t &inputdata );
	void	InputSetAlertMusic( inputdata_t &inputdata );
	void	InputEnableResumeStealthMusic( inputdata_t &inputdata );
	void	InputDisableResumeStealthMusic( inputdata_t &inputdata );

	//-----------------------------------------------

	void	ModifyOrAppendCriteria( CBaseEntity *pEntity, AI_CriteriaSet &set );

	float	GetSoundGrace() const { return m_flSoundGrace; }

private:

	void	StealthDebugPrintf( int iLine, Vector clr, const char *pMsg, ... );
	void	DebugShowStealthState( int iStartLine );

private:

	StealthLevel_t		m_nStealthLevel;
	StealthLevel_t		m_nMinStealthLevel;

	bool	m_bDisabled;
	bool	m_bCleanupWhenDisabled;

	bool	m_bAlerted;							// A squad has been alerted to the player
	float	m_flSoundGrace;

	bool				m_bDoingSweep;

	//-----------------------------------------------

	// Objects
	CUtlVector<StealthObjectState_t>	m_SeenObjects;

	// Squads
	CUtlVector<StealthSquadInfo_t>		m_SquadInfo;

	// Areas
	CUtlVector< CHandle<CTriggerStealthArea> >	m_StealthAreas;

	//-----------------------------------------------

	// Alarm
	bool	m_bAlarmsEnabled;
	bool	m_bAlarmRaised;
	int		m_nBodiesToRaiseAlarm;				// Minimum bodies that need to be found before raising the alarm

	// Music
	bool	m_bPlayMusic;
	bool	m_bResumeStealthMusic;

	// These should be ambient_generics in the level
	string_t	m_iszAlertMusic;
	EHANDLE		m_hAlertMusic;
	string_t	m_iszStealthMusic;
	EHANDLE		m_hStealthMusic;

	COutputEvent	m_OnAlarmRaised;
	COutputEvent	m_OnAlarmReset;
	COutputEvent	m_OnNPCGoToRaiseAlarm;
	COutputEvent	m_OnNPCStopRaiseAlarm;
	COutputString	m_OnSquadAlerted;
	COutputString	m_OnSquadLostPlayer;
	COutputEvent	m_OnAlertEnd;
	COutputEvent	m_OnBodyFound;

	DECLARE_DATADESC();
};

extern CAI_StealthManager *g_hStealthManager;

extern void InsertStealthSound( int iType, const Vector &vecOrigin, int iVolume, float flDuration, CBaseEntity *pOwner = NULL, int soundChannelIndex = 0, CBaseEntity *pSoundTarget = NULL );

//-----------------------------------------------------------------------------

#endif
