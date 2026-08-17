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
#include <memory>

#include "MFVideoReader.h"

class CVideoDoc;

using Microsoft::WRL::ComPtr;

class CVideoView : public CView
{
protected: // create from serialization only
	CVideoView() noexcept;
	DECLARE_DYNCREATE(CVideoView)

	// Attributes
public:
	CVideoDoc* GetDocument() const;

	std::unique_ptr<MFVideoReader> m_reader;

	bool HasVideo() const { return m_reader && m_reader->IsOpen(); }
	int64_t GetCurrentFrame() const;
	int64_t GetTotalFrames() const;
	double  GetFrameRate() const;
	void SetCurrentFrame(int64_t frame);

	bool IsPlaying() const { return m_playing; }
	void StartPlayback();
	void StopPlayback();
	void TogglePlayback();

	void RefreshStatusBarForThisView();
	void SyncFpsSliderFromMain();

	// Overrides
public:
	virtual void OnDraw(CDC* pDC) override;
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;
	virtual void OnInitialUpdate() override;
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

	// Implementation
public:
	virtual ~CVideoView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	// ---- Direct2D / DXGI ----
	ComPtr<ID2D1Factory1>      m_d2dFactory;
	ComPtr<ID2D1Device>        m_d2dDevice;
	ComPtr<ID2D1DeviceContext> m_d2dContext;
	ComPtr<IDXGISwapChain1>    m_swapChain;
	ComPtr<ID2D1Bitmap1>       m_targetBitmap;
	ComPtr<ID2D1Bitmap>        m_frameBitmap;
	ComPtr<ID3D11Device>       m_d3dDevice;
	ComPtr<IDXGIDevice>        m_dxgiDevice;

	bool CreateDeviceResources();
	void DiscardDeviceResources();
	bool CreateFrameBitmap();
	void OnResize(UINT width, UINT height);

	// ---- Play ----
	bool m_playing = false;
	bool m_spaceBusy = false;
	static const UINT_PTR s_playTimerId = 1;

	// ---- Zoom / pan ----
	float m_zoom = 1.0f;
	float m_panX = 0.0f;
	float m_panY = 0.0f;
	bool  m_panning = false;
	CPoint m_lastPanPoint{};

	void ClampPan();
	void ResetViewTransform();

	// ---- Bottom chrome: FPS (global) + seek (this view) + status ----
	CSliderCtrl m_fpsSlider;
	CSliderCtrl m_seekSlider;
	CStatic     m_viewStatus;
	CStatic     m_fpsLabel;

	bool m_updatingFpsSlider = false;
	bool m_updatingSeekSlider = false;

	static const int kFpsHeight = 24;
	static const int kSeekHeight = 28;
	static const int kViewStatusHeight = 22;
	static const int kFpsLabelWidth = 36;

	CRect GetVideoClientRect() const;
	void LayoutBottomControls(int cx, int cy);
	void SyncSeekSliderFromFrame();
	void ApplySeekSliderToFrame();

	// Generated message map functions
protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnFilePrintPreview();
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG
inline CVideoDoc* CVideoView::GetDocument() const
{
	return reinterpret_cast<CVideoDoc*>(m_pDocument);
}
#endif