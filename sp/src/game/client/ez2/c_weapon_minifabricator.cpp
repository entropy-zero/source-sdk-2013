//=============================================================================//
//
// Purpose:		Miniature fabricator which can augment things on the spot.
// 
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "c_weapon_minifabricator.h"



IMPLEMENT_CLIENTCLASS_DT( C_Weapon_MiniFabricator, DT_Weapon_MiniFabricator, CWeapon_MiniFabricator )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_Weapon_MiniFabricator )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_minifabricator, C_Weapon_MiniFabricator )

//-----------------------------------------------------------------------------
// Purpose: Return true if this weapon can be selected via the weapon selection
//-----------------------------------------------------------------------------
bool C_Weapon_MiniFabricator::CanBeSelected( void )
{
	if ( !VisibleInWeaponSelection() )
		return false;

	// Can always be selected regardless of ammo
	return true;
}
