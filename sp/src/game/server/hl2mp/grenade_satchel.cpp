//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "player.h"
#include "soundenvelope.h"
#include "engine/IEngineSound.h"
#include "explode.h"
#include "Sprite.h"
#include "grenade_satchel.h"
#ifdef EZ2
#include "hl2_player.h"
#include "ai_senses.h"
#include "eventqueue.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SLAM_SPRITE	"sprites/redglow1.vmt"

ConVar    sk_plr_dmg_satchel		( "sk_plr_dmg_satchel", "150" );
ConVar    sk_npc_dmg_satchel		( "sk_npc_dmg_satchel", "0" );
ConVar    sk_satchel_radius			( "sk_satchel_radius", "200" );
#ifdef EZ2
ConVar    sk_satchel_always_visible_to_npcs( "sk_satchel_always_visible_to_npcs", "0" );

#define PROXIMITY_SATCHEL_SOUND_ARM						"SatchelGrenade.Proximity_Arm" // "Grenade.Blip"
#define PROXIMITY_SATCHEL_WARN_SOUND_LOW				"SatchelGrenade.Proximity_BlipLow" // "buttons.snd15"
#define PROXIMITY_SATCHEL_WARN_SOUND_MED				"SatchelGrenade.Proximity_BlipMed"
#define PROXIMITY_SATCHEL_WARN_SOUND_HIGH				"SatchelGrenade.Proximity_BlipHigh"
#define PROXIMITY_SATCHEL_WARN_SOUND_DETONATE			"SatchelGrenade.Proximity_BlipDetonate"
#define PROXIMITY_SATCHEL_WARN_RADIUS_SQR				Square( 512.0f )
#define PROXIMITY_SATCHEL_DETONATE_RADIUS_SQR			Square( 96.0f )
#define PROXIMITY_SATCHEL_DETONATE_EXCLUDE_RADIUS_SQR	Square( 128.0f )
#define PROXIMITY_SATCHEL_NUM_WARN_TICKS				15
#endif

BEGIN_DATADESC( CSatchelCharge )

	DEFINE_FIELD( m_flNextBounceSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_bInAir, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vLastPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_pMyWeaponSLAM, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_bIsAttached, FIELD_BOOLEAN, "IsAttached" ),

#ifdef EZ2
	DEFINE_KEYFIELD( m_bVisibleToNPCs, FIELD_BOOLEAN, "VisibleToNPCs" ),

	DEFINE_KEYFIELD( m_bProximitySatchel, FIELD_BOOLEAN, "ProximitySatchel" ),
	DEFINE_FIELD( m_bProximityWarnAlt, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nSatchelWarnTicks, FIELD_INTEGER ),
	DEFINE_FIELD( m_hProximityLight, FIELD_EHANDLE ),
#endif

	// Function Pointers
	DEFINE_THINKFUNC( SatchelThink ),
#ifdef EZ2
	DEFINE_THINKFUNC( ProximitySatchelThink ),
	DEFINE_THINKFUNC( ProximitySatchelPreDetonateThink ),
#endif

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Explode", InputExplode),

END_DATADESC()

#ifdef EZ2
IMPLEMENT_NETWORKCLASS_ALIASED( SatchelCharge, DT_SatchelCharge )

BEGIN_SEND_TABLE( CSatchelCharge, DT_SatchelCharge )
END_SEND_TABLE()
#endif

LINK_ENTITY_TO_CLASS( npc_satchel, CSatchelCharge );

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSatchelCharge::Deactivate( void )
{
	AddSolidFlags( FSOLID_NOT_SOLID );
	UTIL_Remove( this );

	if ( m_hGlowSprite != NULL )
	{
		UTIL_Remove( m_hGlowSprite );
		m_hGlowSprite = NULL;
	}
}


void CSatchelCharge::Spawn( void )
{
	Precache( );
	SetModel( "models/Weapons/w_slam.mdl" );

	VPhysicsInitNormal( SOLID_BBOX, GetSolidFlags() | FSOLID_TRIGGER, false );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );

	UTIL_SetSize(this, Vector( -6, -6, -2), Vector(6, 6, 2));

	SetThink( &CSatchelCharge::SatchelThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	m_flDamage		= sk_plr_dmg_satchel.GetFloat();
	m_DmgRadius		= sk_satchel_radius.GetFloat();
	m_takedamage	= DAMAGE_YES;
	m_iHealth		= 1;
	if ( m_bIsAttached )
	{
		SetMoveType( MOVETYPE_NONE );
	}
	else
	{
		SetMoveType( MOVETYPE_VPHYSICS );
		SetGravity( UTIL_ScaleForGravity( 560 ) );	// slightly lower gravity
		SetFriction( 1.0 );
	}
	SetSequence( 1 );
	SetDamage( sk_plr_dmg_satchel.GetFloat() );

	m_bInAir				= true;
	m_flNextBounceSoundTime	= 0;

	m_vLastPosition	= vec3_origin;

	m_hGlowSprite = NULL;
	CreateEffects();

#ifdef EZ2
	m_bIsLive = true; // Satchels are always live in EZ2

	if (GetOwnerEntity() && GetOwnerEntity()->IsPlayer())
	{
		CHL2_Player *pHL2Player = static_cast<CHL2_Player*>(GetOwnerEntity());
		if (pHL2Player)
		{
			pHL2Player->OnDropSatchel( this );
		}
	}

	m_hAttacker = NULL;

	if ( sk_satchel_always_visible_to_npcs.GetBool() )
		m_bVisibleToNPCs = true;

	if ( m_bVisibleToNPCs )
	{
		// Allow NPCs to see it
		SetBlocksLOS( false );
		AddFlag( FL_OBJECT );
	}

	if ( m_bProximitySatchel && m_hGlowSprite )
	{
		// Sprite is controlled in proximity satchels
		m_hGlowSprite->SetTransparency( kRenderTransAdd, 255, 128, 255, 255, kRenderFxNone );

		// Make the dlight (aaaah!!! would be better as client code!!!)
		m_hProximityLight = CreateNoSpawn( "light_dynamic", GetAbsOrigin(), GetAbsAngles(), this );
		m_hProximityLight->KeyValue( "_light", "255 128 0 200" );
		m_hProximityLight->KeyValue( "_cone", "0" );
		m_hProximityLight->KeyValue( "_inner_cone", "0" );
		m_hProximityLight->KeyValue( "brightness", "5" );
		m_hProximityLight->KeyValue( "distance", "150" );

		m_hProximityLight->SetParent( this );
		DispatchSpawn( m_hProximityLight );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Start up any effects for us
//-----------------------------------------------------------------------------
void CSatchelCharge::CreateEffects( void )
{
	// Only do this once
	if ( m_hGlowSprite != NULL )
		return;

	// Create a blinking light to show we're an active SLAM
	m_hGlowSprite = CSprite::SpriteCreate( SLAM_SPRITE, GetAbsOrigin(), false );
	m_hGlowSprite->SetAttachment( this, 0 );
	m_hGlowSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxStrobeFast );
	m_hGlowSprite->SetBrightness( 255, 1.0f );
	m_hGlowSprite->SetScale( 0.2f, 0.5f );
	m_hGlowSprite->TurnOn();

#ifdef EZ2
	UTIL_EZ2_ExcludeFromCloakCC( m_hGlowSprite );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CSatchelCharge::InputExplode( inputdata_t &inputdata )
{
	ExplosionCreate( GetAbsOrigin() + Vector( 0, 0, 16 ), GetAbsAngles(), GetThrower(), GetDamage(), sk_satchel_radius.GetFloat(),
		SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_NOSMOKE, 0.0f, this );

#ifdef EZ2
	if (GetThrower() && GetThrower()->IsPlayer())
	{
		CHL2_Player *pHL2Player = static_cast<CHL2_Player*>(GetThrower());
		if (pHL2Player)
		{
			pHL2Player->OnSatchelExploded( this, inputdata.pActivator );
		}
	}
#endif

	UTIL_Remove( this );
}

#ifdef EZ2
//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CSatchelCharge::Event_Killed( const CTakeDamageInfo &info )
{
	m_hAttacker = info.GetAttacker();

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CSatchelCharge::Explode( trace_t *pTrace, int bitsDamageType )
{
	if (GetThrower() && GetThrower()->IsPlayer())
	{
		CHL2_Player *pHL2Player = static_cast<CHL2_Player*>(GetThrower());
		if (pHL2Player)
		{
			pHL2Player->OnSatchelExploded( this, m_hAttacker );
		}
	}

	BaseClass::Explode( pTrace, bitsDamageType );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CSatchelCharge::UpdateTransmitState()
{
	if ( GetThrower() && GetThrower()->IsPlayer() )
	{
		// Player satchels need to be transmitted for HUD glow
		return SetTransmitState( FL_EDICT_ALWAYS );
	}
	else
	{
		return BaseClass::UpdateTransmitState();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Allows entities to be 'invisible' to NPC senses.
//-----------------------------------------------------------------------------
bool CSatchelCharge::CanBeSeenBy( CAI_BaseNPC *pNPC )
{
	if ( !BaseClass::CanBeSeenBy( pNPC ) )
		return false;

	//if ( !pNPC->IsPlayerAlly( ToBasePlayer( GetThrower() ) ) )
	{
		// Do an extra FOV check
		Vector vecLookDir = pNPC->HeadDirection3D();
		Vector vecDelta = EyePosition() - pNPC->EyePosition();
		float flDist = VectorNormalize( vecDelta );

		// TODO: More standard number
		if ( flDist > 1024.0f )
			return false;

		float flDot = vecDelta.Dot( vecLookDir );

		float flThreshold = DOT_45DEGREE;
		switch ( pNPC->GetState() )
		{
			default:
			case NPC_STATE_IDLE:
				flThreshold = pNPC->IsMoving() ? DOT_25DEGREE : DOT_45DEGREE;
				break;

			case NPC_STATE_ALERT:
				flThreshold = pNPC->IsMoving() ? DOT_30DEGREE : 0.5f; // 30 or 60 degrees
				break;

			case NPC_STATE_COMBAT:
				flThreshold = pNPC->IsMoving() ? DOT_45DEGREE : 0.258819f; // 45 or 75 degrees
				break;
		}

		// If the satchel is moving, then it's much easier to see it
		Vector vecVelocity;
		if ( VPhysicsGetObject() )
			VPhysicsGetObject()->GetVelocity( &vecVelocity, NULL );

		if ( vecVelocity.LengthSqr() > Square( 4.0f ) )
			flThreshold *= 0.2f;

		if ( !pNPC->IsPlayerAlly( ToBasePlayer( GetThrower() ) ) )
		{
			// Enemies are less aware on lower difficulties
			switch ( g_pGameRules->GetSkillLevel() )
			{
				case SKILL_EASY:
					flThreshold *= 2.0f;
					break;
				case SKILL_MEDIUM:
					flThreshold *= 1.5f;
					break;
			}
		}

		// Rely more on stealth senses for appropriate alertness
		if ( pNPC->IsUsingStealthSenses() )
			flThreshold *= 0.5f;

		if ( flDot < flThreshold )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
CSatchelCharge *Satchel_CreateProximitySatchel( const Vector &position, const QAngle &angles, const Vector &velocity, const AngularImpulse &angVelocity, CBaseEntity *pOwner )
{
	CSatchelCharge *pSatchel = static_cast<CSatchelCharge *>(CBaseEntity::CreateNoSpawn( "npc_satchel", position, angles, pOwner ));
	pSatchel->SetProximitySatchel( true );
	pSatchel->SetThrower( pOwner->MyCombatCharacterPointer() );
	DispatchSpawn( pSatchel );

	if ( pSatchel->VPhysicsGetObject() )
	{
		pSatchel->VPhysicsGetObject()->AddVelocity( &velocity, &angVelocity );
	}

	return pSatchel;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSatchelCharge::ProximitySatchelThink( void )
{
	float flThinkTime = 0.5f;

	// First, make sure we have a valid owner
	CBaseCombatCharacter *pThrower = GetThrower();
	if ( pThrower )
	{
		float flClosestSqr = PROXIMITY_SATCHEL_WARN_RADIUS_SQR;
		CBaseCombatCharacter *pClosest = NULL;

		// Search for nearby players
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && !(pPlayer->GetFlags() & FL_NOTARGET) )
			{
				Disposition_t nRel = pThrower->IRelationType( pPlayer );
				if ( nRel <= D_FR )
				{
					float flDistSqr = (pPlayer->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
					if ( flDistSqr < flClosestSqr )
					{
						pClosest = pPlayer;
						flClosestSqr = flDistSqr;
					}
				}
				else if ( nRel == D_LI )
				{
					// If this ally is too close to the satchel, stop checking entirely
					if ( (pPlayer->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < PROXIMITY_SATCHEL_DETONATE_EXCLUDE_RADIUS_SQR )
					{
						flClosestSqr = -1.0f;
						pClosest = NULL;
						break;
					}
				}
			}
		}

		if ( flClosestSqr != -1.0f )
		{
			// Search for nearby NPCs
			CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
			for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
			{
				if ( ppAIs[i] )
				{
					Disposition_t nRel = pThrower->IRelationType( ppAIs[i] );
					if ( nRel <= D_FR )
					{
						float flDistSqr = (ppAIs[i]->GetAbsOrigin() - GetAbsOrigin()).LengthSqr();
						if ( flDistSqr < flClosestSqr )
						{
							pClosest = ppAIs[i];
							flClosestSqr = flDistSqr;
						}
					}
					else if ( nRel == D_LI )
					{
						// If this ally is too close to the satchel, stop checking entirely
						if ( (ppAIs[i]->GetAbsOrigin() - GetAbsOrigin()).LengthSqr() < PROXIMITY_SATCHEL_DETONATE_EXCLUDE_RADIUS_SQR )
						{
							flClosestSqr = -1.0f;
							pClosest = NULL;
							break;
						}
					}
				}
			}
		}

		if ( pClosest )
		{
			if ( flClosestSqr < PROXIMITY_SATCHEL_DETONATE_RADIUS_SQR /*&& FVisible( pClosest )*/ )
			{
				// Change sprite render
				m_hGlowSprite->SetScale( 1.0f );

				// Alert thrower
				if ( pThrower->IsNPC() )
				{
					pThrower->MyNPCPointer()->UpdateEnemyMemory( pClosest, WorldSpaceCenter(), this ); // pClosest->GetAbsOrigin()
				}

				SetThink( &CSatchelCharge::ProximitySatchelPreDetonateThink );
				SetNextThink( gpGlobals->curtime );
				return;
			}
			else if ( flClosestSqr < PROXIMITY_SATCHEL_WARN_RADIUS_SQR )
			{
				// We didn't find an enemy to detonate from, but if there's someone in the warn radius, increase beeping
				flThinkTime = RemapVal( flClosestSqr, PROXIMITY_SATCHEL_DETONATE_RADIUS_SQR, PROXIMITY_SATCHEL_WARN_RADIUS_SQR, 0.05, 0.5 );

				// Alert thrower if less than half
				if ( flThinkTime < 0.2 && pThrower->IsNPC() && m_bProximityWarnAlt )
				{
					pThrower->MyNPCPointer()->UpdateEnemyMemory( pClosest, WorldSpaceCenter(), this ); // pClosest->GetAbsOrigin()
				}
			}
		}
	}
	else
	{
		// Remove the sprite to indicate we're no longer a threat (even though we could still explode if shot)
		m_hGlowSprite->SetRenderMode( kRenderNone );

		variant_t emptyVariant;
		m_hProximityLight->AcceptInput( "TurnOff", this, this, emptyVariant, 0 );
		SetThink( NULL );
		return;
	}
	
	// Emit warning sound
	if ( m_bProximityWarnAlt )
	{
		const char *pszWarnSound = PROXIMITY_SATCHEL_WARN_SOUND_LOW;

		if ( flThinkTime < 0.2f )
		{
			pszWarnSound = PROXIMITY_SATCHEL_WARN_SOUND_HIGH;
		}
		else if ( flThinkTime < 0.4f )
		{
			pszWarnSound = PROXIMITY_SATCHEL_WARN_SOUND_MED;
		}

		EmitSound( pszWarnSound );

		// Turn on the light
		m_hGlowSprite->TurnOn();

		variant_t emptyVariant;
		m_hProximityLight->AcceptInput( "TurnOn", this, this, emptyVariant, 0 );

		// Turn them back off with I/O
		g_EventQueue.AddEvent( m_hGlowSprite, "HideSprite", emptyVariant, 0.1f, this, this );
		g_EventQueue.AddEvent( m_hProximityLight, "TurnOff", emptyVariant, 0.1f, this, this );
	}

	m_bProximityWarnAlt = !m_bProximityWarnAlt;

	SetNextThink( gpGlobals->curtime + flThinkTime );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSatchelCharge::ProximitySatchelPreDetonateThink( void )
{
	if ( m_nSatchelWarnTicks >= PROXIMITY_SATCHEL_NUM_WARN_TICKS )
	{
		// Can detonate now
		// TODO: Set activator?
		inputdata_t inputdata;
		InputExplode( inputdata );
		return;
	}

	m_nSatchelWarnTicks++;

	// Quick warning ticks
	EmitSound( PROXIMITY_SATCHEL_WARN_SOUND_DETONATE );

	m_hGlowSprite->TurnOn();

	variant_t emptyVariant;
	m_hProximityLight->AcceptInput( "TurnOn", this, this, emptyVariant, 0 );

	// Turn them back off with I/O
	g_EventQueue.AddEvent( m_hGlowSprite, "HideSprite", emptyVariant, 0.025f, this, this );
	g_EventQueue.AddEvent( m_hProximityLight, "TurnOff", emptyVariant, 0.025f, this, this );

	SetNextThink( gpGlobals->curtime + 0.05f );
}
#endif


void CSatchelCharge::SatchelThink( void )
{
	// If attached resize so player can pick up off wall
	if (m_bIsAttached)
	{
		UTIL_SetSize(this, Vector( -2, -2, -6), Vector(2, 2, 6));
	}

	// See if I can lose my owner (has dropper moved out of way?)
	// Want do this so owner can shoot the satchel charge
	if (GetOwnerEntity())
	{
		trace_t tr;
		Vector	vUpABit = GetAbsOrigin();
		vUpABit.z += 5.0;

		CBaseEntity* saveOwner	= GetOwnerEntity();
		SetOwnerEntity( NULL );
		UTIL_TraceEntity( this, GetAbsOrigin(), vUpABit, MASK_SOLID, &tr );
		if ( tr.startsolid || tr.fraction != 1.0 )
		{
			SetOwnerEntity( saveOwner );
		}
	}
	
	// Bounce movement code gets this think stuck occasionally so check if I've 
	// succeeded in moving, otherwise kill my motions.
	else if ((GetAbsOrigin() - m_vLastPosition).LengthSqr()<1)
	{
		SetAbsVelocity( vec3_origin );

		QAngle angVel = GetLocalAngularVelocity();
		angVel.y  = 0;
		SetLocalAngularVelocity( angVel );

#ifdef EZ2
		if ( m_bProximitySatchel )
		{
			m_hGlowSprite->SetRenderColorG( 0 );
			m_hProximityLight->SetRenderColorG( 0 );

			EmitSound( PROXIMITY_SATCHEL_SOUND_ARM );

			// Start proximity satchel think
			SetThink( &CSatchelCharge::ProximitySatchelThink );
			SetNextThink( gpGlobals->curtime + 0.1f );
			return;
		}
#endif

		// Clear think function
		SetThink(NULL);
		return;
	}
	m_vLastPosition= GetAbsOrigin();

	StudioFrameAdvance( );
	SetNextThink( gpGlobals->curtime + 0.1f );

	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	// Is it attached to a wall?
	if (m_bIsAttached)
	{
		return;
	}
}

void CSatchelCharge::Precache( void )
{
	PrecacheModel("models/Weapons/w_slam.mdl");
	PrecacheModel(SLAM_SPRITE);

#ifdef EZ2
	if ( m_bProximitySatchel )
	{
		PrecacheScriptSound( PROXIMITY_SATCHEL_SOUND_ARM );
		PrecacheScriptSound( PROXIMITY_SATCHEL_WARN_SOUND_LOW );
		PrecacheScriptSound( PROXIMITY_SATCHEL_WARN_SOUND_MED );
		PrecacheScriptSound( PROXIMITY_SATCHEL_WARN_SOUND_HIGH );
		PrecacheScriptSound( PROXIMITY_SATCHEL_WARN_SOUND_DETONATE );
	}
#endif
}

void CSatchelCharge::BounceSound( void )
{
	if (gpGlobals->curtime > m_flNextBounceSoundTime)
	{
		m_flNextBounceSoundTime = gpGlobals->curtime + 0.1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  :
// Output :
//-----------------------------------------------------------------------------
CSatchelCharge::CSatchelCharge(void)
{
	m_vLastPosition.Init();
	m_pMyWeaponSLAM = NULL;
}

void CSatchelCharge::UpdateOnRemove(void)
{
	BaseClass::UpdateOnRemove();

	if ( m_hGlowSprite != NULL )
	{
		UTIL_Remove( m_hGlowSprite );
		m_hGlowSprite = NULL;
	}

#ifdef EZ2
	if ( m_hProximityLight != NULL )
	{
		UTIL_Remove( m_hProximityLight );
		m_hProximityLight = NULL;
	}
#endif
}
