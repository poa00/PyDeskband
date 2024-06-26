#include "DeskBand.h"
#include "Logger.h"

#include <windows.h>
#include <uxtheme.h>
#include <string>
#include <thread>

extern ULONG        g_cDllRef;
extern HINSTANCE    g_hInst;

extern CLSID CLSID_PyDeskBand;

static const WCHAR g_szDeskBandSampleClass[] = L"PyDeskband";


CDeskBand::CDeskBand() :
    m_cRef(1), m_pSite(NULL), m_fHasFocus(FALSE), m_fIsDirty(FALSE), m_dwBandID(0), m_hwnd(NULL), m_hwndParent(NULL)
{
    m_controlPipe = std::make_unique<ControlPipe>(this);
}

CDeskBand::~CDeskBand()
{
    if (m_pSite)
    {
        m_pSite->Release();
    }
}

//
// IUnknown
//
STDMETHODIMP CDeskBand::QueryInterface(REFIID riid, void** ppv)
{
    HRESULT hr = S_OK;

    if (IsEqualIID(IID_IUnknown, riid) ||
        IsEqualIID(IID_IOleWindow, riid) ||
        IsEqualIID(IID_IDockingWindow, riid) ||
        IsEqualIID(IID_IDeskBand, riid) ||
        IsEqualIID(IID_IDeskBand2, riid))
    {
        *ppv = static_cast<IOleWindow*>(this);
    }
    else if (IsEqualIID(IID_IPersist, riid) ||
        IsEqualIID(IID_IPersistStream, riid))
    {
        *ppv = static_cast<IPersist*>(this);
    }
    else if (IsEqualIID(IID_IObjectWithSite, riid))
    {
        *ppv = static_cast<IObjectWithSite*>(this);
    }
    else if (IsEqualIID(IID_IInputObject, riid))
    {
        *ppv = static_cast<IInputObject*>(this);
    }
    else
    {
        hr = E_NOINTERFACE;
        *ppv = NULL;
    }

    if (*ppv)
    {
        AddRef();
    }

    return hr;
}

STDMETHODIMP_(ULONG) CDeskBand::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CDeskBand::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef)
    {
        delete this;
    }

    return cRef;
}

//
// IOleWindow
//
STDMETHODIMP CDeskBand::GetWindow(HWND* phwnd)
{
    *phwnd = m_hwnd;
    return S_OK;
}

STDMETHODIMP CDeskBand::ContextSensitiveHelp(BOOL)
{
    return E_NOTIMPL;
}

//
// IDockingWindow
//
STDMETHODIMP CDeskBand::ShowDW(BOOL fShow)
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, fShow ? SW_SHOW : SW_HIDE);
    }

    return S_OK;
}

STDMETHODIMP CDeskBand::CloseDW(DWORD)
{
    if (m_hwnd)
    {
        if (m_controlPipe)
        {
            m_controlPipe->stopAsyncResponseThread();
        }

        ShowWindow(m_hwnd, SW_HIDE);
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }

    return S_OK;
}

STDMETHODIMP CDeskBand::ResizeBorderDW(const RECT*, IUnknown*, BOOL)
{
    return E_NOTIMPL;
}

//
// IDeskBand
//
STDMETHODIMP CDeskBand::GetBandInfo(DWORD dwBandID, DWORD, DESKBANDINFO* pdbi)
{
    HRESULT hr = E_INVALIDARG;

    if (pdbi)
    {
        m_dwBandID = dwBandID;

        if (pdbi->dwMask & DBIM_MINSIZE)
        {
            pdbi->ptMinSize.x = 100;
            pdbi->ptMinSize.y = 10;
        }

        if (pdbi->dwMask & DBIM_MAXSIZE)
        {
            pdbi->ptMaxSize.y = -1;
        }

        if (pdbi->dwMask & DBIM_INTEGRAL)
        {
            pdbi->ptIntegral.y = 1;
        }

        if (pdbi->dwMask & DBIM_ACTUAL)
        {
            pdbi->ptActual.x = 200;
            pdbi->ptActual.y = 30;
        }

        if (pdbi->dwMask & DBIM_TITLE)
        {
            // Don't show title by removing this flag.
            pdbi->dwMask &= ~DBIM_TITLE;
        }

        if (pdbi->dwMask & DBIM_MODEFLAGS)
        {
            pdbi->dwModeFlags = DBIMF_NORMAL | DBIMF_VARIABLEHEIGHT;
        }

        if (pdbi->dwMask & DBIM_BKCOLOR)
        {
            // Use the default background color by removing this flag.
            pdbi->dwMask &= ~DBIM_BKCOLOR;
        }

        hr = S_OK;
    }

    return hr;
}

//
// IDeskBand2
//
STDMETHODIMP CDeskBand::CanRenderComposited(BOOL* pfCanRenderComposited)
{
    *pfCanRenderComposited = TRUE;

    return S_OK;
}

STDMETHODIMP CDeskBand::SetCompositionState(BOOL fCompositionEnabled)
{
    m_fCompositionEnabled = fCompositionEnabled;

    InvalidateRect(m_hwnd, NULL, TRUE);
    UpdateWindow(m_hwnd);

    return S_OK;
}

STDMETHODIMP CDeskBand::GetCompositionState(BOOL* pfCompositionEnabled)
{
    *pfCompositionEnabled = m_fCompositionEnabled;

    return S_OK;
}

//
// IPersist
//
STDMETHODIMP CDeskBand::GetClassID(CLSID* pclsid)
{
    *pclsid = CLSID_PyDeskBand;
    return S_OK;
}

//
// IPersistStream
//
STDMETHODIMP CDeskBand::IsDirty()
{
    return m_fIsDirty ? S_OK : S_FALSE;
}

STDMETHODIMP CDeskBand::Load(IStream* /*pStm*/)
{
    return S_OK;
}

STDMETHODIMP CDeskBand::Save(IStream* /*pStm*/, BOOL fClearDirty)
{
    if (fClearDirty)
    {
        m_fIsDirty = FALSE;
    }

    return S_OK;
}

STDMETHODIMP CDeskBand::GetSizeMax(ULARGE_INTEGER* /*pcbSize*/)
{
    return E_NOTIMPL;
}

//
// IObjectWithSite
//
STDMETHODIMP CDeskBand::SetSite(IUnknown* pUnkSite)
{
    HRESULT hr = S_OK;

    m_hwndParent = NULL;

    if (m_pSite)
    {
        m_pSite->Release();
    }

    if (pUnkSite)
    {
        IOleWindow* pow;
        hr = pUnkSite->QueryInterface(IID_IOleWindow, reinterpret_cast<void**>(&pow));
        if (SUCCEEDED(hr))
        {
            hr = pow->GetWindow(&m_hwndParent);
            if (SUCCEEDED(hr))
            {
                WNDCLASSW wc = { 0 };
                wc.style = CS_HREDRAW | CS_VREDRAW;
                wc.hCursor = LoadCursor(NULL, IDC_ARROW);
                wc.hInstance = g_hInst;
                wc.lpfnWndProc = WndProc;
                wc.lpszClassName = g_szDeskBandSampleClass;
                wc.hbrBackground = NULL; // done to make it so the background is transparent //CreateSolidBrush(RGB(255, 255, 0));

                RegisterClassW(&wc);

                CreateWindowExW(0,
                    g_szDeskBandSampleClass,
                    NULL,
                    WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                    0,
                    0,
                    0,
                    0,
                    m_hwndParent,
                    NULL,
                    g_hInst,
                    this);

                if (!m_hwnd)
                {
                    hr = E_FAIL;
                }
            }

            pow->Release();
        }

        hr = pUnkSite->QueryInterface(IID_IInputObjectSite, reinterpret_cast<void**>(&m_pSite));
    }

    return hr;
}

STDMETHODIMP CDeskBand::GetSite(REFIID riid, void** ppv)
{
    HRESULT hr = E_FAIL;

    if (m_pSite)
    {
        hr = m_pSite->QueryInterface(riid, ppv);
    }
    else
    {
        *ppv = NULL;
    }

    return hr;
}

//
// IInputObject
//
STDMETHODIMP CDeskBand::UIActivateIO(BOOL fActivate, MSG*)
{
    if (fActivate)
    {
        SetFocus(m_hwnd);
    }

    return S_OK;
}

STDMETHODIMP CDeskBand::HasFocusIO()
{
    return m_fHasFocus ? S_OK : S_FALSE;
}

STDMETHODIMP CDeskBand::TranslateAcceleratorIO(MSG*)
{
    return S_FALSE;
};

void CDeskBand::OnFocus(const BOOL fFocus)
{
    m_fHasFocus = fFocus;

    if (m_pSite)
    {
        m_pSite->OnFocusChangeIS(static_cast<IOleWindow*>(this), m_fHasFocus);
    }
}

LRESULT CALLBACK CDeskBand::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT lResult = 0;

    CDeskBand* pDeskBand = reinterpret_cast<CDeskBand*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_CREATE:
        pDeskBand = reinterpret_cast<CDeskBand*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        pDeskBand->m_hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pDeskBand));
        break;

    case WM_SETFOCUS:
        pDeskBand->OnFocus(TRUE);
        break;

    case WM_KILLFOCUS:
        pDeskBand->OnFocus(FALSE);
        break;

    case WM_PAINT:
    case WM_PRINTCLIENT:
        log("WM_PAINT/WM_PRINTCLIENT");
        pDeskBand->m_controlPipe->paintAllTextInfos();
        break;
    default:
        if (pDeskBand)
        {
            lResult = pDeskBand->m_controlPipe->msgHandler(uMsg);
        }
    }

    if (lResult == 0)
    {
        lResult = DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return lResult;
}
