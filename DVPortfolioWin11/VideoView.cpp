
// VideoView.cpp : implementation of the CVideoView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "DVPortfolioWin11.h"
#endif

#include "VideoDoc.h"
#include "VideoView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CVideoView

IMPLEMENT_DYNCREATE(CVideoView, CView)

BEGIN_MESSAGE_MAP(CVideoView, CView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CVideoView construction/destruction

CVideoView::CVideoView() noexcept
{
	// TODO: add construction code here

}

CVideoView::~CVideoView()
{
}

BOOL CVideoView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

// CVideoView drawing

void CVideoView::OnDraw(CDC* /*pDC*/)
{
	CVideoDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// TODO: add draw code for native data here
}

void CVideoView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CVideoView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CVideoView diagnostics

#ifdef _DEBUG
void CVideoView::AssertValid() const
{
	CView::AssertValid();
}

void CVideoView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CVideoDoc* CVideoView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CVideoDoc)));
	return (CVideoDoc*)m_pDocument;
}
#endif //_DEBUG


// CVideoView message handlers
