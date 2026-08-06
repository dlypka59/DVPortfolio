#pragma once

#include "MFVideoReader.h"
#include <memory>
#include <cstdint>

class CVideoDoc : public CDocument
{
protected: // create from serialization only
    CVideoDoc() noexcept;
    DECLARE_DYNCREATE(CVideoDoc)

public:
    virtual ~CVideoDoc();

    BOOL HasVideoPath() const { return !GetPathName().IsEmpty(); }
    int64_t GetCurrentFrame() const;
    int64_t GetTotalFrames() const;
    double  GetFrameRate() const;

    void SetCurrentFrame(int64_t frame);

    // Overrides
public:
    virtual BOOL OnNewDocument() override;
    virtual BOOL OnOpenDocument(LPCTSTR lpszPathName) override;
    virtual void OnCloseDocument() override;
#ifdef _DEBUG
    virtual void AssertValid() const override;
    virtual void Dump(CDumpContext& dc) const override;
#endif

protected:
    DECLARE_MESSAGE_MAP()
};