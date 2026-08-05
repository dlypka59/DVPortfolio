#include "pch.h"
#include "MFVideoReader.h"

#include <propvarutil.h>

static BYTE ClampToByte(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<BYTE>(v);
}

// NV12 (Y plane + interleaved UV) -> BGRA
static void ConvertNV12ToBGRA(
    const BYTE* src,
    UINT width,
    UINT height,
    UINT srcStride,
    BYTE* dst,          // tightly packed BGRA
    UINT dstStride)
{
    const BYTE* yPlane = src;
    const BYTE* uvPlane = src + srcStride * height;

    for (UINT y = 0; y < height; ++y)
    {
        const BYTE* yRow = yPlane + y * srcStride;
        const BYTE* uvRow = uvPlane + (y / 2) * srcStride;
        BYTE* dstRow = dst + y * dstStride;

        for (UINT x = 0; x < width; ++x)
        {
            const int Y = static_cast<int>(yRow[x]);
            const int U = static_cast<int>(uvRow[(x & ~1u) + 0]) - 128;
            const int V = static_cast<int>(uvRow[(x & ~1u) + 1]) - 128;

            const int C = Y - 16;
            int R = (298 * C + 409 * V + 128) >> 8;
            int G = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * C + 516 * U + 128) >> 8;

            dstRow[x * 4 + 0] = ClampToByte(B);
            dstRow[x * 4 + 1] = ClampToByte(G);
            dstRow[x * 4 + 2] = ClampToByte(R);
            dstRow[x * 4 + 3] = 255;
        }
    }
}

MFVideoReader::MFVideoReader() = default;

MFVideoReader::~MFVideoReader()
{
    Close();
}

bool MFVideoReader::CreateD3DManager()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    // Optional: flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context);

    if (FAILED(hr))
    {
        // Software fallback
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            nullptr, 0,
            D3D11_SDK_VERSION,
            &m_device,
            &featureLevel,
            &m_context);
        if (FAILED(hr))
            return false;
    }

    ComPtr<ID3D10Multithread> multi;
    if (SUCCEEDED(m_device.As(&multi)))
        multi->SetMultithreadProtected(TRUE);

    hr = MFCreateDXGIDeviceManager(&m_resetToken, &m_dxgiManager);
    if (FAILED(hr))
        return false;

    hr = m_dxgiManager->ResetDevice(m_device.Get(), m_resetToken);
    return SUCCEEDED(hr);
}

bool MFVideoReader::SelectVideoStream()
{
    if (!m_reader)
        return false;

    m_reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);

    HRESULT hr = m_reader->SetStreamSelection(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (FAILED(hr))
        return false;

    // Read size/fps from the first native type (may be compressed)
    ComPtr<IMFMediaType> nativeType;
    hr = m_reader->GetNativeMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeType);
    if (FAILED(hr) || !nativeType)
        return false;

    UINT32 width = 0, height = 0;
    MFGetAttributeSize(nativeType.Get(), MF_MT_FRAME_SIZE, &width, &height);

    UINT32 num = 0, den = 0;
    MFGetAttributeRatio(nativeType.Get(), MF_MT_FRAME_RATE, &num, &den);

    // Request decoded NV12 output
    ComPtr<IMFMediaType> outType;
    hr = MFCreateMediaType(&outType);
    if (FAILED(hr))
        return false;

    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);

    if (width > 0 && height > 0)
        MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, width, height);

    if (num > 0 && den > 0)
        MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, num, den);

    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    outType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    hr = m_reader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        nullptr,
        outType.Get());

    if (FAILED(hr))
    {
        CString msg;
        msg.Format(_T("SetCurrentMediaType NV12 failed: 0x%08X"), hr);
        AfxMessageBox(msg, MB_ICONERROR);
        return false;
    }

    return true;
}

void MFVideoReader::Close()
{
    m_reader.Reset();
    m_texture.Reset();
    m_dxgiManager.Reset();
    m_context.Reset();
    m_device.Reset();

    m_currentFrame = 0;
    m_totalFrames = 0;
    m_frameRate = 0.0;
    m_width = 0;
    m_height = 0;
    m_frameValid = false;
    m_resetToken = 0;

    m_holdFrameNumber = false;

    m_pixels.clear();
    m_stride = 0;
    m_rotationDegrees = 0;
}

void MFVideoReader::SetRotationDegrees(UINT degrees)
{
    m_rotationDegrees = degrees % 360;
}

bool MFVideoReader::GetVideoInfo()
{
    if (!m_reader)
        return false;

    ComPtr<IMFMediaType> mediaType;
    HRESULT hr = m_reader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        &mediaType);
    if (FAILED(hr))
        return false;

    UINT32 num = 0, den = 0;
    if (SUCCEEDED(MFGetAttributeRatio(mediaType.Get(), MF_MT_FRAME_RATE, &num, &den)) && den > 0)
        m_frameRate = static_cast<double>(num) / static_cast<double>(den);
    else
        m_frameRate = 30.0;

    UINT32 w = 0, h = 0;
    if (SUCCEEDED(MFGetAttributeSize(mediaType.Get(), MF_MT_FRAME_SIZE, &w, &h)))
    {
        m_width = w;
        m_height = h;
    }

    m_rotationDegrees = 0;
    UINT32 rot = 0;
    if (SUCCEEDED(mediaType->GetUINT32(MF_MT_VIDEO_ROTATION, &rot)))
    {
        // MF_MT_VIDEO_ROTATION is in degrees for many pipelines
        m_rotationDegrees = rot % 360;
    }


    PROPVARIANT var;
    PropVariantInit(&var);
    hr = m_reader->GetPresentationAttribute(
        (DWORD)MF_SOURCE_READER_MEDIASOURCE,
        MF_PD_DURATION,
        &var);

    if (SUCCEEDED(hr) && var.vt == VT_UI8 && m_frameRate > 0.0)
    {
        const double frameDuration = 10'000'000.0 / m_frameRate; // 100-ns units
        m_totalFrames = static_cast<int64_t>(var.uhVal.QuadPart / frameDuration + 0.5);
    }
    else
    {
        m_totalFrames = 0;
    }

    PropVariantClear(&var);
    return true;
}

bool MFVideoReader::DecodeToFrame(int64_t targetFrame)
{
    if (!m_reader || m_frameRate <= 0.0)
        return false;

    if (targetFrame < 0)
        targetFrame = 0;
    if (m_totalFrames > 0 && targetFrame >= m_totalFrames)
        targetFrame = m_totalFrames - 1;

    const double frameDuration = 10'000'000.0 / m_frameRate; // 100-ns units
    const LONGLONG targetTime =
        static_cast<LONGLONG>(targetFrame * frameDuration + 0.5);

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = targetTime;

    HRESULT hr = m_reader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);
    if (FAILED(hr))
        return false;

    // We will count frames from sample timestamps
    m_holdFrameNumber = false;
    m_frameValid = true;

    // Decode forward until we reach the target frame
    for (int i = 0; i < 900; ++i)
    {
        if (!ReadCurrentFrame())
            break;

        if (m_currentFrame >= targetFrame)
        {
            m_currentFrame = targetFrame;
            return true;
        }
    }

    m_currentFrame = targetFrame;
    return true;
}

bool MFVideoReader::SeekToFrame(int64_t frameNumber)
{
    if (!m_reader || m_frameRate <= 0.0)
        return false;

    if (m_totalFrames > 0)
        frameNumber = std::clamp(frameNumber, int64_t{ 0 }, m_totalFrames - 1);
    else if (frameNumber < 0)
        frameNumber = 0;

    const double frameDuration = 10'000'000.0 / m_frameRate;
    const LONGLONG targetTime = static_cast<LONGLONG>(frameNumber * frameDuration + 0.5);

    PROPVARIANT var;
    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = targetTime;

    HRESULT hr = m_reader->SetCurrentPosition(GUID_NULL, var);
    PropVariantClear(&var);

    if (SUCCEEDED(hr))
    {
        m_currentFrame = frameNumber;
        m_frameValid = true;
        m_holdFrameNumber = true;   // don't let ReadCurrentFrame overwrite
        return true;
    }

    m_frameValid = false;
    return false;
}

bool MFVideoReader::ReadCurrentFrame()
{
    if (!m_reader)
        return false;

    ComPtr<IMFSample> sample;
    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG timestamp = 0;

    HRESULT hr = m_reader->ReadSample(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &flags,
        &timestamp,
        &sample);

    if (FAILED(hr))
        return false;

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
        return false;

    if (!sample)
        return false;

    // Frame number:
    // - After SeekToFrame: keep the requested frame (m_holdFrameNumber == true)
    // - After sequential ReadSample (+1): derive from sample timestamp
    if (m_holdFrameNumber)
    {
        m_holdFrameNumber = false;
    }
    else if (m_frameRate > 0.0)
    {
        const double frameDuration = 10'000'000.0 / m_frameRate;
        m_currentFrame = static_cast<int64_t>(timestamp / frameDuration + 0.5);
        m_frameValid = true;
    }

    if (m_totalFrames > 0 && m_currentFrame >= m_totalFrames)
        m_currentFrame = m_totalFrames - 1;
    if (m_currentFrame < 0)
        m_currentFrame = 0;

    // ---- pixels: NV12 -> BGRA ----
    ComPtr<IMFMediaBuffer> buffer;
    hr = sample->ConvertToContiguousBuffer(&buffer);
    if (FAILED(hr) || !buffer || m_width == 0 || m_height == 0)
        return true;

    BYTE* data = nullptr;
    DWORD maxLen = 0, curLen = 0;
    hr = buffer->Lock(&data, &maxLen, &curLen);
    if (FAILED(hr) || !data || curLen == 0)
    {
        if (SUCCEEDED(hr))
            buffer->Unlock();
        return true;
    }

    m_stride = m_width * 4;
    const size_t bgraSize = static_cast<size_t>(m_stride) * m_height;
    const size_t nv12Min = static_cast<size_t>(m_width) * m_height * 3 / 2;

    m_pixels.resize(bgraSize);

    if (curLen >= nv12Min)
    {
        ConvertNV12ToBGRA(
            data,
            m_width,
            m_height,
            m_width,
            m_pixels.data(),
            m_stride);
    }
    else if (curLen >= bgraSize)
    {
        memcpy(m_pixels.data(), data, bgraSize);
    }
    else
    {
        m_pixels.clear();
    }

    buffer->Unlock();
    return true;
}

bool MFVideoReader::Open(const std::wstring& path)
{
    Close();

    // Do NOT require D3D for CPU RGB32 frames
    ComPtr<IMFAttributes> attrs;
    HRESULT hr = MFCreateAttributes(&attrs, 2);
    if (FAILED(hr))
        return false;

    attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    // Important: do not set MF_SOURCE_READER_D3D_MANAGER here

    hr = MFCreateSourceReaderFromURL(path.c_str(), attrs.Get(), &m_reader);
    if (FAILED(hr))
    {
        CString msg;
        msg.Format(_T("CreateSourceReader failed: 0x%08X"), hr);
        AfxMessageBox(msg, MB_ICONERROR);
        return false;
    }

    if (!SelectVideoStream())
    {
        AfxMessageBox(_T("SelectVideoStream / RGB32 failed"), MB_ICONERROR);
        return false;
    }

    if (!GetVideoInfo())
        return false;

    SeekToFrame(0);
    return true;
}