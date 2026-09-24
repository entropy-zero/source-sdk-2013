//=============================================================================//
//
// Purpose:		Gun-attached laser that can compromise cloaked targets.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_weaponlaser.h"
#include "clienteffectprecachesystem.h"

//-----------------------------------------------------------------------------

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectWeaponLaser )
	CLIENTEFFECT_MATERIAL( "sprites/animglow01_animproxy" )
CLIENTEFFECT_REGISTER_END()
