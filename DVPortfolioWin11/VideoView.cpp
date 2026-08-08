
// VideoView.cpp : implementation of the CVideoView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "DVPortfolioWin11.h"
#endif

#include "MainFrm.h"
#include "VideoDoc.h"
#include "VideoView.h"

#include <d2d1_1.h>
#pragma comment(lib, "d2d1.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CVideoView

IMPLEMENT_DYNCREATE(CVideoView, CView)

BEGIN_MESSAGE_MAP(CVideoView, CView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
    ON_WM_KEYDOWN()
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()


int64_t CVideoView::GetCurrentFrame() const
{
    return HasVideo() ? m_reader->GetCurrentFrame() : 0;
}

int64_t CVideoView::GetTotalFrames() const
{
    return HasVideo() ? m_reader->GetTotalFrames() : 0;
}

double CVideoView::GetFrameRate() const
{
    return HasVideo() ? m_reader->GetFrameRate() : 0.0;
}

void CVideoView::SetCurrentFrame(int64_t frame)
{
    if (!HasVideo())
        return;

    if (frame < 0)
        frame = 0;
    const int64_t total = m_reader->GetTotalFrames();
    if (total > 0 && frame >= total)
        frame = total - 1;

    const int64_t current = m_reader->GetCurrentFrame();
    if (frame == current)
        return;

    if (frame == current + 1)
    {
        if (m_reader->ReadCurrentFrame())
        {
            RefreshStatusBarForThisView();
            Invalidate(FALSE);
        }
        return;
    }

    if (m_reader->DecodeToFrame(frame))
    {
        RefreshStatusBarForThisView();
        Invalidate(FALSE);
    }
}

void CVideoView::RefreshStatusBarForThisView()
{
    if (!m_viewStatus.GetSafeHwnd())
        return;

    CString text;
    if (!HasVideo())
    {
        text = _T("No video");
    }
    else
    {
        const int64_t frame = GetCurrentFrame();
        const int64_t total = GetTotalFrames();
        const double fps = GetFrameRate();

        CString timeText = _T("00:00:00@00");
        if (fps > 0.0 && frame >= 0)
        {
            const double seconds = static_cast<double>(frame) / fps;
            const int h = static_cast<int>(seconds) / 3600;
            const int m = (static_cast<int>(seconds) % 3600) / 60;
            const int s = static_cast<int>(seconds) % 60;
            int ff = static_cast<int>(frame % static_cast<int64_t>(fps + 0.5));
            if (ff < 0) ff = 0;
            timeText.Format(_T("%02d:%02d:%02d@%02d"), h, m, s, ff);
        }

        if (total > 0)
            text.Format(_T("Frame %lld / %lld    %s    %.3f fps"),
                frame, total, timeText.GetString(), fps);
        else
            text.Format(_T("Frame %lld    %s"), frame, timeText.GetString());
    }

    m_viewStatus.SetWindowText(text);
}

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

int CVideoView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;

    if (!m_viewStatus.Create(
        _T("No video"),
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        CRect(0, 0, 0, 0),
        this,
        1))
    {
        return -1;
    }

    // after Create, in OnCreate:
    m_viewStatus.SetFont(CFont::FromHandle(
        (HFONT)GetStockObject(DEFAULT_GUI_FONT)));

    // Do NOT require CreateDeviceResources() here.
    // D2D is created on first OnSize/OnDraw when the client size is known.

    return 0;
}

void CVideoView::OnDestroy()
{
    StopPlayback();

    CView::OnDestroy();

    // Recount after this view is gone
    if (auto* pMain = dynamic_cast<CMainFrame*>(AfxGetMainWnd()))
        pMain->UpdateViewCountStatus();
}

BOOL CVideoView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // Direct2D paints background
}

bool CVideoView::CreateDeviceResources()
{
    if (m_d2dContext)
        return true;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr))
        return false;

    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    D3D_FEATURE_LEVEL fl{};
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice, &fl, &d3dContext);
    if (FAILED(hr))
        return false;


    m_d3dDevice = d3dDevice;

    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr2 = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr2) || !dxgiDevice)
        return false;

    m_dxgiDevice = dxgiDevice;

    hr = m_d2dFactory->CreateDevice(m_dxgiDevice.Get(), &m_d2dDevice);

    if (FAILED(hr))
        return false;

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    if (FAILED(hr))
        return false;

    CRect rc;
    rc = GetVideoClientRect();
    if (rc.Width() > 0 && rc.Height() > 0)
        OnResize(rc.Width(), rc.Height());

    return true;
}

void CVideoView::DiscardDeviceResources()
{
    m_dxgiDevice.Reset();
    m_d3dDevice.Reset();
    m_frameBitmap.Reset();
    m_targetBitmap.Reset();
    m_swapChain.Reset();
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
}

void CVideoView::OnResize(UINT width, UINT height)
{
    if (!m_d2dContext || width == 0 || height == 0)
        return;

    m_targetBitmap.Reset();
    m_d2dContext->SetTarget(nullptr);

    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            DiscardDeviceResources();
            CreateDeviceResources();
            return;
        }
    }
    else
    {
        if (!m_dxgiDevice)
            return;

        ComPtr<IDXGIAdapter> adapter;
        HRESULT hr = m_dxgiDevice->GetAdapter(&adapter);
        if (FAILED(hr) || !adapter)
            return;

        ComPtr<IDXGIFactory2> factory;
        hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        if (FAILED(hr) || !factory)
            return;

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

        hr = factory->CreateSwapChainForHwnd(
            m_d3dDevice.Get(),   // use D3D device, not D2D device
            GetSafeHwnd(),
            &desc,
            nullptr,
            nullptr,
            &m_swapChain);

        if (FAILED(hr) || !m_swapChain)
            return;
    }

    if (!m_swapChain)
        return;

    ComPtr<IDXGISurface> surface;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

    m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &m_targetBitmap);
    m_d2dContext->SetTarget(m_targetBitmap.Get());
}

CRect CVideoView::GetVideoClientRect() const
{
    CRect rc;
    GetClientRect(&rc);
    if (rc.Height() > kViewStatusHeight)
        rc.bottom -= kViewStatusHeight;
    else
        rc.bottom = rc.top;
    return rc;
}

void CVideoView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);

    if (cx <= 0 || cy <= 0)
        return;

    const int statusH = kViewStatusHeight;
    const int videoH = (cy > statusH) ? (cy - statusH) : 0;

    if (m_viewStatus.GetSafeHwnd())
    {
        m_viewStatus.MoveWindow(0, videoH, cx, statusH);
        m_viewStatus.Invalidate(FALSE);
    }

    // D2D only covers the video area above the status strip
    if (videoH > 0)
        OnResize(static_cast<UINT>(cx), static_cast<UINT>(videoH));
}
bool CVideoView::CreateFrameBitmap()
{
    if (!HasVideo() || !m_d2dContext)
        return false;

    if (!m_reader->GetPixels())
        return false;

    const UINT w = m_reader->GetWidth();
    const UINT h = m_reader->GetHeight();
    const UINT stride = m_reader->GetStride();
    if (w == 0 || h == 0 || stride < w * 4)
        return false;

    m_frameBitmap.Reset();

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);

    HRESULT hr = m_d2dContext->CreateBitmap(
        D2D1::SizeU(w, h),
        nullptr,
        0,
        props,
        &m_frameBitmap);

    if (FAILED(hr) || !m_frameBitmap)
        return false;

    hr = m_frameBitmap->CopyFromMemory(nullptr, m_reader->GetPixels(), stride);
    return SUCCEEDED(hr);
}

void CVideoView::OnDraw(CDC* /*pDC*/)
{
    CRect rc;
    rc = GetVideoClientRect();
    if (rc.Width() <= 0 || rc.Height() <= 0)
        return;

    if (!m_d2dContext || !m_swapChain || !m_targetBitmap)
    {
        if (!CreateDeviceResources())
            return;
        if (!m_d2dContext || !m_swapChain || !m_targetBitmap)
            return;
    }

    if (HasVideo())
        CreateFrameBitmap();

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    if (m_frameBitmap)
    {
        D2D1_SIZE_F bmpSize = m_frameBitmap->GetSize();

        UINT rot = 0;
        if (m_reader)
            rot = m_reader->GetRotationDegrees();

        const bool swapWH = (rot == 90 || rot == 270);
        const float srcW = swapWH ? bmpSize.height : bmpSize.width;
        const float srcH = swapWH ? bmpSize.width : bmpSize.height;

        float fit = 1.0f;
        if (srcW > 0.f && srcH > 0.f)
        {
            const float sx = static_cast<float>(rc.Width()) / srcW;
            const float sy = static_cast<float>(rc.Height()) / srcH;
            fit = (sx < sy) ? sx : sy;
        }

        const float z = (m_zoom < 1.0f) ? 1.0f : m_zoom;
        const float scale = fit * z;

        const float dw = srcW * scale;
        const float dh = srcH * scale;
        const float ox = (static_cast<float>(rc.Width()) - dw) * 0.5f + m_panX;
        const float oy = (static_cast<float>(rc.Height()) - dh) * 0.5f + m_panY;

        const D2D1_POINT_2F center = D2D1::Point2F(ox + dw * 0.5f, oy + dh * 0.5f);

        m_d2dContext->SetTransform(
            D2D1::Matrix3x2F::Rotation(static_cast<FLOAT>(rot), center));

        const float bw = bmpSize.width * scale;
        const float bh = bmpSize.height * scale;
        const D2D1_RECT_F dest = D2D1::RectF(
            center.x - bw * 0.5f,
            center.y - bh * 0.5f,
            center.x + bw * 0.5f,
            center.y + bh * 0.5f);

        m_d2dContext->DrawBitmap(m_frameBitmap.Get(), dest);
        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
        return;
    }

    if (m_swapChain)
        m_swapChain->Present(1, 0);
}

void CVideoView::OnInitialUpdate()
{
    CView::OnInitialUpdate();

    m_reader.reset();
    ResetViewTransform();
    StopPlayback();

    CVideoDoc* pDoc = GetDocument();
    if (pDoc)
    {
        const CString path = pDoc->GetPathName();
        if (!path.IsEmpty())
        {
            m_reader = std::make_unique<MFVideoReader>();
            if (!m_reader->Open(std::wstring(path)))
            {
                m_reader.reset();
                AfxMessageBox(_T("Failed to open video in this view."), MB_ICONERROR);
            }
            else
            {
                m_reader->ReadCurrentFrame();
            }
        }
    }

    SetFocus();
    RefreshStatusBarForThisView();
    Invalidate(FALSE);

    // Update main-frame view count
    if (auto* pMain = dynamic_cast<CMainFrame*>(AfxGetMainWnd()))
        pMain->UpdateViewCountStatus();
}

BOOL CVideoView::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN)
    {
        switch (pMsg->wParam)
        {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_SPACE:
        case 'R':
        case 'r':
        case VK_OEM_PLUS:    // '=' key (often Shift for '+')
        case VK_ADD:         // numpad '+'
        case VK_OEM_MINUS:   // '-' key
        case VK_SUBTRACT:    // numpad '-'
        case '0':
            OnKeyDown(static_cast<UINT>(pMsg->wParam), 1, 0);
            return TRUE; // handled here
        default:
            break;
        }
    }

    return CView::PreTranslateMessage(pMsg);
}

void CVideoView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/)
{
    RefreshStatusBarForThisView();
    Invalidate(FALSE);
}

void CVideoView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CVideoView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    CVideoDoc* pDoc = GetDocument();
    if (!pDoc || !HasVideo())
    {
        CView::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }

    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (nChar)
    {
/*
    case VK_RIGHT:
        if (m_playing)
            StopPlayback();
        SetCurrentFrame(GetCurrentFrame() + (ctrl ? 10 : 1));
        break;

    case VK_LEFT:
        if (m_playing)
            StopPlayback();
        SetCurrentFrame(GetCurrentFrame() - (ctrl ? 10 : 1));
        break;
*/
    case VK_HOME:
        if (m_playing)
            StopPlayback();
        SetCurrentFrame(0);
        break;

    case VK_END:
        if (m_playing)
            StopPlayback();
        if (GetTotalFrames() > 0)
            SetCurrentFrame(GetTotalFrames() - 1);
        break;

    case VK_SPACE:
        TogglePlayback();
        break;

    case 'R':
    case 'r':
        if (m_reader)
        {
            UINT rot = m_reader->GetRotationDegrees();
            rot = (rot + 90) % 360;
            m_reader->SetRotationDegrees(rot);
            Invalidate(FALSE);
        }
        break;

// ----- Keyboard zoom / pan — extend OnKeyDown + PreTranslateMessage -----
case VK_OEM_PLUS:
case VK_ADD:
    if (ctrl)
    {
        m_zoom *= 1.1f;
        if (m_zoom > 32.0f) m_zoom = 32.0f;
        ClampPan();
        Invalidate(FALSE);
    }
    break;

case VK_OEM_MINUS:
case VK_SUBTRACT:
    if (ctrl)
    {
        m_zoom /= 1.1f;
        if (m_zoom < 1.0f) m_zoom = 1.0f;
        if (m_zoom <= 1.0f) { m_panX = 0.0f; m_panY = 0.0f; }
        ClampPan();
        Invalidate(FALSE);
    }
    break;

case '0':
    if (ctrl)
        ResetViewTransform();
    break;

case VK_LEFT:
    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        m_panX += 40.0f;   // shift+left pans content right-ish / camera left
        ClampPan();
        Invalidate(FALSE);
    }
    else
    {
        if (m_playing) StopPlayback();
        SetCurrentFrame(GetCurrentFrame() - (ctrl ? 10 : 1));
    }
    break;

case VK_RIGHT:
    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        m_panX -= 40.0f;
        ClampPan();
        Invalidate(FALSE);
    }
    else
    {
        if (m_playing) StopPlayback();
        SetCurrentFrame(GetCurrentFrame() + (ctrl ? 10 : 1));
    }
    break;

case VK_UP:
    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        m_panY += 40.0f;
        ClampPan();
        Invalidate(FALSE);
    }
    break;

case VK_DOWN:
    if (GetKeyState(VK_SHIFT) & 0x8000)
    {
        m_panY -= 40.0f;
        ClampPan();
        Invalidate(FALSE);
    }
    break;

    default:
        CView::OnKeyDown(nChar, nRepCnt, nFlags);
        break;
    }
}

void CVideoView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}

void CVideoView::StartPlayback()
{
    CVideoDoc* pDoc = GetDocument();
    if (!pDoc || !HasVideo())
        return;

    if (m_playing)
        return;

    double fps = GetFrameRate();
    if (fps <= 1.0)
        fps = 30.0;

    // Timer interval in ms (min 1 ms)
    UINT interval = static_cast<UINT>(1000.0 / fps + 0.5);
    if (interval < 1)
        interval = 1;

    m_playing = true;
    SetTimer(s_playTimerId, interval, nullptr);
}

void CVideoView::StopPlayback()
{
    if (!m_playing)
        return;

    m_playing = false;
    KillTimer(s_playTimerId);
}

void CVideoView::TogglePlayback()
{
    if (m_playing)
        StopPlayback();
    else
        StartPlayback();
}

void CVideoView::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent != s_playTimerId)
    {
        CView::OnTimer(nIDEvent);
        return;
    }

    CVideoDoc* pDoc = GetDocument();
    if (!pDoc || !HasVideo())
    {
        StopPlayback();
        return;
    }

    const int64_t cur = GetCurrentFrame();
    const int64_t total = GetTotalFrames();

    // Stop at last frame
    if (total > 0 && cur >= total - 1)
    {
        StopPlayback();
        return;
    }

    // Sequential +1 (smooth path inside SetCurrentFrame)
    SetCurrentFrame(cur + 1);
}

void CVideoView::ResetViewTransform()
{
    m_zoom = 1.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    Invalidate(FALSE);
}

void CVideoView::ClampPan()
{
    // Soft clamp; keeps pan from drifting forever
    const float limit = 4000.0f * m_zoom;
    if (m_panX > limit) m_panX = limit;
    if (m_panX < -limit) m_panX = -limit;
    if (m_panY > limit) m_panY = limit;
    if (m_panY < -limit) m_panY = -limit;
}

BOOL CVideoView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    CVideoDoc* pDoc = GetDocument();
    if (!pDoc || !HasVideo())
        return CView::OnMouseWheel(nFlags, zDelta, pt);

    ScreenToClient(&pt);

    const float oldZoom = m_zoom;
    if (zDelta > 0)
        m_zoom *= 1.1f;
    else
        m_zoom /= 1.1f;

    if (m_zoom < 1.0f) m_zoom = 1.0f;
    if (m_zoom > 32.0f) m_zoom = 32.0f;

    // Zoom toward cursor
    CRect rc;
    rc = GetVideoClientRect();
    const float cx = static_cast<float>(rc.Width()) * 0.5f;
    const float cy = static_cast<float>(rc.Height()) * 0.5f;
    const float mx = static_cast<float>(pt.x);
    const float my = static_cast<float>(pt.y);

    const float relX = (mx - cx - m_panX) / oldZoom;
    const float relY = (my - cy - m_panY) / oldZoom;

    m_panX = mx - cx - relX * m_zoom;
    m_panY = my - cy - relY * m_zoom;

    if (m_zoom <= 1.0f)
    {
        m_panX = 0.0f;
        m_panY = 0.0f;
    }

    ClampPan();
    Invalidate(FALSE);
    return TRUE;
}

void CVideoView::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_zoom > 1.0f)
    {
        m_panning = true;
        m_lastPanPoint = point;
        SetCapture();
    }
    CView::OnLButtonDown(nFlags, point);
}

void CVideoView::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_panning)
    {
        m_panning = false;
        ReleaseCapture();
    }
    CView::OnLButtonUp(nFlags, point);
}

void CVideoView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_panning && (nFlags & MK_LBUTTON))
    {
        m_panX += static_cast<float>(point.x - m_lastPanPoint.x);
        m_panY += static_cast<float>(point.y - m_lastPanPoint.y);
        m_lastPanPoint = point;
        ClampPan();
        Invalidate(FALSE);
    }
    CView::OnMouseMove(nFlags, point);
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
