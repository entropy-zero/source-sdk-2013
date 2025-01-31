//=============================================================================//
//
// Purpose:		Workshop mounting system.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "filesystem.h"
#include "tier0/icommandline.h"
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_WORKSHOP_ITEMS 128

#define WORKSHOP_MANIFEST_NAME "addoninfo.txt"

ConVar workshop_mount_vpks( "workshop_mount_vpks", "1" );
ConVar workshop_game_override( "workshop_game_override", "", FCVAR_CHEAT | FCVAR_REPLICATED, "If a value is specified, then the workshop mounting system will pretend this is the name of the current mod. Use * to mount addons from any game." );

//=============================================================================
//=============================================================================
class CWorkshopMountSystem : public CAutoGameSystem
{
public:
	bool Init();

	void LoadWorkshopItems();
	bool ParseGameParam( const char *pszToken, const char *szModDir );
};

//-----------------------------------------------------------------------------

CWorkshopMountSystem g_WorkshopMountSystem;

static int SortStricmp( char *const *sz1, char *const *sz2 )
{
	return V_stricmp( *sz1, *sz2 );
}

bool CWorkshopMountSystem::Init()
{
	LoadWorkshopItems();
	return true;
}

void CWorkshopMountSystem::LoadWorkshopItems()
{
	if (CommandLine()->FindParm( "-noworkshop" ) != 0)
		return;

	uint64 nItemIDs[MAX_WORKSHOP_ITEMS];
	int nNumItems = steamapicontext->SteamUGC()->GetSubscribedItems( nItemIDs, MAX_WORKSHOP_ITEMS );

	char szFolder[MAX_PATH];

	char szManifestPath[MAX_PATH];

	// These are needed for the function even though we don't use them
	uint64 nSizeOnDisk;
	bool bLegacyItem;

	for (int i = 0; i < nNumItems; i++)
	{
		if (!steamapicontext->SteamUGC()->GetItemInstallInfo( nItemIDs[i], &nSizeOnDisk, szFolder, sizeof( szFolder ), &bLegacyItem ))
			continue;

		V_snprintf( szManifestPath, sizeof( szManifestPath ), "%s%c" WORKSHOP_MANIFEST_NAME, szFolder, CORRECT_PATH_SEPARATOR );

		// Add path by default in case there is no manifest
		bool bAddPath = true;

		KeyValues *pManifest = new KeyValues( "AddonInfo" );
		if (pManifest->LoadFromFile( g_pFullFileSystem, szManifestPath ))
		{
			//----------------------------------------------------------------------------
			// Addons may target specific games/mods
			//----------------------------------------------------------------------------
			const char *pszTargetGames = pManifest->GetString( "game", NULL );
			if (pszTargetGames)
			{
				// Don't load until we confirm this mod is part of the list
				bAddPath = false;

#ifdef CLIENT_DLL
				char szModDir[MAX_PATH];
				const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );

				// Copied from UTIL_GetModDir
				Q_strncpy( szModDir, pGameDir, sizeof( szModDir ) );
				if ( Q_strnchr( szModDir, '/', sizeof( szModDir ) ) || Q_strnchr( szModDir, '\\', sizeof( szModDir ) ) )
				{
					// Strip the last directory off (which will be our game dir)
					Q_StripLastDir( szModDir, sizeof( szModDir ) );
		
					// Find the difference in string lengths and take that difference from the original string as the mod dir
					int dirlen = Q_strlen( szModDir );
					Q_strncpy( szModDir, pGameDir + dirlen, Q_strlen( pGameDir ) - dirlen + 1 );
				}

				bool bValidGame = true;
#else
				char szModDir[MAX_PATH];
				bool bValidGame = UTIL_GetModDir( szModDir, sizeof( szModDir ) );
#endif

				if (workshop_game_override.GetString()[0])
				{
					V_strncpy( szModDir, workshop_game_override.GetString(), sizeof( szModDir ) );

					if (szModDir[0] == '*')
					{
						// Just pass without checking the game
						bAddPath = true;
						bValidGame = false;
					}
				}

				if (bValidGame)
				{
					char szTargetGames[128];
					V_strncpy( szTargetGames, pszTargetGames, sizeof( szTargetGames ) );

					char *pszToken = strtok( szTargetGames, "+" );
					for (; pszToken != NULL; pszToken = strtok( NULL, "+" ))
					{
						if (!pszToken || !*pszToken)
							continue;

						bool bInvert = false;
						if (pszToken[0] == '!') // Not this game
						{
							pszToken++;
							bInvert = true;
						}
						else if (bAddPath)
						{
							// If we've already been included, then don't care about this game unless it might exclude us
							continue;
						}

						bool bAllowed = ParseGameParam( pszToken, szModDir );
						if (bInvert)
						{
							if (bAllowed)
							{
								// This game was specifically excluded. Early out
								bAddPath = false;
								break;
							}

							// Just because this game wasn't excluded doesn't mean it'll be allowed, so don't do anything
						}
						else
						{
							bAddPath = bAllowed;
						}
					}
				}
			}
		}
		pManifest->deleteThis();

		if (bAddPath)
		{
			//----------------------------------------------------------------------------
			// Check for VPKs
			//----------------------------------------------------------------------------
			CUtlStringList vecVPKs;
			if (workshop_mount_vpks.GetBool())
			{
				char szVPKSearchPath[MAX_PATH];
				V_snprintf( szVPKSearchPath, sizeof( szVPKSearchPath ), "%s%c*.vpk", szFolder, CORRECT_PATH_SEPARATOR );

				FileFindHandle_t hFindHandle = NULL;
				const char *pszFoundName = g_pFullFileSystem->FindFirst( szVPKSearchPath, &hFindHandle );
				if (pszFoundName)
				{
					do
					{
						vecVPKs.CopyAndAddToTail( pszFoundName );

						pszFoundName = g_pFullFileSystem->FindNext( hFindHandle );

					} while (pszFoundName);

					g_pFullFileSystem->FindClose( hFindHandle );

					vecVPKs.Sort( SortStricmp );

					// Now for any _dir.vpk files, remove the _nnn.vpk ones.
					// (Copied from filesystem_init.cpp)
					int idx = vecVPKs.Count()-1;
					while ( idx > 0 )
					{
						char szTemp[ MAX_PATH ];
						V_strcpy_safe( szTemp, vecVPKs[ idx ] );
						--idx;

						char *szDirVpk = V_stristr( szTemp, "_dir.vpk" );
						if ( szDirVpk != NULL )
						{
							*szDirVpk = '\0';
							while ( idx >= 0 )
							{
								char *pszPath = vecVPKs[ idx ];
								if ( V_stristr( pszPath, szTemp ) != pszPath )
									break;
								delete pszPath;
								vecVPKs.Remove( idx );
								--idx;
							}
						}
					}
				}
			}

			if (vecVPKs.Count() > 0)
			{
				for (int i = 0; i < vecVPKs.Count(); i++)
				{
					char szVPK[MAX_PATH];
					V_snprintf( szVPK, sizeof( szVPK ), "%s%c%s", szFolder, CORRECT_PATH_SEPARATOR, vecVPKs[i] );

					DevMsg( "Steam Workshop: Mounting VPK \"%s\"\n", szVPK );

					filesystem->AddSearchPath( szVPK, "GAME");
					filesystem->AddSearchPath( szVPK, "MOD" );
					filesystem->AddSearchPath( szVPK, "ADDON" );
				}
			}

			DevMsg( "Steam Workshop: Mounting folder \"%s\"\n", szFolder );

			filesystem->AddSearchPath( szFolder, "GAME" );
			filesystem->AddSearchPath( szFolder, "MOD" );
			filesystem->AddSearchPath( szFolder, "ADDON" );
		}
	}
}

bool CWorkshopMountSystem::ParseGameParam( const char *pszToken, const char *pszModDir )
{
	if (pszToken[0] == '#') // Special commands
	{
		pszToken++;

		if (FStrEq( pszToken, "episodes" ))
		{
			// Return true for AP, etc.
			if (FStrEq( pszModDir, "axonpariah" ) || FStrEq( pszModDir, "progenitors" ))
				return true;
			else
				return false;
		}
		else if (FStrEq( pszToken, "all" ))
		{
			return true;
		}

		return false;
	}

	return FStrEq( pszModDir, pszToken );
}

#ifdef CLIENT_DLL
CON_COMMAND_F( workshop_reload_addons_client, "Reloads addons", FCVAR_CHEAT )
#else
CON_COMMAND_F( workshop_reload_addons, "Reloads addons", FCVAR_CHEAT )
#endif
{
	// TODO: Remove addon paths before reloading them? Not sure if necessary
	g_WorkshopMountSystem.LoadWorkshopItems();
}
