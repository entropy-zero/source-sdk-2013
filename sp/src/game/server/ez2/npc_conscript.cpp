//=============================================================================//
//
// Purpose:		Early Combine soldier conscripted from Earth's pre-war militaries
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "npc_conscript.h"
#include "ez2_player.h"
#include "ai_interactions.h"
#include "IEffects.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

#define CONSCRIPT_SKIN_MEDIC		1
#define CONSCRIPT_SKIN_COMMANDER	2

#define CONSCRIPT_BODY_HEADWEAR_HELMET			1
#define CONSCRIPT_BODY_HEADWEAR_BERET			2
#define CONSCRIPT_BODY_HEADWEAR_RIOT			3
#define CONSCRIPT_BODY_HEADWEAR_RIOT_UP			4
#define CONSCRIPT_BODY_HEADWEAR_MAX				5

#define CONSCRIPT_BODY_EYEWEAR_SUNGLASSES		1
#define CONSCRIPT_BODY_EYEWEAR_GLASSES			2

#define CONSCRIPT_MIN_CROUCH_DISTANCE_SQR	(384*384)
#define CONSCRIPT_MIN_SIGNAL_DISTANCE_SQR	(256*256)

ConVar sk_conscript_health( "sk_conscript_health", "90" );
ConVar sk_conscript_gasmask_head( "sk_conscript_gasmask_head", "0.5" );
ConVar sk_conscript_gasmask_helmet( "sk_conscript_gasmask_helmet", "0.5" );
ConVar sk_conscript_helmet_protection( "sk_conscript_helmet_protection", "10" );

ConVar sk_conscript_default_proficiency( "sk_conscript_default_proficiency", "2" );
ConVar sk_conscript_oicw_proficiency( "sk_conscript_oicw_proficiency", "2" );
ConVar sk_conscript_shield_proficiency( "sk_conscript_shield_proficiency", "1" );

ConVar npc_conscript_headwear_collide( "npc_conscript_headwear_collide", "1" );
ConVar npc_conscript_default_male( "npc_conscript_default_male", "1" );

//-----------------------------------------------------------------------------

static const char *g_ppszConscriptRandomHeads[] =
{
	"male_01.mdl",
	"male_02.mdl",
	"female_01.mdl",
	"male_03.mdl",
	"female_02.mdl",
	"male_04.mdl",
	"female_03.mdl",
	"male_05.mdl",
	"female_04.mdl",
	"male_06.mdl",
	"female_06.mdl",
	"male_07.mdl",
	"female_07.mdl",
	"male_08.mdl",
	"male_09.mdl",
};

static const char *g_ppszConscriptModelLocs[] =
{
	"group_conscripts",		// Default
	"group_conscripts",
	"group_conscripts_reb",
	"group_hecu",
	"group_conscripts",		// Unique
};

// Aligns with bodygroup number
static const char *g_ppszConscriptHeadwearGibs[CONSCRIPT_BODY_HEADWEAR_MAX] =
{
	"gib_helmet.mdl",
	"gib_helmet.mdl",
	"gib_beret.mdl",
	"gib_face_shield.mdl",
	"gib_face_shield_up.mdl",
};

// Aligns with bodygroup number
static const char *g_ppszConscriptHeadwearContexts[CONSCRIPT_BODY_HEADWEAR_MAX] =
{
	"helmet",
	"helmet",
	"beret",
	"helmet",
	"helmet",
};

//-----------------------------------------------------------------------------

// This is independent because it's used by both helmets and gasmasks (the latter of which are not a separate entity)
static void ModifyHelmetDamage( CTakeDamageInfo &info, float flPenetrationScale = 1.0f )
{
	// UNDONE: Bit of a hack since player pistol damage is technically higher than the damage of rifles
	//	-- Pistol damage itself has been nerfed for now. TODO: Proper armor penetration?
	//if ( FStrEq( info.GetAmmoName(), "Pistol" ) )
	//	info.ScaleDamage( 0.8f );

	if (flPenetrationScale == 0.0f)
		flPenetrationScale = 1.0f;

	info.SubtractDamage( sk_conscript_helmet_protection.GetFloat() / flPenetrationScale );
}

class CPropConscriptHeadwear : public CArmorProp
{
	DECLARE_CLASS( CPropConscriptHeadwear, CArmorProp );
public:
	DECLARE_DATADESC();

	static CPropConscriptHeadwear *CreateConscriptHeadwear( CNPC_Conscript *pConscript, const char *pszModelName, const char *pszType )
	{
		CPropConscriptHeadwear *pHeadwear = (CPropConscriptHeadwear*)CreateNoSpawn( "prop_armor_conscript_headwear", pConscript->GetAbsOrigin(), pConscript->GetAbsAngles(), pConscript );
		pHeadwear->SetModelName( AllocPooledString( pszModelName ) );
		pHeadwear->SetParent( pConscript, pConscript->LookupAttachment( "helmet" ) );
		pHeadwear->SetLocalOrigin( vec3_origin );
		pHeadwear->SetLocalAngles( vec3_angle );

		pHeadwear->SetRenderMode( kRenderNone );
		pHeadwear->KeyValue( "disableshadows", "1" );
		pHeadwear->KeyValue( "solid", "6" );
		PhysDisableEntityCollisions( pConscript, pHeadwear );

		pHeadwear->SetHealth( 5 );
		pHeadwear->SetMaxHealth( 5 );

		pHeadwear->m_iszType = AllocPooledString( pszType );
		pHeadwear->m_bHelmet = FStrEq( pszType, "beret" ) ? false : true;
		pHeadwear->m_bFaceShield = V_strstr( pszModelName, "face_shield" ) ? true : false;

		DispatchSpawn( pHeadwear );
		pHeadwear->m_takedamage = pHeadwear->m_bFaceShield ? DAMAGE_EVENTS_ONLY : DAMAGE_YES;

		int nPlacementAttachment = pHeadwear->LookupAttachment( "placementOrigin" );
		if (nPlacementAttachment > -1)
		{
			Vector vecPlacementPos;
			QAngle angPlacementPos;
			pHeadwear->GetAttachmentLocal( nPlacementAttachment, vecPlacementPos, angPlacementPos );
			pHeadwear->SetLocalOrigin( -vecPlacementPos );
			pHeadwear->SetLocalAngles( angPlacementPos );
		}

		return pHeadwear;
	}

	void Precache()
	{
		BaseClass::Precache();

		if ( m_bHelmet )
			PrecacheScriptSound( "NPC_Conscript.HelmetDetach" );
	}

	bool ShouldBlockTraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
	{
		if ( !m_bHelmet )
			return true;

		return info.GetDamage() < sk_conscript_helmet_protection.GetFloat();
	}

	void ModifyTraceAttackDamage( CTakeDamageInfo &info, float flPenetrationScale )
	{
		// Face shield has more protection when hit from the front
		// (m_takedamage is set to DAMAGE_EVENTS_ONLY when this is the case)
		if ( m_takedamage == DAMAGE_EVENTS_ONLY )
			flPenetrationScale *= 0.6f;

		ModifyHelmetDamage( info, flPenetrationScale );
	}

	int GetHitgroup() const
	{
		return HITGROUP_HEAD;
	}

	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
	{
		if ( m_bFaceShield )
		{
			// Figure out where this attack came from. If hit from the front, the helmet shouldn't come off
			Vector vecDir = info.GetDamageForce().Normalized();
			Vector vecForward;
			GetVectors( &vecForward, NULL, NULL );

			float flDot = DotProduct( vecForward, vecDir );
			if ( flDot > -0.1f )
			{
				// From the back
				m_takedamage = DAMAGE_YES;
			}
			else
			{
				// From the front (protected)
				m_takedamage = DAMAGE_EVENTS_ONLY;
			}
		}

		BaseClass::TraceAttack( info, vecDir, ptr, pAccumulator );

		if ( m_bHelmet )
			g_pEffects->Sparks( ptr->endpos );
	}

	void Event_Killed( const CTakeDamageInfo &info )
	{
		if ( HasNPCParent() && GetNPCParent()->GetHealth() > 0 )
		{
			CBaseEntity *pGib = CNPC_Conscript::SpawnHeadGib( GetNPCParent(), info, GetModelName(), STRING( m_iszType ) );
			if ( pGib )
			{
				if ( m_bHelmet )
					pGib->EmitSound( "NPC_Conscript.HelmetDetach" );

				int nHelmetBody = GetNPCParent()->FindBodygroupByName( "headwear" );
				if (nHelmetBody != -1)
				{
					GetNPCParent()->SetBodygroup( nHelmetBody, 0 );
				}
			}

			GetNPCParent()->AddGesture( ACT_GESTURE_FLINCH_HEAD );
		}

		// Skip traditional breakage code because we handle everything here
		CBaseEntity::Event_Killed( info );
	}

	string_t	m_iszType;
	bool		m_bHelmet;
	bool		m_bFaceShield;

private:
};

BEGIN_DATADESC( CPropConscriptHeadwear )
	DEFINE_FIELD( m_iszType, FIELD_STRING ),
	DEFINE_FIELD( m_bHelmet, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bFaceShield, FIELD_BOOLEAN ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( prop_armor_conscript_headwear, CPropConscriptHeadwear );

//-----------------------------------------------------------------------------

BEGIN_DATADESC( CNPC_Conscript )
	DEFINE_KEYFIELD( m_Subtype, FIELD_INTEGER, "citizensubtype" ),	// For consistency with citizentype
	DEFINE_KEYFIELD( m_nHelmetPreference, FIELD_INTEGER, "helmetpreference" ),

	DEFINE_FIELD( m_hHeadwear, FIELD_EHANDLE ),

	DEFINE_FIELD( m_bFirstEncounter, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bShouldPoint, FIELD_BOOLEAN ),

	DEFINE_CONSCRIPT_DATADESC()
	DEFINE_PROPSHIELD_DATADESC()
END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_conscript, CNPC_Conscript );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_Conscript::CNPC_Conscript()
{
	m_Subtype = CST_DEFAULT;
	m_nHelmetPreference = TRS_NONE;

	m_hHeadwear = NULL;

	m_bFirstEncounter = false;
	m_bShouldPoint = false;

	//m_iWillpowerModifier = 1;
	m_iNumGrenades = 5;
	KeyValue( "SetGrenadeCapabilities", UTIL_VarArgs( "%i", GRENCAP_GRENADE ) ); // TODO: Direct access?

	GetSurrenderBehavior().KeyValue( "cansurrender", "0" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Spawn()
{
	BaseClass::Spawn();

	m_iHealth = sk_conscript_health.GetFloat();

	if ( GetCitizenType() != CT_UNIQUE )
	{
		int nHelmetBody = FindBodygroupByName( "headwear" );
		int nDesiredHelmet = CONSCRIPT_BODY_HEADWEAR_HELMET;

		if (m_Subtype == CST_RIOT)
			nDesiredHelmet = CONSCRIPT_BODY_HEADWEAR_RIOT;

		// Set up model appearance
		if ( IsMedic() )
		{
			m_nSkin = CONSCRIPT_SKIN_MEDIC;
			
			int nGlassesBody = FindBodygroupByName( "eyewear" );
			if (nGlassesBody != -1)
			{
				// Wear sunglasses if we have a SG552
				if (IsCommander())
				{
					SetBodygroup( nGlassesBody, CONSCRIPT_BODY_EYEWEAR_SUNGLASSES );
				}
					
				// Otherwise, 50% chance of regular glasses
				else if (RandomInt(1,2) == 1)
					SetBodygroup( nGlassesBody, CONSCRIPT_BODY_EYEWEAR_GLASSES );
			}

			if ( m_nHelmetPreference == TRS_NONE )
			{
				// Always wear helmet
				if (nHelmetBody != -1)
					SetBodygroup( nHelmetBody, nDesiredHelmet );
			}
		}
		else
		{
			// Wear beret and sunglasses if we're a commander
			if (IsCommander())
			{
				m_nSkin = CONSCRIPT_SKIN_COMMANDER;

				if (nHelmetBody != -1)
					SetBodygroup( nHelmetBody, CONSCRIPT_BODY_HEADWEAR_BERET );
					
				int nGlassesBody = FindBodygroupByName( "eyewear" );
				if (nGlassesBody != -1)
					SetBodygroup( nGlassesBody, CONSCRIPT_BODY_EYEWEAR_SUNGLASSES );
			}
			else if ( m_nHelmetPreference == TRS_NONE )
			{
				// 75% chance of helmet
				if (nHelmetBody != -1 && RandomInt(1,4) > 1)
					SetBodygroup( nHelmetBody, nDesiredHelmet );
			}
		}

		if ( m_nHelmetPreference == TRS_TRUE )
		{
			// Always wear helmet
			if (nHelmetBody != -1)
				SetBodygroup( nHelmetBody, nDesiredHelmet );
		}
	}

	if ( npc_conscript_headwear_collide.GetBool() )
	{
		const char *pszModelName = NULL;
		const char *pszType = NULL;
		if (GetDataForHeadwear( &pszModelName, &pszType ))
		{
			m_hHeadwear = CPropConscriptHeadwear::CreateConscriptHeadwear( this, pszModelName, pszType );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Precache()
{
	if ( m_Subtype == CST_DEFAULT )
	{
		// Ascertain subtype from weapon
		// (actual weapon entity is not given at this point)
		if ( m_spawnEquipment != NULL_STRING )
		{
			if ( FStrEq( STRING( m_spawnEquipment ), "weapon_oicw" ) )
			{
				m_Subtype = CST_COMMANDER;
				KeyValue( "SetGrenadeCapabilities", UTIL_VarArgs( "%i", GRENCAP_ALTFIRE ) ); // TODO: Direct access?
			}
			else if ( FStrEq( STRING( m_spawnEquipment ), "weapon_shotgun" ) )
			{
				m_Subtype = CST_BRUTE;
			}
		}

		if ( m_Subtype == CST_DEFAULT )
		{
			// If not already determined from weapon, try other properties
			if ( SpawnsWithShield() )
			{
				m_Subtype = CST_RIOT;
				m_iNumGrenades = 0;
			}
			else if ( GetModelName() != NULL_STRING )
			{
				if ( V_strstr( STRING( GetModelName() ), "gasmask" ) )
					m_Subtype = CST_GASMASK;
				else if ( V_strstr( STRING( GetModelName() ), "brute" ) )
					m_Subtype = CST_BRUTE;
			}
		}
	}

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::PrecacheAllOfType( CitizenType_t type )
{
	if (type == CT_UNIQUE)
		return;

	int nHeads = ARRAYSIZE( g_ppszConscriptRandomHeads );
	int i;
	for ( i = 0; i < nHeads; ++i )
	{
		PrecacheModel( CFmtStr( "models/Humans/%s/%s", (const char *)(CFmtStr(g_ppszConscriptModelLocs[type], "")), g_ppszConscriptRandomHeads[i] ) );
	}

	// Now precache headwear
	for ( i = 1; i < CONSCRIPT_BODY_HEADWEAR_MAX; i++ )
	{
		// For some reason, CFmtStr corrupts whatever is passed from g_ppszConscriptHeadwearGibs despite working perfectly fine
		// with g_ppszConscriptRandomHeads above.
		// UTIL_VarArgs does effectively the same thing, and it's safe here since the pointer passed into PrecacheModel() isn't
		// afterwards.
		PrecacheModel( UTIL_VarArgs( "models/Humans/%s/%s", g_ppszConscriptModelLocs[type], g_ppszConscriptHeadwearGibs[i] ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::SelectModel()
{
	// If making reslists, precache everything!!!
	static bool madereslists = false;

	if ( CommandLine()->CheckParm("-makereslists") && !madereslists )
	{
		madereslists = true;

		PrecacheAllOfType( CT_CONSCRIPT_REGULAR );
		PrecacheAllOfType( CT_CONSCRIPT_REBEL );
		PrecacheAllOfType( CT_CONSCRIPT_HECU );
	}

	if ( GetCitizenType() == CT_DEFAULT )
	{
		// TODO: Consider adding gamerules default?
		/*if (HL2GameRules()->GetDefaultCitizenType() != CT_DEFAULT)
		{
			SetCitizenType( static_cast<CitizenType_t>(HL2GameRules()->GetDefaultCitizenType()) );
		}
		else*/
		{
			// Simple determination based on alignment for now
			CitizenType_t nType = CT_CONSCRIPT_DEFAULT;

			if ( IsCombineAligned() )
			{
				nType = CT_CONSCRIPT_REGULAR;
			}
			else
			{
				nType = CT_CONSCRIPT_REBEL;
			}

			SetCitizenType( nType );
		}
	}

	if ( GetModelName() == NULL_STRING && npc_conscript_default_male.GetBool() )
	{
		AddSpawnFlags( SF_CITIZEN_RANDOM_HEAD_MALE );
	}

	if ( ( m_Subtype == CST_GASMASK || m_Subtype == CST_BRUTE ) &&
		( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD | SF_CITIZEN_RANDOM_HEAD_MALE | SF_CITIZEN_RANDOM_HEAD_FEMALE ) || GetModelName() == NULL_STRING ) )
	{
		// Simpler random selection between one model for each gender
		gender_t gender = GENDER_NONE;

		if ( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD_MALE ) )
		{
			gender = GENDER_MALE;
		}
		else if ( HasSpawnFlags( SF_CITIZEN_RANDOM_HEAD_FEMALE ) )
		{
			gender = GENDER_FEMALE;
		}
		else
		{
			gender = RandomInt( 0, 1 ) == 1 ? GENDER_MALE : GENDER_FEMALE;
		}

		const char *pszHead = NULL;

		if ( gender == GENDER_MALE )
		{
			pszHead = (m_Subtype == CST_GASMASK) ? "male_gasmasked.mdl" : "male_brute.mdl";
		}
		else
		{
			pszHead = (m_Subtype == CST_GASMASK) ? "female_gasmasked.mdl" : "female_brute.mdl";
		}

		if ( pszHead )
		{
			RemoveSpawnFlags( SF_CITIZEN_RANDOM_HEAD | SF_CITIZEN_RANDOM_HEAD_MALE | SF_CITIZEN_RANDOM_HEAD_FEMALE );
			SetCitizenModel( pszHead );
			return;
		}
	}

	// We need to use the base for tracking which heads were used, the VScript hook, etc.
	// This is safe now that we've selected a default model
	BaseClass::SelectModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::SetCitizenModel( const char *pszHeadName )
{
	SetModelName( AllocPooledString( CFmtStr( "models/Humans/%s/%s", g_ppszConscriptModelLocs[ GetCitizenType() ], pszHeadName ) ) );
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_Conscript::HandleInteraction(int interactionType, void *data, CBaseCombatCharacter* sourceEnt)
{
	if ( interactionType == g_interactionBadCopKick )
	{
		// If we've already pulled out a backup weapon, don't let it be kicked out of our hands unless we're allowed to surrender
		if ( m_bUsedBackupWeapon && !GetSurrenderBehavior().CanSurrender() )
			return false;

		// Fall through to base
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Conscript::GiveBackupWeapon( CBaseCombatWeapon *pWeapon, CBaseEntity *pActivator )
{
	if ( m_iLastHolsteredWeapon > 0)
	{
		DevMsg( "Already have multiple weapons: %i\n", m_iLastHolsteredWeapon );
		// Already have another weapon, don't need a new backup
		// Return true because there is another weapon

		m_bUsedBackupWeapon = true; // Don't give any more backup weapons!

		return true;
	}

	if ( m_bUsedBackupWeapon )
	{
		return false;
	}

	// Don't give any more backup weapons!
	m_bUsedBackupWeapon = true;

	if ( pWeapon != NULL )
	{
		// Is this a melee weapon?
		if ( pWeapon->IsMeleeWeapon() || pWeapon->ClassMatches( "weapon_crowbar" ) )
		{
			// No more backups
			return false;
		}
		// Is this weapon already a side arm?
		else if ( pWeapon->ClassMatches( "weapon_smg1" ) || pWeapon->ClassMatches( "weapon_smg2" ) || pWeapon->WeaponClassify() == WEPCLASS_HANDGUN )
		{
			if ( !pWeapon->WeaponClassify() != WEPCLASS_HANDGUN && RandomInt( 1, 6 ) != 1 )
			{
				if ( m_Subtype == CST_COMMANDER && RandomInt(1,6) == 1
					&& pActivator->IsCombatCharacter() && pActivator->MyCombatCharacterPointer()->Weapon_OwnsThisType( "weapon_css_deagle" ) )
				{
					// Very lucky conscript commanders pull out deagles if the player already owns one
					GiveWeaponHolstered( AllocPooledString( "weapon_css_deagle" ) );
					return true;
				}

				GiveWeaponHolstered( AllocPooledString( "weapon_css_glock" ) );
				return true;
			}
			else
			{
				// Very lucky conscripts get pipes as backup
				CBaseCombatWeapon *pCrowbar = GiveWeaponHolstered( AllocPooledString( "weapon_mattpipe" ) );
				pCrowbar->SetName( AllocPooledString( "worthless" ) ); // Bad Cop will say the crowbar pickup line upon interacting with this

				return true;
			}

			return false;
		}
	}

	GiveWeaponHolstered( AllocPooledString( "weapon_smg2" ) );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Conscript::CalculateWillpower()
{
	int l_iWillpower = BaseClass::CalculateWillpower();

	// Armor gives confidence
	if ( m_hHeadwear != NULL || IsGasmask() )
		l_iWillpower++;

	// Shield gives even more confidence
	if ( HasPropShield() )
		l_iWillpower += 2;

	return l_iWillpower;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Conscript::ModifyOrAppendCriteria( AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "citizensubtype", UTIL_VarArgs( "%i", m_Subtype ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Conscript::ModifyEmitSoundParams( EmitSound_t &params )
{
	BaseClass::ModifyEmitSoundParams( params );

	if ( IsGasmask() )
	{
		if ( params.m_nChannel == CHAN_VOICE || ( GetHealth() <= 0 && params.m_nChannel == CHAN_AUTO ) )
		{
			params.m_nSpecialDSP = 56;	// SPEAKER VERY SMALL
		}
	}
	else if ( m_Subtype == CST_RIOT )
	{
		int nHelmetBody = FindBodygroupByName( "headwear" );
		if ( nHelmetBody == -1 || GetBodygroup( nHelmetBody ) == CONSCRIPT_BODY_HEADWEAR_RIOT )
		{
			if ( params.m_nChannel == CHAN_VOICE || ( GetHealth() <= 0 && params.m_nChannel == CHAN_AUTO ) )
			{
				params.m_nSpecialDSP = 108;	// DUCT DIFFUSE SMALL BRIGHT
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CNPC_Conscript::GetSquadIDPrefix()
{
	const char *pszID = BaseClass::GetSquadIDPrefix();
	if ( pszID != NULL )
		return pszID;

	switch ( m_Subtype )
	{
		case CST_BRUTE:
			return "brute";
		case CST_RIOT:
			return "riot";
	}

	// Always fall back to generic grunt type
	return "grunt";
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CNPC_Conscript::GetDataForHeadwear( const char **ppszModelName, const char **ppszType )
{
	int nHelmetBody = FindBodygroupByName( "headwear" );
	if (nHelmetBody != -1)
	{
		int nBody = GetBodygroup( nHelmetBody );
		if ( nBody > 0 && nBody < CONSCRIPT_BODY_HEADWEAR_MAX )
		{
			CitizenType_t type = (CitizenType_t)GetCitizenType();
			if (type < CT_DEFAULT || type > ARRAYSIZE( g_ppszConscriptModelLocs ))
				type = CT_DEFAULT;

			*ppszModelName = UTIL_VarArgs( "models/Humans/%s/%s", g_ppszConscriptModelLocs[type], g_ppszConscriptHeadwearGibs[nBody] );
			*ppszType = g_ppszConscriptHeadwearContexts[nBody];
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Conscript::SpawnHeadGib( CAI_BaseNPC *pNPC, const CTakeDamageInfo &info, string_t iszModelName, const char *pszType )
{
	Vector vecMaskPos;
	QAngle vecMaskAng;
	pNPC->GetAttachment( "anim_attachment_head", vecMaskPos, vecMaskAng );

	CBaseEntity *pGib = CreateNoSpawn( "prop_physics", vecMaskPos, vecMaskAng, pNPC );
	if (pGib)
	{
		pGib->SetModelName( iszModelName );
		pGib->GetBaseAnimating()->m_nSkin = pNPC->m_nSkin;
		pGib->AddSpawnFlags( SF_PHYSPROP_DEBRIS | SF_PHYSPROP_IS_GIB );
		DispatchSpawn( pGib );
		
		int nPlacementAttachment = pGib->GetBaseAnimating()->LookupAttachment( "placementOrigin" );
		if (nPlacementAttachment > -1)
		{
			Vector vecPlacementPos;
			pNPC->GetAttachment( nPlacementAttachment, vecPlacementPos );
			pGib->SetAbsOrigin( vecMaskPos + (vecPlacementPos - vecMaskPos) );
		}

		if (pNPC->VPhysicsGetObject() && pGib->VPhysicsGetObject())
		{
			Vector velocity = info.GetDamageForce() * pNPC->VPhysicsGetObject()->GetInvMass();

			// Give the mask some extra velocity so it's easier to see
			velocity.z += 90.0f;
			velocity *= 1.5f;

			if ( pNPC->GetHealth() > 0 && info.GetAttacker() )
			{
				// If this is flying off of a NPC that was shot, make sure it's visible to the attacker
				// by making it go right or left relative to them, rather than directly forwards
				Vector vecRight;
				info.GetAttacker()->GetVectors( NULL, &vecRight, NULL );

				if (RandomInt( 0, 1 ) == 1)
					vecRight = -vecRight;

				velocity += (vecRight * 75.0f);
			}

			AngularImpulse angVelocity = RandomAngularImpulse( -400.0f, 400.0f );

			pGib->VPhysicsGetObject()->AddVelocity(&velocity, &angVelocity);
		}

		if (info.GetDamageType() & DMG_DISSOLVE)
		{
			pGib->GetBaseAnimating()->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
		}
		else
		{
			if ( g_hStealthManager && !g_hStealthManager->IsStealthLevel( STEALTH_LEVEL_LOUD ) )
			{
				pGib->AddContext( "curious_prop", "1", 0.0 );
				pGib->AddContext( "pickup_prop", "1", 0.0 );
				pGib->AddContext( "headwear", pszType, 0.0 );

				g_AI_SensedObjectsManager.AddEntity( pGib );
			}
			else
			{
				pGib->SUB_StartFadeOut( 10.0f, false );
			}
		}

		return pGib;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::Event_Killed( const CTakeDamageInfo &info )
{
	if ( m_hHeadwear )
	{
		UTIL_Remove( m_hHeadwear );
		m_hHeadwear = NULL;
	}

	int nHelmetBody = FindBodygroupByName( "headwear" );
	if (nHelmetBody != -1)
	{
		CBaseEntity *pGib = NULL;

		const char *pszModelName = NULL;
		const char *pszType = NULL;
		if (GetDataForHeadwear( &pszModelName, &pszType ))
		{
			pGib = SpawnHeadGib( this, info, MAKE_STRING( pszModelName ), pszType );
		}

		if ( pGib != NULL )
			SetBodygroup( nHelmetBody, 0 );
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Conscript::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	if ( !BaseClass::OnTakeDamage_Alive( info ) )
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	// If ptr->m_pEnt isn't this, then this came from the headwear and shouldn't be redirected
	if ( m_hHeadwear && !m_hHeadwear->IsMarkedForDeletion() && ptr->m_pEnt == this )
	{
		// Check to see if we're actually colliding with the headwear
		Ray_t ray;
		trace_t tr;
		ICollideable *pCollide = m_hHeadwear->CollisionProp();
		Vector vecTraceDist = (vecDir * 5.0f);
		ray.Init( ptr->endpos - vecTraceDist, ptr->endpos + vecTraceDist );
		enginetrace->ClipRayToCollideable( ray, MASK_ALL, pCollide, &tr );
		if ( tr.fraction != 1.0f )
		{
			DevMsg( "%s: Redirected TraceAttack to headwear\n", GetDebugName() );
			m_hHeadwear->TraceAttack( info, vecDir, ptr, pAccumulator );
			return;
		}
	}
	else if ( m_Subtype == CST_GASMASK && ptr->hitgroup == HITGROUP_GEAR )
	{
		// Emulate CPropConscriptHeadwear
		CTakeDamageInfo newInfo = info;
		ModifyHelmetDamage( newInfo, CArmorProp::GetPenetrationScale( info ) );
		g_pEffects->Sparks( ptr->endpos );

		ptr->hitgroup = HITGROUP_HEAD;

		// We need a flag to deal damage to the head hitgroup while also indicating that this came from the gas mask helmet.
		// DMG_DIRECT is normally used in combination with DMG_BURN to indicate burn damage from fires.
		// Every other instance of it is used in combination with DMG_BURN, so this is safe to use.
		newInfo.AddDamageType( DMG_DIRECT );

		// See CNPC_Citizen::TraceAttack for why we have to do it this way right now
		CNPC_PlayerCompanion::TraceAttack( newInfo, vecDir, ptr, pAccumulator );
		return;
	}

	// See CNPC_Citizen::TraceAttack for why we have to do it this way right now
	CNPC_PlayerCompanion::TraceAttack( info, vecDir, ptr, pAccumulator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Conscript::CanBeSneakAttacked( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( ptr->m_pEnt != this || ( m_Subtype == CST_GASMASK && info.GetDamageType() & DMG_DIRECT ) )
	{
		// The helmet saved us
		return false;
	}

	return BaseClass::CanBeSneakAttacked( info, vecDir, ptr );
}

extern ConVar sk_npc_head;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Conscript::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
		case HITGROUP_HEAD:
			{
				if ( m_Subtype == CST_GASMASK )
				{
					if ( info.GetDamageType() & DMG_DIRECT )
					{
						// Hit the gas mask

						// Use base head damage to emulate CPropConscriptHeadwear, but also factor in our own value
						return sk_conscript_gasmask_helmet.GetFloat() * BaseClass::GetHitgroupDamageMultiplier( HITGROUP_HEAD, info );
					}
					else
					{
						// Hit another part of the mask

						float flScale = sk_conscript_gasmask_head.GetFloat();

						// Also use helmet armor penetration
						flScale *= CArmorProp::GetPenetrationScale( info );
						if ( flScale > 1.0f )
						{
							// Completely nullified
							flScale = 1.0f;
						}

						// Multiplied by sk_npc_head
						return BaseClass::GetHitgroupDamageMultiplier( HITGROUP_HEAD, info ) * flScale;
					}
				}
			} break;
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Conscript::ShouldPickADeathPose( void )
{
	if ( IsCrouching() )
		return false;

	return BaseClass::ShouldPickADeathPose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Conscript::GatherConditions( void )
{
	BaseClass::GatherConditions();

	if ( HasCondition( COND_LOST_ENEMY ) )
	{
		// Lost enemy, should point when found again
		m_bShouldPoint = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CNPC_Conscript::TranslateSchedule( int scheduleType )
{
	switch ( scheduleType )
	{
		case SCHED_RANGE_ATTACK1:
			{
				if ( GetEnemy() && IRelationType( GetEnemy() ) != D_FR && /*CrouchIsDesired() &&*/ !HasCondition( COND_HEAVY_DAMAGE ) )
				{
					// See if we can crouch and shoot
					float flDistSqr = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).LengthSqr();
			
					// only crouch if they are relatively far away
					if (flDistSqr > CONSCRIPT_MIN_CROUCH_DISTANCE_SQR)
					{
						// try crouching
						Crouch();
						Vector targetPos = GetEnemy()->BodyTarget(GetActiveWeapon()->GetLocalOrigin());

						// if we can't see it crouched, stand up
						if (!WeaponLOSCondition(GetLocalOrigin(),targetPos,false))
						{
							Stand();
						}
					}
			
					// If we should point, then point
					if (m_bShouldPoint && flDistSqr > CONSCRIPT_MIN_SIGNAL_DISTANCE_SQR && GetSquad())
					{
						AddGesture( ACT_GESTURE_SIGNAL_FORWARD );
						m_bShouldPoint = false;

						// Apply to squadmates
						AISquadIter_t iter;
						CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
						while ( pSquadmate )
						{
							CNPC_Conscript *pConscript = dynamic_cast<CNPC_Conscript*>(pSquadmate);

							if( pConscript )
							{
								pConscript->m_bShouldPoint = false;
							}

							pSquadmate = m_pSquad->GetNextMember( &iter );
						}
					}
				}
				else
				{
					Stand();
				}
			}
			break;
		case SCHED_WAKE_ANGRY:
			{
				if ( m_bFirstEncounter && GetEnemy() && HasCondition(COND_SEE_ENEMY) && GetSquad() && GetSquad()->NumMembers() > 1 )
				{
					// See if we can signal
					float flDistSqr = (GetAbsOrigin() - GetEnemy()->GetAbsOrigin()).LengthSqr();
					if (flDistSqr > CONSCRIPT_MIN_SIGNAL_DISTANCE_SQR)
					{
						AddGesture( ACT_GESTURE_SIGNAL_GROUP );
						m_bFirstEncounter = false;

						// Apply to squadmates
						AISquadIter_t iter;
						CAI_BaseNPC *pSquadmate = m_pSquad->GetFirstMember( &iter );
						while ( pSquadmate )
						{
							CNPC_Conscript *pConscript = dynamic_cast<CNPC_Conscript*>(pSquadmate);

							if( pConscript )
							{
								pConscript->m_bFirstEncounter = false;
							}

							pSquadmate = m_pSquad->GetNextMember( &iter );
						}
					}
				}
			}
			break;
		case SCHED_CHASE_ENEMY:
		case SCHED_ESTABLISH_LINE_OF_FIRE:
			{
				if ( m_Subtype == CST_ENGINEER )
				{
					if ( !HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
					{
						// Do not establish LOF if we can't attack but can place tripmines
						if ( GetTripminePlaceBehavior().ShouldPlaceTripmine() )
						{
							DeferSchedulingToBehavior( &GetTripminePlaceBehavior() );
							return BaseClass::TranslateSchedule( scheduleType );
						}
					}
				}
			}
			break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CNPC_Conscript::NPC_TranslateActivity( Activity eNewActivity )
{
	if ( eNewActivity == ACT_COMBINE_THROW_GRENADE )
	{
		return ACT_SPECIAL_ATTACK1;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
WeaponProficiency_t CNPC_Conscript::CalcWeaponProficiency( CBaseCombatWeapon *pWeapon )
{
	int nProficiency = sk_conscript_default_proficiency.GetInt();

	if ( HasPropShield() )
	{
		nProficiency = sk_conscript_shield_proficiency.GetInt();
	}
	else if ( FClassnameIs( pWeapon, "weapon_oicw" ) )
	{
		nProficiency = sk_conscript_oicw_proficiency.GetInt();
	}

	return Clamp( (WeaponProficiency_t)nProficiency, WEAPON_PROFICIENCY_POOR, WEAPON_PROFICIENCY_PERFECT );
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( npc_conscript, CNPC_Conscript )

AI_END_CUSTOM_NPC()
