
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

int CVideoView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;
    CreateDeviceResources();
    return 0;
}

void CVideoView::OnDestroy()
{
    DiscardDeviceResources();
    CView::OnDestroy();
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
    GetClientRect(&rc);
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

void CVideoView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0)
        OnResize(static_cast<UINT>(cx), static_cast<UINT>(cy));
}

bool CVideoView::CreateFrameBitmap()
{
    CVideoDoc* doc = GetDocument();
    if (!doc || !doc->HasVideo() || !m_d2dContext)
        return false;

    auto* reader = doc->m_reader.get();
    if (!reader || !reader->GetPixels())
        return false;

    const UINT w = reader->GetWidth();
    const UINT h = reader->GetHeight();
    const UINT stride = reader->GetStride();
    if (w == 0 || h == 0 || stride < w * 4)
        return false;

    m_frameBitmap.Reset();

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);

    // 1) Create an empty bitmap with the correct size
    HRESULT hr = m_d2dContext->CreateBitmap(
        D2D1::SizeU(w, h),
        nullptr,
        0,
        props,
        &m_frameBitmap);

    if (FAILED(hr) || !m_frameBitmap)
        return false;

    // 2) Copy the pixel bytes into that bitmap
    hr = m_frameBitmap->CopyFromMemory(nullptr, reader->GetPixels(), stride);
    return SUCCEEDED(hr);
}

void CVideoView::OnDraw(CDC* /*pDC*/)
{
    if (!CreateDeviceResources() || !m_d2dContext || !m_swapChain)
        return;

    CVideoDoc* doc = GetDocument();
    CreateFrameBitmap();

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    if (m_frameBitmap)
    {
        D2D1_SIZE_F size = m_frameBitmap->GetSize();
        CRect rc;
        GetClientRect(&rc);

        UINT rot = 0;
        if (doc && doc->HasVideo() && doc->m_reader)
            rot = doc->m_reader->GetRotationDegrees();

        const bool swapWH = (rot == 90 || rot == 270);
        const float srcW = swapWH ? size.height : size.width;
        const float srcH = swapWH ? size.width : size.height;

        const float sx = rc.Width() / srcW;
        const float sy = rc.Height() / srcH;
        const float scale = (sx < sy) ? sx : sy;

        const float dw = srcW * scale;
        const float dh = srcH * scale;
        const float ox = (rc.Width() - dw) * 0.5f;
        const float oy = (rc.Height() - dh) * 0.5f;

        D2D1_POINT_2F center = D2D1::Point2F(ox + dw * 0.5f, oy + dh * 0.5f);

        D2D1_MATRIX_3X2_F transform =
            D2D1::Matrix3x2F::Rotation(static_cast<FLOAT>(rot), center);

        m_d2dContext->SetTransform(transform);

        // Bitmap drawn in unswapped size, centered on same center
        const float bw = size.width * scale;
        const float bh = size.height * scale;
        D2D1_RECT_F dest = D2D1::RectF(
            center.x - bw * 0.5f,
            center.y - bh * 0.5f,
            center.x + bw * 0.5f,
            center.y + bh * 0.5f);

        m_d2dContext->DrawBitmap(m_frameBitmap.Get(), dest);

        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    // Frame text overlay
    // (optional for now — can add DirectWrite later)

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
        return;
    }

    m_swapChain->Present(1, 0);
}

void CVideoView::OnInitialUpdate()
{
    CView::OnInitialUpdate();
    SetFocus();
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
        case 'R':
        case 'r':
            OnKeyDown(static_cast<UINT>(pMsg->wParam), 1, 0);
            return TRUE; // handled here
        default:
            break;
        }
    }

    return CView::PreTranslateMessage(pMsg);
}

void CVideoView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CVideoView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    CVideoDoc* pDoc = GetDocument();
    if (!pDoc || !pDoc->HasVideo())
    {
        CView::OnKeyDown(nChar, nRepCnt, nFlags);
        return;
    }

    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (nChar)
    {
    case VK_RIGHT:
        pDoc->SetCurrentFrame(pDoc->GetCurrentFrame() + (ctrl ? 10 : 1));
        break;
    case VK_LEFT:
        pDoc->SetCurrentFrame(pDoc->GetCurrentFrame() - (ctrl ? 10 : 1));
        break;
    case VK_HOME:
        pDoc->SetCurrentFrame(0);
        break;
    case VK_END:
        if (pDoc->GetTotalFrames() > 0)
            pDoc->SetCurrentFrame(pDoc->GetTotalFrames() - 1);
        break;
    case 'R':
    case 'r':
    {
        CVideoDoc* pDoc = GetDocument();
        if (pDoc && pDoc->HasVideo() && pDoc->m_reader)
        {
            UINT rot = pDoc->m_reader->GetRotationDegrees();
            rot = (rot + 90) % 360;
            pDoc->m_reader->SetRotationDegrees(rot);
            Invalidate(FALSE);
        }
        break;
    }
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
