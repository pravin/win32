/* DrawLite - Win32 Tutorial
 * Chapter 20 - Effects
 *
 * File: effects.c
 */

#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <stdlib.h>
#include "effects.h"
#include "composite.h"
#include "history.h"
#include "pixel.h"
#include "resource.h"

// How long we wait before bothering the user with a progress box
#define SHOW_PROGRESS_AFTER_MS  250

// Struct: Job
// Everything the worker thread needs, and the few things it tells us back.
// The main thread and the worker share this struct, so the fields they both
// touch are volatile and only changed with the Interlocked functions.
typedef struct Job
{
    EffectId id;
    int param;
    const Surface *src;     // The layer as it was. Read only.
    Surface *dst;           // The result. The worker writes here.
    RECT area;              // The only part we need to compute
    LONG total;             // How many steps there are (rows, mostly)
    volatile LONG done;     // How many are finished
    volatile LONG cancel;   // Set to 1 by the main thread to say "stop"
    volatile LONG finished; // Set to 1 by the worker as its very last act
} Job;

// Function: Job_Step
// Called by the worker after each row. Returns FALSE if we should give up.
static BOOL
Job_Step(Job *job)
{
    InterlockedIncrement(&job->done);
    return InterlockedCompareExchange(&job->cancel, 0, 0) == 0;
}

/* ---- Effects that look at one pixel at a time ---- */

static uint32_t
Fx_Pixel(EffectId id, int param, uint32_t p)
{
    // Colour maths is easier on "straight" colours, so undo the premultiplying and redo it after
    uint32_t c = Pixel_Unpremultiply(p);
    int r = (int)PIX_R(c), g = (int)PIX_G(c), b = (int)PIX_B(c);
    int gray;

    switch (id)
    {
    case FX_INVERT:
        r = 255 - r; g = 255 - g; b = 255 - b;
        break;
    case FX_GRAYSCALE:
        // The eye likes green best and blue least
        gray = (r * 77 + g * 151 + b * 28) >> 8;
        r = g = b = gray;
        break;
    case FX_SEPIA:
    {
        int nr = (r * 101 + g * 197 + b * 48) >> 8;
        int ng = (r * 89 + g * 175 + b * 43) >> 8;
        int nb = (r * 70 + g * 137 + b * 33) >> 8;
        r = nr; g = ng; b = nb;
        break;
    }
    case FX_BRIGHTEN:
        r += param; g += param; b += param;
        break;
    default:
        break;
    }
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return Pixel_Premultiply(PIX_MAKE(PIX_A(c), (uint32_t)r, (uint32_t)g, (uint32_t)b));
}

static BOOL
Fx_PerPixel(Job *job)
{
    int x, y, w = job->src->width;

    for (y = job->area.top; y < job->area.bottom; y++)
    {
        for (x = job->area.left; x < job->area.right; x++)
            job->dst->pixels[(size_t)y * w + x] = Fx_Pixel(job->id, job->param, job->src->pixels[(size_t)y * w + x]);
        if (!Job_Step(job))
            return FALSE;
    }
    return TRUE;
}

/* ---- Box blur ----
 * Averaging every pixel with all the ones around it takes (2r+1)^2 additions
 * per pixel. Too slow. But a box blur can be done in two passes, along the rows
 * then down the columns, and each pass can keep a running total: add the pixel
 * that comes into the window, subtract the one that leaves. That costs the
 * same for any radius. Pixels beyond the edge count as copies of the edge pixel.
 */
static void
Fx_BlurLine(const uint32_t *in, uint32_t *out, int count, int stride, int from, int to, int r)
{
    // in and out are the first pixel of a row (stride 1) or a column (stride = width)
    int64_t a = 0, rr = 0, g = 0, b = 0;
    int k, x;
    int window = 2 * r + 1;

    #define CLAMPED(i)  in[(size_t)((i) < 0 ? 0 : ((i) >= count ? count - 1 : (i))) * stride]
    for (k = from - r; k <= from + r; k++)
    {
        uint32_t p = CLAMPED(k);
        a += PIX_A(p); rr += PIX_R(p); g += PIX_G(p); b += PIX_B(p);
    }
    for (x = from; x < to; x++)
    {
        out[(size_t)x * stride] = PIX_MAKE((uint32_t)((a + window / 2) / window), (uint32_t)((rr + window / 2) / window),
            (uint32_t)((g + window / 2) / window), (uint32_t)((b + window / 2) / window));
        {
            uint32_t leaving = CLAMPED(x - r);
            uint32_t entering = CLAMPED(x + r + 1);
            a += (int64_t)PIX_A(entering) - PIX_A(leaving);
            rr += (int64_t)PIX_R(entering) - PIX_R(leaving);
            g += (int64_t)PIX_G(entering) - PIX_G(leaving);
            b += (int64_t)PIX_B(entering) - PIX_B(leaving);
        }
    }
    #undef CLAMPED
}

static BOOL
Fx_Blur(Job *job)
{
    int w = job->src->width, h = job->src->height;
    int r = job->param;
    int y, x;
    int top = max(0, job->area.top - r), bottom = min(h, job->area.bottom + r);
    uint32_t *tmp = (uint32_t *)calloc((size_t)w * h, sizeof(uint32_t));
    BOOL ok = TRUE;

    if (!tmp)
        return FALSE;
    // Pass one: along the rows. We need rows a little above and below the area
    // too, because the second pass will pull from them.
    for (y = top; y < bottom && ok; y++)
    {
        Fx_BlurLine(job->src->pixels + (size_t)y * w, tmp + (size_t)y * w, w, 1, job->area.left, job->area.right, r);
        ok = Job_Step(job);
    }
    // Pass two: down the columns
    for (x = job->area.left; x < job->area.right && ok; x++)
    {
        Fx_BlurLine(tmp + x, job->dst->pixels + x, h, w, job->area.top, job->area.bottom, r);
        ok = Job_Step(job);
    }
    free(tmp);
    return ok;
}

/* ---- Pixelate ---- */

static BOOL
Fx_Pixelate(Job *job)
{
    int w = job->src->width, h = job->src->height, n = max(1, job->param);
    int by, bx, x, y;
    int firstRow = job->area.top / n * n, firstCol = job->area.left / n * n;

    for (by = firstRow; by < job->area.bottom; by += n)
    {
        for (bx = firstCol; bx < job->area.right; bx += n)
        {
            int x1 = min(bx + n, w), y1 = min(by + n, h);
            uint64_t a = 0, r = 0, g = 0, b = 0;
            uint32_t avg;
            int count = (x1 - bx) * (y1 - by);

            for (y = by; y < y1; y++)
                for (x = bx; x < x1; x++)
                {
                    uint32_t p = job->src->pixels[(size_t)y * w + x];
                    a += PIX_A(p); r += PIX_R(p); g += PIX_G(p); b += PIX_B(p);
                }
            avg = PIX_MAKE((uint32_t)(a / count), (uint32_t)(r / count), (uint32_t)(g / count), (uint32_t)(b / count));
            // Only write inside the area: a block can stick out of it
            for (y = max(by, job->area.top); y < min(y1, job->area.bottom); y++)
                for (x = max(bx, job->area.left); x < min(x1, job->area.right); x++)
                    job->dst->pixels[(size_t)y * w + x] = avg;
        }
        if (!Job_Step(job))
            return FALSE;
    }
    return TRUE;
}

// Function: Job_Thread
// The worker thread's main function.
static unsigned __stdcall
Job_Thread(void *arg)
{
    Job *job = (Job *)arg;
    BOOL ok;

    switch (job->id)
    {
    case FX_BLUR:       ok = Fx_Blur(job); break;
    case FX_PIXELATE:   ok = Fx_Pixelate(job); break;
    default:            ok = Fx_PerPixel(job); break;
    }
    if (!ok)
        InterlockedExchange(&job->cancel, 1);   // Tell the main thread not to use the result
    // This must be the very last thing: the main thread may free the job the moment it sees it
    InterlockedExchange(&job->finished, 1);
    return 0;
}

// Function: Job_StepCount
// How many times the worker will call Job_Step. Needed to scale the progress bar.
static LONG
Job_StepCount(const Job *job)
{
    LONG rows = job->area.bottom - job->area.top;
    LONG cols = job->area.right - job->area.left;

    switch (job->id)
    {
    case FX_BLUR:
        return (min(job->src->height, job->area.bottom + job->param) - max(0, job->area.top - job->param)) + cols;
    case FX_PIXELATE:
        return (rows + job->param - 1) / job->param + 1;
    default:
        return rows;
    }
}

/* ---- The progress box ---- */

static INT_PTR CALLBACK
Progress_Proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Job *job;

    switch (msg)
    {
    case WM_INITDIALOG:
        job = (Job *)lParam;
        SetWindowLongPtrW(hDlg, DWLP_USER, lParam);
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETRANGE32, 0, job->total);
        // The worker cannot tell us "I have finished" with a message, because the box does
        // not exist yet when a fast job ends. So we simply look every so often.
        SetTimer(hDlg, 1, 50, NULL);
        return TRUE;
    case WM_TIMER:
        job = (Job *)GetWindowLongPtrW(hDlg, DWLP_USER);
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, (WPARAM)job->done, 0);
        if (job->finished)
        {
            KillTimer(hDlg, 1);
            EndDialog(hDlg, IDOK);
        }
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDCANCEL)
        {
            // Do not close the box yet. Ask the worker to stop, and the timer closes it
            // when the worker has really stopped.
            job = (Job *)GetWindowLongPtrW(hDlg, DWLP_USER);
            InterlockedExchange(&job->cancel, 1);
            EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL
Effect_Run(HINSTANCE hInstance, HWND hParent, Document *doc, EffectId id, int param, RECT *area)
{
    Surface *layer = Doc_ActiveSurface(doc);
    Surface *before = Surface_Clone(layer);
    Surface *after = Surface_Clone(layer);
    const Selection *sel = &doc->sel;
    Job job;
    uintptr_t thread;
    BOOL ok = FALSE;
    int x, y;

    if (!before || !after)
        goto done;

    ZeroMemory(&job, sizeof(job));
    job.id = id;
    job.param = param;
    job.src = before;
    job.dst = after;
    SetRect(&job.area, 0, 0, doc->width, doc->height);
    if (Sel_IsActive(sel))
        job.area = sel->bounds;
    job.total = Job_StepCount(&job);

    thread = _beginthreadex(NULL, 0, Job_Thread, &job, 0, NULL);
    if (!thread)
        goto done;

    // Most effects on most pictures are over in a blink. Only if we are still
    // waiting after a quarter of a second do we put up the progress box.
    if (WaitForSingleObject((HANDLE)thread, SHOW_PROGRESS_AFTER_MS) == WAIT_TIMEOUT)
        DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_PROGRESS), hParent, Progress_Proc, (LPARAM)&job);
    WaitForSingleObject((HANDLE)thread, INFINITE);    // Never free what a running thread uses
    CloseHandle((HANDLE)thread);

    if (job.cancel)
        goto done;

    // Success. Move the result into the layer. Where there is a selection, a pixel
    // that is only partly selected is only partly changed.
    for (y = job.area.top; y < job.area.bottom; y++)
    {
        for (x = job.area.left; x < job.area.right; x++)
        {
            size_t i = (size_t)y * doc->width + x;
            layer->pixels[i] = sel->mask ? Pixel_Lerp(before->pixels[i], after->pixels[i], sel->mask[i]) : after->pixels[i];
        }
    }
    History_PushPixels(doc->history, doc->active, before, layer, &job.area);
    Doc_UpdateView(doc, &job.area);
    doc->modified = TRUE;
    *area = job.area;
    ok = TRUE;

done:
    Surface_Destroy(before);
    Surface_Destroy(after);
    return ok;
}
