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

		if (filesystem->FileExists( name, "MOD" ))
		{
			const char *pszFileName = V_GetFileName( pchSaveFile );
			bool bAutoOrQuickSave = (V_stricmp( pszFileName, "autosave.sav" ) == 0 || V_stricmp( pszFileName, "quick.sav" ) == 0);

			// autosave.txt -> autosave01.txt
			// quick.txt -> quick01.txt
			// (TODO: Account for save_history_count)
			if (bAutoOrQuickSave)
			{
				char name2[MAX_PATH];
				Q_StripExtension( pchSaveFile, name2, sizeof( name2 ) );
				Q_strlower( name2 );
				Q_FixSlashes( name );

				Q_strncat( name2, "01.txt", sizeof( name2 ) );

				if ( filesystem->FileExists( name2, "MOD" ) )
					filesystem->RemoveFile( name2, "MOD" );

				filesystem->RenameFile( name, name2, "MOD" );
			}
		}

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

} g_CustomSaveMetadata;
