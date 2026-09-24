//=============================================================================//
//
// Purpose:		Manager for stealth mechanics.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"

DECLARE_AUTO_LIST( IStealthManagerAutoList );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_AI_StealthManager : public C_BaseEntity, public IStealthManagerAutoList
{
public:
	DECLARE_CLASS( C_AI_StealthManager, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	inline bool IsActive() { return !m_bDisabled; } // !IsDormant()

	void	OnDataChanged( DataUpdateType_t type );

	bool	IsStealthLevel( int nStealthLevel ) const { return m_nStealthLevel == nStealthLevel; }
	bool	IsStealthLevel( int nMinStealthLevel, int nMaxStealthLevel ) const { return m_nStealthLevel >= nMinStealthLevel && m_nStealthLevel <= nMaxStealthLevel; }

private:
	int		m_nStealthLevel;
	int		m_nOldStealthLevel;
	bool	m_bDisabled;
	bool	m_bChangedSoundMixer;
};

extern C_AI_StealthManager *GetStealthManager();
