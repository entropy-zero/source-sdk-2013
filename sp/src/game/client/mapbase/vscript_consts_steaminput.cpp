//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: VScript constants and enums used by Steam Input.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "steam/isteaminput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//=============================================================================
//=============================================================================

BEGIN_SCRIPTENUM( SteamInputType, "Types of controllers that correspond to steaminput.GetControllerType." )

	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_Unknown,				"Unknown", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_SteamController,		"SteamController", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_XBox360Controller,	"XBox360Controller", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_XBoxOneController,	"XBoxOneController", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_GenericGamepad,		"GenericGamepad", "DirectInput controllers" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_PS4Controller,		"PS4Controller", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_AppleMFiController,	"AppleMFiController", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_AndroidController,	"AndroidController", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_SwitchJoyConPair,		"SwitchJoyConPair", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_SwitchJoyConSingle,	"SwitchJoyConSingle", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_SwitchProController,	"SwitchProController", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_MobileTouch,			"MobileTouch", "Steam Link App On-screen Virtual Controller" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_PS3Controller,		"PS3Controller", "Currently uses PS4 Origins" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_PS5Controller,		"PS5Controller", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_Count,				"Count", "" )
	DEFINE_ENUMCONST_NAMED( k_ESteamInputType_MaximumPossibleValue,	"MaximumPossibleValue", "" )

END_SCRIPTENUM();

//=============================================================================
//=============================================================================

void RegisterSteamInputConstants()
{
	ScriptRegisterConstant( g_pScriptVM, STEAM_INPUT_MAX_COUNT, "Maximum number of controllers that can be connected to Steam Input." );
}
