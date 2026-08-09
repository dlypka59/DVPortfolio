// MainFrm.cpp : implementation of the CMainFrame class
//

#include "pch.h"
#include "framework.h"
#include "DVPortfolioWin11.h"
#include "VideoView.h"
#include <functional>
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWndEx)

const int  iMaxUserToolbars = 10;
const UINT uiFirstUserToolBarId = AFX_IDW_CONTROLBAR_FIRST + 40;
const UINT uiLastUserToolBarId = uiFirstUserToolBarId + iMaxUserToolbars - 1;

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWndEx)
	ON_WM_CREATE()
	ON_COMMAND(ID_WINDOW_MANAGER, &CMainFrame::OnWindowManager)
	ON_COMMAND(ID_VIEW_CUSTOMIZE, &CMainFrame::OnViewCustomize)
	ON_REGISTERED_MESSAGE(AFX_WM_CREATETOOLBAR, &CMainFrame::OnToolbarCreateNew)
	ON_COMMAND_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnApplicationLook)
	ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_APPLOOK_WIN_2000, ID_VIEW_APPLOOK_WINDOWS_7, &CMainFrame::OnUpdateApplicationLook)
	ON_COMMAND(ID_PLAY_ALL, &CMainFrame::OnPlayAll)
	ON_COMMAND(ID_PAUSE_ALL, &CMainFrame::OnPauseAll)
	ON_COMMAND(ID_TOGGLE_PLAY_ALL, &CMainFrame::OnTogglePlayAll)
	ON_UPDATE_COMMAND_UI(ID_PLAY_ALL, &CMainFrame::OnUpdatePlayAll)
	ON_UPDATE_COMMAND_UI(ID_PAUSE_ALL, &CMainFrame::OnUpdatePauseAll)
END_MESSAGE_MAP()

CMainFrame::CMainFrame() noexcept
{
	theApp.m_nAppLook = theApp.GetInt(_T("ApplicationLook"), ID_VIEW_APPLOOK_VS_2008);
	m_playFps = 30.0;
	m_playFpsUserSet = false;
}

CMainFrame::~CMainFrame()
{}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	BOOL bNameValid;

	// ---- Menu bar ----
	if (!m_wndMenuBar.Create(this))
	{
		TRACE0("Failed to create menubar\n");
		return -1;
	}

	m_wndMenuBar.SetPaneStyle(
		m_wndMenuBar.GetPaneStyle() | CBRS_SIZE_DYNAMIC | CBRS_TOOLTIPS | CBRS_FLYBY);

	CMFCPopupMenu::SetForceMenuFocus(FALSE);

	// ---- Toolbar ----
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT,
		WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(theApp.m_bHiColorIcons ? IDR_MAINFRAME_256 : IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;
	}

	CString strToolBarName;
	bNameValid = strToolBarName.LoadString(IDS_TOOLBAR_STANDARD);
	ASSERT(bNameValid);
	m_wndToolBar.SetWindowText(strToolBarName);

	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);
	m_wndToolBar.EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);

	InitUserToolbars(nullptr, uiFirstUserToolBarId, uiLastUserToolBarId);

	// ---- Status bar: pane 0 = Ready, pane 1 = view count ----
	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;
	}

	static UINT statusIndicators[] =
	{
		ID_SEPARATOR,
		ID_SEPARATOR
	};

	if (!m_wndStatusBar.SetIndicators(statusIndicators, 2))
	{
		TRACE0("Failed to set status bar indicators\n");
		return -1;
	}

	m_wndStatusBar.SetPaneInfo(0, ID_SEPARATOR, SBPS_STRETCH, 1);
	m_wndStatusBar.SetPaneInfo(1, ID_SEPARATOR, SBPS_NORMAL, 100);
	m_wndStatusBar.SetPaneText(1, _T("Views: 0"));

	// Global play FPS is stored on this frame (m_playFps).
	// The FPS slider UI is on each CVideoView (avoids CDialogBar issues).
	m_playFps = 30.0;
	m_playFpsUserSet = false;

	// ---- Docking ----
	m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndMenuBar);
	DockPane(&m_wndToolBar);

	CDockingManager::SetDockingMode(DT_SMART);
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	OnApplicationLook(theApp.m_nAppLook);

	EnableWindowsDialog(ID_WINDOW_MANAGER, ID_WINDOW_MANAGER, TRUE);
	EnablePaneMenu(TRUE, ID_VIEW_CUSTOMIZE, strCustomize, ID_VIEW_TOOLBAR);
	CMFCToolBar::EnableQuickCustomization();

	if (CMFCToolBar::GetUserImages() == nullptr)
	{
		if (m_UserImages.Load(_T(".\\UserImages.bmp")))
			CMFCToolBar::SetUserImages(&m_UserImages);
	}

	CList<UINT, UINT> lstBasicCommands;
	lstBasicCommands.AddTail(ID_FILE_NEW);
	lstBasicCommands.AddTail(ID_FILE_OPEN);
	lstBasicCommands.AddTail(ID_FILE_SAVE);
	lstBasicCommands.AddTail(ID_APP_EXIT);
	lstBasicCommands.AddTail(ID_EDIT_CUT);
	lstBasicCommands.AddTail(ID_EDIT_PASTE);
	lstBasicCommands.AddTail(ID_EDIT_UNDO);
	lstBasicCommands.AddTail(ID_APP_ABOUT);
	lstBasicCommands.AddTail(ID_VIEW_STATUS_BAR);
	lstBasicCommands.AddTail(ID_VIEW_TOOLBAR);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2003);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_VS_2005);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_BLUE);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_SILVER);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_BLACK);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_OFF_2007_AQUA);
	lstBasicCommands.AddTail(ID_VIEW_APPLOOK_WINDOWS_7);
	CMFCToolBar::SetBasicCommands(lstBasicCommands);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CMDIFrameWndEx::PreCreateWindow(cs))
		return FALSE;
	return TRUE;
}

void CMainFrame::UpdateViewCountStatus()
{
	int count = 0;

	CWinApp* pApp = AfxGetApp();
	if (pApp)
	{
		POSITION posTemplate = pApp->GetFirstDocTemplatePosition();
		while (posTemplate)
		{
			CDocTemplate* pTemplate = pApp->GetNextDocTemplate(posTemplate);
			if (!pTemplate)
				continue;

			POSITION posDoc = pTemplate->GetFirstDocPosition();
			while (posDoc)
			{
				CDocument* pDoc = pTemplate->GetNextDoc(posDoc);
				if (!pDoc)
					continue;

				POSITION posView = pDoc->GetFirstViewPosition();
				while (posView)
				{
					CView* pView = pDoc->GetNextView(posView);
					auto* pVideoView = DYNAMIC_DOWNCAST(CVideoView, pView);
					if (pVideoView != nullptr && pVideoView->HasVideo())
						++count;
				}
			}
		}
	}

	CString text;
	text.Format(_T("Views: %d"), count);

	if (m_wndStatusBar.GetSafeHwnd())
		m_wndStatusBar.SetPaneText(1, text);
}

static void ForEachVideoView(const std::function<void(CVideoView*)>& fn)
{
	CWinApp* pApp = AfxGetApp();
	if (!pApp)
		return;

	POSITION posTemplate = pApp->GetFirstDocTemplatePosition();
	while (posTemplate)
	{
		CDocTemplate* pTemplate = pApp->GetNextDocTemplate(posTemplate);
		if (!pTemplate)
			continue;

		POSITION posDoc = pTemplate->GetFirstDocPosition();
		while (posDoc)
		{
			CDocument* pDoc = pTemplate->GetNextDoc(posDoc);
			if (!pDoc)
				continue;

			POSITION posView = pDoc->GetFirstViewPosition();
			while (posView)
			{
				CView* pView = pDoc->GetNextView(posView);
				auto* v = DYNAMIC_DOWNCAST(CVideoView, pView);
				if (v && v->HasVideo())
					fn(v);
			}
		}
	}
}

void CMainFrame::StartAllPlayback()
{
	ForEachVideoView([](CVideoView* v) { v->StartPlayback(); });
}

void CMainFrame::StopAllPlayback()
{
	ForEachVideoView([](CVideoView* v) { v->StopPlayback(); });
}

void CMainFrame::ToggleAllPlayback()
{
	bool anyPlaying = false;
	ForEachVideoView([&](CVideoView* v)
		{
			if (v->IsPlaying())
				anyPlaying = true;
		});

	if (anyPlaying)
		StopAllPlayback();
	else
		StartAllPlayback();
}

void CMainFrame::OnPlayAll()
{
	StartAllPlayback();
}

void CMainFrame::OnPauseAll()
{
	StopAllPlayback();
}

void CMainFrame::OnTogglePlayAll()
{
	ToggleAllPlayback();
}

void CMainFrame::OnUpdatePlayAll(CCmdUI* pCmdUI)
{
	bool any = false;
	ForEachVideoView([&](CVideoView*) { any = true; });
	pCmdUI->Enable(any);
}

void CMainFrame::OnUpdatePauseAll(CCmdUI* pCmdUI)
{
	bool anyPlaying = false;
	ForEachVideoView([&](CVideoView* v)
		{
			if (v->IsPlaying())
				anyPlaying = true;
		});
	pCmdUI->Enable(anyPlaying);
}

void CMainFrame::SetPlayFps(double fps)
{
	if (fps < 1.0)
		fps = 1.0;
	if (fps > 240.0)
		fps = 240.0;

	m_playFps = fps;

	// Restart timers on views that are playing
	ForEachVideoView([](CVideoView* v)
		{
			if (v->IsPlaying())
			{
				v->StopPlayback();
				v->StartPlayback();
			}
		});

	// Keep each view's FPS slider thumb in sync
	ForEachVideoView([](CVideoView* v)
		{
			v->SyncFpsSliderFromMain();
		});
}

void CMainFrame::InitPlayFpsFromVideo(double nativeFps)
{
	if (m_playFpsUserSet)
		return;
	if (nativeFps < 1.0)
		nativeFps = 30.0;
	SetPlayFps(nativeFps);
}

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWndEx::Dump(dc);
}
#endif

void CMainFrame::OnWindowManager()
{
	ShowWindowsDialog();
}

void CMainFrame::OnViewCustomize()
{
	CMFCToolBarsCustomizeDialog* pDlgCust =
		new CMFCToolBarsCustomizeDialog(this, TRUE);
	pDlgCust->EnableUserDefinedToolbars();
	pDlgCust->Create();
}

LRESULT CMainFrame::OnToolbarCreateNew(WPARAM wp, LPARAM lp)
{
	LRESULT lres = CMDIFrameWndEx::OnToolbarCreateNew(wp, lp);
	if (lres == 0)
		return 0;

	CMFCToolBar* pUserToolbar = (CMFCToolBar*)lres;
	ASSERT_VALID(pUserToolbar);

	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);
	pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
	return lres;
}

void CMainFrame::OnApplicationLook(UINT id)
{
	CWaitCursor wait;
	theApp.m_nAppLook = id;

	switch (theApp.m_nAppLook)
	{
	case ID_VIEW_APPLOOK_WIN_2000:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManager));
		break;
	case ID_VIEW_APPLOOK_OFF_XP:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOfficeXP));
		break;
	case ID_VIEW_APPLOOK_WIN_XP:
		CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
		break;
	case ID_VIEW_APPLOOK_OFF_2003:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2003));
		CDockingManager::SetDockingMode(DT_SMART);
		break;
	case ID_VIEW_APPLOOK_VS_2005:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));
		CDockingManager::SetDockingMode(DT_SMART);
		break;
	case ID_VIEW_APPLOOK_VS_2008:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2008));
		CDockingManager::SetDockingMode(DT_SMART);
		break;
	case ID_VIEW_APPLOOK_WINDOWS_7:
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
		CDockingManager::SetDockingMode(DT_SMART);
		break;
	default:
		switch (theApp.m_nAppLook)
		{
		case ID_VIEW_APPLOOK_OFF_2007_BLUE:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_LunaBlue);
			break;
		case ID_VIEW_APPLOOK_OFF_2007_BLACK:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_ObsidianBlack);
			break;
		case ID_VIEW_APPLOOK_OFF_2007_SILVER:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Silver);
			break;
		case ID_VIEW_APPLOOK_OFF_2007_AQUA:
			CMFCVisualManagerOffice2007::SetStyle(CMFCVisualManagerOffice2007::Office2007_Aqua);
			break;
		}
		CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerOffice2007));
		CDockingManager::SetDockingMode(DT_SMART);
	}

	RedrawWindow(nullptr, nullptr, RDW_ALLCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME | RDW_ERASE);
	theApp.WriteInt(_T("ApplicationLook"), theApp.m_nAppLook);
}

void CMainFrame::OnUpdateApplicationLook(CCmdUI* pCmdUI)
{
	pCmdUI->SetRadio(theApp.m_nAppLook == pCmdUI->m_nID);
}

BOOL CMainFrame::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)
{
	if (!CMDIFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
		return FALSE;

	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	for (int i = 0; i < iMaxUserToolbars; i++)
	{
		CMFCToolBar* pUserToolbar = GetUserToolBarByIndex(i);
		if (pUserToolbar != nullptr)
			pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
	}

	return TRUE;
}