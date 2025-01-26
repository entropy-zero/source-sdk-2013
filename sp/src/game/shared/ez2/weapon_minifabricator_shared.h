//=============================================================================//
//
// Purpose:		Base mini fabricator values.
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef WEAPON_MINIFABRICATOR_SHARED_H
#define WEAPON_MINIFABRICATOR_SHARED_H
#ifdef _WIN32
#pragma once
#endif

struct MiniFabricatorAugmentation_t
{
	// Used mainly for debugging
	const char *pszName;

	enum
	{
		FILTER_CLASSNAME,	// Direct classname
		FILTER_PROPINT,		// CBreakableProp interation
	};

	const char *pszFilter;
	int nFilterType;

	string_t pszModel;
	int nSkin;

	enum
	{
		IMPLANT_FOLLOW_FROM_TRACE,		// Puts implant at trace position and parents it (default)
		IMPLANT_FOLLOW_ATTACHMENT,		// Parents implant to specific attachment
		IMPLANT_FOLLOW_BONEMERGE,		// Bonemerges the implant with the target
	};

	const char *pszAttachment;
	int nFollowType;

	// How much to push the entity when applying or removing the augmentation
	float flPushScale = 2.0f;

	const char *pszVScriptFile;

	int nCost;
};

//enum MiniFabricatorAbilities_t
//{
//	MINIFABRICATOR_ABILITY_CONNECT,		// Connects two objects
//	MINIFABRICATOR_ABILITY_AUGMENT,		// Augments something
//
//	NUM_MINIFABRICATOR_ABILITIES,
//	LAST_MINIFABRICATOR_ABILITY = NUM_MINIFABRICATOR_ABILITIES-1,
//};

#endif // WEAPON_MINIFABRICATOR_SHARED_H
