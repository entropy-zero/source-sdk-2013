//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Achievements for Entropy : Zero 2.
// The implementation is heavily based on the achievements from Entropy : Zero
//
//=============================================================================

#include "cbase.h"

#ifdef GAME_DLL

#include "achievementmgr.h"
#include "baseachievement.h"


////////////////////////////////////////////////////////////////////////////////////////////////////
// Chapter Completion Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAchievementEZ2CompleteChapter0 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C0"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C0" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter0, ACHIEVEMENT_EZ2_CH0, "ACH_EZ2_CH0", 5 );

class CAchievementEZ2CompleteChapter1 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C1"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C1" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter1, ACHIEVEMENT_EZ2_CH1, "ACH_EZ2_CH1", 5 );

class CAchievementEZ2CompleteChapter2 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C2"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C2" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter2, ACHIEVEMENT_EZ2_CH2, "ACH_EZ2_CH2", 5 );

class CAchievementEZ2CompleteChapter3 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C3"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C3" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter3, ACHIEVEMENT_EZ2_CH3, "ACH_EZ2_CH3", 5 );

class CAchievementEZ2CompleteChapter4 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C4"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C4" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter4, ACHIEVEMENT_EZ2_CH4, "ACH_EZ2_CH4", 5 );

class CAchievementEZ2CompleteChapter4a : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C4a"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C4a" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter4a, ACHIEVEMENT_EZ2_CH4a, "ACH_EZ2_CH4a", 5 );

class CAchievementEZ2CompleteChapter5 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C5"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C5" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter5, ACHIEVEMENT_EZ2_CH5, "ACH_EZ2_CH5", 5 );

class CAchievementEZ2CompleteChapter6 : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_COMPLETE_C6"
		};
		SetFlags( ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL );
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetComponentPrefix( "EZ2_COMPLETE_C6" );
		SetGameDirFilter( "EntropyZero2" );
		SetGoal( 1 );
	}
};
DECLARE_ACHIEVEMENT( CAchievementEZ2CompleteChapter6, ACHIEVEMENT_EZ2_CH6, "ACH_EZ2_CH6", 5 );

////////////////////////////////////////////////////////////////////////////////////////////////////
// Collectible Achievements
////////////////////////////////////////////////////////////////////////////////////////////////////

// Find all of the Arbeit 1 recording boxes
class CAchievementEZ2FindAllRecordingBoxes : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_RECORDING_RG01", "EZ2_RECORDING_RG02", "EZ2_RECORDING_RG03", "EZ2_RECORDING_RG04", "EZ2_RECORDING_RG05", "EZ2_RECORDING_CC01", "EZ2_RECORDING_CC02", "EZ2_RECORDING_CC03", "EZ2_RECORDING_CC04", "EZ2_RECORDING_CC05", "EZ2_RECORDING_CC06", "EZ2_RECORDING_CC07", "EZ2_RECORDING_CC08", "EZ2_RECORDING_CC09", "EZ2_RECORDING_CC10", "EZ2_RECORDING_CC11", "EZ2_RECORDING_CC12", "EZ2_RECORDING_CC13", "EZ2_RECORDING_RG06"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ2_RECORDING");
		SetGameDirFilter("EntropyZero2");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FindAllRecordingBoxes, ACHIEVEMENT_EZ2_RECORDING, "ACH_EZ2_RECORDING", 5);

////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_EZ2_MAP_EVENT_ACHIEVEMENT( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero2", iPointValue, false )

#define DECLARE_EZ2_MAP_EVENT_ACHIEVEMENT_HIDDEN( achievementID, achievementName, iPointValue )					\
	DECLARE_MAP_EVENT_ACHIEVEMENT_( achievementID, achievementName, "EntropyZero2", iPointValue, true )

////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Normal difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2FinishNormal : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero2");
		SetGoal(1);
	}

	virtual void Event_SkillChanged(int iSkillLevel, IGameEvent * event)
	{
		if (g_iSkillLevel < SKILL_MEDIUM)
		{
			SetAchieved(false);
			SetFailed();
		}
	}

	// Upon activating this achievement, test the current skill level
	virtual void OnActivationEvent() {
		Activate();
		Event_SkillChanged(g_iSkillLevel, NULL);
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FinishNormal, ACHIEVEMENT_EZ2_NMODE, "ACH_EZ2_NMODE", 15);
////////////////////////////////////////////////////////////////////////////////////////////////////
// Complete the mod on Hard difficulty
////////////////////////////////////////////////////////////////////////////////////////////////////
class CAchievementEZ2FinishHard : public CFailableAchievement
{
protected:

	void Init()
	{
		SetFlags(ACH_LISTEN_SKILL_EVENTS | ACH_LISTEN_MAP_EVENTS | ACH_SAVE_WITH_GAME);
		SetGameDirFilter("EntropyZero2");
		SetGoal(1);
	}

	virtual void Event_SkillChanged(int iSkillLevel, IGameEvent * event)
	{
		if (g_iSkillLevel < SKILL_HARD)
		{
			SetAchieved(false);
			SetFailed();
		}
	}

	// Upon activating this achievement, test the current skill level
	virtual void OnActivationEvent() {
		Activate();
		Event_SkillChanged(g_iSkillLevel, NULL);
	}

	// map event where achievement is activated
	virtual const char *GetActivationEventName() { return "EZ2_START_GAME"; }
	// map event where achievement is evaluated for success
	virtual const char *GetEvaluationEventName() { return "EZ2_BEAT_GAME"; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FinishHard, ACHIEVEMENT_EZ2_HMODE, "ACH_EZ2_HMODE", 15);
#endif // GAME_DLL