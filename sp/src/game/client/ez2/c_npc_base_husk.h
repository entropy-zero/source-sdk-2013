#ifndef C_NPC_BaseHusk_H
#define C_NPC_BaseHusk_H
#ifdef _WIN32
#pragma once
#endif

#include "c_ai_basenpc.h"

//-----------------------------------------------------------------------------
// Classes can cast to this and access some basic husk functions.
//-----------------------------------------------------------------------------
abstract_class C_AI_HuskSink
{
public:
	virtual int GetHuskAggressionLevel() = 0;
	virtual int GetHuskCognitionFlags() = 0;
};

//-----------------------------------------------------------------------------
// Husk template class
//-----------------------------------------------------------------------------
template <class BASE_NPC>
class C_NPC_BaseHusk : public BASE_NPC, public C_AI_HuskSink
{
public:
	DECLARE_CLASS( C_NPC_BaseHusk, BASE_NPC );

	// From CAI_HuskSink
	int GetHuskAggressionLevel() override { return this->m_nHuskAggressionLevel; }
	int GetHuskCognitionFlags() override { return this->m_nHuskCognitionFlags; }

	int		m_nHuskAggressionLevel;
	int		m_nHuskCognitionFlags;
	bool	m_bAlliedWithPlayer;
};

//-----------------------------------------------------------------------------

#define HUSK_CLIENT_STUB( className, baseClass, entityClass ) \
class C_NPC_Husk##className : public C_NPC_BaseHusk<baseClass> \
{ \
public: \
	DECLARE_CLASS( C_NPC_Husk##className, C_NPC_BaseHusk<baseClass> ); \
	DECLARE_CLIENTCLASS(); \
}; \
LINK_ENTITY_TO_CLASS( entityClass, C_NPC_Husk##className ); \
IMPLEMENT_CLIENTCLASS_DT( C_NPC_Husk##className, DT_NPC_Husk##className, CNPC_Husk##className ) \
	RecvPropInt( RECVINFO( m_nHuskAggressionLevel ) ), \
	RecvPropInt( RECVINFO( m_nHuskCognitionFlags ) ), \
	RecvPropBool( RECVINFO( m_bAlliedWithPlayer ) ), \
END_RECV_TABLE()

//-----------------------------------------------------------------------------

#endif // C_NPC_BaseHusk_H