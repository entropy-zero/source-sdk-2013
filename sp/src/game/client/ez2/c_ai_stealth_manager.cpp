//=============================================================================//
//
// Purpose:		Manager for stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_stealth_manager.h"
#include "ez2/ai_stealth_shared.h"

//-----------------------------------------------------------------------------

ConVar	ai_stealth_soundmixer( "ai_stealth_soundmixer", "1" );

LINK_ENTITY_TO_CLASS( ai_stealth_manager, C_AI_StealthManager );

IMPLEMENT_CLIENTCLASS_DT( C_AI_StealthManager, DT_AI_StealthManager, CAI_StealthManager )
	RecvPropInt( RECVINFO( m_nStealthLevel ) ),
	RecvPropBool( RECVINFO( m_bDisabled ) ),
END_RECV_TABLE()

IMPLEMENT_AUTO_LIST( IStealthManagerAutoList );

//-----------------------------------------------------------------------------

C_AI_StealthManager *GetStealthManager()
{
	// We use an auto list instead of a global pointer since the server is in control of this
	for ( int i = 0; i < IStealthManagerAutoList::AutoList().Count(); i++ )
	{
		C_AI_StealthManager *pManager = static_cast<C_AI_StealthManager *>( IStealthManagerAutoList::AutoList()[i] );
		if ( pManager->IsActive() )
			return pManager;
	}

	return NULL;
}

//-----------------------------------------------------------------------------

void C_AI_StealthManager::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( m_bDisabled )
	{
		if ( m_bChangedSoundMixer )
		{
			ConVarRef snd_soundmixer( "snd_soundmixer" );
			if ( snd_soundmixer.IsValid() )
			{
				((ConVar *)snd_soundmixer.GetLinkedConVar())->Revert();
				m_bChangedSoundMixer = false;
			}
		}

		// Forces us to reevaluate sound mixer if reenabled
		m_nOldStealthLevel = STEALTH_LEVEL_NONE;
	}
	else if ( m_nOldStealthLevel != m_nStealthLevel )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer && pPlayer->CanSetSoundMixer() && ai_stealth_soundmixer.GetBool() )
		{
			// Set sound mixer, if applicable
			ConVarRef snd_soundmixer( "snd_soundmixer" );
			if ( snd_soundmixer.IsValid() )
			{
				// Set sound mixer from stealth level
				const char *pszMixer = NULL;
				switch ( m_nStealthLevel )
				{
					case STEALTH_LEVEL_QUIET:
					case STEALTH_LEVEL_GUARD:
					case STEALTH_LEVEL_TENSE:
						pszMixer = "EZ2_Stealth_Generic";
						break;
				}

				if ( pszMixer )
				{
					snd_soundmixer.SetValue( pszMixer );
					m_bChangedSoundMixer = true;
				}
				else if ( m_bChangedSoundMixer )
				{
					((ConVar *)snd_soundmixer.GetLinkedConVar())->Revert();
					m_bChangedSoundMixer = false;
				}
			}
		}

		m_nOldStealthLevel = m_nStealthLevel;
	}
}
