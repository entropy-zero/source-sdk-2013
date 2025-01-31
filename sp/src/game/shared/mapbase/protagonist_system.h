//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		System to easily switch between player characters.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef PROTAGONIST_SYSTEM_H
#define PROTAGONIST_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_baseplayer.h"
#else
#include "player.h"
#endif

#define MAX_PROTAGONIST_NAME	32

//=============================================================================
//=============================================================================
class CProtagonistSystem : public CAutoGameSystem
{
public:
	CProtagonistSystem() : m_Strings( 256, 0, &StringLessThan ) { }
	~CProtagonistSystem() { PurgeProtagonists(); }

	bool Init();
	void Shutdown();
	void LevelInitPreEntity();
	void LevelShutdownPostEntity();

	//----------------------------------------------------------------------------

	void LoadProtagonistManifest( const char *pszFile );
	void LoadProtagonistFile( const char *pszFile );

	//----------------------------------------------------------------------------

	int FindProtagonistIndex( const char *pszName );

	void PrecacheProtagonist( CBaseEntity *pSource, int nIdx );

	//----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#else
	const char *GetProtagonist_PlayerModel( const CBasePlayer *pPlayer );
	int GetProtagonist_PlayerModelSkin( const CBasePlayer *pPlayer );
	int GetProtagonist_PlayerModelBody( const CBasePlayer *pPlayer );
	const char *GetProtagonist_HandModel( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	int GetProtagonist_HandModelSkin( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	int GetProtagonist_HandModelBody( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	const char *GetProtagonist_ResponseContexts( const CBasePlayer *pPlayer );
#endif

	const char *GetProtagonist_ViewModel( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	float *GetProtagonist_ViewModelFOV( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	bool *GetProtagonist_UsesHands( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	int *GetProtagonist_HandRig( const CBasePlayer *pPlayer, const CBaseCombatWeapon *pWeapon );
	
	//----------------------------------------------------------------------------

	void PurgeProtagonists();
	void PrintProtagonistData();

	//----------------------------------------------------------------------------

private:

	struct ProtagonistData_t
	{
		ProtagonistData_t()
		{

		}

		char szName[MAX_PROTAGONIST_NAME];

		CUtlVector<int>	vecParents;

#ifdef CLIENT_DLL
#else
		// Playermodel
		const char *pszPlayerModel = NULL;
		int nPlayerSkin = -1;
		int nPlayerBody = -1;

		// Hands
		const char *pszHandModels[NUM_HAND_RIG_TYPES] = {};
		int nHandSkin = -1;
		int nHandBody = -1;

		// Responses
		const char *pszResponseContexts = NULL;
#endif

		// Weapon Data
		struct WeaponDataOverride_t
		{
			const char *pszVM = NULL;
			bool bUsesHands = false;
			int nHandRig = 0;
			float flVMFOV = 0.0f;
		};

		CUtlDict<WeaponDataOverride_t> dictWpnData;
	};

	ProtagonistData_t *GetPlayerProtagonist( const CBasePlayer *pPlayer );
	ProtagonistData_t *FindProtagonist( const char *pszName );
	ProtagonistData_t *FindProtagonist( int nIndex );

	// For recursion
#ifdef CLIENT_DLL
#else
	const char *DoGetProtagonist_PlayerModel( ProtagonistData_t &pProtag );
	int DoGetProtagonist_PlayerModelSkin( ProtagonistData_t &pProtag );
	int DoGetProtagonist_PlayerModelBody( ProtagonistData_t &pProtag );
	const char *DoGetProtagonist_HandModel( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	int DoGetProtagonist_HandModelSkin( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	int DoGetProtagonist_HandModelBody( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	void DoGetProtagonist_ResponseContexts( ProtagonistData_t &pProtag, char *pszContexts, int nContextsSize );
#endif

	const char *DoGetProtagonist_ViewModel( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	float *DoGetProtagonist_ViewModelFOV( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	bool *DoGetProtagonist_UsesHands( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );
	int *DoGetProtagonist_HandRig( ProtagonistData_t &pProtag, const CBaseCombatWeapon *pWeapon );

	//----------------------------------------------------------------------------

	const char *FindString( const char *string );
	const char *AllocateString( const char *string );
	
	//----------------------------------------------------------------------------

private:
	CUtlVector<ProtagonistData_t>	m_Protagonists;

	// Dedicated strings, copied from game string pool
	CUtlRBTree<const char *> m_Strings;

};

extern CProtagonistSystem g_ProtagonistSystem;

#endif // PROTAGONIST_SYSTEM_H
