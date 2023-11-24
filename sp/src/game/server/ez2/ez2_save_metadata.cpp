//=============================================================================//
//
// Purpose: Creates custom metadata for each save file
//
//=============================================================================//

#include "cbase.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "npc_wilson.h"

//=============================================================================
//=============================================================================
class CCustomSaveMetadata : public CAutoGameSystem
{
public:

	void OnSave()
	{
		char const *pchSaveFile = engine->GetSaveFileName();
		if (!pchSaveFile || !pchSaveFile[0])
		{
			Msg( "NO SAVE FILE\n" );
			return;
		}

		char name[MAX_PATH];
		Q_strncpy( name, pchSaveFile, sizeof( name ) );
		Q_strlower( name );
		Q_SetExtension( name, ".txt", sizeof( name ) );
		Q_FixSlashes( name );

		ConVarRef save_history_count("save_history_count");
		RotateFile(name, pchSaveFile, save_history_count.GetInt());

		KeyValues *pCustomSaveMetadata = new KeyValues( "CustomSaveMetadata" );
		if (pCustomSaveMetadata)
		{
			// E:Z2 Version
			{
				ConVarRef ez2Version( "ez2_version" );
				pCustomSaveMetadata->SetString( "ez2_version", ez2Version.GetString() );
			}

			// Map Version
			pCustomSaveMetadata->SetInt( "mapversion", gpGlobals->mapversion );

			// UNDONE: Host Name
			//{
			//	char szHostName[128];
			//	gethostname( szHostName, sizeof( szHostName ) );
			//	pCustomSaveMetadata->SetString( "hostname", szHostName );
			//}

			// OS Platform
			{
#ifdef _WIN32
				pCustomSaveMetadata->SetString( "platform", "windows" );
#elif defined(LINUX)
				pCustomSaveMetadata->SetString( "platform", "linux" );
#endif
			}

			// Steam Deck
			{
				const char *pszSteamDeckEnv = getenv( "SteamDeck" );
				pCustomSaveMetadata->SetBool( "is_deck", (pszSteamDeckEnv && *pszSteamDeckEnv) ); // g_pSteamInput->IsSteamRunningOnSteamDeck()
			}

			// Wilson
			if (CNPC_Wilson *pWilson = CNPC_Wilson::GetWilson())
			{
				pCustomSaveMetadata->SetBool("wilson", true );
			}

			Msg( "Saving custom metadata to %s\n", name );

			pCustomSaveMetadata->SaveToFile( filesystem, name, "MOD" );
		}
		pCustomSaveMetadata->deleteThis();
	}

protected:

	// Recursive function to rotate metadata files
	void RotateFile(char name[MAX_PATH], char const* pchSaveFile, int save_history_count, int counter=1)
	{
		// Base case: File does not exist
		if (!filesystem->FileExists(name, "MOD"))
		{
			return;
		}

		// If this file is not an autosave, do not rotate
		const char* pszFileName = V_GetFileName(pchSaveFile);
		if (!(V_stricmp(pszFileName, "autosave.sav") == 0 || V_stricmp(pszFileName, "quick.sav") == 0))
		{
			return;
		}

		// autosave.txt -> autosave01.txt
		// quick.txt -> quick01.txt
		char pszNewMetadataFilename[MAX_PATH];
		Q_StripExtension(pchSaveFile, pszNewMetadataFilename, sizeof(pszNewMetadataFilename));
		Q_strlower(pszNewMetadataFilename);
		Q_FixSlashes(name);

		// If the counter is below 10, append a leading 0
		const char* pszMetadataFileSuffix = counter < 10 ? CFmtStr("0%d.txt", counter) : CFmtStr("%d.txt", counter);

		Q_strncat(pszNewMetadataFilename, pszMetadataFileSuffix, sizeof(pszNewMetadataFilename));

		char pszNewSaveFilename[MAX_PATH];
		Q_StripExtension(pchSaveFile, pszNewSaveFilename, sizeof(pszNewSaveFilename));
		Q_strlower(pszNewSaveFilename);
		Q_FixSlashes(name);

		// If the counter is below 10, append a leading 0
		const char* pszSaveFileSuffix = counter < 10 ? CFmtStr("0%d.sav", counter) : CFmtStr("%d.sav", counter);

		Q_strncat(pszNewSaveFilename, pszSaveFileSuffix, sizeof(pszNewSaveFilename));

		// If the save associated with the next filename exists, rotate the next filename first
		if (filesystem->FileExists(pszNewSaveFilename, "MOD"))
		{
			RotateFile(pszNewMetadataFilename, pchSaveFile, save_history_count, counter + 1);
		}
		else if(filesystem->FileExists(pszNewMetadataFilename, "MOD"))
		{
			Msg("File %s does not exist. Removing metadata files %s and %s\n", pszNewSaveFilename, name, pszNewMetadataFilename);
			filesystem->RemoveFile(name, "MOD");
			filesystem->RemoveFile(pszNewMetadataFilename, "MOD");
			return;
		}
		// If we have passed the save history count, remove the files instead of rotating
		else if (counter > save_history_count)
		{
			Msg("Tried to rotate to save metadata file %s but save history count is %d. Removing file %s\n", pszNewMetadataFilename, save_history_count, name);
			filesystem->RemoveFile(name, "MOD");
			return;
		}

		filesystem->RenameFile(name, pszNewMetadataFilename, "MOD");
	}
} g_CustomSaveMetadata;
