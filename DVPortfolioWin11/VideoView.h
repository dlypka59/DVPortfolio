
// VideoView.h : interface of the CVideoView class
//

#pragma once

#include <afxwin.h>

#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>

using Microsoft::WRL::ComPtr;

class CVideoView : public CView
{
private:
	ComPtr<ID2D1Factory1>          m_d2dFactory;
	ComPtr<ID2D1Device>            m_d2dDevice;
	ComPtr<ID2D1DeviceContext>     m_d2dContext;
	ComPtr<IDXGISwapChain1>        m_swapChain;
	ComPtr<ID2D1Bitmap1>           m_targetBitmap;
	ComPtr<ID2D1Bitmap>            m_frameBitmap;
	ComPtr<ID3D11Device>		   m_d3dDevice;
	ComPtr<IDXGIDevice>			   m_dxgiDevice;

	bool CreateDeviceResources();
	void DiscardDeviceResources();
	bool CreateFrameBitmap();
	void OnResize(UINT width, UINT height);

	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);


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
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;

protected:

// Implementation
public:
	virtual ~CVideoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
	virtual void OnInitialUpdate() override;

protected:

// Generated message map functions
protected:
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in VideoView.cpp
inline CVideoDoc* CVideoView::GetDocument() const
   { return reinterpret_cast<CVideoDoc*>(m_pDocument); }
#endif

