//=============================================================================//
//
// Purpose: Unfortunate vortiguants that have succumbed to headcrabs
// 		either in Xen or in the temporal anomaly in the Arctic.
//
//=============================================================================//

#include "npc_vortigaunt_episodic.h"

#define TLK_VORT_CHARGE "TLK_VORT_CHARGE"

//=========================================================
//	>> CNPC_Zombigaunt
//=========================================================
class CNPC_Zombigaunt : public CNPC_Vortigaunt
{
	DECLARE_CLASS( CNPC_Zombigaunt, CNPC_Vortigaunt );

public:
	virtual void	Spawn( void );
	virtual void	Precache( void );

	virtual bool	SelectIdleSpeech( AISpeechSelection_t *pSelection );
	virtual void	OnStartSchedule( int scheduleType );

protected:
	// Glowing eyes
	int					GetNumGlows() { return 0; } // No glows under headcrabs

	virtual Class_T		Classify ( void ) { return CLASS_ZOMBIE; }
	
	virtual bool	ShouldMoveAndShoot( void ) { return false; } // Zombigaunts never move and shoot, even if normal Vortigaunts would

	virtual Activity	NPC_TranslateActivity( Activity eNewActivity );

	virtual void		HandleAnimEvent( animevent_t *pEvent );

	bool IsJumpLegal( const Vector &startPos, const Vector &apex, const Vector &endPos ) const;
	bool MovementCost( int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost );

	// Since all dynamic interactions are shared with vortigaunts, zombigaunts cannot use dynamic interactions
	virtual bool CanRunAScriptedNPCInteraction( bool bForced = false ) { return false;  }

	// Zombigaunts have a much slower recharge time than vortigaunts, making them more likely to close in for the kill
	virtual float GetNextRangeAttackTime( void ) { return gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f ); }

public:
	DECLARE_DATADESC();
};
