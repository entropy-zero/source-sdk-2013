//=============================================================================//
//
// Purpose:		Reads level lighting directly so that it can be factored into stealth
// 
//				Based on Saul Rennison's worldlight code (see worldlight.cpp)
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "ai_stealth_lighting.h"
#include "info_darknessmode_lightsource.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

ConVar	ai_stealth_lighting( "ai_stealth_lighting", "1" );
ConVar	ai_stealth_lighting_level_max( "ai_stealth_lighting_level_max", "1.0" );
ConVar	ai_stealth_lighting_level_midpoint( "ai_stealth_lighting_level_midpoint", "0.65" );
ConVar	ai_stealth_lighting_ratio_min_player( "ai_stealth_lighting_ratio_min_player", "0.025" );
ConVar	ai_stealth_lighting_ratio_min_npc( "ai_stealth_lighting_ratio_min_npc", "0.025" );

ConVar	g_debug_stealth_light( "g_debug_stealth_light", "0" );
ConVar	g_debug_stealth_light_grid_size( "g_debug_stealth_light_grid_size", "8" );
ConVar	g_debug_stealth_light_grid_radius( "g_debug_stealth_light_grid_radius", "256" );
ConVar	g_debug_stealth_light_grid_duration( "g_debug_stealth_light_grid_duration", "5" );

#define AI_STEALTH_LIGHTING_CACHE_TIME			1.0
#define AI_STEALTH_LIGHTING_CACHE_TIME_PLAYER	0.25
#define AI_STEALTH_LIGHTING_CACHE_TIME_NPC		0.5

// This has to be really small since a fully lit position could be very close to an unlit one
#define AI_STEALTH_LIGHTING_CACHE_POS_TOLERANCE		4.0

CStealthLightingSystem g_StealthLightingSystem;

//-----------------------------------------------------------------------------
// Purpose: calculate intensity ratio for a worldlight by distance
// Author: Valve Software
//-----------------------------------------------------------------------------
static float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta )
{
	float falloff;

	switch (wl->type)
	{
	case emit_surface:
		// Cull out stuff that's too far
		if(wl->radius != 0)
		{
			if(DotProduct( delta, delta ) > (wl->radius * wl->radius))
				return 0.0f;
		}

		return InvRSquared(delta);
		break;

	case emit_skylight:
		return 1.f;
		break;

	case emit_quakelight:
		// X - r;
		falloff = wl->linear_attn - FastSqrt( DotProduct( delta, delta ) );
		if(falloff < 0)
			return 0.f;

		return falloff;
		break;

	case emit_skyambient:
		return 1.f;
		break;

	case emit_point:
	case emit_spotlight:	// directional & positional
		{
			float dist2, dist;

			dist2 = DotProduct(delta, delta);
			dist = FastSqrt(dist2);

			// Cull out stuff that's too far
			if(wl->radius != 0 && dist > wl->radius)
				return 0.f;

			return 1.f / (wl->constant_attn + wl->linear_attn * dist + wl->quadratic_attn * dist2);
		}

		break;
	}

	return 1.f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( g_debug_stealth_light_grid, "Shows a grid of points previewing the light levels around the player" )
{
	if ( gpGlobals->maxClients > 1 )
		return;

	if ( engine->IsPaused() )
	{
		Msg( "Can't show while paused. Bind this command to a key (debug overlay limitation)\n" );
		return;
	}

	// Show light around the player
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( pPlayer )
	{
		Vector vecColumn = pPlayer->GetAbsOrigin();
		vecColumn.x -= g_debug_stealth_light_grid_radius.GetFloat();
		vecColumn.y -= g_debug_stealth_light_grid_radius.GetFloat();

		float flOffsetAmt = (g_debug_stealth_light_grid_radius.GetFloat() * 2.0f) / g_debug_stealth_light_grid_size.GetFloat();

		for ( int i = 0; i < g_debug_stealth_light_grid_size.GetInt(); i++ )
		{
			Vector vecRow = vecColumn;
			for ( int j = 0; j < g_debug_stealth_light_grid_size.GetInt(); j++ )
			{
				trace_t tr;
				UTIL_TraceLine( pPlayer->EyePosition(), vecRow, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

				// Get the light level at this point and display it
				float flLightLevel = g_StealthLightingSystem.GetLightLevelAtPoint( tr.endpos );
				int nLight = 255.0f * (flLightLevel / ai_stealth_lighting_level_max.GetFloat());
				NDebugOverlay::Cross3D( tr.endpos, 5.0f, nLight, nLight, nLight, true, g_debug_stealth_light_grid_duration.GetFloat() );

				vecRow.y += flOffsetAmt;
			}

			vecColumn.x += flOffsetAmt;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStealthLightingSystem::InitLights( void )
{
	if ( m_pWorldLights )
		return;

	// Get the map path
	const char *pszMapName = modelinfo->GetModelName( modelinfo->GetModel( 1 ) );

	// Open map
	FileHandle_t hFile = g_pFullFileSystem->Open( pszMapName, "rb" );
	if (!hFile)
	{
		Warning( "CStealthLightingSystem: unable to open map\n" );
		return;
	}

	// Read the BSP header. We don't need to do any version checks, etc. as we
	// can safely assume that the engine did this for us
	dheader_t hdr;
	g_pFullFileSystem->Read( &hdr, sizeof( hdr ), hFile );

	// Grab the light lump and seek to it
	lump_t &lightLump = hdr.lumps[LUMP_WORLDLIGHTS];

	// INSOLENCE: If the worldlights lump is empty, that means theres no normal, LDR lights to extract
	//			  This can happen when, for example, the map is compiled in HDR mode only
	//			  So move on to the HDR worldlights lump
	if (lightLump.filelen == 0)
	{
		lightLump = hdr.lumps[LUMP_WORLDLIGHTS_HDR];
	}

	// If we can't divide the lump data into a whole number of worldlights,
	// then the BSP format changed and we're unaware
	if (lightLump.filelen % sizeof( dworldlight_t ))
	{
		Warning( "CStealthLightingSystem: unknown world light lump\n" );

		// Close file
		g_pFullFileSystem->Close( hFile );
		return;
	}

	g_pFullFileSystem->Seek( hFile, lightLump.fileofs, FILESYSTEM_SEEK_HEAD );

	// Allocate memory for the worldlights
	m_nWorldLights = lightLump.filelen / sizeof( dworldlight_t );
	m_pWorldLights = new dworldlight_t[m_nWorldLights];

	// Read worldlights then close
	g_pFullFileSystem->Read( m_pWorldLights, lightLump.filelen, hFile );
	g_pFullFileSystem->Close( hFile );

	DevMsg( "CStealthLightingSystem: load successful (%d lights at 0x%p)\n", m_nWorldLights, m_pWorldLights );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStealthLightingSystem::IsInitialized( void ) const
{
	return m_pWorldLights != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CStealthLightingSystem::IsAvailable( void ) const
{
	if ( !ai_stealth_lighting.GetBool() )
		return false;

	if ( !m_pWorldLights )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStealthLightingSystem::ClearEntityCache( void )
{
	m_EntLightLevels.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: clear worldlights, free memory
//-----------------------------------------------------------------------------
void CStealthLightingSystem::Clear()
{
	m_nWorldLights = 0;

	if(m_pWorldLights)
	{
		delete [] m_pWorldLights;
		m_pWorldLights = NULL;
	}

	m_EntLightLevels.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStealthLightingSystem::GetLightRatioForEntity( CBaseEntity *pEntity, CBaseEntity *pLooker )
{
	if ( !IsInitialized() )
		return 1.0f;

	// Allow use of EP1 darkness system as map-defined exception (and also accounts for flashlights)
	if ( LookerCouldSeeTargetInDarkness( pLooker, pEntity ) )
		return 1.0f;

	float flRatio = (GetLightLevelAtEntity( pEntity ) / ai_stealth_lighting_level_midpoint.GetFloat());

	// Still see players and NPCs in the dark, even slightly. The stealth system will not see them anyway if the player is hidden well enough
	// 
	// TODO:	There's an argument to be made that Combine units have glowing eyes, therefore you should see them better in the dark.
	//			Such an idea can apply to the player in E:Z2 as well as Combine soldiers.
	//			I did consider checking if a NPC has eye glows and having that affect this, but that seems a little too arbitrary.
	//			Consider looking into this properly in the future, although the player should still expect to be able to take advantage of darkness.
	if ( pEntity->IsPlayer() )
	{
		flRatio += ai_stealth_lighting_ratio_min_player.GetFloat();
	}
	else if ( pEntity->IsNPC() )
	{
		flRatio += ai_stealth_lighting_ratio_min_npc.GetFloat();
	}

	//if ( flRatio > 1.0f )
	//	flRatio = 1.0f;

	return flRatio;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStealthLightingSystem::GetLightLevelAtEntity( CBaseEntity *pEntity )
{
	// See if we have a valid one first
	int i = 0;
	for ( ; i < m_EntLightLevels.Count(); i++ )
	{
		if ( m_EntLightLevels[i].hEnt == pEntity )
		{
			if (m_EntLightLevels[i].flLastCheckTime > gpGlobals->curtime)
				return m_EntLightLevels[i].flLightLevel;
			break;
		}
	}
	
	if ( i == m_EntLightLevels.Count() )
	{
		i = m_EntLightLevels.AddToTail();
		m_EntLightLevels[i].hEnt = pEntity;
		m_EntLightLevels[i].vecLastPosition = pEntity->GetAbsOrigin();
	}
	else
	{
		// See if the entity has moved from its last position enough
		Vector vecToLastPos = (m_EntLightLevels[i].vecLastPosition - pEntity->GetAbsOrigin());
		if ( vecToLastPos.LengthSqr() < Square( AI_STEALTH_LIGHTING_CACHE_POS_TOLERANCE ) )
			return m_EntLightLevels[i].flLightLevel;
	}

	// Find the light level at eye position
	float flLightLevel = GetLightLevelAtPoint( pEntity->EyePosition() );
	
	// Don't check again for different times, depending on how much we're expected to move
	if ( pEntity->IsPlayer() )
		m_EntLightLevels[i].flLastCheckTime = gpGlobals->curtime + AI_STEALTH_LIGHTING_CACHE_TIME_PLAYER;
	else if ( pEntity->IsNPC() )
		m_EntLightLevels[i].flLastCheckTime = gpGlobals->curtime + AI_STEALTH_LIGHTING_CACHE_TIME_NPC;
	else
		m_EntLightLevels[i].flLastCheckTime = gpGlobals->curtime + AI_STEALTH_LIGHTING_CACHE_TIME;

	m_EntLightLevels[i].flLightLevel = flLightLevel;
	m_EntLightLevels[i].vecLastPosition = pEntity->GetAbsOrigin();

	if ( g_debug_stealth_light.GetBool() )
	{
		// Show it more often for the player while debugging
		if ( pEntity->IsPlayer() )
		{
			m_EntLightLevels[i].flLastCheckTime = gpGlobals->curtime + 0.2f;
		}

		DevMsg( "%s light level: %.4f\n", pEntity->GetDebugName(), flLightLevel );
	}

	return flLightLevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CStealthLightingSystem::GetLightLevelAtPoint( const Vector &vecOrigin )
{
	// Find the size of the PVS for our current position
	int nCluster = engine->GetClusterForOrigin( vecOrigin );
	int nPVSSize = engine->GetPVSForCluster( nCluster, 0, NULL );

	// Get the PVS at our position
	byte *pvs = new byte[nPVSSize];
	engine->GetPVSForCluster(nCluster, nPVSSize, pvs);

	float flLightLevel = 0.0f;

	// Iterate through all the worldlights
	for(int i = 0; i < m_nWorldLights; ++i)
	{
		dworldlight_t *light = &m_pWorldLights[i];

		// Skip skyambient
		if(light->type == emit_skyambient)
		{
			//engine->Con_NPrintf(i, "%d: skyambient", i);
			continue;
		}

		// Handle sun
		if(light->type == emit_skylight)
		{
			// Calculate sun position
			Vector vecAbsStart = vecOrigin + Vector(0,0,30);
			Vector vecAbsEnd = vecAbsStart - (light->normal * MAX_TRACE_LENGTH);

			trace_t tr;
			CTraceFilterWorldAndPropsOnly traceFilter;
			UTIL_TraceLine(vecOrigin, vecAbsEnd, MASK_OPAQUE, &traceFilter, &tr);

			// If we didn't hit anything then we have a problem
			if(!tr.DidHit())
			{
				//engine->Con_NPrintf(i, "%d: skylight: couldn't touch sky", i);
				continue;
			}

			// If we did hit something, and it wasn't the skybox, then skip
			// this worldlight
			if(!(tr.surface.flags & SURF_SKY) && !(tr.surface.flags & SURF_SKY2D))
			{
				//engine->Con_NPrintf(i, "%d: skylight: no sight to sun", i);
				continue;
			}

			// If we're directly in the sun, then we can assume we're completely visible
			flLightLevel = ai_stealth_lighting_level_max.GetFloat();
			break;
		}

		// Calculate square distance to this worldlight
		Vector vecDelta = light->origin - vecOrigin;
		float flDistSqr = vecDelta.LengthSqr();
		float flRadiusSqr = light->radius * light->radius;

		// Skip lights that are out of our radius
		if(flRadiusSqr > 0 && flDistSqr >= flRadiusSqr)
		{
			//engine->Con_NPrintf(i, "%d: out-of-radius (dist: %d, radius: %d)", i, sqrt(flDistSqr), light->radius);
			continue;
		}

		// Is it out of our PVS?
		if(!engine->CheckOriginInPVS(light->origin, pvs, nPVSSize))
		{
			//engine->Con_NPrintf(i, "%d: out of PVS", i);
			continue;
		}

		// Calculate intensity at our position
		float flRatio = Engine_WorldLightDistanceFalloff(light, vecDelta);

		if ( light->type == emit_spotlight )
		{
			// Account for the cone
			VectorNormalize( vecDelta );
			float flDot = DotProduct( -light->normal, vecDelta );
			if ( flDot < light->stopdot2 )
				continue;

			// Artificially decrease intensity based on how far from the inner cone we are
			flRatio *= RemapValClamped( flDot, light->stopdot2, light->stopdot, 0.0f, 1.0f );
		}

		Vector vecIntensity = light->intensity * flRatio;

		// Can we see the light?
		trace_t tr;
		CTraceFilterWorldAndPropsOnly traceFilter;
		UTIL_TraceLine( vecOrigin, light->origin, MASK_OPAQUE, &traceFilter, &tr );

		if(tr.DidHit())
		{
			//engine->Con_NPrintf(i, "%d: trace failed", i);
			continue;
		}

		flLightLevel += vecIntensity.Length();

		//engine->Con_NPrintf(i, "%d: set (%.2f)", i, vecIntensity.Length());

		if ( flLightLevel >= ai_stealth_lighting_level_max.GetFloat() )
		{
			flLightLevel = ai_stealth_lighting_level_max.GetFloat();
			break;
		}
	}

	delete[] pvs;

	return flLightLevel;
}
