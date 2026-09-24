//=============================================================================//
//
// Purpose:		Progenitor sidearm functions
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "npc_progenitor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern Activity ACT_IDLE_ANGRY_SIDEARM;
extern Activity ACT_RANGE_ATTACK_SIDEARM;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Progenitor::InputSidearmFireAtTarget( inputdata_t &inputdata )
{
	char szParam[128];
	V_strncpy( szParam, inputdata.value.String(), sizeof( szParam ) );

	char *pszSpace = V_strstr( szParam, " " );
	if ( pszSpace )
	{
		// Wait time specified
		m_flSidearmWaitTime = atof( pszSpace + 1 );
		*pszSpace = '\0';
	}
	else
		m_flSidearmWaitTime = 0.1f;

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, szParam, this, inputdata.pActivator, inputdata.pCaller );
	if ( !pTarget )
		return;

	m_hForcedSidearmTarget = pTarget;

	ClearSchedule( "Told to fire sidearm at target" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::CanUseSidearm()
{
	if ( !m_bUseSidearm )
		return false;

	if ( IsPropShieldEquipped() )
		return false;

	if ( !GetEnemy() || !HasCondition( COND_SEE_ENEMY ) || HasCondition( COND_ENEMY_DEAD ) )
		return false;

	if ( m_flNextSidearmUseTime > gpGlobals->curtime )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::RunSidearmAim()
{
	CBaseEntity *pEnemy = GetEnemy();

	if ( GetState() == NPC_STATE_COMBAT )
	{
		// Abandon if too close
		float flDistSqr = ( GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();
		if ( flDistSqr < Square( 50.0f ) )
			return false;

		// Out of ammo
		if ( m_hLeftHandWeapon && m_hLeftHandWeapon->m_iClip1 <= 0 )
			return false;

		if ( ShouldChooseNewEnemy() || !HasCondition( COND_SEE_ENEMY ) )
		{
			// See if we should switch targets
			CBaseEntity *pNewEnemy = BestEnemy();
			if ( pNewEnemy && pNewEnemy != GetEnemy() && pNewEnemy->IsAlive() && FVisible( pNewEnemy ) )
			{
				SetEnemy( pNewEnemy );
				return true;
			}
		}

		// If we have a clear shot, then fire
		if ( HasCondition( COND_SEE_ENEMY ) && m_flNextSidearmFireTime < gpGlobals->curtime &&
			( !m_bLaserOn
				|| ( m_bLaserAimsAtEnemy && gpGlobals->curtime - m_flLaserTargetTime > 0.5f )
				|| ( flDistSqr < Square( 150.0f ) ) ) )
		{
			SetActivity( ACT_RANGE_ATTACK_SIDEARM );
			return true;
		}
		else if ( gpGlobals->curtime - GetEnemies()->LastTimeSeen( pEnemy ) > 3.0f )
		{
			// Stop aiming with sidearm
			return false;
		}
	}
	else
	{
		// TODO: Scan around
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Vector CNPC_Progenitor::GetSidearmIdleAim()
{
	// Rough aiming position
	Vector vecRef, vecDir, vecRight;
	float flDistance = 200.0f;

	CSound *pBestSound = GetBestSound();
	if ( pBestSound )
	{
		vecRef = pBestSound->GetSoundReactOrigin();

		vecDir = (vecRef - GetAbsOrigin());
		VectorNormalize( vecDir );

		QAngle angToRef;
		VectorAngles( vecDir, angToRef );
		AngleVectors( angToRef, &vecDir, &vecRight, NULL );

		// 10 seconds = 200 units
		flDistance = gpGlobals->curtime - GetStealthSenses()->GetLastSoundTime();
		flDistance *= flDistance * 2.0f;
		if (flDistance > 200.0f)
			flDistance = 200.0f;
	}
	else
	{
		GetVectors( &vecDir, &vecRight, NULL );
		vecRef = GetAbsOrigin() + (vecDir * 256.0f);
	}

	vecRef.z += GetViewOffset().z;

	// Scan from left to right
	Vector vecLaserStart = vecRef + (vecRight * flDistance);
	Vector vecLaserEnd = vecRef - (vecRight * flDistance);

	float t = (sin( gpGlobals->curtime * 2.0f ) * 0.5f) + 0.5f;
	return VectorLerp( vecLaserStart, vecLaserEnd, t );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseAnimating *CNPC_Progenitor::GetActiveLaserWeapon()
{
	if (m_hLeftHandGun)
	{
		return m_hLeftHandGun;
	}

	return BaseClass::GetActiveLaserWeapon();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CNPC_Progenitor::GetLaserTarget()
{
	if (m_hForcedSidearmTarget && m_hLeftHandGun)
	{
		return m_hForcedSidearmTarget;
	}

	return BaseClass::GetLaserTarget();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Vector CNPC_Progenitor::GetLaserTargetPos( CBaseEntity *pTarget, const Vector &posSrc )
{
	if (m_hLeftHandGun)
	{
		if ( pTarget == m_hForcedSidearmTarget )
		{
			return m_hForcedSidearmTarget->EyePosition();
		}
		else
		{
			// Custom version of GetShootEnemyDir()
			Vector vecEnemyLKP;
			if ( GetEnemies()->HasMemory( pTarget ) )
				vecEnemyLKP = GetEnemies()->LastSeenPosition( pTarget ); // LastKnownPosition
			else
				vecEnemyLKP = pTarget->GetAbsOrigin();

			Vector vecEnemyOffset;

			if ( pTarget->IsNPC() && pTarget->GetMoveType() != MOVETYPE_VPHYSICS )
			{
				// Aim for the head, like players do
				vecEnemyOffset = pTarget->HeadTarget( posSrc ) - pTarget->GetAbsOrigin();
				vecEnemyOffset.z -= 4.0f;

				// Make it easier to hit zombies
				if ( pTarget->Classify() == CLASS_ZOMBIE )
					vecEnemyOffset.z -= 2.0f;
			}
			else
			{
				vecEnemyOffset = pTarget->BodyTarget( posSrc ) - pTarget->GetAbsOrigin();
			}

			// 1000 units away = max offset of 1
			float flOffsetRandom = ( vecEnemyLKP - GetAbsOrigin() ).LengthSqr() / Square( 1000.0f );
			vecEnemyOffset += RandomVector( -flOffsetRandom, flOffsetRandom );

			// The more they're moving, the more noise we have
			//Vector vecEnemyVel = pTarget->GetSmoothedVelocity();
			//float flSpeed = VectorNormalize( vecEnemyVel );
			//vecEnemyOffset += vecEnemyVel * RandomVector( 0.0f, flSpeed * 0.01f );

			return vecEnemyOffset + vecEnemyLKP;
		}
	}

	return BaseClass::GetLaserTargetPos( pTarget, posSrc );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CNPC_Progenitor::ShouldAimLaserAtEnemy( CBaseEntity *pEnemy )
{
	// Sidearm has different rules
	if ( m_hLeftHandGun )
	{
		if ( !pEnemy )
			return false;

		if ( GetActivity() == ACT_RANGE_ATTACK_SIDEARM )
		{
			// If we just fired, don't aim laser
			if ( GetCycle() < 1.0f )
				return false;
		}
		else if ( GetActivity() != ACT_IDLE_ANGRY_SIDEARM )
			return false;

		// Dot tolerance varies based on body direction
		Vector2D vecToTarget2D = (pEnemy->EyePosition().AsVector2D() - EyePosition().AsVector2D());
		Vector2DNormalize( vecToTarget2D );
		float flBodyDot = DotProduct2D( BodyDirection3D().AsVector2D(), vecToTarget2D );

		if ( GetLaserDotToTarget( pEnemy ) < MAX( 0.95 * flBodyDot, DOT_45DEGREE ) || !FVisible( pEnemy ) )
		{
			return false;
		}

		return true;
	}

	return BaseClass::ShouldAimLaserAtEnemy( pEnemy );
}

//-----------------------------------------------------------------------------
// Purpose: degrees to turn in 0.1 seconds
//-----------------------------------------------------------------------------
float CNPC_Progenitor::MaxYawSpeed( void )
{
	if ( GetActivity() == ACT_IDLE_ANGRY_SIDEARM || GetActivity() == ACT_RANGE_ATTACK_SIDEARM )
		return 15;

	return BaseClass::MaxYawSpeed();
}
