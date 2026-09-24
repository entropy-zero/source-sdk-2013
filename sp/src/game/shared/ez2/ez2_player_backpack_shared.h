//=============================================================================//
//
// Purpose: Generic Backpack
// 
// Author: Blixibon
//
//=============================================================================//

#ifndef EZ2_PLAYER_BACKPACK_SHARED_H
#define EZ2_PLAYER_BACKPACK_SHARED_H
#ifdef _WIN32
#pragma once
#endif

// The maximum technically supported. Actual allowed value is sk_backpack_max, which cannot exceed this
#define MAX_BACKPACK_ITEMS		8

#define IN_BACKPACK	IN_WALK

#define BACKPACK_HINT_COOLDOWN	120.0f		// Can only happen once in 2 minutes

#endif // C_HUD_STEALTHALERT_H