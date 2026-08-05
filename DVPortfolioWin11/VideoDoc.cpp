#include "pch.h"
#include "DVPortfolioWin11.h"
#include "VideoDoc.h"
#include "VideoView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CVideoDoc, CDocument)

BEGIN_MESSAGE_MAP(CVideoDoc, CDocument)
END_MESSAGE_MAP()

CVideoDoc::CVideoDoc() noexcept
{}

CVideoDoc::~CVideoDoc()
{
    m_reader.reset();
}

int64_t CVideoDoc::GetCurrentFrame() const
{
    if (m_reader && m_reader->IsOpen())
        return m_reader->GetCurrentFrame();
    return 0;
}

int64_t CVideoDoc::GetTotalFrames() const
{
    if (m_reader && m_reader->IsOpen())
        return m_reader->GetTotalFrames();
    return 0;
}

double CVideoDoc::GetFrameRate() const
{
    if (m_reader && m_reader->IsOpen())
        return m_reader->GetFrameRate();
    return 0.0;
}

void CVideoDoc::SetCurrentFrame(int64_t frame)
{
    if (!m_reader || !m_reader->IsOpen())
        return;

    if (frame < 0)
        frame = 0;

    const int64_t total = m_reader->GetTotalFrames();
    if (total > 0 && frame >= total)
        frame = total - 1;

    const int64_t current = m_reader->GetCurrentFrame();
    if (frame == current)
        return;

    // Forward 1: smooth sequential decode (no seek)
    if (frame == current + 1)
    {
        if (m_reader->ReadCurrentFrame())
            UpdateAllViews(nullptr);
        return;
    }

    // Backward or jump: seek + decode forward to exact frame
    if (m_reader->DecodeToFrame(frame))
        UpdateAllViews(nullptr);
}

BOOL CVideoDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    m_reader.reset();
    return TRUE;
}

BOOL CVideoDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    m_reader = std::make_unique<MFVideoReader>();

    std::wstring path(lpszPathName);
    if (!m_reader->Open(path))
    {
        m_reader.reset();
        AfxMessageBox(_T("Failed to open video with Media Foundation."), MB_ICONERROR);
        return FALSE;
    }

    // Load first frame
    //m_reader->ReadCurrentFrame();

    if (m_reader->ReadCurrentFrame())
    {
        CString msg;
        msg.Format(_T("Open OK\n%u x %u\npixels=%u stride=%u"),
            m_reader->GetWidth(),
            m_reader->GetHeight(),
            (unsigned)m_reader->GetPixelsSize(),
            m_reader->GetStride());
        AfxMessageBox(msg);
    }

    SetPathName(lpszPathName, TRUE);
    SetModifiedFlag(FALSE);
    return TRUE;
}

void CVideoDoc::OnCloseDocument()
{
    m_reader.reset();
    CDocument::OnCloseDocument();
}

#ifdef _DEBUG
void CVideoDoc::AssertValid() const
{
    CDocument::AssertValid();
}

void CVideoDoc::Dump(CDumpContext& dc) const
{
    CDocument::Dump(dc);
}
#endif