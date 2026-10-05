/* DrawLite - Win32 Tutorial
 * Chapter 19 - The text tool
 *
 * File: dlt.c
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "dlt.h"
#include "composite.h"

#define DLT_MAGIC       0x31544C44u     // "DLT1" read as a little endian number
#define DLT_MAX_SIZE    32768
#define DLT_MAX_LAYERS  256

BOOL
Dlt_IsDltPath(const wchar_t *path)
{
    size_t len = wcslen(path);
    return len >= 4 && _wcsicmp(path + len - 4, L".dlt") == 0;
}

// Function: Dlt_Write
// Writes bytes to a file. Remembers if anything went wrong.
static BOOL
Dlt_Write(HANDLE file, const void *data, size_t size)
{
    // WriteFile takes a DWORD, so very big blocks go in pieces
    const uint8_t *p = (const uint8_t *)data;
    while (size > 0)
    {
        DWORD chunk = size > 0x10000000 ? 0x10000000 : (DWORD)size;
        DWORD written = 0;
        if (!WriteFile(file, p, chunk, &written, NULL) || written != chunk)
            return FALSE;
        p += chunk;
        size -= chunk;
    }
    return TRUE;
}

static BOOL
Dlt_Read(HANDLE file, void *data, size_t size)
{
    uint8_t *p = (uint8_t *)data;
    while (size > 0)
    {
        DWORD chunk = size > 0x10000000 ? 0x10000000 : (DWORD)size;
        DWORD got = 0;
        if (!ReadFile(file, p, chunk, &got, NULL) || got != chunk)
            return FALSE;
        p += chunk;
        size -= chunk;
    }
    return TRUE;
}

BOOL
Dlt_Save(const Document *doc, const wchar_t *path, wchar_t *error, size_t errorLen)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    uint32_t header[5];
    BOOL ok;
    int i;

    if (file == INVALID_HANDLE_VALUE)
    {
        StringCchCopyW(error, errorLen, L"The file could not be created. Is it read only, or open in another program?");
        return FALSE;
    }
    header[0] = DLT_MAGIC;
    header[1] = (uint32_t)doc->width;
    header[2] = (uint32_t)doc->height;
    header[3] = (uint32_t)doc->layerCount;
    header[4] = (uint32_t)doc->active;
    ok = Dlt_Write(file, header, sizeof(header));

    for (i = 0; ok && i < doc->layerCount; i++)
    {
        const Layer *layer = doc->layers[i];
        wchar_t name[LAYER_NAME_MAX];
        uint32_t props[3];

        ZeroMemory(name, sizeof(name));
        StringCchCopyW(name, LAYER_NAME_MAX, layer->name);
        props[0] = layer->visible ? 1 : 0;
        props[1] = (uint32_t)layer->opacity;
        props[2] = (uint32_t)layer->blend;
        ok = Dlt_Write(file, name, sizeof(name))
            && Dlt_Write(file, props, sizeof(props))
            && Dlt_Write(file, layer->surface->pixels, (size_t)doc->width * doc->height * sizeof(uint32_t));
    }
    ok = CloseHandle(file) && ok;
    if (!ok)
        StringCchCopyW(error, errorLen, L"The file could not be written. Is the disk full?");
    return ok;
}

Document *
Dlt_Load(const wchar_t *path, wchar_t *error, size_t errorLen)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    uint32_t header[5];
    Document *doc = NULL;
    Surface *first = NULL;
    int i, w, h;
    uint32_t count, active;

    if (file == INVALID_HANDLE_VALUE)
    {
        StringCchCopyW(error, errorLen, L"The file could not be opened.");
        return NULL;
    }
    if (!Dlt_Read(file, header, sizeof(header)) || header[0] != DLT_MAGIC)
    {
        StringCchCopyW(error, errorLen, L"This is not a DrawLite file.");
        goto fail;
    }
    // Never trust numbers from a file. Check them before using them to allocate memory.
    w = (int)header[1];
    h = (int)header[2];
    count = header[3];
    active = header[4];
    if (header[1] < 1 || header[1] > DLT_MAX_SIZE || header[2] < 1 || header[2] > DLT_MAX_SIZE
        || count < 1 || count > DLT_MAX_LAYERS || active >= count)
    {
        StringCchCopyW(error, errorLen, L"The file is damaged (strange sizes).");
        goto fail;
    }

    // The document is born with its first layer, so read that one first
    for (i = 0; i < (int)count; i++)
    {
        wchar_t name[LAYER_NAME_MAX];
        uint32_t props[3];
        Layer *layer;

        if (!Dlt_Read(file, name, sizeof(name)) || !Dlt_Read(file, props, sizeof(props)))
        {
            StringCchCopyW(error, errorLen, L"The file is damaged (too short).");
            goto fail;
        }
        name[LAYER_NAME_MAX - 1] = L'\0';

        layer = Layer_Create(w, h, name);
        if (!layer)
        {
            StringCchCopyW(error, errorLen, L"There is not enough memory for an image this big.");
            goto fail;
        }
        layer->visible = props[0] != 0;
        layer->opacity = (int)min(props[1], 255u);
        layer->blend = props[2] < BLEND_COUNT ? (BlendMode)props[2] : BLEND_NORMAL;
        if (!Dlt_Read(file, layer->surface->pixels, (size_t)w * h * sizeof(uint32_t)))
        {
            Layer_Destroy(layer);
            StringCchCopyW(error, errorLen, L"The file is damaged (too short).");
            goto fail;
        }

        if (i == 0)
        {
            // Take the pixels out of the first layer to start the document
            first = layer->surface;
            doc = Doc_CreateFromSurface(first);
            if (!doc)
            {
                Layer_Destroy(layer);
                StringCchCopyW(error, errorLen, L"Not enough memory.");
                goto fail;
            }
            // Doc_CreateFromSurface made its own layer around the surface. Give it our settings,
            // and free the layer wrapper we no longer need (but not the surface, which is in use).
            Layer_CopyProps(doc->layers[0], layer);
            layer->surface = NULL;
            Layer_Destroy(layer);
        }
        else if (!Doc_InsertLayer(doc, i, layer))
        {
            Layer_Destroy(layer);
            StringCchCopyW(error, errorLen, L"Not enough memory.");
            goto fail;
        }
    }
    CloseHandle(file);
    doc->active = (int)active;
    Doc_UpdateAllOfView(doc);
    return doc;

fail:
    CloseHandle(file);
    Doc_Destroy(doc);   // Safe with NULL. If the document exists, it owns the first surface.
    return NULL;
}
