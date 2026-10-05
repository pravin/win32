/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: imageio.c
 */

#define COBJMACROS          // Gives us IFoo_Method(obj, ...) macros for calling COM from C
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "imageio.h"
#include "bmp.h"
#include "pixel.h"

// The one WIC factory. Everything WIC does starts with asking it for things.
static IWICImagingFactory *g_factory = NULL;
static BOOL g_comStarted = FALSE;

// Function: ImageIO_Fail
// Writes "what happened (0x80070002)" into error.
static void
ImageIO_Fail(wchar_t *error, size_t errorLen, const wchar_t *what, HRESULT hr)
{
    if (error && errorLen)
        StringCchPrintfW(error, errorLen, L"%s (error 0x%08lX)", what, (unsigned long)hr);
}

BOOL
ImageIO_Init(void)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    // S_FALSE means "already initialised on this thread". That is fine too.
    g_comStarted = SUCCEEDED(hr);

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        &IID_IWICImagingFactory, (void **)&g_factory);
    return SUCCEEDED(hr);
}

void
ImageIO_Shutdown(void)
{
    if (g_factory)
    {
        IWICImagingFactory_Release(g_factory);
        g_factory = NULL;
    }
    if (g_comStarted)
    {
        CoUninitialize();
        g_comStarted = FALSE;
    }
}

// Function: ImageIO_FromDecoder
// Takes the first frame of a decoder (GIFs and TIFFs can have several) and
// copies its pixels into a new surface.
static Surface *
ImageIO_FromDecoder(IWICBitmapDecoder *decoder, wchar_t *error, size_t errorLen)
{
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    Surface *s = NULL;
    UINT w = 0, h = 0;
    HRESULT hr;

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr))
    {
        ImageIO_Fail(error, errorLen, L"The picture has no image in it.", hr);
        return NULL;
    }
    IWICBitmapFrameDecode_GetSize(frame, &w, &h);
    if (w == 0 || h == 0 || w > 32768 || h > 32768)
    {
        IWICBitmapFrameDecode_Release(frame);
        ImageIO_Fail(error, errorLen, L"The picture has a strange size.", E_FAIL);
        return NULL;
    }

    // Pictures come in all sorts of pixel formats: 8 bit grey, 16 bit colour,
    // palettes and so on. The format converter turns any of them into the one
    // we use: 32 bits, blue first, premultiplied. Yes, WIC speaks premultiplied too.
    hr = IWICImagingFactory_CreateFormatConverter(g_factory, &converter);
    if (SUCCEEDED(hr))
        hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame,
            &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
            WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        ImageIO_Fail(error, errorLen, L"The picture is in a format we cannot convert.", hr);
        goto done;
    }

    s = Surface_Create((int)w, (int)h);
    if (!s)
    {
        ImageIO_Fail(error, errorLen, L"There is not enough memory for an image this big.", E_OUTOFMEMORY);
        goto done;
    }
    hr = IWICFormatConverter_CopyPixels(converter, NULL, w * 4, w * h * 4, (BYTE *)s->pixels);
    if (FAILED(hr))
    {
        ImageIO_Fail(error, errorLen, L"The picture could not be read.", hr);
        Surface_Destroy(s);
        s = NULL;
    }

done:
    if (converter)
        IWICFormatConverter_Release(converter);
    IWICBitmapFrameDecode_Release(frame);
    return s;
}

Surface *
ImageIO_Load(const wchar_t *path, wchar_t *error, size_t errorLen)
{
    IWICBitmapDecoder *decoder = NULL;
    Surface *s;
    HRESULT hr;

    if (!g_factory)
    {
        ImageIO_Fail(error, errorLen, L"Windows Imaging Component is not available.", E_FAIL);
        return NULL;
    }
    // WIC looks inside the file to see what it is, so a PNG called .jpg still loads
    hr = IWICImagingFactory_CreateDecoderFromFilename(g_factory, path, NULL, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr))
    {
        ImageIO_Fail(error, errorLen, L"This file could not be opened as a picture.", hr);
        return NULL;
    }
    s = ImageIO_FromDecoder(decoder, error, errorLen);
    IWICBitmapDecoder_Release(decoder);
    return s;
}

Surface *
ImageIO_LoadMemory(const uint8_t *data, size_t size, wchar_t *error, size_t errorLen)
{
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    Surface *s = NULL;
    HRESULT hr;

    if (!g_factory)
        return NULL;
    hr = IWICImagingFactory_CreateStream(g_factory, &stream);
    if (SUCCEEDED(hr))
        hr = IWICStream_InitializeFromMemory(stream, (BYTE *)data, (DWORD)size);
    if (SUCCEEDED(hr))
        hr = IWICImagingFactory_CreateDecoderFromStream(g_factory, (IStream *)stream, NULL,
            WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr))
        ImageIO_Fail(error, errorLen, L"The picture could not be read.", hr);
    else
    {
        s = ImageIO_FromDecoder(decoder, error, errorLen);
        IWICBitmapDecoder_Release(decoder);
    }
    if (stream)
        IWICStream_Release(stream);
    return s;
}

// Function: ImageIO_Encode
// Writes the surface as a PNG or JPEG into a COM stream.
static HRESULT
ImageIO_Encode(const Surface *s, REFGUID container, BOOL jpeg, IStream *stream)
{
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IPropertyBag2 *props = NULL;
    WICPixelFormatGUID format = jpeg ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
    int bytesPerPixel = jpeg ? 3 : 4;
    UINT stride = (UINT)(s->width * bytesPerPixel);
    uint8_t *buffer = NULL;
    size_t i, count = (size_t)s->width * s->height;
    HRESULT hr;

    // Our pixels are premultiplied. PNG and JPEG files are not, so convert.
    // JPEG has no transparency at all, so we put the picture on white paper.
    buffer = (uint8_t *)malloc((size_t)stride * s->height);
    if (!buffer)
        return E_OUTOFMEMORY;
    for (i = 0; i < count; i++)
    {
        uint32_t p = s->pixels[i];
        if (jpeg)
        {
            uint32_t over = 255 - PIX_A(p);     // How much white shows through
            buffer[i * 3 + 0] = (uint8_t)(PIX_B(p) + over);
            buffer[i * 3 + 1] = (uint8_t)(PIX_G(p) + over);
            buffer[i * 3 + 2] = (uint8_t)(PIX_R(p) + over);
        }
        else
        {
            uint32_t q = Pixel_Unpremultiply(p);
            memcpy(buffer + i * 4, &q, 4);      // 0xAARRGGBB is B, G, R, A in memory
        }
    }

    hr = IWICImagingFactory_CreateEncoder(g_factory, container, NULL, &encoder);
    if (SUCCEEDED(hr))
        hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr))
        hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, &props);

    if (SUCCEEDED(hr) && jpeg)
    {
        // Quality 0.0 to 1.0. We always use 90%.
        PROPBAG2 option;
        VARIANT value;

        ZeroMemory(&option, sizeof(option));
        option.pstrName = (LPOLESTR)L"ImageQuality";
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = 0.9f;
        IPropertyBag2_Write(props, 1, &option, &value);
    }
    if (SUCCEEDED(hr))
        hr = IWICBitmapFrameEncode_Initialize(frame, props);
    if (SUCCEEDED(hr))
        hr = IWICBitmapFrameEncode_SetSize(frame, (UINT)s->width, (UINT)s->height);
    if (SUCCEEDED(hr))
    {
        // We ask for a pixel format. The encoder changes it if it cannot do that one.
        WICPixelFormatGUID wanted = format;
        hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &format);
        if (SUCCEEDED(hr) && !IsEqualGUID(&wanted, &format))
            hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
    }
    if (SUCCEEDED(hr))
        hr = IWICBitmapFrameEncode_WritePixels(frame, (UINT)s->height, stride, stride * (UINT)s->height, buffer);
    if (SUCCEEDED(hr))
        hr = IWICBitmapFrameEncode_Commit(frame);
    if (SUCCEEDED(hr))
        hr = IWICBitmapEncoder_Commit(encoder);

    if (props)
        IPropertyBag2_Release(props);
    if (frame)
        IWICBitmapFrameEncode_Release(frame);
    if (encoder)
        IWICBitmapEncoder_Release(encoder);
    free(buffer);
    return hr;
}

// Function: ImageIO_HasExtension
// Case insensitive check of how a path ends.
static BOOL
ImageIO_HasExtension(const wchar_t *path, const wchar_t *ext)
{
    size_t pl = wcslen(path), el = wcslen(ext);
    return pl >= el && _wcsicmp(path + pl - el, ext) == 0;
}

BOOL
ImageIO_CanSave(const wchar_t *path)
{
    return ImageIO_HasExtension(path, L".bmp") || ImageIO_HasExtension(path, L".png")
        || ImageIO_HasExtension(path, L".jpg") || ImageIO_HasExtension(path, L".jpeg");
}

BOOL
ImageIO_Save(const Surface *s, const wchar_t *path, wchar_t *error, size_t errorLen)
{
    IWICStream *stream = NULL;
    HRESULT hr;
    BOOL jpeg;

    if (ImageIO_HasExtension(path, L".bmp"))
        return Bmp_Save(s, path, error, errorLen);

    jpeg = ImageIO_HasExtension(path, L".jpg") || ImageIO_HasExtension(path, L".jpeg");
    if (!jpeg && !ImageIO_HasExtension(path, L".png"))
    {
        ImageIO_Fail(error, errorLen, L"DrawLite can save .png, .jpg and .bmp files.", E_INVALIDARG);
        return FALSE;
    }
    if (!g_factory)
        return FALSE;

    hr = IWICImagingFactory_CreateStream(g_factory, &stream);
    if (SUCCEEDED(hr))
        hr = IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE);
    if (SUCCEEDED(hr))
        hr = ImageIO_Encode(s, jpeg ? &GUID_ContainerFormatJpeg : &GUID_ContainerFormatPng, jpeg, (IStream *)stream);
    // Releasing the stream closes the file
    if (stream)
        IWICStream_Release(stream);
    if (FAILED(hr))
    {
        ImageIO_Fail(error, errorLen, L"The file could not be saved.", hr);
        return FALSE;
    }
    return TRUE;
}

uint8_t *
ImageIO_EncodePng(const Surface *s, size_t *size)
{
    IStream *stream = NULL;
    HGLOBAL hglobal = NULL;
    uint8_t *result = NULL;
    HRESULT hr;

    if (!g_factory)
        return NULL;
    // A stream that lives in a block of global memory. TRUE means that the
    // memory is freed when the stream is released.
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hr))
        return NULL;
    hr = ImageIO_Encode(s, &GUID_ContainerFormatPng, FALSE, stream);
    if (SUCCEEDED(hr))
        hr = GetHGlobalFromStream(stream, &hglobal);
    if (SUCCEEDED(hr))
    {
        STATSTG stat;
        void *src = GlobalLock(hglobal);
        if (src && SUCCEEDED(IStream_Stat(stream, &stat, STATFLAG_NONAME)))
        {
            result = (uint8_t *)malloc((size_t)stat.cbSize.QuadPart);
            if (result)
            {
                memcpy(result, src, (size_t)stat.cbSize.QuadPart);
                *size = (size_t)stat.cbSize.QuadPart;
            }
        }
        if (src)
            GlobalUnlock(hglobal);
    }
    IStream_Release(stream);
    return result;
}
