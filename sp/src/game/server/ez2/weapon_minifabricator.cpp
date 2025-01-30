//=============================================================================//
//
// Purpose:		Miniature fabricator which can augment objects in the world.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "weapon_minifabricator.h"
#include "IEffects.h"
#include "physics_prop_ragdoll.h"
#include "decals.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_minifabricator_range( "sk_minifabricator_range", "96" );

ConVar	sk_minifabricator_assimilate_min_mass( "sk_minifabricator_assimilate_min_mass", "5" );
ConVar	sk_minifabricator_assimilate_max_mass( "sk_minifabricator_assimilate_max_mass", "40" );

ConVar	sk_minifabricator_income_prop_ratio( "sk_minifabricator_income_prop_ratio", "0.1" );
ConVar	sk_minifabricator_income_corpse_ratio( "sk_minifabricator_income_corpse_ratio", "0.04" );
ConVar	sk_minifabricator_income_weapon( "sk_minifabricator_income_weapon", "3" );
ConVar	sk_minifabricator_income_item( "sk_minifabricator_income_item", "2" );

ConVar	g_debug_minifabricator( "g_debug_minifabricator", "0" );

#define MINIFABRICATOR_AUGMENTATIONS_SCRIPT "scripts/minifabricator_augmentations.txt"

CUtlVector<CFabricatorImplant*> g_FabricatorImplantList;

//-----------------------------------------------------------------------------
// Purpose: Ensures mini fabricator augmentations are loaded and precached when they are needed.
// This is based on the Xen Grenade recipe manager, but it's much simpler.
//-----------------------------------------------------------------------------
class CMiniFabricatorAugmentationManager : public CAutoGameSystem
{
public:
	CMiniFabricatorAugmentationManager() : CAutoGameSystem( "CMiniFabricatorAugmentationManager" )
	{
	}

	virtual bool Init()
	{
		return true;
	}

	virtual void LevelInitPreEntity()
	{
		// Unless cheats are on, assume the fabricator doesn't exist by default
		if (sv_cheats->GetBool())
		{
			m_bFabricatorExists = true;
			m_pszFabricreator = "sv_cheats";
		}
		else
		{
			m_bFabricatorExists = false;
			m_pszFabricreator = "Unknown"; // Default value
		}

		m_bFabricatorPrecached = false;

		m_bPrecachedLate = false;
	}

	virtual void LevelInitPostEntity()
	{
		// If Xen exists, precache Xen
		if (m_bFabricatorExists)
		{
			PrecacheMiniFabricatorAugmentations();
			m_bFabricatorPrecached = true;
		}
	}

	virtual void LevelShutdownPreEntity()
	{
	}

	virtual void LevelShutdownPostEntity()
	{
	}

	virtual void OnRestore()
	{
		if (m_bFabricatorExists)
		{
			PrecacheMiniFabricatorAugmentations();
		}
	}

	//------------------------------------------------------------------------------------

	inline bool FabricatorExists() const { return m_bFabricatorExists; }

	inline bool PrecachedLate() const { return m_bPrecachedLate; }
	inline const char *GetFabricatorCreator() const { return m_pszFabricreator; }

	// Affirms the existence of the fabricator
	void VerifyAugmentationManager( const char *pszActivator )
	{
		if (!m_bFabricatorExists)
		{
			Msg( "Mini-fabricator augmentations manager activated\n" );
			m_pszFabricreator = pszActivator;
		}

		m_bFabricatorExists = true;

		if (!m_bFabricatorPrecached && gpGlobals->curtime > 2.0f)
		{
			PrecacheMiniFabricatorAugmentations();

			Msg( "Late precache of mini-fabricator augmentations\n" );

			m_bPrecachedLate = true;
			m_bFabricatorPrecached = true;
		}
	}

	void PrecacheMiniFabricatorAugmentations();

	const MiniFabricatorAugmentation_t *FindAugmentationForEntity( CBaseEntity *pTarget, CWeapon_MiniFabricator *pFabricator, CBaseCombatCharacter *pOwner );
	const MiniFabricatorAugmentation_t *FindAugmentationByName( const char *pszName );

protected:
	bool m_bFabricatorExists;
	bool m_bFabricatorPrecached;

	// The one who precached the fabricator augmentations
	const char *m_pszFabricreator;

	bool m_bPrecachedLate;

	CUtlVector<MiniFabricatorAugmentation_t> m_Augmentations;
};

CMiniFabricatorAugmentationManager	g_MiniFabricatorAugmentationManager;

//-----------------------------------------------------------------------------
// Purpose: Debug commands
//-----------------------------------------------------------------------------
CON_COMMAND( minifabricator_print_state, "Prints the augmentation manager state." )
{
	char szMsg[512];
	if (g_MiniFabricatorAugmentationManager.FabricatorExists())
	{
		Q_strncpy( szMsg, "Fabricator exists.\n{\n", sizeof( szMsg ) );

		Q_snprintf( szMsg, sizeof( szMsg ), "%s	Precacher: %s\n", szMsg, g_MiniFabricatorAugmentationManager.GetFabricatorCreator() );
		Q_snprintf( szMsg, sizeof( szMsg ), "%s	Precached late: %s\n", szMsg, g_MiniFabricatorAugmentationManager.PrecachedLate() ? "Yes" : "No" );

		Q_snprintf( szMsg, sizeof( szMsg ), "%s}\n", szMsg );
	}
	else
	{
		Q_strncpy( szMsg, "Fabricator does not exist.\n", sizeof( szMsg ) );
	}

	Msg( "%s", szMsg );
}

//-----------------------------------------------------------------------------
// Purpose: Precaches mini fabricator augmentations
//-----------------------------------------------------------------------------
void CMiniFabricatorAugmentationManager::PrecacheMiniFabricatorAugmentations( void )
{
	if ( m_bFabricatorPrecached )
		return;

	KeyValues *pAugmentationsFile = new KeyValues( "MiniFabricatorAugmentations" );
	if (pAugmentationsFile->LoadFromFile( filesystem, MINIFABRICATOR_AUGMENTATIONS_SCRIPT ))
	{
		for (KeyValues *pAugmentation = pAugmentationsFile->GetFirstSubKey(); pAugmentation != NULL; pAugmentation = pAugmentation->GetNextKey())
		{
			int i = m_Augmentations.AddToTail();
			m_Augmentations[i].pszName = STRING( AllocPooledString( pAugmentation->GetName() ) );

			for (KeyValues *pSubKey = pAugmentation->GetFirstSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextKey())
			{
				const char *pszKeyName = pSubKey->GetName();
				if (V_strnicmp( pszKeyName, "filter_", 7 ) == 0)
				{
					m_Augmentations[i].pszFilter = STRING( AllocPooledString( pSubKey->GetString() ) );
					pszKeyName += 7;

					// Determine filter type
					if (FStrEq( pszKeyName, "classname" ))
						m_Augmentations[i].nFilterType = MiniFabricatorAugmentation_t::FILTER_CLASSNAME;
					else if (FStrEq( pszKeyName, "propinter" ))
						m_Augmentations[i].nFilterType = MiniFabricatorAugmentation_t::FILTER_PROPINT;
				}
				else if (FStrEq( pszKeyName, "model" ))
				{
					m_Augmentations[i].pszModel = AllocPooledString( pSubKey->GetString() );

					// Precache the model
					UTIL_PrecacheOther( "prop_fabricator_implant", STRING( m_Augmentations[i].pszModel ) );
				}
				else if (FStrEq( pszKeyName, "skin" ))
				{
					m_Augmentations[i].nSkin = pSubKey->GetInt();
				}
				else if (FStrEq( pszKeyName, "attachment" ))
				{
					m_Augmentations[i].pszAttachment = STRING( AllocPooledString( pSubKey->GetString() ) );

					if (m_Augmentations[i].nFollowType != MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_BONEMERGE)
						m_Augmentations[i].nFollowType = MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_ATTACHMENT;
				}
				else if (FStrEq( pszKeyName, "bonemerge" ) && pSubKey->GetBool() == true)
				{
					m_Augmentations[i].nFollowType = MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_BONEMERGE;
				}
				else if (FStrEq( pszKeyName, "push_scale" ))
				{
					m_Augmentations[i].flPushScale = pSubKey->GetFloat();
				}
				else if (FStrEq( pszKeyName, "vscript_file" ))
				{
					m_Augmentations[i].pszVScriptFile = STRING( AllocPooledString( pSubKey->GetString() ) );
				}
				else if (FStrEq( pszKeyName, "cost" ))
				{
					m_Augmentations[i].nCost = pSubKey->GetInt();
				}
			}
		}
	}

	pAugmentationsFile->deleteThis();

	if (g_debug_minifabricator.GetBool())
	{
		Msg( "Precached %i augmentations\n", m_Augmentations.Count() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds a matching mini fabricator augmentation
//-----------------------------------------------------------------------------
const MiniFabricatorAugmentation_t *CMiniFabricatorAugmentationManager::FindAugmentationForEntity( CBaseEntity *pTarget, CWeapon_MiniFabricator *pFabricator, CBaseCombatCharacter *pOwner )
{
	if (pTarget == NULL)
		return NULL;

	for (int i = 0; i < m_Augmentations.Count(); i++)
	{
		bool bPassed = false;
		switch (m_Augmentations[i].nFilterType)
		{
			case MiniFabricatorAugmentation_t::FILTER_CLASSNAME:
				bPassed = pTarget->ClassMatches( m_Augmentations[i].pszFilter );
				break;

			case MiniFabricatorAugmentation_t::FILTER_PROPINT:
			{
				if (V_strncmp( pTarget->GetClassname(), "prop_", 5 ) != 0)
					break;

				// TODO: Would all classes starting with "prop_" be descended from CBreakableProp?
				CBreakableProp *pProp = dynamic_cast<CBreakableProp *>(pTarget);
				if (pProp)
				{
					if (FStrEq( m_Augmentations[i].pszFilter, "explosive" ))
					{
						bPassed = pProp->HasInteraction( PROPINTER_PHYSGUN_BREAK_EXPLODE ) ||
							pProp->HasInteraction( PROPINTER_FIRE_FLAMMABLE ) ||
							pProp->GetExplosiveRadius() > 0 || pProp->GetExplosiveDamage() > 0;
					}
				}
				break;
			}
		}

		if (bPassed)
		{
			if (g_debug_minifabricator.GetBool())
			{
				Msg( "Found augmentation %s for %s\n", m_Augmentations[i].pszName, pTarget->GetDebugName() );
			}
			return &m_Augmentations[i];
		}
	}

	if (g_debug_minifabricator.GetBool())
	{
		Msg( "No augmentation found for %s\n", pTarget->GetDebugName() );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Finds a mini fabricator augmentation by name
//-----------------------------------------------------------------------------
const MiniFabricatorAugmentation_t *CMiniFabricatorAugmentationManager::FindAugmentationByName( const char *pszName )
{
	for (int i = 0; i < m_Augmentations.Count(); i++)
	{
		if (FStrEq( m_Augmentations[i].pszName, pszName ))
			return &m_Augmentations[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST( CWeapon_MiniFabricator, DT_Weapon_MiniFabricator )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( weapon_minifabricator, CWeapon_MiniFabricator );
//PRECACHE_WEAPON_REGISTER( weapon_minifabricator );

BEGIN_DATADESC( CWeapon_MiniFabricator )
	DEFINE_FIELD( m_hTargetEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecTargetEndPos, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecTargetNormal, FIELD_VECTOR ),

	DEFINE_FIELD( m_flNextTargetCheckTime, FIELD_TIME ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Precache
//-----------------------------------------------------------------------------
void CWeapon_MiniFabricator::Precache( void )
{
	BaseClass::Precache();

	g_MiniFabricatorAugmentationManager.VerifyAugmentationManager( GetClassname() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeapon_MiniFabricator::ItemPreFrame( void )
{
	BaseClass::ItemPreFrame();

	if (m_flNextTargetCheckTime < gpGlobals->curtime && GetOwner())
	{
		// Check for a target entity every interval
		CheckForTarget( GetOwner() );
		m_flNextTargetCheckTime = gpGlobals->curtime + 0.25f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the target entity
//-----------------------------------------------------------------------------
CBaseEntity *CWeapon_MiniFabricator::FindTarget( CBaseCombatCharacter *pOwner, Vector &vecEndPos, Vector &vecNormal )
{
	trace_t tr;
	CTraceFilterSkipTwoEntities traceFilter( this, pOwner, COLLISION_GROUP_NONE );
	UTIL_TraceLine( pOwner->EyePosition(), pOwner->EyePosition() + (pOwner->EyeDirection3D() * sk_minifabricator_range.GetFloat()), MASK_SHOT_HULL, &traceFilter, &tr );

	if (tr.m_pEnt)
	{
		if (tr.m_pEnt->IsWorld())
			return NULL;

		// For now, pretend "displacement impossible" also means it's impossible to augment
		if (tr.m_pEnt->IsDisplacementImpossible())
			return NULL;
	}
	else
		return NULL;

	vecEndPos = tr.endpos;
	vecNormal = tr.plane.normal;
	return tr.m_pEnt;
}

//-----------------------------------------------------------------------------
// Purpose: Assigns the target entity and augmentation
//-----------------------------------------------------------------------------
void CWeapon_MiniFabricator::CheckForTarget( CBaseCombatCharacter *pOwner )
{
	CBaseEntity *pTarget = FindTarget( pOwner, m_vecTargetEndPos, m_vecTargetNormal );

	// Skip augmentation check if it's the same target we already have
	if (m_hTargetEnt != pTarget)
	{
		// Get appropriate augmentation
		m_pTargetAugmentation = g_MiniFabricatorAugmentationManager.FindAugmentationForEntity( pTarget, this, pOwner );
	}

	m_hTargetEnt = pTarget;
}

//-----------------------------------------------------------------------------
// Purpose: Main attack
//-----------------------------------------------------------------------------
void CWeapon_MiniFabricator::PrimaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	pPlayer->m_flNextAttack = gpGlobals->curtime + 1.0f;

	// Get our target entity
	if (!m_hTargetEnt)
		CheckForTarget( pPlayer );

	if (!m_hTargetEnt || !m_pTargetAugmentation || m_pTargetAugmentation->nCost > pPlayer->GetAmmoCount( m_iPrimaryAmmoType ))
	{
		if (g_debug_minifabricator.GetBool())
		{
			if (!m_hTargetEnt)
				Msg( "Mini fabricator: No target\n" );
			else if (!m_pTargetAugmentation)
				Msg( "Mini fabricator: No target augmentation\n" );
			else if (m_pTargetAugmentation->nCost > pPlayer->GetAmmoCount( m_iPrimaryAmmoType ))
				Msg( "Mini fabricator: Too expensive\n" );
		}

		WeaponSound( EMPTY );
		return;
	}

	// Make sure there's not already an augmentation
	for (int i = 0; i < g_FabricatorImplantList.Count(); i++)
	{
		if (g_FabricatorImplantList[i] && g_FabricatorImplantList[i]->GetOwnerEntity() == m_hTargetEnt)
		{
			if (g_debug_minifabricator.GetBool())
				Msg( "Mini fabricator: Target already augmented\n" );
			WeaponSound( EMPTY );
			return;
		}
	}

	// Augment the thing
	{
		Vector vecImplantOrigin = vec3_origin;
		QAngle angImplantAngles = vec3_angle;

		// Create the implant
		CFabricatorImplant *pImplant = static_cast<CFabricatorImplant*>(CreateNoSpawn( "prop_fabricator_implant", vecImplantOrigin, angImplantAngles, m_hTargetEnt ));
		if (pImplant)
		{
			pImplant->SetModelName( m_pTargetAugmentation->pszModel );
			pImplant->m_nSkin = m_pTargetAugmentation->nSkin;

			switch (m_pTargetAugmentation->nFollowType)
			{
				case MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_FROM_TRACE:
					{
						// Use the trace position
						vecImplantOrigin = m_vecTargetEndPos;
						VectorAngles( m_vecTargetNormal, angImplantAngles );
						pImplant->SetParent( m_hTargetEnt, -1 );
					}
					break;
				case MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_BONEMERGE:
					pImplant->FollowEntity( m_hTargetEnt, true );
					// Fall through
				case MiniFabricatorAugmentation_t::IMPLANT_FOLLOW_ATTACHMENT:
					if (m_hTargetEnt->GetBaseAnimating() && m_pTargetAugmentation->pszAttachment != NULL)
					{
						// Use the attachment
						int nAttachment = m_hTargetEnt->GetBaseAnimating()->LookupAttachment( m_pTargetAugmentation->pszAttachment );

						m_hTargetEnt->GetBaseAnimating()->GetAttachment( m_pTargetAugmentation->pszAttachment, vecImplantOrigin, angImplantAngles );

						pImplant->SetParent( m_hTargetEnt, nAttachment );
					}
					break;
			}

			if (vecImplantOrigin == vec3_origin)
			{
				Warning( "WARNING: Cannot resolve augmentation \"%s\" position for %s\n", m_pTargetAugmentation->pszName, m_hTargetEnt->GetDebugName() );
			}

			pImplant->SetAbsOrigin( vecImplantOrigin );
			pImplant->SetAbsAngles( angImplantAngles );

			pImplant->InitFabricationData( this, pPlayer, m_pTargetAugmentation );

			DispatchSpawn( pImplant );
		}
		else
		{
			Warning( "ERROR: No implant created for augmentation %s on entity %s", m_pTargetAugmentation->pszName, m_hTargetEnt->GetDebugName() );
			return;
		}

		// Bump the entity slightly
		if (m_pTargetAugmentation->flPushScale > 0.0f)
			m_hTargetEnt->ApplyAbsVelocityImpulse( m_vecTargetNormal * m_pTargetAugmentation->flPushScale );

		g_pEffects->Sparks( vecImplantOrigin, 1, 2, &m_vecTargetNormal );
		UTIL_Smoke( vecImplantOrigin, random->RandomInt( 5, 10 ), 10 );

		// Run the script
		if (m_hTargetEnt->ValidateScriptScope())
		{
			g_pScriptVM->SetValue( m_hTargetEnt->GetScriptScope(), "m_hImplant", ToHScript( pImplant ) );
			g_pScriptVM->SetValue( m_hTargetEnt->GetScriptScope(), "m_hFabricatorUser", ToHScript( pPlayer ) );
			m_hTargetEnt->RunScriptFile( m_pTargetAugmentation->pszVScriptFile );
		}
		else
		{
			Warning( "ERROR: Script scope failed to validate on %s, augmentation failed\n", m_hTargetEnt->GetDebugName() );
		}

		// Apply the cost
		pPlayer->RemoveAmmo( m_pTargetAugmentation->nCost, m_iPrimaryAmmoType );
		
		WeaponSound( SINGLE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Secondary attack - assimilate object
//-----------------------------------------------------------------------------
void CWeapon_MiniFabricator::SecondaryAttack( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	pPlayer->m_flNextAttack = gpGlobals->curtime + 1.0f;

	// Get our target entity
	if (!m_hTargetEnt)
		CheckForTarget( pPlayer );

	if (!m_hTargetEnt)
	{
		if (g_debug_minifabricator.GetBool())
			Msg( "Mini fabricator: No target\n" );
		WeaponSound( EMPTY );
		return;
	}

	extern ConVar sk_plr_dmg_resin;
	CTakeDamageInfo info( this, pPlayer, sk_plr_dmg_resin.GetFloat(), DMG_SHOCK );

	// See if any augmentations are connected to this and we can retract it
	for (int i = 0; i < g_FabricatorImplantList.Count(); i++)
	{
		if (g_FabricatorImplantList[i] && g_FabricatorImplantList[i]->GetOwnerEntity() == m_hTargetEnt)
		{
			// Find this implant's augmentation
			const MiniFabricatorAugmentation_t *pAugmentation = g_MiniFabricatorAugmentationManager.FindAugmentationByName( g_FabricatorImplantList[i]->GetAugmentationName() );

			// Compensate with half of the cost being given back
			pPlayer->GiveAmmo( pAugmentation->nCost / 2, m_iPrimaryAmmoType );

			// Break the implant (handles stuff like removing the code automatically)
			g_FabricatorImplantList[i]->Break( pPlayer, info );

			// Bump the entity slightly
			if (m_pTargetAugmentation->flPushScale > 0.0f)
				m_hTargetEnt->ApplyAbsVelocityImpulse( -m_vecTargetNormal * m_pTargetAugmentation->flPushScale );
			return;
		}
	}

	// If we're already full on resin, don't do anything
	if (!g_pGameRules->CanHaveAmmo( pPlayer, m_iPrimaryAmmoType ))
	{
		if (g_debug_minifabricator.GetBool())
			Msg( "Mini fabricator: Resin full\n" );
		WeaponSound( EMPTY );
		return;
	}

	// If the target can't take damage from us, don't do anything
	if (!m_hTargetEnt->PassesDamageFilter( info ))
	{
		if (g_debug_minifabricator.GetBool())
			Msg( "Mini fabricator: Doesn't pass damage filter\n" );
		WeaponSound( EMPTY );
		return;
	}

	Vector vecTargetCenter = m_hTargetEnt->WorldSpaceCenter();

	// See if this is something we can assimilate
	if (m_hTargetEnt->IsNPC())
	{
		// If it's a Combine unit, just damage it slightly instead
		if ( m_hTargetEnt->Classify() == CLASS_COMBINE ||
			m_hTargetEnt->Classify() == CLASS_METROPOLICE ||
			m_hTargetEnt->Classify() == CLASS_MANHACK ||
			m_hTargetEnt->Classify() == CLASS_SCANNER ||
			m_hTargetEnt->Classify() == CLASS_COMBINE_HUSK )
		{
			m_hTargetEnt->TakeDamage( info );
		}
		else
		{
			if (g_debug_minifabricator.GetBool())
				Msg( "Mini fabricator: Not a Combine NPC\n" );
			WeaponSound( EMPTY );
			return;
		}
	}
	else if ( m_hTargetEnt->ClassMatches( "prop_ragdoll" ) )
	{
		// Can assimilate Combine corpses
		CRagdollProp *pRagdoll = static_cast<CRagdollProp *>(m_hTargetEnt.Get());
		if (pRagdoll->GetSourceClassification() == CLASS_COMBINE ||
			pRagdoll->GetSourceClassification() == CLASS_METROPOLICE ||
			pRagdoll->GetSourceClassification() == CLASS_COMBINE_HUSK)
		{
			float flMass = 0.0f;
			for ( int i = 0; i < pRagdoll->GetRagdoll()->listCount; i++)
			{
				if ( pRagdoll->GetRagdoll()->list[i].pObject != NULL )
				{
					flMass += pRagdoll->GetRagdoll()->list[i].pObject->GetMass();
				}
			}
			pPlayer->GiveAmmo( sk_minifabricator_income_corpse_ratio.GetFloat() * flMass, m_iPrimaryAmmoType );

			info.SetDamage( pRagdoll->GetHealth() );
			info.AddDamageType( DMG_ALWAYSGIB );
			pRagdoll->TakeDamage( info );
		}
		else
		{
			if (g_debug_minifabricator.GetBool())
				Msg( "Mini fabricator: Not a Combine ragdoll\n" );
			WeaponSound( EMPTY );
			return;
		}
	}
	else if ( m_hTargetEnt->IsBaseCombatWeapon() )
	{
		// TODO
	}
	else if ( m_hTargetEnt->IsCombatItem() )
	{
		// TODO
	}
	else if ( m_hTargetEnt->ClassMatches( "prop_physics" ) ) // For now, do physics props only
	{
		// Check if it's a small metal object we can assimilate
		if (m_hTargetEnt->VPhysicsGetObject())
		{
			const surfacedata_t *pSurfaceData = physprops->GetSurfaceData( m_hTargetEnt->VPhysicsGetObject()->GetMaterialIndex() );
			float flMass = m_hTargetEnt->VPhysicsGetObject()->GetMass();
			if (pSurfaceData->game.material == CHAR_TEX_METAL && flMass >= sk_minifabricator_assimilate_min_mass.GetFloat() && flMass <= sk_minifabricator_assimilate_max_mass.GetFloat())
			{
				// Round to nearest int
				pPlayer->GiveAmmo( roundf( sk_minifabricator_income_prop_ratio.GetFloat() * flMass ), m_iPrimaryAmmoType );

				info.SetDamage( m_hTargetEnt->GetHealth() );
				CBreakableProp *pProp = static_cast<CBreakableProp*>(m_hTargetEnt.Get());
				if (pProp)
					pProp->Break( pPlayer, info );
			}
			else
			{
				if (g_debug_minifabricator.GetBool())
					Msg( "Mini fabricator: Not metal or not within mass range\n" );
				WeaponSound( EMPTY );
				return;
			}
		}
		else
		{
			if (g_debug_minifabricator.GetBool())
				Msg( "Mini fabricator: No VPhysics\n" );
			WeaponSound( EMPTY );
			return;
		}
	}

	g_pEffects->Sparks( m_vecTargetEndPos, 2, 1 );
	UTIL_Smoke( m_vecTargetEndPos, random->RandomInt( 5, 10 ), 10 );

	QAngle angGibAngles;
	VectorAngles( m_vecTargetNormal, angGibAngles );

	if (!m_hTargetEnt || m_hTargetEnt->GetHealth() <= 0)
	{
		// Throw out some small chunks
		CPVSFilter filter( vecTargetCenter );
		for (int i = 0; i < 4; i++)
		{
			Vector gibVelocity = RandomVector( -100, 100 );
			int iModelIndex = modelinfo->GetModelIndex( g_PropDataSystem.GetRandomChunkModel( "MetalChunks" ) );
			te->BreakModel( filter, 0.0, vecTargetCenter, angGibAngles, Vector( 40, 40, 40 ), gibVelocity, iModelIndex, 150, 4, 2.5, BREAK_METAL );
		}
	}

	WeaponSound( WPN_DOUBLE );
}

//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( prop_fabricator_implant, CFabricatorImplant );

BEGIN_DATADESC( CFabricatorImplant )
	DEFINE_FIELD( m_hFabricator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hFabricatorOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iszAugmentationName, FIELD_STRING ),
END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFabricatorImplant::CFabricatorImplant()
{
	g_FabricatorImplantList.AddToTail( this );
}

CFabricatorImplant::~CFabricatorImplant()
{
	g_FabricatorImplantList.FindAndRemove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFabricatorImplant::Break( CBaseEntity *pBreaker, const CTakeDamageInfo &info )
{
	// Mask the effect with some sparks
	g_pEffects->Sparks( GetAbsOrigin(), 2, 1);
	UTIL_Smoke( GetAbsOrigin(), random->RandomInt( 5, 10 ), 10 );

	if (GetOwnerEntity())
	{
		// Tell the script to remove itself
		GetOwnerEntity()->RunScript("RemoveAugmentation()");
	}

	BaseClass::Break( pBreaker, info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFabricatorImplant::InitFabricationData( CWeapon_MiniFabricator *pFabricator, CBaseCombatCharacter *pOwner, const MiniFabricatorAugmentation_t *pAugmentation )
{
	m_hFabricator = pFabricator;
	m_hFabricatorOwner = pOwner;
	m_iszAugmentationName = AllocPooledString( pAugmentation->pszName );
}
