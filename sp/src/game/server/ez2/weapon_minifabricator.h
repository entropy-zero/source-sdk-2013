//=============================================================================//
//
// Purpose:		Miniature fabricator which can augment things on the spot.
//
// Author:		Blixibon
//
//=============================================================================//

#include "basehlcombatweapon.h"
#include "ez2/weapon_minifabricator_shared.h"
#include "props.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWeapon_MiniFabricator : public CBaseHLCombatWeapon
{
public:
	DECLARE_CLASS( CWeapon_MiniFabricator, CBaseHLCombatWeapon );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	void	Precache( void );

	void			ItemPreFrame( void );
	CBaseEntity*	FindTarget( CBaseCombatCharacter *pOwner, Vector &vecEndPos, Vector &vecNormal );
	void			CheckForTarget( CBaseCombatCharacter *pOwner );

	void	PrimaryAttack( void );
	void	SecondaryAttack( void );

	bool	UsesClipsForAmmo1( void ) const { return false; } // Don't use clips!

	bool	CanSwitchToWhileEmpty() { return true; } // Can switch to while empty

	bool	ShouldDisplayHUDHint() { return true; } // This weapon will need some explanation

private:

	EHANDLE		m_hTargetEnt;	// The entity the player is currently looking at

	const MiniFabricatorAugmentation_t *m_pTargetAugmentation;	// The augmentation which can be used by the target entity (if valid)
	Vector		m_vecTargetEndPos;	// The trace end position for the target
	Vector		m_vecTargetNormal;	// The trace normal for the target

	float		m_flNextTargetCheckTime;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CFabricatorImplant : public CDynamicProp
{
	DECLARE_CLASS( CFabricatorImplant, CDynamicProp );
	DECLARE_DATADESC();
public:
	CFabricatorImplant();
	~CFabricatorImplant();

	void							Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info );

	void							InitFabricationData( CWeapon_MiniFabricator *pFabricator, CBaseCombatCharacter *pOwner, const MiniFabricatorAugmentation_t *pAugmentation );

	CWeapon_MiniFabricator*			GetFabricator() const { return m_hFabricator; }
	CBaseCombatCharacter*			GetFabricatorOwner() const { return m_hFabricatorOwner; }
	const char*						GetAugmentationName() const { return STRING( m_iszAugmentationName ); }

private:
	CHandle<CWeapon_MiniFabricator>		m_hFabricator;
	CHandle<CBaseCombatCharacter>		m_hFabricatorOwner;
	string_t							m_iszAugmentationName;
};