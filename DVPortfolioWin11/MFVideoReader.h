#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <d3d11.h>
#include <dxgi.h>
#include <d3d10.h>   // ID3D10Multithread

#include <wrl/client.h>

#include <string>
#include <cstdint>
#include <algorithm>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class MFVideoReader
{
public:
    MFVideoReader();
    ~MFVideoReader();

    bool Open(const std::wstring& path);
    void Close();

    bool IsOpen() const { return m_reader != nullptr; }

    // Frame-accurate control
    bool DecodeToFrame(int64_t targetFrame);
    bool SeekToFrame(int64_t frameNumber);
    bool ReadCurrentFrame();

    // Accessors
    int64_t GetCurrentFrame() const { return m_currentFrame; }
    int64_t GetTotalFrames()  const { return m_totalFrames; }
    double  GetFrameRate()    const { return m_frameRate; }
    UINT    GetWidth()        const { return m_width; }
    UINT    GetHeight()       const { return m_height; }

    ID3D11Texture2D* GetTexture() const { return m_texture.Get(); }

    const uint8_t* GetPixels() const { return m_pixels.empty() ? nullptr : m_pixels.data(); }
    size_t GetPixelsSize() const { return m_pixels.size(); }
    UINT GetStride() const { return m_stride; }

private:
    bool CreateD3DManager();
    bool SelectVideoStream();
    bool GetVideoInfo();

    ComPtr<IMFSourceReader>      m_reader;
    ComPtr<ID3D11Device>         m_device;
    ComPtr<ID3D11DeviceContext>  m_context;
    ComPtr<IMFDXGIDeviceManager> m_dxgiManager;
    ComPtr<ID3D11Texture2D>      m_texture;
    UINT                         m_resetToken = 0;

    int64_t m_currentFrame = 0;
    int64_t m_totalFrames = 0;
    double  m_frameRate = 0.0;
    UINT    m_width = 0;
    UINT    m_height = 0;
    bool    m_frameValid = false;

    bool m_holdFrameNumber = false;   // after SeekToFrame, keep m_currentFrame

    std::vector<uint8_t> m_pixels;  // BGRA8
    UINT m_stride = 0;
};