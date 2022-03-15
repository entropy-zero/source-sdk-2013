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
// Find all of the poem pieces
class CAchievementEZ2FindAllRecordingBoxes : public CBaseAchievement
{
	virtual void Init()
	{
		static const char *szComponents[] =
		{
			"EZ2_CCRADIO_01", "EZ2_CCRADIO_02", "EZ2_CCRADIO_03", "EZ2_CCRADIO_04", "EZ2_CCRADIO_05", "EZ2_CCRADIO_06", "EZ2_CCRADIO_07", "EZ2_CCRADIO_08", "EZ2_CCRADIO_09", "EZ2_CCRADIO_10", "EZ2_CCRADIO_11", "EZ2_CCRADIO_12", "EZ2_CCRADIO_13"
		};
		SetFlags(ACH_HAS_COMPONENTS | ACH_LISTEN_COMPONENT_EVENTS | ACH_SAVE_GLOBAL);
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE(szComponents);
		SetComponentPrefix("EZ2_CCRADIO");
		SetGameDirFilter("EntropyZero2");
		SetGoal(m_iNumComponents);
	}

	// Show progress for this achievement
	virtual bool ShouldShowProgressNotification() { return true; }
};
DECLARE_ACHIEVEMENT(CAchievementEZ2FindAllRecordingBoxes, ACHIEVEMENT_EZ2_CCRADIO, "ACH_EZ2_CCRADIO", 5);

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