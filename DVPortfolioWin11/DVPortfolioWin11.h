
// DVPortfolioWin11.h : main header file for the DVPortfolioWin11 application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CDVPortfolioWin11App:
// See DVPortfolioWin11.cpp for the implementation of this class
//

class CDVPortfolioWin11App : public CWinAppEx
{
public:
	CDVPortfolioWin11App() noexcept;


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;

	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();

	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CDVPortfolioWin11App theApp;
