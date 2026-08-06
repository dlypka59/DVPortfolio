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

}

BOOL CVideoDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;
    return TRUE;
}

BOOL CVideoDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    // Views open their own readers in OnInitialUpdate
    return CDocument::OnOpenDocument(lpszPathName);
}

void CVideoDoc::OnCloseDocument()
{
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