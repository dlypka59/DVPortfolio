// MainFrm.h : interface of the CMainFrame class
//
#pragma once

#include "VideoView.h"

class CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame() noexcept;

	friend CVideoView;

	// Attributes
public:
	double GetPlayFps() const { return m_playFps; }
	void SetPlayFps(double fps);
	void InitPlayFpsFromVideo(double nativeFps);

	void UpdateViewCountStatus();

	void StartAllPlayback();
	void StopAllPlayback();
	void ToggleAllPlayback();

	// Operations
public:

	// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE,
		CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr);

	// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CMFCMenuBar       m_wndMenuBar;
	CMFCToolBar       m_wndToolBar;
	CMFCStatusBar     m_wndStatusBar;
	CMFCToolBarImages m_UserImages;

	// Global playback speed (frames per second). UI slider lives on each CVideoView.
	double m_playFps = 30.0;
	bool   m_playFpsUserSet = false;

	// Generated message map functions
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWindowManager();
	afx_msg void OnViewCustomize();
	afx_msg LRESULT OnToolbarCreateNew(WPARAM wp, LPARAM lp);
	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);

	afx_msg void OnPlayAll();
	afx_msg void OnPauseAll();
	afx_msg void OnTogglePlayAll();
	afx_msg void OnUpdatePlayAll(CCmdUI* pCmdUI);
	afx_msg void OnUpdatePauseAll(CCmdUI* pCmdUI);

	DECLARE_MESSAGE_MAP()
};