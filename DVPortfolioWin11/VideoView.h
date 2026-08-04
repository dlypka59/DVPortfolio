
// VideoView.h : interface of the CVideoView class
//

#pragma once


class CVideoView : public CView
{
protected: // create from serialization only
	CVideoView() noexcept;
	DECLARE_DYNCREATE(CVideoView)

// Attributes
public:
	CVideoDoc* GetDocument() const;

// Operations
public:

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~CVideoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in VideoView.cpp
inline CVideoDoc* CVideoView::GetDocument() const
   { return reinterpret_cast<CVideoDoc*>(m_pDocument); }
#endif

