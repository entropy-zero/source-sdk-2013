//=============================================================================//
//
// Purpose:		Tripmine that activates turrets.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "prop_turret_mine.h"
#include "npc_turret_floor.h"
#include "beam_shared.h"
#include "ez2_player.h"
#include "saverestore_utlvector.h"
#include "ai_stealth_senses.h"
#include "particle_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_ENTITY_TO_CLASS( prop_turret_mine, CPropTurretMine );

//-----------------------------------------------------------------------------

#define LASER_MAX_LENGTH	2048
#define LASER_GRACE_PERIOD	2.0
#define TRIP_SOUND			"JNK_Radar_Ping_Friendly" // TEMP
#define CONTEXT_THINK_LASER	"TurretLaserThink"

BEGIN_DATADESC( CPropTurretMine )

	DEFINE_KEYFIELD( m_bDisabled,	FIELD_BOOLEAN, "StartDisabled" ),

	DEFINE_FIELD( m_hLastHitEntity,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLaser,		FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_flGracePeriod,	FIELD_FLOAT,	"GracePeriod" ),

	DEFINE_KEYFIELD( m_iszTurretName,	FIELD_STRING,	"TurretName" ),
	DEFINE_UTLVECTOR( m_hTurrets,	FIELD_EHANDLE ),
	
	DEFINE_THINKFUNC( LaserTurnOnThink ),
	DEFINE_THINKFUNC( LaserThink ),
	DEFINE_THINKFUNC( ActivatedThink ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Trip", InputTrip ),

	DEFINE_OUTPUT( m_OnTripped, "OnTripped" ),
	DEFINE_OUTPUT( m_OnEnabled, "OnEnabled" ),
	DEFINE_OUTPUT( m_OnDisabled, "OnDisabled" ),

END_DATADESC()

IMPLEMENT_AUTO_LIST( IPropTurretMineAutoList );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Spawn()
{
	BaseClass::Spawn();

	if ( m_iszTurretName != NULL_STRING )
	{
		CBaseEntity *pEnt = gEntList.FindEntityByName( NULL, m_iszTurretName, this );
		while ( pEnt )
		{
			CNPC_FloorTurret *pTurret = dynamic_cast<CNPC_FloorTurret *>(pEnt);
			if ( pTurret )
				m_hTurrets.AddToTail( pTurret );

			pEnt = gEntList.FindEntityByName( pEnt, m_iszTurretName, this );
		}
	}

	MakeBeam();
	SetContextThink( &CPropTurretMine::LaserTurnOnThink, gpGlobals->curtime + m_flGracePeriod, CONTEXT_THINK_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Activate()
{
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Precache()
{
	BaseClass::Precache();

	PrecacheScriptSound( TRIP_SOUND );

	PrecacheParticleSystem( "turretmine_flash_activate" );
	PrecacheParticleSystem( "turretmine_flash_detonate" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::MakeBeam()
{
	m_hLaser = CBeam::BeamCreate( "sprites/laser.vmt", 2.0f );
	m_hLaser->PointEntInit( GetAbsOrigin(), this );

	m_hLaser->SetColor( 255, 0, 0 );
	m_hLaser->SetScrollRate( 35 );
	m_hLaser->SetNoise( 0.1 );
	m_hLaser->SetBrightness( 255 );

	UTIL_EZ2_ExcludeFromCloakCC( m_hLaser );

	int beamAttach = LookupAttachment( "beam_attach" );
	m_hLaser->SetEndAttachment( beamAttach );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::TurnOnLaser()
{
	m_hLaser->RemoveEffects( EF_NODRAW );

	Vector vecColor( 1, 0, 0 ); // Adjust if beam color ever becomes customizable
	DispatchParticleEffect( "turretmine_flash_activate", PATTACH_POINT, this, "beam_attach", vecColor, vecColor, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::TurnOffLaser()
{
	m_hLaser->AddEffects( EF_NODRAW );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Enable( CBaseEntity *pActivator )
{
	m_bDisabled = false;
	m_hLastHitEntity = NULL;
	TurnOnLaser();
	SetContextThink( &CPropTurretMine::LaserThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
	m_OnEnabled.FireOutput( pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Disable( CBaseEntity *pActivator )
{
	m_bDisabled = true;
	TurnOffLaser();
	SetContextThink( NULL, TICK_NEVER_THINK, CONTEXT_THINK_LASER );
	m_OnDisabled.FireOutput( pActivator, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::Trip( CBaseEntity *pActivator )
{
	m_hLastHitEntity = pActivator;

	m_OnTripped.FireOutput( pActivator, this );

	TurnOffLaser();

	Vector vecColor( 1, 0, 0 ); // Adjust if beam color ever becomes customizable
	DispatchParticleEffect( "turretmine_flash_detonate", PATTACH_POINT, this, "beam_attach", vecColor, vecColor, true );

	EmitSound( TRIP_SOUND );
	CSoundEnt::InsertSound( SOUND_COMBAT, pActivator->GetAbsOrigin(), 512, 2.0f, this, SOUNDENT_CHANNEL_STEALTH_TURRET_DEPLOY, pActivator );

	if (pActivator->IsPlayer() )
	{
		// Tell the player that they're compromised
		((CEZ2_Player *)pActivator)->NoteCompromiseCloak( COMPROMISE_TYPE_LASER );
	}

	FOR_EACH_VEC( m_hTurrets, i )
	{
		if ( m_hTurrets[i] )
			m_hTurrets[i]->Enable();
	}

	// Don't wait to deactivate if we have turrets, as the turrets will now reactivate their mines automatically once they retire
	if ( m_hTurrets.Count() == 0 )
		SetContextThink( &CPropTurretMine::ActivatedThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::LaserTurnOnThink()
{
	if ( !m_hLaser )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, CONTEXT_THINK_LASER );
		return;
	}

	if ( m_nBeamAttach == -1 )
		m_nBeamAttach = LookupAttachment( "beam_attach" );

	Vector vecOrigin, vecForward;
	GetAttachment( m_nBeamAttach, vecOrigin, &vecForward );

	// Just get the first entity we hit
	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + ( vecForward * LASER_MAX_LENGTH ), MASK_VISIBLE_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );

	m_hLastHitEntity = tr.m_pEnt;
	TurnOnLaser();
	SetContextThink( &CPropTurretMine::LaserThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::LaserThink()
{
	if ( !m_hLaser )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, CONTEXT_THINK_LASER );
		return;
	}

	if ( m_nBeamAttach == -1 )
		m_nBeamAttach = LookupAttachment( "beam_attach" );

	Vector vecOrigin, vecForward;
	GetAttachment( m_nBeamAttach, vecOrigin, &vecForward );

	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + ( vecForward * LASER_MAX_LENGTH ), MASK_VISIBLE_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );

	m_hLaser->SetStartPos( tr.endpos );

	if ( m_hLastHitEntity != tr.m_pEnt && tr.m_pEnt && !tr.m_pEnt->IsWorld() )
	{
		// New entity! Check if it's an ally
		if ( m_hTurrets.Count() > 0 && m_hTurrets[0] && m_hTurrets[0]->IRelationType( tr.m_pEnt ) == D_LI )
		{
			SetContextThink( &CPropTurretMine::LaserThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
			return;
		}

		// Not an ally, activate
		Trip( tr.m_pEnt );
		return;
	}

	SetContextThink( &CPropTurretMine::LaserThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::ActivatedThink()
{
	if ( !m_hLaser )
	{
		SetContextThink( NULL, TICK_NEVER_THINK, CONTEXT_THINK_LASER );
		return;
	}

	if ( m_nBeamAttach == -1 )
		m_nBeamAttach = LookupAttachment( "beam_attach" );

	Vector vecOrigin, vecForward;
	GetAttachment( m_nBeamAttach, vecOrigin, &vecForward );

	trace_t tr;
	UTIL_TraceLine( vecOrigin, vecOrigin + ( vecForward * LASER_MAX_LENGTH ), MASK_VISIBLE_AND_NPCS, this, COLLISION_GROUP_NONE, &tr );

	// Wait until we get a different entity before re-activating
	if ( m_hLastHitEntity != tr.m_pEnt )
	{
		// Only count if it's farther away than the current ent (or if there is no current ent)
		float flOldEntDistSqr = 1.0f;
		float flNewEntDistSqr = 0.0f;
		if ( m_hLastHitEntity )
		{
			flOldEntDistSqr = (vecOrigin - m_hLastHitEntity->GetAbsOrigin()).LengthSqr();
			flNewEntDistSqr = (vecOrigin - tr.m_pEnt->GetAbsOrigin()).LengthSqr();
		}

		if ( flNewEntDistSqr > flOldEntDistSqr )
		{
			m_hLastHitEntity = tr.m_pEnt;
			TurnOnLaser();
			SetContextThink( &CPropTurretMine::LaserThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
			return;
		}
	}

	SetContextThink( &CPropTurretMine::ActivatedThink, gpGlobals->curtime + 0.1f, CONTEXT_THINK_LASER );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::OnTurretActivate( CNPC_FloorTurret *pTurret )
{
	CHandle<CNPC_FloorTurret> hTurret = pTurret;
	for ( int i = 0; i < IPropTurretMineAutoList::AutoList().Count(); i++ )
	{
		CPropTurretMine *pMine = static_cast<CPropTurretMine*>( IPropTurretMineAutoList::AutoList()[i] );
		if ( pMine->m_hTurrets.HasElement( hTurret ) )
		{
			int i = 0;
			for ( ; i < pMine->m_hTurrets.Count(); i++ )
			{
				// There is another turret that is not yet active
				if ( pMine->m_hTurrets[i] && !pMine->m_hTurrets[i]->m_bActive )
					break;
			}

			if ( i == pMine->m_hTurrets.Count() )
				pMine->Disable( pTurret );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::OnTurretRetire( CNPC_FloorTurret *pTurret )
{
	CHandle<CNPC_FloorTurret> hTurret = pTurret;
	for ( int i = 0; i < IPropTurretMineAutoList::AutoList().Count(); i++ )
	{
		CPropTurretMine *pMine = static_cast<CPropTurretMine*>( IPropTurretMineAutoList::AutoList()[i] );
		if ( pMine->m_hTurrets.HasElement( hTurret ) )
		{
			int i = 0;
			for ( ; i < pMine->m_hTurrets.Count(); i++ )
			{
				// There is another turret that is still alive
				if ( pMine->m_hTurrets[i] && pMine->m_hTurrets[i]->m_bActive )
					break;
			}

			if ( i == pMine->m_hTurrets.Count() )
				pMine->Enable( pTurret );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::OnTurretDeath( CNPC_FloorTurret *pTurret )
{
	CHandle<CNPC_FloorTurret> hTurret = pTurret;
	for ( int i = 0; i < IPropTurretMineAutoList::AutoList().Count(); i++ )
	{
		CPropTurretMine *pMine = static_cast<CPropTurretMine*>( IPropTurretMineAutoList::AutoList()[i] );
		if ( pMine->m_hTurrets.HasElement( hTurret ) )
		{
			int i = 0;
			for ( ; i < pMine->m_hTurrets.Count(); i++ )
			{
				// There is another turret that is still alive
				if ( pMine->m_hTurrets[i] && pMine->m_hTurrets[i]->IsAlive() )
					break;
			}

			if ( i == pMine->m_hTurrets.Count() )
				pMine->Disable( pTurret );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::OnTurretRevive( CNPC_FloorTurret *pTurret )
{
	// Will be handled by OnTurretActivate()
	/*
	CHandle<CNPC_FloorTurret> hTurret = pTurret;
	for (int i = 0; i < IPropTurretMineAutoList::AutoList().Count(); i++)
	{
		CPropTurretMine *pMine = static_cast<CPropTurretMine *>( IPropTurretMineAutoList::AutoList()[i] );
		if (pMine->m_hTurrets.HasElement( hTurret ))
		{
			if ( pMine->m_bDisabled )
				pMine->Enable( pTurret );
		}
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::InputToggle( inputdata_t &inputdata )
{
	m_bDisabled ? InputEnable( inputdata ) : InputDisable( inputdata );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::InputEnable( inputdata_t &inputdata )
{
	Enable( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::InputDisable( inputdata_t &inputdata )
{
	Disable( inputdata.pActivator );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropTurretMine::InputTrip( inputdata_t &inputdata )
{
	Trip( inputdata.pActivator );
}
