//=============================================================================//
//
// Purpose:		Base class for conscript NPCs
//
// Author:		Blixibon
//
//=============================================================================//

#ifndef NPC_CONSCRIPT_BASE_H
#define NPC_CONSCRIPT_BASE_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"

#define	DEFINE_CONSCRIPT_DATADESC() \
	DEFINE_INPUT( m_bCombineAligned, FIELD_BOOLEAN, "SetCombineAligned" ),	\

//-----------------------------------------------------------------------------
// Template class for conscript NPCs.
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class CAI_ConscriptBase : public BASE_NPC
{
	DECLARE_CLASS_NOFRIEND( CAI_ConscriptBase, BASE_NPC );

public:
	CAI_ConscriptBase() { }

	Class_T Classify ( void )
	{
		if ( this->IsCombineAligned() )
			return CLASS_COMBINE;

		return CLASS_CONSCRIPT;
	}

	Disposition_t IRelationType( CBaseEntity *pTarget )
	{
		// Old way (for NPCs that are still not classified as conscripts)
		if ( pTarget && pTarget->HasContext( "conscript", "1" ) )
			return D_LI;

		return BaseClass::IRelationType( pTarget );
	}

	void ModifyOrAppendCriteria( AI_CriteriaSet &set )
	{
		BaseClass::ModifyOrAppendCriteria( set );

		set.AppendCriteria( "combinealigned", this->m_bCombineAligned ? "1" : "0" );
	}

	bool			IsCombineAligned() const { return this->m_bCombineAligned; }
	bool			IsConscript() { return true; }

protected: // We can't have any private saved variables because only derived classes use the datadescs

	bool			m_bCombineAligned;
};

#endif
