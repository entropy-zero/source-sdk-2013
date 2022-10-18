//=============================================================================//
//
// Purpose: Community integration of Steam Input on Source SDK 2013.
//
// Author: Blixibon
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "input.h"
#include "inputsystem/iinputsystem.h"
#include "GameUI/IGameUI.h"
#include "IGameUIFuncs.h"
#include "ienginevgui.h"
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include "clientmode_shared.h"
#include "weapon_parse.h"
#include "steam/isteaminput.h"
#include "steam/isteamutils.h"
#include "in_steaminput.h"
#include "icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define USE_HL2_INSTALLATION 1 // This attempts to obtain HL2's action manifest from the user's own HL2 or Portal installations
#define MENU_ACTIONS_ARE_BINDS 1

InputActionSetHandle_t g_AS_GameControls;
InputActionSetHandle_t g_AS_VehicleControls;
InputActionSetHandle_t g_AS_MenuControls;

enum ActionSet_t
{
	AS_GameControls,
	AS_VehicleControls,
	AS_MenuControls,
};

//-------------------------------------------

static InputDigitalActionCommandBind_t g_DigitalActionBinds[] = {
	{ "attack",							"+attack" },
	{ "attack2",						"+attack2" },
	{ "zoom",							"+zoom" },
	{ "reload",							"+reload" },
	{ "jump",							"+jump" },
	{ "duck",							"+duck" },
	{ "use",							"+use" },
	{ "invnext",						"invnext" },
	{ "invprev",						"invprev" },
	{ "lastinv",						"lastinv" },
	{ "walk",							"+walk" },
	{ "speed",							"+speed" },
	{ "pause_menu",						"pause_menu" },
	{ "sc_flashlight",					"impulse 100" }, // sc_flashlight
	{ "sc_squad",						"impulse 50" }, // sc_squad
	{ "lookspin",						"+lookspin" },
	{ "resetcamera",					"+resetcamera" },
	{ "slot1",							"slot1" },
	{ "slot2",							"slot2" },
	{ "slot3",							"slot3" },
	{ "slot4",							"slot4" },
	{ "phys_swap",						"phys_swap" },
	{ "bug_swap",						"bug_swap" },
	{ "sc_save_quick",					"save quick" }, // sc_save_quick
	{ "sc_load_quick",					"load quick" }, // sc_load_quick

	{ "toggle_zoom",					"toggle_zoom" },
	{ "toggle_duck",					"toggle_duck" },

	// Custom
	{ "attack3",						"+attack3" },
	{ "screenshot",						"jpeg" },
};

// Keep in sync with the above array!!!
#define ZOOM_ACTION_BIND_INDEX 2
#define BRAKE_ACTION_BIND_INDEX 4
#define DUCK_ACTION_BIND_INDEX 5
#define PAUSE_ACTION_BIND_INDEX 12
#define TOGGLE_ZOOM_ACTION_BIND_INDEX 25
#define TOGGLE_DUCK_ACTION_BIND_INDEX 26

//-------------------------------------------

InputAnalogActionHandle_t g_AA_Move;
InputAnalogActionHandle_t g_AA_Camera;
InputAnalogActionHandle_t g_AA_JoystickCamera;

//-------------------------------------------

InputAnalogActionHandle_t g_AA_Steer;
InputAnalogActionHandle_t g_AA_Accelerate;
InputAnalogActionHandle_t g_AA_Brake;

InputDigitalActionBind_t *g_DAB_Brake;

//-------------------------------------------

#if MENU_ACTIONS_ARE_BINDS
InputDigitalActionBind_t g_DAB_MenuUp;
InputDigitalActionBind_t g_DAB_MenuDown;
InputDigitalActionBind_t g_DAB_MenuLeft;
InputDigitalActionBind_t g_DAB_MenuRight;
InputDigitalActionBind_t g_DAB_MenuSelect;
InputDigitalActionBind_t g_DAB_MenuCancel;
InputDigitalActionBind_t g_DAB_MenuLB;
InputDigitalActionBind_t g_DAB_MenuRB;
#else
InputDigitalActionHandle_t g_DA_MenuUp;
InputDigitalActionHandle_t g_DA_MenuDown;
InputDigitalActionHandle_t g_DA_MenuLeft;
InputDigitalActionHandle_t g_DA_MenuRight;
InputDigitalActionHandle_t g_DA_MenuSelect;
InputDigitalActionHandle_t g_DA_MenuCancel;
InputDigitalActionHandle_t g_DA_MenuLB;
InputDigitalActionHandle_t g_DA_MenuRB;
#endif
InputDigitalActionHandle_t g_DA_MenuX;
InputDigitalActionHandle_t g_DA_MenuY;

InputDigitalActionBind_t *g_DAB_MenuPause;

InputAnalogActionHandle_t g_AA_Mouse;

//-------------------------------------------

CON_COMMAND( pause_menu, "Shortcut to toggle pause menu" )
{
	if (enginevgui->IsGameUIVisible())
	{
		engine->ClientCmd_Unrestricted( "gameui_hide" );
	}
	else
	{
		engine->ClientCmd_Unrestricted( "gameui_activate" );
	}
}

//-------------------------------------------

static ConVar si_current_cfg( "si_current_cfg", "0", FCVAR_ARCHIVE, "Steam Input's current controller." );

static ConVar si_verify_controller_every_frame( "si_verify_controller_every_frame", "1", FCVAR_NONE, "Verifies that the controller is active every frame. This is a workaround for broken callbacks." );

static ConVar si_force_glyph_controller( "si_force_glyph_controller", "-1", FCVAR_NONE, "Forces glyphs to translate to the specified ESteamInputType." );
static ConVar si_default_glyph_controller( "si_default_glyph_controller", "0", FCVAR_NONE, "The default ESteamInputType to use when a controller's glyphs aren't available." );

static ConVar si_use_glyphs( "si_use_glyphs", "1", FCVAR_NONE, "Whether or not to use controller glyphs for hints." );
static ConVar si_tint_glyphs( "si_tint_glyphs", "0", FCVAR_NONE, "Whether or not to tint controller glyphs according to the client scheme." );

static ConVar si_enable_rumble( "si_enable_rumble", "1", FCVAR_NONE, "Enables controller rumble triggering vibration events in Steam Input. If disabled, rumble is directed back to the input system as before." );

static ConVar si_hintremap( "si_hintremap", "1", FCVAR_NONE, "Enables the hint remap system, which remaps HUD hints based on the current controller configuration." );

static ConVar si_print_action_set( "si_print_action_set", "0" );
static ConVar si_print_joy_src( "si_print_joy_src", "0" );
static ConVar si_print_rumble( "si_print_rumble", "0" );
static ConVar si_print_hintremap( "si_print_hintremap", "0" );

//-------------------------------------------

class CSource2013SteamInput : public ISource2013SteamInput
{
public:

	CSource2013SteamInput() :
		m_InputDeviceConnected( this, &CSource2013SteamInput::InputDeviceConnected ),
		m_InputDeviceDisconnected( this, &CSource2013SteamInput::InputDeviceDisconnected )
	{

	}

	~CSource2013SteamInput();

	void Init() override;
	void InitActionManifest();

	void LevelInitPreEntity() override;

	void Shutdown();

	void RunFrame() override;
	
	bool IsEnabled() override;

	//-------------------------------------------
	
	const char *GetControllerName() override;

	void CheckControllerConnected();

	//-------------------------------------------

	void TestDigitalActionBind( InputDigitalActionBind_t *DigitalAction );
#if MENU_ACTIONS_ARE_BINDS
	void PressKeyFromDigitalActionHandle( InputDigitalActionBind_t &nHandle, ButtonCode_t nKey );
#else
	void PressKeyFromDigitalActionHandle( InputDigitalActionHandle_t nHandle, ButtonCode_t nKey );
#endif

	bool UsingJoysticks() override;
	void GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
		bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw ) override;

	void SetRumble( float fLeftMotor, float fRightMotor, int userId = INVALID_USER_ID ) override;
	void StopRumble( void ) override;

	//-------------------------------------------
	
	void SetLEDColor( byte r, byte g, byte b ) override;
	void ResetLEDColor() override;

	//-------------------------------------------

	int FindDigitalActionsForCommand( const char *pszCommand, InputDigitalActionHandle_t *pHandles );
	int FindAnalogActionsForCommand( const char *pszCommand, InputAnalogActionHandle_t *pHandles );
	void GetInputActionOriginsForCommand( const char *pszCommand, CUtlVector<EInputActionOrigin> &actionOrigins );

	const char *GetGlyphForCommand( const char *pszCommand, bool bSVG );
	const char *GetGlyphPNGForCommand( const char *pszCommand ) override { return GetGlyphForCommand( pszCommand, false ); }
	const char *GetGlyphSVGForCommand( const char *pszCommand ) override { return GetGlyphForCommand( pszCommand, true ); }

	virtual bool UseGlyphs() override { return si_use_glyphs.GetBool(); };
	virtual bool TintGlyphs() override { return si_tint_glyphs.GetBool(); };
	void GetGlyphFontForCommand( const char *pszCommand, char *szChars, int szCharsSize, vgui::HFont &hFont, vgui::IScheme *pScheme ) override;
	void GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char*> &szStringList ) override;

	//-------------------------------------------

	void LoadHintRemap( const char *pszFileName );
	void RemapHudHint( const char **pszInputHint ) override;

private:
	const char *IdentifyControllerParam( ESteamInputType inputType );

	STEAM_CALLBACK( CSource2013SteamInput, InputDeviceConnected, SteamInputDeviceConnected_t, m_InputDeviceConnected );
	STEAM_CALLBACK( CSource2013SteamInput, InputDeviceDisconnected, SteamInputDeviceDisconnected_t, m_InputDeviceDisconnected );
	void DeckConnected( InputHandle_t nDeviceHandle );

	//-------------------------------------------
	
	// Utilizes fonts
	char LookupGlyphCharForActionOrigin( EInputActionOrigin eAction, vgui::HFont &hFont, vgui::IScheme *pScheme );

	// Provides a description for the specified action using GetStringForActionOrigin()
	const char *LookupDescriptionForActionOrigin( EInputActionOrigin eAction );

	//-------------------------------------------

	bool m_bEnabled;

	InputHandle_t m_nControllerHandle;

	InputAnalogActionData_t m_analogMoveData, m_analogCameraData;

	InputActionSetHandle_t m_iLastActionSet;

	//-------------------------------------------

	enum
	{
		HINTREMAPCOND_NONE,
		HINTREMAPCOND_INPUT_TYPE,		// Only for the specified type of controller
		HINTREMAPCOND_ACTION_BOUND,		// Only if the specified action is bound
	};

	struct HintRemapCondition_t
	{
		int iType;
		bool bNot;
		char szParam[32];
	};

	struct HintRemap_t
	{
		const char *pszOldHint;
		const char *pszNewHint;

		CUtlVector<HintRemapCondition_t> nRemapConds;
	};

	CUtlVector< HintRemap_t >	m_HintRemaps;
};

static CSource2013SteamInput g_SteamInput;
ISource2013SteamInput *g_pSteamInput = &g_SteamInput;

//-------------------------------------------

CON_COMMAND( si_print_state, "" )
{
	bool bEnabled = g_pSteamInput->IsEnabled();
	const char *pszControllerParam = g_pSteamInput->GetControllerName();

	char szState[256];
	Q_snprintf( szState, sizeof( szState ), "STEAM INPUT: %s", bEnabled ? "Enabled" : "Disabled" );

	if (bEnabled)
	{
		Q_snprintf( szState, sizeof( szState ),
			"%s"
			"\n\n"
			"Current Controller: %s\n"
			, szState,
			pszControllerParam );
	}

	Msg( "%s\n", szState );
}

CON_COMMAND( si_restart, "" )
{
	g_SteamInput.Shutdown();
	g_pSteamInput->Init();
}

//-------------------------------------------

CSource2013SteamInput::~CSource2013SteamInput()
{
	SteamInput()->Shutdown();
}

//-------------------------------------------

void CSource2013SteamInput::Init()
{
	bool bInit = false;

	if (CommandLine()->CheckParm( "-nosteamcontroller" ) == 0 && SteamUtils()->IsOverlayEnabled())
	{
		// Do this before initializing SteamInput()
		InitActionManifest();

		bInit = SteamInput()->Init( true );
	}

	if (!bInit)
	{
		Msg( "SteamInput didn't initialize\n" );

		if (si_current_cfg.GetString()[0] != '0')
		{
			Msg("Reverting leftover Steam Input cvars\n");
			engine->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );
			engine->ClientCmd_Unrestricted( VarArgs( "exec steam_uninput_%s.cfg", si_current_cfg.GetString() ) );
		}

		return;
	}

	Msg( "SteamInput initialized\n" );

	m_bEnabled = false;
	SteamInput()->EnableDeviceCallbacks();

	g_AS_GameControls		= SteamInput()->GetActionSetHandle( "GameControls" );
	g_AS_VehicleControls	= SteamInput()->GetActionSetHandle( "VehicleControls" );
	g_AS_MenuControls		= SteamInput()->GetActionSetHandle( "MenuControls" );

	SteamInput()->ActivateActionSet( m_nControllerHandle, g_AS_GameControls );

	for (int i = 0; i < ARRAYSIZE( g_DigitalActionBinds ); i++)
	{
		g_DigitalActionBinds[i].handle = SteamInput()->GetDigitalActionHandle( g_DigitalActionBinds[i].pszActionName );
	}

	g_DAB_Brake				= &g_DigitalActionBinds[BRAKE_ACTION_BIND_INDEX];

	g_AA_Move				= SteamInput()->GetAnalogActionHandle( "Move" );
	g_AA_Camera				= SteamInput()->GetAnalogActionHandle( "Camera" );
	g_AA_JoystickCamera		= SteamInput()->GetAnalogActionHandle( "JoystickCamera" );
	g_AA_Steer				= SteamInput()->GetAnalogActionHandle( "Steer" );
	g_AA_Accelerate			= SteamInput()->GetAnalogActionHandle( "Accelerate" );
	g_AA_Brake				= SteamInput()->GetAnalogActionHandle( "Brake" );
	g_AA_Mouse				= SteamInput()->GetAnalogActionHandle( "Mouse" );

#if MENU_ACTIONS_ARE_BINDS
	g_DAB_MenuUp.handle		= SteamInput()->GetDigitalActionHandle( "menu_up" );
	g_DAB_MenuDown.handle	= SteamInput()->GetDigitalActionHandle( "menu_down" );
	g_DAB_MenuLeft.handle	= SteamInput()->GetDigitalActionHandle( "menu_left" );
	g_DAB_MenuRight.handle	= SteamInput()->GetDigitalActionHandle( "menu_right" );
	g_DAB_MenuSelect.handle	= SteamInput()->GetDigitalActionHandle( "menu_select" );
	g_DAB_MenuCancel.handle	= SteamInput()->GetDigitalActionHandle( "menu_cancel" );
	g_DAB_MenuLB.handle		= SteamInput()->GetDigitalActionHandle( "menu_lb" );
	g_DAB_MenuRB.handle		= SteamInput()->GetDigitalActionHandle( "menu_rb" );
#else
	g_DA_MenuUp				= SteamInput()->GetDigitalActionHandle( "menu_up" );
	g_DA_MenuDown			= SteamInput()->GetDigitalActionHandle( "menu_down" );
	g_DA_MenuLeft			= SteamInput()->GetDigitalActionHandle( "menu_left" );
	g_DA_MenuRight			= SteamInput()->GetDigitalActionHandle( "menu_right" );
	g_DA_MenuSelect			= SteamInput()->GetDigitalActionHandle( "menu_select" );
	g_DA_MenuCancel			= SteamInput()->GetDigitalActionHandle( "menu_cancel" );
	g_DA_MenuLB				= SteamInput()->GetDigitalActionHandle( "menu_lb" );
	g_DA_MenuRB				= SteamInput()->GetDigitalActionHandle( "menu_rb" );
#endif
	g_DA_MenuX				= SteamInput()->GetDigitalActionHandle( "menu_x" );
	g_DA_MenuY				= SteamInput()->GetDigitalActionHandle( "menu_y" );

	g_DAB_MenuPause			= &g_DigitalActionBinds[PAUSE_ACTION_BIND_INDEX];

	SteamInput()->RunFrame();

	if (!m_bEnabled)
	{
		if (SteamUtils()->IsSteamRunningOnSteamDeck())
		{
			InputHandle_t *inputHandles = new InputHandle_t[ STEAM_INPUT_MAX_COUNT ];
			int iNumHandles = SteamInput()->GetConnectedControllers( inputHandles );
			Msg( "On Steam Deck and number of controllers is %i\n", iNumHandles );

			if (iNumHandles > 0)
			{
				DeckConnected( inputHandles[0] );
			}
		}
		else if (si_current_cfg.GetString()[0] != '0')
		{
			Msg("Reverting leftover Steam Input cvars\n");
			engine->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );
			engine->ClientCmd_Unrestricted( VarArgs( "exec steam_uninput_%s.cfg", si_current_cfg.GetString() ) );
		}
	}

	LoadHintRemap( "scripts/steaminput_hintremap.txt" );

	// Also load mod remap script
	LoadHintRemap( "scripts/mod_hintremap.txt" );

	g_pVGuiLocalize->AddFile( "resource/steaminput_%language%.txt" );
}

#define ACTION_MANIFEST_MOD					"steam_input/action_manifest_mod.vdf"
#define ACTION_MANIFEST_RELATIVE_HL2		"%s/../Half-Life 2/steam_input/action_manifest_hl2.vdf"
#define ACTION_MANIFEST_RELATIVE_PORTAL		"%s/../Portal/steam_input/action_manifest_hl2.vdf"

void CSource2013SteamInput::InitActionManifest()
{
	// First, check for a mod-specific action manifest
	if (filesystem->FileExists( ACTION_MANIFEST_MOD, "MOD" ))
	{
		char szFullPath[MAX_PATH];
		g_pFullFileSystem->RelativePathToFullPath( ACTION_MANIFEST_MOD, "MOD", szFullPath, sizeof( szFullPath ) );
		V_FixSlashes( szFullPath );

		Msg( "Loading mod action manifest file at \"%s\"\n", szFullPath );
		SteamInput()->SetInputActionManifestFilePath( szFullPath );
	}
#if USE_HL2_INSTALLATION
	// We don't need to make this specific to SDK 2013. Steam mods which don't want this behavior can toggle USE_HL2_INSTALLATION and/or use action_manifest_mod.vdf
	else //if (SteamUtils()->GetAppID() == 243730 || SteamUtils()->GetAppID() == 243750)
	{
		char szCurDir[MAX_PATH];
		filesystem->GetCurrentDirectory( szCurDir, sizeof( szCurDir ) );

		char szTargetApp[MAX_PATH];
		Q_snprintf( szTargetApp, sizeof( szTargetApp ), ACTION_MANIFEST_RELATIVE_HL2, szCurDir );
		V_FixSlashes( szTargetApp );

		if (g_pFullFileSystem->FileExists( szTargetApp ))
		{
			Msg( "Loading Half-Life 2 action manifest file at \"%s\"\n", szTargetApp );
			SteamInput()->SetInputActionManifestFilePath( szTargetApp );
		}
		else
		{
			// If Half-Life 2 is not installed, check if Portal has it
			Q_snprintf( szTargetApp, sizeof( szTargetApp ), ACTION_MANIFEST_RELATIVE_PORTAL, szCurDir );
			V_FixSlashes( szTargetApp );

			if (g_pFullFileSystem->FileExists( szTargetApp ))
			{
				Msg( "Loading Portal's copy of HL2 action manifest file at \"%s\"\n", szTargetApp );
				SteamInput()->SetInputActionManifestFilePath( szTargetApp );
			}
		}
	}
#endif
}

void CSource2013SteamInput::LevelInitPreEntity()
{
	if (IsEnabled())
	{
		// Sometimes, the archived value overwrites the cvar. This is a compromise to make sure that doesn't happen
		ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
		si_current_cfg.SetValue( IdentifyControllerParam( inputType ) );
	}
}

void CSource2013SteamInput::Shutdown()
{
	SteamInput()->Shutdown();
	m_nControllerHandle = 0;
}

//-------------------------------------------

const char *CSource2013SteamInput::IdentifyControllerParam( ESteamInputType inputType )
{
	switch (inputType)
	{
		case k_ESteamInputType_SteamDeckController:
			return "deck";
			break;
		case k_ESteamInputType_SteamController:
			return "steamcontroller";
			break;
		case k_ESteamInputType_XBox360Controller:
			return "xbox360";
			break;
		case k_ESteamInputType_XBoxOneController:
			return "xboxone";
			break;
		case k_ESteamInputType_PS3Controller:
			return "ps3";
			break;
		case k_ESteamInputType_PS4Controller:
			return "ps4";
			break;
		case k_ESteamInputType_PS5Controller:
			return "ps5";
			break;
		case k_ESteamInputType_SwitchProController:
			return "switchpro";
			break;
		case k_ESteamInputType_SwitchJoyConPair:
			return "joyconpair";
			break;
		case k_ESteamInputType_SwitchJoyConSingle:
			return "joyconsingle";
			break;
	}

	return NULL;
}

void CSource2013SteamInput::InputDeviceConnected( SteamInputDeviceConnected_t *pParam )
{
	// Don't need another controller
	if (m_bEnabled)
	{
		Msg( "Steam Input rejected additional controller\n" );
		return;
	}

	m_nControllerHandle = pParam->m_ulConnectedDeviceHandle;
	m_bEnabled = true;

	engine->ClientCmd_Unrestricted( "exec steam_input.cfg" );

	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
	const char *pszInputPrintType = IdentifyControllerParam( inputType );

	Msg( "Steam Input running with a controller (%i: %s)\n", inputType, pszInputPrintType );

	if (pszInputPrintType)
	{
		engine->ClientCmd_Unrestricted( VarArgs( "exec steam_input_%s.cfg", pszInputPrintType ) );
		si_current_cfg.SetValue( pszInputPrintType );
	}

	if ( engine->IsConnected() )
	{
		// Refresh weapon buckets
		PrecacheFileWeaponInfoDatabase( filesystem, g_pGameRules->GetEncryptionKey() );
	}
}

void CSource2013SteamInput::InputDeviceDisconnected( SteamInputDeviceDisconnected_t *pParam )
{
	Msg( "Steam Input controller disconnected\n" );

	m_nControllerHandle = 0;
	m_bEnabled = false;

	engine->ClientCmd_Unrestricted( "exec steam_uninput.cfg" );

	const char *pszInputPrintType = NULL;
	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( pParam->m_ulDisconnectedDeviceHandle );
	pszInputPrintType = IdentifyControllerParam( inputType );

	if (pszInputPrintType)
	{
		engine->ClientCmd_Unrestricted( VarArgs( "exec steam_uninput_%s.cfg", pszInputPrintType ) );
	}

	si_current_cfg.SetValue( "0" );

	if ( engine->IsConnected() )
	{
		// Refresh weapon buckets
		PrecacheFileWeaponInfoDatabase( filesystem, g_pGameRules->GetEncryptionKey() );
	}
}

void CSource2013SteamInput::DeckConnected( InputHandle_t nDeviceHandle )
{
	// Don't need another controller
	if (m_bEnabled)
	{
		Msg( "Steam Input rejected additional controller\n" );
		return;
	}

	Msg( "Steam Input running with a Steam Deck\n" );

	m_nControllerHandle = nDeviceHandle;
	m_bEnabled = true;

	engine->ClientCmd_Unrestricted( "exec steam_input.cfg" );
	engine->ClientCmd_Unrestricted( "exec steam_input_deck.cfg" );
	si_current_cfg.SetValue( "deck" );
}

//-------------------------------------------

void CSource2013SteamInput::RunFrame()
{
	SteamInput()->RunFrame();

	// This is a workaround for broken callbacks
	if (si_verify_controller_every_frame.GetBool())
	{
		CheckControllerConnected();
	}

	if (!IsEnabled())
		return;

	//if (!SteamInput()->BNewDataAvailable())
	//	return;

	int iActionSet = AS_MenuControls;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if( pPlayer )
	{
		if ( !engine->IsPaused() && !engine->IsLevelMainMenuBackground() )
		{
			if (pPlayer->GetVehicle())
			{
				iActionSet = AS_VehicleControls;
			}
			else
			{
				iActionSet = AS_GameControls;
			}
		}
	}

	switch (iActionSet)
	{
		case AS_GameControls:
		{
			SteamInput()->ActivateActionSet( m_nControllerHandle, g_AS_GameControls );
			
			// Run commands for all digital actions
			for (int i = 0; i < ARRAYSIZE( g_DigitalActionBinds ); i++)
			{
				TestDigitalActionBind( &g_DigitalActionBinds[i] );
			}

			m_analogMoveData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Move );

		} break;

		case AS_VehicleControls:
		{
			SteamInput()->ActivateActionSet( m_nControllerHandle, g_AS_VehicleControls );
			
			// Run commands for all digital actions
			for (int i = 0; i < ARRAYSIZE( g_DigitalActionBinds ); i++)
			{
				TestDigitalActionBind( &g_DigitalActionBinds[i] );
			}

			m_analogMoveData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Move );

			// Add steer data to the X value
			InputAnalogActionData_t steerData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Steer );
			m_analogMoveData.x += steerData.x;

			// Add acceleration to the Y value
			steerData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Accelerate );
			m_analogMoveData.y += steerData.x;

			if (g_DAB_Brake->bDown == false)
			{
				// For now, braking is equal to the digital action
				steerData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Brake );
				if (steerData.x >= 0.25f)
				{
					engine->ClientCmd_Unrestricted( "+jump" );
				}
				else
				{
					engine->ClientCmd_Unrestricted( "-jump" );
				}
			}

		} break;

		case AS_MenuControls:
		{
			SteamInput()->ActivateActionSet( m_nControllerHandle, g_AS_MenuControls );

#if MENU_ACTIONS_ARE_BINDS
			PressKeyFromDigitalActionHandle( g_DAB_MenuUp, KEY_UP ); // KEY_XBUTTON_UP
			PressKeyFromDigitalActionHandle( g_DAB_MenuDown, KEY_DOWN ); // KEY_XBUTTON_DOWN
			PressKeyFromDigitalActionHandle( g_DAB_MenuLeft, KEY_LEFT ); // KEY_XBUTTON_LEFT
			PressKeyFromDigitalActionHandle( g_DAB_MenuRight, KEY_RIGHT ); // KEY_XBUTTON_RIGHT
			PressKeyFromDigitalActionHandle( g_DAB_MenuSelect, KEY_XBUTTON_A );
			PressKeyFromDigitalActionHandle( g_DAB_MenuCancel, KEY_XBUTTON_B );
			PressKeyFromDigitalActionHandle( g_DAB_MenuLB, KEY_XBUTTON_LEFT ); // KEY_XBUTTON_LEFT_SHOULDER
			PressKeyFromDigitalActionHandle( g_DAB_MenuRB, KEY_XBUTTON_RIGHT ); // KEY_XBUTTON_RIGHT_SHOULDER
#else
			PressKeyFromDigitalActionHandle( g_DA_MenuUp, KEY_UP ); // KEY_XBUTTON_UP
			PressKeyFromDigitalActionHandle( g_DA_MenuDown, KEY_DOWN ); // KEY_XBUTTON_DOWN
			PressKeyFromDigitalActionHandle( g_DA_MenuLeft, KEY_LEFT ); // KEY_XBUTTON_LEFT
			PressKeyFromDigitalActionHandle( g_DA_MenuRight, KEY_RIGHT ); // KEY_XBUTTON_RIGHT
			PressKeyFromDigitalActionHandle( g_DA_MenuSelect, KEY_XBUTTON_A );
			PressKeyFromDigitalActionHandle( g_DA_MenuCancel, KEY_XBUTTON_B );
			//PressKeyFromDigitalActionHandle( g_DA_MenuX, KEY_X );
			//PressKeyFromDigitalActionHandle( g_DA_MenuY, KEY_Y );
			PressKeyFromDigitalActionHandle( g_DA_MenuLB, KEY_XBUTTON_LEFT ); // KEY_XBUTTON_LEFT_SHOULDER
			PressKeyFromDigitalActionHandle( g_DA_MenuRB, KEY_XBUTTON_RIGHT ); // KEY_XBUTTON_RIGHT_SHOULDER
#endif

			TestDigitalActionBind( g_DAB_MenuPause );

			//InputDigitalActionData_t xData = SteamInput()->GetDigitalActionData( m_nControllerHandle, g_DA_MenuX );
			InputDigitalActionData_t yData = SteamInput()->GetDigitalActionData( m_nControllerHandle, g_DA_MenuY );

			//if (xData.bState)
			//	engine->ClientCmd_Unrestricted( "gamemenucommand OpenOptionsDialog\n" );

			if (yData.bState)
				engine->ClientCmd_Unrestricted( "gamemenucommand OpenOptionsDialog\n" );

		} break;
	}

	if (iActionSet != AS_MenuControls)
	{
		InputAnalogActionData_t cameraData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_Camera );
		InputAnalogActionData_t cameraJoystickData = SteamInput()->GetAnalogActionData( m_nControllerHandle, g_AA_JoystickCamera );

		if (cameraJoystickData.bActive)
		{
			m_analogCameraData = cameraJoystickData;
		}
		else
		{
			m_analogCameraData = cameraData;
		}
	}

	m_iLastActionSet = iActionSet;

	if (si_print_action_set.GetBool())
	{
		switch (iActionSet)
		{
			case AS_GameControls:
				Msg( "Steam Input: GameControls\n" );
				break;

			case AS_VehicleControls:
				Msg( "Steam Input: VehicleControls\n" );
				break;

			case AS_MenuControls:
				Msg( "Steam Input: MenuControls\n" );
				break;
		}
	}
}

bool CSource2013SteamInput::IsEnabled()
{
	return m_bEnabled;
}

const char *CSource2013SteamInput::GetControllerName()
{
	ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
	return IdentifyControllerParam( inputType );
}

void CSource2013SteamInput::CheckControllerConnected()
{
	InputHandle_t *inputHandles = new InputHandle_t[ STEAM_INPUT_MAX_COUNT ];
	int iNumHandles = SteamInput()->GetConnectedControllers( inputHandles );

	if (inputHandles[0] != m_nControllerHandle)
	{
		if (IsEnabled())
		{
			SteamInputDeviceDisconnected_t deviceDisconnected;
			deviceDisconnected.m_ulDisconnectedDeviceHandle = m_nControllerHandle;
			InputDeviceDisconnected( &deviceDisconnected );
		}

		if (iNumHandles > 0)
		{
			// Register the new controller
			SteamInputDeviceConnected_t deviceConnected;
			deviceConnected.m_ulConnectedDeviceHandle = inputHandles[0];
			InputDeviceConnected( &deviceConnected );
		}
	}
}

void CSource2013SteamInput::TestDigitalActionBind( InputDigitalActionBind_t *DigitalAction )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( m_nControllerHandle, DigitalAction->handle );

	if (data.bState)
	{
		// Key is not down
		if (!DigitalAction->bDown)
		{
			DigitalAction->bDown = true;
			DigitalAction->OnDown();
		}
	}
	else
	{
		// Key is already down
		if (DigitalAction->bDown)
		{
			DigitalAction->bDown = false;
			DigitalAction->OnUp();
		}
	}
}

#if MENU_ACTIONS_ARE_BINDS
void CSource2013SteamInput::PressKeyFromDigitalActionHandle( InputDigitalActionBind_t &nHandle, ButtonCode_t nKey )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( m_nControllerHandle, nHandle.handle );

	bool bSendKey = false;
	if (data.bState)
	{
		// Key is not down
		if (!nHandle.bDown)
		{
			nHandle.bDown = true;
			bSendKey = true;
		}
	}
	else
	{
		// Key is already down
		if (nHandle.bDown)
		{
			nHandle.bDown = false;
			bSendKey = true;
		}
	}
		
	if (bSendKey)
	{
		if (nHandle.bDown)
			ivgui()->PostMessage( vgui::input()->GetFocus(), new KeyValues( "KeyCodePressed", "code", nKey ), NULL );
	}
}
#else
void CSource2013SteamInput::PressKeyFromDigitalActionHandle( InputDigitalActionHandle_t nHandle, ButtonCode_t nKey )
{
	InputDigitalActionData_t data = SteamInput()->GetDigitalActionData( m_nControllerHandle, nHandle );

	/*if (data.bActive)
	{
		//g_pClientMode->GetViewport()->OnKeyCodePressed( nKey );

		//InputEvent_t inputEvent;
		//inputEvent.m_nType = IE_ButtonPressed;
		//inputEvent.m_nData = nKey;
		//inputEvent.m_nData2 = inputsystem->ButtonCodeToVirtualKey( nKey );
		//inputsystem->PostUserEvent( inputEvent );
	}*/
}
#endif

static inline bool IsRelativeAnalog( EInputSourceMode mode )
{
	// TODO: Is there a better way of doing this?
	return mode == k_EInputSourceMode_AbsoluteMouse ||
		mode == k_EInputSourceMode_RelativeMouse ||
		mode == k_EInputSourceMode_JoystickMouse ||
		mode == k_EInputSourceMode_MouseRegion;
}

bool CSource2013SteamInput::UsingJoysticks()
{
	// For now, any controller uses joysticks
	return IsEnabled();
}

void CSource2013SteamInput::GetJoystickValues( float &flForward, float &flSide, float &flPitch, float &flYaw,
	bool &bRelativeForward, bool &bRelativeSide, bool &bRelativePitch, bool &bRelativeYaw )
{

	if (IsRelativeAnalog( m_analogMoveData.eMode ))
	{
		bRelativeForward = true;
		bRelativeSide = true;

		flForward = (m_analogMoveData.y / 180.0f) * MAX_BUTTONSAMPLE;
		flSide = (m_analogMoveData.x / 180.0f) * MAX_BUTTONSAMPLE;
	}
	else
	{
		bRelativeForward = false;
		bRelativeSide = false;

		flForward = m_analogMoveData.y * MAX_BUTTONSAMPLE;
		flSide = m_analogMoveData.x * MAX_BUTTONSAMPLE;
	}
	
	if (IsRelativeAnalog( m_analogCameraData.eMode ))
	{
		bRelativePitch = true;
		bRelativeYaw = true;

		flPitch = (m_analogCameraData.y / 180.0f) * MAX_BUTTONSAMPLE;
		flYaw = (m_analogCameraData.x / 180.0f) * MAX_BUTTONSAMPLE;
	}
	else
	{
		bRelativePitch = false;
		bRelativeYaw = false;

		flPitch = m_analogCameraData.y * MAX_BUTTONSAMPLE;
		flYaw = m_analogCameraData.x * MAX_BUTTONSAMPLE;
	}

	if (si_print_joy_src.GetBool())
	{
		Msg( "moveData = %i (%f, %f)\ncameraData = %i (%f, %f)\n\n",
			m_analogMoveData.eMode, m_analogMoveData.x, m_analogMoveData.y,
			m_analogCameraData.eMode, m_analogCameraData.x, m_analogCameraData.y );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void CSource2013SteamInput::SetRumble( float fLeftMotor, float fRightMotor, int userId )
{
	if (!IsEnabled() || !si_enable_rumble.GetBool())
	{
		inputsystem->SetRumble( fLeftMotor, fRightMotor, userId );
		return;
	}

	SteamInput()->TriggerVibrationExtended( m_nControllerHandle, fLeftMotor, fRightMotor, fLeftMotor, fRightMotor );

	if (si_print_rumble.GetBool())
	{
		Msg( "fLeftMotor = %f, fRightMotor = %f\n\n", fLeftMotor, fRightMotor );
	}
}

void CSource2013SteamInput::StopRumble()
{
	if (!IsEnabled())
	{
		inputsystem->StopRumble();
		return;
	}

	// N/A
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void CSource2013SteamInput::SetLEDColor( byte r, byte g, byte b )
{
	SteamInput()->SetLEDColor( m_nControllerHandle, r, g, b, k_ESteamControllerLEDFlag_SetColor );
}

void CSource2013SteamInput::ResetLEDColor()
{
	SteamInput()->SetLEDColor( m_nControllerHandle, 0, 0, 0, k_ESteamControllerLEDFlag_RestoreUserDefault );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int CSource2013SteamInput::FindDigitalActionsForCommand( const char *pszCommand, InputDigitalActionHandle_t *pHandles )
{
	int iNumHandles = 0;

	// Special cases
	if (FStrEq( pszCommand, "duck" ))
	{
		// Add toggle_duck
		pHandles[iNumHandles] = g_DigitalActionBinds[TOGGLE_DUCK_ACTION_BIND_INDEX].handle;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "zoom" ))
	{
		// Add toggle_zoom
		pHandles[iNumHandles] = g_DigitalActionBinds[TOGGLE_ZOOM_ACTION_BIND_INDEX].handle;
		iNumHandles++;
	}

	// Figure out which command this is
	for (int i = 0; i < ARRAYSIZE( g_DigitalActionBinds ); i++)
	{
		const char *pszBindCommand = g_DigitalActionBinds[i].pszBindCommand;
		if (pszBindCommand[0] == '+')
			pszBindCommand++;

		if (FStrEq( pszCommand, pszBindCommand ))
		{
			pHandles[iNumHandles] = g_DigitalActionBinds[i].handle;
			iNumHandles++;
			break;
		}
	}

	return iNumHandles;
}

int CSource2013SteamInput::FindAnalogActionsForCommand( const char *pszCommand, InputAnalogActionHandle_t *pHandles )
{
	int iNumHandles = 0;

	// Check pre-set analog action names
	if (FStrEq( pszCommand, "xlook" ))
	{
		// Add g_AA_Camera and g_AA_JoystickCamera
		pHandles[iNumHandles] = g_AA_Camera;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_JoystickCamera;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "xaccel" ))
	{
		// Add g_AA_Accelerate and g_AA_Move
		pHandles[iNumHandles] = g_AA_Accelerate;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_Move;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "xmove" ) )
	{
		// Add g_AA_Accelerate and g_AA_Move
		pHandles[iNumHandles] = g_AA_Accelerate;
		iNumHandles++;
		pHandles[iNumHandles] = g_AA_Move;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "xsteer" ))
	{
		pHandles[iNumHandles] = g_AA_Steer;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "xbrake" ))
	{
		pHandles[iNumHandles] = g_AA_Brake;
		iNumHandles++;
	}
	else if (FStrEq( pszCommand, "xmouse" ))
	{
		pHandles[iNumHandles] = g_AA_Mouse;
		iNumHandles++;
	}

	return iNumHandles;
}

void CSource2013SteamInput::GetInputActionOriginsForCommand( const char *pszCommand, CUtlVector<EInputActionOrigin> &actionOrigins )
{
	InputActionSetHandle_t actionSet = g_AS_MenuControls;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if( pPlayer )
	{
		if ( !engine->IsPaused() && !engine->IsLevelMainMenuBackground() )
		{
			if (pPlayer->GetVehicle())
			{
				actionSet = g_AS_VehicleControls;
			}
			else
			{
				actionSet = g_AS_GameControls;
			}
		}
	}

	InputDigitalActionHandle_t digitalActions[STEAM_INPUT_MAX_ORIGINS];
	int iNumActions = FindDigitalActionsForCommand( pszCommand, digitalActions );
	if (iNumActions > 0)
	{
		for (int i = 0; i < iNumActions; i++)
		{
			EInputActionOrigin actionOriginsLocal[STEAM_INPUT_MAX_ORIGINS];
			int iNumOriginsLocal = SteamInput()->GetDigitalActionOrigins( m_nControllerHandle, actionSet, digitalActions[i], actionOriginsLocal );

			if (iNumOriginsLocal > 0)
			{
				// Add them to the list
				actionOrigins.AddMultipleToTail( iNumOriginsLocal, actionOriginsLocal );

				//memcpy( actionOrigins+iNumOrigins, actionOriginsLocal, sizeof(EInputActionOrigin)*iNumOriginsLocal );
				//iNumOrigins += iNumOriginsLocal;
			}
		}
	}
	else
	{
		InputAnalogActionHandle_t analogActions[STEAM_INPUT_MAX_ORIGINS];
		iNumActions = FindAnalogActionsForCommand( pszCommand, analogActions );
		for (int i = 0; i < iNumActions; i++)
		{
			EInputActionOrigin actionOriginsLocal[STEAM_INPUT_MAX_ORIGINS];
			int iNumOriginsLocal = SteamInput()->GetAnalogActionOrigins( m_nControllerHandle, actionSet, analogActions[i], actionOriginsLocal );

			if (iNumOriginsLocal > 0)
			{
				// Add them to the list
				actionOrigins.AddMultipleToTail( iNumOriginsLocal, actionOriginsLocal );

				//memcpy( actionOrigins+iNumOrigins, actionOriginsLocal, sizeof(EInputActionOrigin)*iNumOriginsLocal );
				//iNumOrigins += iNumOriginsLocal;
			}
		}
	}
}

const char *CSource2013SteamInput::GetGlyphForCommand( const char *pszCommand, bool bSVG )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins );

	if (bSVG)
	{
		return SteamInput()->GetGlyphSVGForActionOrigin( actionOrigins[0], 0 );
	}
	else
	{
		// TODO: Multiple sizes?
		return SteamInput()->GetGlyphPNGForActionOrigin( actionOrigins[0], k_ESteamInputGlyphSize_Medium, 0 );
	}
}

void CSource2013SteamInput::GetGlyphFontForCommand( const char *pszCommand, char *szChars, int szCharsSize, vgui::HFont &hFont, vgui::IScheme *pScheme )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins );

	for (int i = 0; i < actionOrigins.Count(); i++)
	{
		if (si_force_glyph_controller.GetInt() != -1)
		{
			actionOrigins[i] = SteamInput()->TranslateActionOrigin( (ESteamInputType)si_force_glyph_controller.GetInt(), actionOrigins[i] );
		}

		char cChar = LookupGlyphCharForActionOrigin( actionOrigins[i], hFont, pScheme );
		if (cChar == 0)
		{
			// Translate to default (k_ESteamInputType_Unknown) and try again
			actionOrigins[i] = SteamInput()->TranslateActionOrigin( (ESteamInputType)si_default_glyph_controller.GetInt(), actionOrigins[i] );
			cChar = LookupGlyphCharForActionOrigin( actionOrigins[i], hFont, pScheme );
		}

		if (cChar != 0)
		{
			int len = strlen(szChars);
			szChars[len] = cChar;
			
			if (len+1 >= szCharsSize)
				break;
		}
	}
}

void CSource2013SteamInput::GetButtonStringsForCommand( const char *pszCommand, CUtlVector<const char*> &szStringList )
{
	if (pszCommand[0] == '+')
		pszCommand++;

	CUtlVector<EInputActionOrigin> actionOrigins;
	GetInputActionOriginsForCommand( pszCommand, actionOrigins );

	for (int i = 0; i < actionOrigins.Count(); i++)
	{
		szStringList.AddToTail( LookupDescriptionForActionOrigin( actionOrigins[i] ) );
	}
}

char CSource2013SteamInput::LookupGlyphCharForActionOrigin( EInputActionOrigin eAction, vgui::HFont &hFont, vgui::IScheme *pScheme )
{
	// Steam Deck
	if (eAction >= k_EInputActionOrigin_SteamDeck_A && eAction <= k_EInputActionOrigin_SteamDeck_Reserved20)
	{
		hFont = pScheme->GetFont( "SteamButtons_SD", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_SteamDeck_A:
				return 'A';
			case k_EInputActionOrigin_SteamDeck_B:
				return 'B';
			case k_EInputActionOrigin_SteamDeck_X:
				return 'X';
			case k_EInputActionOrigin_SteamDeck_Y:
				return 'Y';

			case k_EInputActionOrigin_SteamDeck_L1:
				return 'K';
			case k_EInputActionOrigin_SteamDeck_L2:
			case k_EInputActionOrigin_SteamDeck_L2_SoftPull:
				return 'L';
			case k_EInputActionOrigin_SteamDeck_L4:
				return 'M';
			case k_EInputActionOrigin_SteamDeck_L5:
				return 'N';

			case k_EInputActionOrigin_SteamDeck_R1:
				return 'k';
			case k_EInputActionOrigin_SteamDeck_R2:
			case k_EInputActionOrigin_SteamDeck_R2_SoftPull:
				return 'l';
			case k_EInputActionOrigin_SteamDeck_R4:
				return 'm';
			case k_EInputActionOrigin_SteamDeck_R5:
				return 'n';

			case k_EInputActionOrigin_SteamDeck_LeftStick_Move:
			case k_EInputActionOrigin_SteamDeck_LeftStick_Touch:
				return ',';
			case k_EInputActionOrigin_SteamDeck_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_SteamDeck_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_SteamDeck_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_SteamDeck_LeftStick_DPadEast:
				return '|';
			case k_EInputActionOrigin_SteamDeck_L3:
				return '<';

			case k_EInputActionOrigin_SteamDeck_RightStick_Move:
			case k_EInputActionOrigin_SteamDeck_RightStick_Touch:
				return '.';
			case k_EInputActionOrigin_SteamDeck_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_SteamDeck_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_SteamDeck_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_SteamDeck_RightStick_DPadEast:
				return '?';
			case k_EInputActionOrigin_SteamDeck_R3:
				return '>';

			case k_EInputActionOrigin_SteamDeck_LeftPad_Touch:
				return '(';
			case k_EInputActionOrigin_SteamDeck_LeftPad_Swipe:
				return '[';
			case k_EInputActionOrigin_SteamDeck_LeftPad_Click:
				return '{';
			case k_EInputActionOrigin_SteamDeck_LeftPad_DPadNorth:
				return 'O';
			case k_EInputActionOrigin_SteamDeck_LeftPad_DPadSouth:
				return 'P';
			case k_EInputActionOrigin_SteamDeck_LeftPad_DPadWest:
				return 'Q';
			case k_EInputActionOrigin_SteamDeck_LeftPad_DPadEast:
				return 'R';

			case k_EInputActionOrigin_SteamDeck_RightPad_Touch:
				return ')';
			case k_EInputActionOrigin_SteamDeck_RightPad_Swipe:
				return ']';
			case k_EInputActionOrigin_SteamDeck_RightPad_Click:
				return '}';
			case k_EInputActionOrigin_SteamDeck_RightPad_DPadNorth:
				return 'o';
			case k_EInputActionOrigin_SteamDeck_RightPad_DPadSouth:
				return 'p';
			case k_EInputActionOrigin_SteamDeck_RightPad_DPadWest:
				return 'q';
			case k_EInputActionOrigin_SteamDeck_RightPad_DPadEast:
				return 'r';

			case k_EInputActionOrigin_SteamDeck_DPad_North:
				return 'F';
			case k_EInputActionOrigin_SteamDeck_DPad_South:
				return 'G';
			case k_EInputActionOrigin_SteamDeck_DPad_West:
				return 'D';
			case k_EInputActionOrigin_SteamDeck_DPad_East:
				return 'E';

			case k_EInputActionOrigin_SteamDeck_Gyro_Move:
				return 'c';
			case k_EInputActionOrigin_SteamDeck_Gyro_Pitch:
				return 'f';
			case k_EInputActionOrigin_SteamDeck_Gyro_Yaw:
				return 'e';
			case k_EInputActionOrigin_SteamDeck_Gyro_Roll:
				return 'f';

			case k_EInputActionOrigin_SteamDeck_Menu:
				return 'T';
			case k_EInputActionOrigin_SteamDeck_View:
				return 'S';
		}
	}
	// Xbox 360 Controller
	else if (eAction >= k_EInputActionOrigin_XBox360_A && eAction <= k_EInputActionOrigin_XBox360_Reserved10)
	{
		hFont = pScheme->GetFont( "SteamButtons_Xbox", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_XBox360_A:
				return 'A';
			case k_EInputActionOrigin_XBox360_B:
				return 'B';
			case k_EInputActionOrigin_XBox360_X:
				return 'X';
			case k_EInputActionOrigin_XBox360_Y:
				return 'Y';

			case k_EInputActionOrigin_XBox360_LeftBumper:
				return 'K';
			case k_EInputActionOrigin_XBox360_RightBumper:
				return 'k';

			case k_EInputActionOrigin_XBox360_Start:
				return 'W';
			case k_EInputActionOrigin_XBox360_Back:
				return 'V';

			case k_EInputActionOrigin_XBox360_LeftTrigger_Pull:
			case k_EInputActionOrigin_XBox360_LeftTrigger_Click:
				return 'L';
			case k_EInputActionOrigin_XBox360_RightTrigger_Pull:
			case k_EInputActionOrigin_XBox360_RightTrigger_Click:
				return 'l';

			case k_EInputActionOrigin_XBox360_LeftStick_Move:
				return ',';
			case k_EInputActionOrigin_XBox360_LeftStick_Click:
				return '<';
			case k_EInputActionOrigin_XBox360_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_XBox360_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_XBox360_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_XBox360_LeftStick_DPadEast:
				return '|';

			case k_EInputActionOrigin_XBox360_RightStick_Move:
				return '.';
			case k_EInputActionOrigin_XBox360_RightStick_Click:
				return '>';
			case k_EInputActionOrigin_XBox360_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_XBox360_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_XBox360_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_XBox360_RightStick_DPadEast:
				return '?';

			case k_EInputActionOrigin_XBox360_DPad_North:
				return 'F';
			case k_EInputActionOrigin_XBox360_DPad_South:
				return 'G';
			case k_EInputActionOrigin_XBox360_DPad_West:
				return 'D';
			case k_EInputActionOrigin_XBox360_DPad_East:
				return 'E';
		}
	}
	// Xbox One Controller
	else if (eAction >= k_EInputActionOrigin_XBoxOne_A && eAction <= k_EInputActionOrigin_XBoxOne_Reserved10)
	{
		hFont = pScheme->GetFont( "SteamButtons_Xbox", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_XBoxOne_A:
				return 'A';
			case k_EInputActionOrigin_XBoxOne_B:
				return 'B';
			case k_EInputActionOrigin_XBoxOne_X:
				return 'X';
			case k_EInputActionOrigin_XBoxOne_Y:
				return 'Y';

			case k_EInputActionOrigin_XBoxOne_LeftBumper:
				return 'K';
			case k_EInputActionOrigin_XBoxOne_RightBumper:
				return 'k';

			case k_EInputActionOrigin_XBoxOne_Menu:
				return 'w';
			case k_EInputActionOrigin_XBoxOne_View:
				return 'v';

			case k_EInputActionOrigin_XBoxOne_LeftTrigger_Pull:
			case k_EInputActionOrigin_XBoxOne_LeftTrigger_Click:
				return 'L';
			case k_EInputActionOrigin_XBoxOne_RightTrigger_Pull:
			case k_EInputActionOrigin_XBoxOne_RightTrigger_Click:
				return 'l';

			case k_EInputActionOrigin_XBoxOne_LeftStick_Move:
				return ',';
			case k_EInputActionOrigin_XBoxOne_LeftStick_Click:
				return '<';
			case k_EInputActionOrigin_XBoxOne_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_XBoxOne_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_XBoxOne_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_XBoxOne_LeftStick_DPadEast:
				return '|';

			case k_EInputActionOrigin_XBoxOne_RightStick_Move:
				return '.';
			case k_EInputActionOrigin_XBoxOne_RightStick_Click:
				return '>';
			case k_EInputActionOrigin_XBoxOne_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_XBoxOne_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_XBoxOne_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_XBoxOne_RightStick_DPadEast:
				return '?';

			case k_EInputActionOrigin_XBoxOne_DPad_North:
				return 'F';
			case k_EInputActionOrigin_XBoxOne_DPad_South:
				return 'G';
			case k_EInputActionOrigin_XBoxOne_DPad_West:
				return 'D';
			case k_EInputActionOrigin_XBoxOne_DPad_East:
				return 'E';
			case k_EInputActionOrigin_XBoxOne_DPad_Move:
				return 'I';

			case k_EInputActionOrigin_XBoxOne_LeftGrip_Lower:
				return 'N';
			case k_EInputActionOrigin_XBoxOne_LeftGrip_Upper:
				return 'M';
			case k_EInputActionOrigin_XBoxOne_RightGrip_Lower:
				return 'n';
			case k_EInputActionOrigin_XBoxOne_RightGrip_Upper:
				return 'm';

			case k_EInputActionOrigin_XBoxOne_Share:
				return 'u';
		}
	}
	// Switch Controller
	else if (eAction >= k_EInputActionOrigin_Switch_A && eAction <= k_EInputActionOrigin_Switch_Reserved20)
	{
		bool bIsPro = (SteamInput()->GetInputTypeForHandle( m_nControllerHandle ) == k_ESteamInputType_SwitchProController);
		hFont = pScheme->GetFont( "SteamButtons_Switch", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_Switch_A:
				return 'A';
			case k_EInputActionOrigin_Switch_B:
				return 'B';
			case k_EInputActionOrigin_Switch_X:
				return 'X';
			case k_EInputActionOrigin_Switch_Y:
				return 'Y';

			case k_EInputActionOrigin_Switch_LeftBumper:
				return 'K';
			case k_EInputActionOrigin_Switch_RightBumper:
				return 'k';

			case k_EInputActionOrigin_Switch_Plus:
				return '+';
			case k_EInputActionOrigin_Switch_Minus:
				return '-';
			case k_EInputActionOrigin_Switch_Capture:
				return 'w';

			case k_EInputActionOrigin_Switch_LeftTrigger_Pull:
			case k_EInputActionOrigin_Switch_LeftTrigger_Click:
				return 'L';
			case k_EInputActionOrigin_Switch_RightTrigger_Pull:
			case k_EInputActionOrigin_Switch_RightTrigger_Click:
				return 'l';

			case k_EInputActionOrigin_Switch_LeftStick_Move:
				return ',';
			case k_EInputActionOrigin_Switch_LeftStick_Click:
				return '<';
			case k_EInputActionOrigin_Switch_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_Switch_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_Switch_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_Switch_LeftStick_DPadEast:
				return '|';

			case k_EInputActionOrigin_Switch_RightStick_Move:
				return '.';
			case k_EInputActionOrigin_Switch_RightStick_Click:
				return '>';
			case k_EInputActionOrigin_Switch_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_Switch_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_Switch_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_Switch_RightStick_DPadEast:
				return '?';

			// Pro Controller and Joy-Con use different icons
			case k_EInputActionOrigin_Switch_DPad_Move:
				return bIsPro ? 'I' : 'o';
			case k_EInputActionOrigin_Switch_DPad_North:
				return bIsPro ? 'F' : 'r';
			case k_EInputActionOrigin_Switch_DPad_South:
				return bIsPro ? 'G' : 's';
			case k_EInputActionOrigin_Switch_DPad_West:
				return bIsPro ? 'D' : 'p';
			case k_EInputActionOrigin_Switch_DPad_East:
				return bIsPro ? 'E' : 'q';

			case k_EInputActionOrigin_Switch_RightGyro_Move:
			case k_EInputActionOrigin_Switch_LeftGyro_Move:
			case k_EInputActionOrigin_Switch_ProGyro_Move:
				return 'c';
			case k_EInputActionOrigin_Switch_RightGyro_Pitch:
			case k_EInputActionOrigin_Switch_LeftGyro_Pitch:
			case k_EInputActionOrigin_Switch_ProGyro_Pitch:
				return 'f';
			case k_EInputActionOrigin_Switch_RightGyro_Yaw:
			case k_EInputActionOrigin_Switch_LeftGyro_Yaw:
			case k_EInputActionOrigin_Switch_ProGyro_Yaw:
				return 'e';
			case k_EInputActionOrigin_Switch_ProGyro_Roll:
			case k_EInputActionOrigin_Switch_RightGyro_Roll:
			case k_EInputActionOrigin_Switch_LeftGyro_Roll:
				return 'f';

			case k_EInputActionOrigin_Switch_LeftGrip_Lower:
			case k_EInputActionOrigin_Switch_RightGrip_Upper:
				return 'm';
			case k_EInputActionOrigin_Switch_RightGrip_Lower:
			case k_EInputActionOrigin_Switch_LeftGrip_Upper:
				return 'M';
		}
	}
	// PS4 Controller
	else if (eAction >= k_EInputActionOrigin_PS4_X && eAction <= k_EInputActionOrigin_PS4_Reserved10)
	{
		hFont = pScheme->GetFont( "SteamButtons_PS", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_PS4_X:
				return 'A';
			case k_EInputActionOrigin_PS4_Circle:
				return 'B';
			case k_EInputActionOrigin_PS4_Triangle:
				return 'Y';
			case k_EInputActionOrigin_PS4_Square:
				return 'X';

			case k_EInputActionOrigin_PS4_LeftBumper:
				return 'K';
			case k_EInputActionOrigin_PS4_RightBumper:
				return 'k';

			case k_EInputActionOrigin_PS4_Options:
				return 'w';
			case k_EInputActionOrigin_PS4_Share:
				return 'z';

			case k_EInputActionOrigin_PS4_LeftPad_Touch:
				return '(';
			case k_EInputActionOrigin_PS4_LeftPad_Swipe:
				return '[';
			case k_EInputActionOrigin_PS4_LeftPad_Click:
				return '{';
			case k_EInputActionOrigin_PS4_LeftPad_DPadNorth:
				return 'O';
			case k_EInputActionOrigin_PS4_LeftPad_DPadSouth:
				return 'P';
			case k_EInputActionOrigin_PS4_LeftPad_DPadWest:
				return 'Q';
			case k_EInputActionOrigin_PS4_LeftPad_DPadEast:
				return 'R';

			case k_EInputActionOrigin_PS4_RightPad_Touch:
				return ')';
			case k_EInputActionOrigin_PS4_RightPad_Swipe:
				return ']';
			case k_EInputActionOrigin_PS4_RightPad_Click:
				return '}';
			case k_EInputActionOrigin_PS4_RightPad_DPadNorth:
				return 'o';
			case k_EInputActionOrigin_PS4_RightPad_DPadSouth:
				return 'p';
			case k_EInputActionOrigin_PS4_RightPad_DPadWest:
				return 'q';
			case k_EInputActionOrigin_PS4_RightPad_DPadEast:
				return 'r';

			case k_EInputActionOrigin_PS4_CenterPad_Touch:
			case k_EInputActionOrigin_PS4_CenterPad_Swipe:
			case k_EInputActionOrigin_PS4_CenterPad_Click:
			case k_EInputActionOrigin_PS4_CenterPad_DPadNorth:
			case k_EInputActionOrigin_PS4_CenterPad_DPadSouth:
			case k_EInputActionOrigin_PS4_CenterPad_DPadWest:
			case k_EInputActionOrigin_PS4_CenterPad_DPadEast:
				return '^';

			case k_EInputActionOrigin_PS4_LeftTrigger_Pull:
			case k_EInputActionOrigin_PS4_LeftTrigger_Click:
				return 'L';
			case k_EInputActionOrigin_PS4_RightTrigger_Pull:
			case k_EInputActionOrigin_PS4_RightTrigger_Click:
				return 'l';

			case k_EInputActionOrigin_PS4_LeftStick_Move:
				return ',';
			case k_EInputActionOrigin_PS4_LeftStick_Click:
				return '<';
			case k_EInputActionOrigin_PS4_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_PS4_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_PS4_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_PS4_LeftStick_DPadEast:
				return '|';

			case k_EInputActionOrigin_PS4_RightStick_Move:
				return '.';
			case k_EInputActionOrigin_PS4_RightStick_Click:
				return '>';
			case k_EInputActionOrigin_PS4_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_PS4_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_PS4_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_PS4_RightStick_DPadEast:
				return '?';

			case k_EInputActionOrigin_PS4_DPad_North:
				return 'F';
			case k_EInputActionOrigin_PS4_DPad_South:
				return 'G';
			case k_EInputActionOrigin_PS4_DPad_West:
				return 'D';
			case k_EInputActionOrigin_PS4_DPad_East:
				return 'E';
			case k_EInputActionOrigin_PS4_DPad_Move:
				return 'C';
		}
	}
	// PS5 Controller
	else if (eAction >= k_EInputActionOrigin_PS5_X && eAction <= k_EInputActionOrigin_PS5_Reserved20)
	{
		hFont = pScheme->GetFont( "SteamButtons_PS", true );
		switch (eAction)
		{
			case k_EInputActionOrigin_PS5_X:
				return 'A';
			case k_EInputActionOrigin_PS5_Circle:
				return 'B';
			case k_EInputActionOrigin_PS5_Triangle:
				return 'Y';
			case k_EInputActionOrigin_PS5_Square:
				return 'X';

			case k_EInputActionOrigin_PS5_LeftBumper:
				return 'M';
			case k_EInputActionOrigin_PS5_RightBumper:
				return 'm';

			case k_EInputActionOrigin_PS5_Option:
				return 'W';
			case k_EInputActionOrigin_PS5_Create:
				return 'Z';

			case k_EInputActionOrigin_PS5_LeftPad_Touch:
			case k_EInputActionOrigin_PS5_LeftPad_Swipe:
				return 'g';
			case k_EInputActionOrigin_PS5_LeftPad_Click:
				return 'i';
			case k_EInputActionOrigin_PS5_LeftPad_DPadNorth:
				return 'S';
			case k_EInputActionOrigin_PS5_LeftPad_DPadSouth:
				return 'T';
			case k_EInputActionOrigin_PS5_LeftPad_DPadWest:
				return 'U';
			case k_EInputActionOrigin_PS5_LeftPad_DPadEast:
				return 'V';

			case k_EInputActionOrigin_PS5_RightPad_Touch:
			case k_EInputActionOrigin_PS5_RightPad_Swipe:
				return 'h';
			case k_EInputActionOrigin_PS5_RightPad_Click:
				return 'j';
			case k_EInputActionOrigin_PS5_RightPad_DPadNorth:
				return 's';
			case k_EInputActionOrigin_PS5_RightPad_DPadSouth:
				return 't';
			case k_EInputActionOrigin_PS5_RightPad_DPadWest:
				return 'u';
			case k_EInputActionOrigin_PS5_RightPad_DPadEast:
				return 'v';

			case k_EInputActionOrigin_PS5_CenterPad_Touch:
			case k_EInputActionOrigin_PS5_CenterPad_Swipe:
			case k_EInputActionOrigin_PS5_CenterPad_Click:
			case k_EInputActionOrigin_PS5_CenterPad_DPadNorth:
			case k_EInputActionOrigin_PS5_CenterPad_DPadSouth:
			case k_EInputActionOrigin_PS5_CenterPad_DPadWest:
			case k_EInputActionOrigin_PS5_CenterPad_DPadEast:
				return '`';

			case k_EInputActionOrigin_PS5_LeftTrigger_Pull:
			case k_EInputActionOrigin_PS5_LeftTrigger_Click:
				return 'N';
			case k_EInputActionOrigin_PS5_RightTrigger_Pull:
			case k_EInputActionOrigin_PS5_RightTrigger_Click:
				return 'n';

			case k_EInputActionOrigin_PS5_LeftStick_Move:
				return ',';
			case k_EInputActionOrigin_PS5_LeftStick_Click:
				return '<';
			case k_EInputActionOrigin_PS5_LeftStick_DPadNorth:
				return ';';
			case k_EInputActionOrigin_PS5_LeftStick_DPadSouth:
				return ':';
			case k_EInputActionOrigin_PS5_LeftStick_DPadWest:
				return '\\';
			case k_EInputActionOrigin_PS5_LeftStick_DPadEast:
				return '|';

			case k_EInputActionOrigin_PS5_RightStick_Move:
				return '.';
			case k_EInputActionOrigin_PS5_RightStick_Click:
				return '>';
			case k_EInputActionOrigin_PS5_RightStick_DPadNorth:
				return '\'';
			case k_EInputActionOrigin_PS5_RightStick_DPadSouth:
				return '\"';
			case k_EInputActionOrigin_PS5_RightStick_DPadWest:
				return '_';
			case k_EInputActionOrigin_PS5_RightStick_DPadEast:
				return '?';

			case k_EInputActionOrigin_PS5_DPad_North:
				return 'F';
			case k_EInputActionOrigin_PS5_DPad_South:
				return 'G';
			case k_EInputActionOrigin_PS5_DPad_West:
				return 'D';
			case k_EInputActionOrigin_PS5_DPad_East:
				return 'E';
			case k_EInputActionOrigin_PS5_DPad_Move:
				return 'C';
				
			case k_EInputActionOrigin_PS5_Gyro_Move:
				return 'c';
			case k_EInputActionOrigin_PS5_Gyro_Pitch:
				return 'f';
			case k_EInputActionOrigin_PS5_Gyro_Yaw:
				return 'e';
			case k_EInputActionOrigin_PS5_Gyro_Roll:
				return 'f';
		}
	}

	return 0;
}

inline const char *CSource2013SteamInput::LookupDescriptionForActionOrigin( EInputActionOrigin eAction )
{
	return SteamInput()->GetStringForActionOrigin( eAction );
}

//-----------------------------------------------------------------------------
// Purpose: Steam Input variation of UTIL_ReplaceKeyBindings which replaces key
//			bindings with corresponding glyphs.
//-----------------------------------------------------------------------------
void UTIL_ReplaceKeyBindingsWithGlyphs( const wchar_t *inbuf, int inbufsizebytes, OUT_Z_BYTECAP(outbufsizebytes) wchar_t *outbuf, int outbufsizebytes, vgui::HFont &hFont, vgui::IScheme *pScheme )
{
	Assert( outbufsizebytes >= sizeof(outbuf[0]) );
	// copy to a new buf if there are vars
	outbuf[0]=0;

	if ( !inbuf || !inbuf[0] )
		return;

	int pos = 0;
	const wchar_t *inbufend = NULL;
	if ( inbufsizebytes > 0 )
	{
		inbufend = inbuf + ( inbufsizebytes / 2 );
	}

	while( inbuf != inbufend && *inbuf != 0 )
	{
		// check for variables
		if ( *inbuf == '%' )
		{
			++inbuf;

			const wchar_t *end = wcschr( inbuf, '%' );
			if ( end && ( end != inbuf ) ) // make sure we handle %% in the string, which should be treated in the output as %
			{
				wchar_t token[64];
				wcsncpy( token, inbuf, end - inbuf );
				token[end - inbuf] = 0;

				inbuf += end - inbuf;

				// lookup key names
				char binding[64];
				g_pVGuiLocalize->ConvertUnicodeToANSI( token, binding, sizeof(binding) );

				char key[16];
				g_pSteamInput->GetGlyphFontForCommand( binding, key, sizeof( key ), hFont, pScheme );

				g_pVGuiLocalize->ConvertANSIToUnicode( key, token, sizeof(token) );

				outbuf[pos] = '\0';
				wcscat( outbuf, token );
				pos += wcslen(token);
			}
			else
			{
				outbuf[pos] = *inbuf;
				++pos;
			}
		}
		else
		{
			outbuf[pos] = *inbuf;
			++pos;
		}

		++inbuf;
	}

	outbuf[pos] = '\0';
}

void CSource2013SteamInput::LoadHintRemap( const char *pszFileName )
{
	KeyValues *pKV = new KeyValues("HintRemap");
	if ( pKV->LoadFromFile( filesystem, pszFileName ) )
	{
		// Parse each hint to remap
		KeyValues *pKVHint = pKV->GetFirstSubKey();
		while ( pKVHint )
		{
			// Parse hint remaps
			KeyValues *pKVRemappedHint = pKVHint->GetFirstSubKey();
			while ( pKVRemappedHint )
			{
				int i = m_HintRemaps.AddToTail();
			
				m_HintRemaps[i].pszOldHint = pKVHint->GetName();
				m_HintRemaps[i].pszNewHint = pKVRemappedHint->GetName();

				// Parse remap conditions
				KeyValues *pKVRemapCond = pKVRemappedHint->GetFirstSubKey();
				while ( pKVRemapCond )
				{
					int i2 = m_HintRemaps[i].nRemapConds.AddToTail();

					const char *pszParam = pKVRemapCond->GetString();
					if (pszParam[0] == '!')
					{
						m_HintRemaps[i].nRemapConds[i2].bNot = true;
						pszParam++;
					}

					Q_strncpy( m_HintRemaps[i].nRemapConds[i2].szParam, pszParam, sizeof( m_HintRemaps[i].nRemapConds[i2].szParam ) );

					if (FStrEq( pKVRemapCond->GetName(), "if_input_type" ))
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_INPUT_TYPE;
					else if (FStrEq( pKVRemapCond->GetName(), "if_action_bound" ))
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_ACTION_BOUND;
					else
						m_HintRemaps[i].nRemapConds[i2].iType = HINTREMAPCOND_NONE;

					pKVRemapCond = pKVRemapCond->GetNextKey();
				}

				pKVRemappedHint = pKVRemappedHint->GetNextKey();
			}

			pKVHint = pKVHint->GetNextKey();
		}
	}
	pKV->deleteThis();
}

void CSource2013SteamInput::RemapHudHint( const char **pszInputHint )
{
	if (!si_hintremap.GetBool())
		return;

	int iRemap = -1;

	for (int i = 0; i < m_HintRemaps.Count(); i++)
	{
		if (!FStrEq( *pszInputHint, m_HintRemaps[i].pszOldHint ))
			continue;

		// If we've already selected a remap, ignore ones without conditions
		if (iRemap != -1 && m_HintRemaps[i].nRemapConds.Count() <= 0)
			continue;

		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Testing hint remap for %s to %s...\n", *pszInputHint, m_HintRemaps[i].pszNewHint );

		bool bPass = true;

		for (int i2 = 0; i2 < m_HintRemaps[i].nRemapConds.Count(); i2++)
		{
			if (si_print_hintremap.GetBool())
				Msg( "	Hint Remap: Testing remap condition %i (param %s)\n", m_HintRemaps[i].nRemapConds[i2].iType, m_HintRemaps[i].nRemapConds[i2].szParam );

			switch (m_HintRemaps[i].nRemapConds[i2].iType)
			{
				case HINTREMAPCOND_INPUT_TYPE:
				{
					ESteamInputType inputType = SteamInput()->GetInputTypeForHandle( m_nControllerHandle );
					bPass = FStrEq( IdentifyControllerParam( inputType ), m_HintRemaps[i].nRemapConds[i2].szParam );
				} break;

				case HINTREMAPCOND_ACTION_BOUND:
				{
					CUtlVector<EInputActionOrigin> actionOrigins;
					GetInputActionOriginsForCommand( m_HintRemaps[i].nRemapConds[i2].szParam, actionOrigins );
					bPass = (actionOrigins.Count() > 0);
				} break;
			}

			if (m_HintRemaps[i].nRemapConds[i2].bNot)
				bPass = !bPass;

			if (!bPass)
				break;
		}

		if (bPass)
		{
			if (si_print_hintremap.GetBool())
				Msg( "Hint Remap: Hint remap for %s to %s succeeded\n", *pszInputHint, m_HintRemaps[i].pszNewHint );

			iRemap = i;
		}
		else if (si_print_hintremap.GetBool())
		{
			Msg( "Hint Remap: Hint remap for %s to %s did not pass\n", *pszInputHint, m_HintRemaps[i].pszNewHint );
		}
	}

	if (iRemap != -1)
	{
		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Remapping hint %s to %s\n", *pszInputHint, m_HintRemaps[iRemap].pszNewHint );

		*pszInputHint = m_HintRemaps[iRemap].pszNewHint;
	}
	else
	{
		if (si_print_hintremap.GetBool())
			Msg( "Hint Remap: Didn't find a hint for %s to remap to\n", *pszInputHint );
	}
}
