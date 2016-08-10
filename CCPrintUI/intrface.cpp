/**
	@file
	@brief Implementation of interface for PScript UI plugin
			based on intrface.cpp
			Printer Driver UI Plugin Sample
			by Microsoft Corporation
*/

/*
 * CC PDF Converter: Windows PDF Printer with Creative Commons license support
 * Excel to PDF Converter: Excel PDF printing addin, keeping hyperlinks AND Creative Commons license support
 * Copyright (C) 2007-2010 Guy Hachlili <hguy@cogniview.com>, Cogniview LTD.
 * 
 * This file is part of CC PDF Converter / Excel to PDF Converter
 * 
 * CC PDF Converter and Excel to PDF Converter are free software;
 * you can redistribute them and/or modify them under the terms of the 
 * GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 * 
 * CC PDF Converter and Excel to PDF Converter are is distributed in the hope 
 * that they will be useful, but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. * 
 */

#include "precomp.h"
#include <INITGUID.H>
#include <PRCOMOEM.H>

#include "oemui.h"
#include "debug.h"
#include "intrface.h"



////////////////////////////////////////////////////////
//      Internal Globals
////////////////////////////////////////////////////////

/// Count of active components
static long g_cComponents = 0;
/// Count of locks
static long g_cServerLocks = 0;



////////////////////////////////////////////////////////////////////////////////
//
// IOemUI body
//
/**
	
*/
IOemUI::~IOemUI()
{
    // Make sure that helper interface is released.
    if(NULL != m_pOEMHelp)
    {
        m_pOEMHelp->Release();
        m_pOEMHelp = NULL;
    }

    // If this instance of the object is being deleted, then the reference 
    // count should be zero.
    assert(0 == m_cRef);
}

/**
	@param iid ID of wanted interface
	@param ppv Pointer to a location to put the interface address at (if available)
	@return S_OK if the wanted interface is implemented by this object, E_NOINTERFACE otherwise
*/
HRESULT __stdcall IOemUI::QueryInterface(const IID& iid, void** ppv)
{    
    VERBOSE(DLLTEXT("IOemUI:QueryInterface entry.\r\n\r\n")); 
    if (iid == IID_IUnknown)
    {
		// ALL OLE objects implement IUnkown...
        *ppv = static_cast<IUnknown*>(this); 
        VERBOSE(DLLTEXT("IOemUI:Return pointer to IUnknown.\r\n\r\n")); 
    }
    else if (iid == IID_IPrintOemUI)
    {
		// This object implements IPrintOemUI (print UI plugin object)
        *ppv = static_cast<IPrintOemUI*>(this) ;
        VERBOSE(DLLTEXT("IOemUI:Return pointer to IPrintOemUI.\r\n")); 
    }
    else
    {
		// Something we don't know about:
#if DBG
        TCHAR szOutput[80] = {0};
        StringFromGUID2(iid, szOutput, COUNTOF(szOutput)); // can not fail!
        WARNING(DLLTEXT("IOemUI::QueryInterface %s not supported.\r\n"), szOutput); 
#endif

        *ppv = NULL ;
        return E_NOINTERFACE ;
    }

	// OK, add a reference
    reinterpret_cast<IUnknown*>(*ppv)->AddRef() ;
    return S_OK ;
}

/**
	@return The current number of references for this object
*/
ULONG __stdcall IOemUI::AddRef()
{
    VERBOSE(DLLTEXT("IOemUI:AddRef entry.\r\n")); 
    return InterlockedIncrement(&m_cRef) ;
}

/**
	@return The current number of references to this object
*/
ULONG __stdcall IOemUI::Release() 
{
    VERBOSE(DLLTEXT("IOemUI:Release entry.\r\n")); 
    if (InterlockedDecrement(&m_cRef) == 0)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

/**
	@param pIUnknown Pointer to the Print Core object's interface
	@return E_FAIL if the interface cannot be retrieved, S_OK if it can
*/
HRESULT __stdcall IOemUI::PublishDriverInterface(IUnknown *pIUnknown)
{
    VERBOSE(DLLTEXT("IOemUI:PublishDriverInterface entry.\r\n")); 

    // Need to store pointer to Driver Helper functions, if we already haven't.
    if (m_pOEMHelp == NULL)
    {
        HRESULT hResult;


        // Get Interface to Helper Functions.
        hResult = pIUnknown->QueryInterface(IID_IPrintOemDriverUI, (void** ) &(m_pOEMHelp));

        if(!SUCCEEDED(hResult))
        {
            // Make sure that interface pointer reflects interface query failure.
            m_pOEMHelp = NULL;

            return E_FAIL;
        }
    }

    return S_OK;
}

/**
	@param dwMode Type of information to retrieve
	@param pBuffer Pointer of buffer to put data at
	@param cbSize Size of buffer
	@param pcbNeeded Pointer to receive the size of the buffer needed
	@return S_OK if the data was put in the buffer, E_FAIL otherwise
*/
HRESULT __stdcall IOemUI::GetInfo(DWORD dwMode, PVOID pBuffer, DWORD cbSize, PDWORD pcbNeeded)
{
    VERBOSE(DLLTEXT("IOemUI::GetInfo(%d) entry.\r\r\n"), dwMode);

    // Validate parameters.
    if( (NULL == pcbNeeded)
        ||
        ( (OEMGI_GETSIGNATURE != dwMode)
          &&
          (OEMGI_GETVERSION != dwMode)
          &&
          (OEMGI_GETPUBLISHERINFO != dwMode)
        )
      )
    {
        WARNING(DLLTEXT("IOemUI::GetInfo() exit pcbNeeded is NULL! ERROR_INVALID_PARAMETER\r\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return E_FAIL;
    }

    // Set expected buffer size and number of bytes written.
    *pcbNeeded = sizeof(DWORD);

    // Check buffer size is sufficient.
    if((cbSize < *pcbNeeded) || (NULL == pBuffer))
    {
        WARNING(DLLTEXT("IOemUI::GetInfo() exit insufficient buffer!\r\r\n"));
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return E_FAIL;
    }

    switch(dwMode)
    {
        // OEM DLL Signature
        case OEMGI_GETSIGNATURE:
            *(PDWORD)pBuffer = CCPRINT_SIGNATURE;
            break;

        // OEM DLL version
        case OEMGI_GETVERSION:
            *(PDWORD)pBuffer = CCPRINT_VERSION;
            break;

        // dwMode not supported.
        default:
            // Set written bytes to zero since nothing was written.
            WARNING(DLLTEXT("IOemUI::GetInfo() exit mode not supported.\r\r\n"));
            *pcbNeeded = 0;
            SetLastError(ERROR_NOT_SUPPORTED);
            return E_FAIL;
    }

    VERBOSE(DLLTEXT("IOemUI::GetInfo() exit S_OK, (*pBuffer is %#x).\r\r\n"), *(PDWORD)pBuffer);
    return S_OK;
}

/**
	@param dwMode Type of information/action to perform on the OEMDMPARAM structure
	@param pOemDMParam Pointer to the OEMDMPARAM structure
	@return S_OK if operation was successful, E_FAIL if failed
*/
HRESULT __stdcall IOemUI::DevMode(DWORD dwMode, POEMDMPARAM pOemDMParam)
{   
    VERBOSE(DLLTEXT("IOemUI:DevMode(%d, %#x) entry.\r\n"), dwMode, pOemDMParam); 

    return hrOEMDevMode(dwMode, pOemDMParam);
}

/**
	@param dwMode Type of user interface pages requried
	@param pOemCUIPParam Pointer to structure containing user interface pages data
	@return S_OK if all went well, E_FAIL if not
*/
HRESULT __stdcall IOemUI::CommonUIProp(DWORD dwMode, POEMCUIPPARAM pOemCUIPParam)
{
    VERBOSE(DLLTEXT("IOemUI:CommonUIProp entry.\r\n")); 

    return hrOEMPropertyPage(dwMode, pOemCUIPParam);
}

/**
	@param pPSUIInfo Pointer to the property pages data structure
	@param lParam Additional data (depends on values in pPSUIInfo)
	@return S_OK if all went well, E_FAIL if not
*/
HRESULT __stdcall IOemUI::DocumentPropertySheets(PPROPSHEETUI_INFO pPSUIInfo, LPARAM lParam)
{
    VERBOSE(DLLTEXT("IOemUI:DocumentPropertySheets entry.\r\n")); 

    return hrOEMDocumentPropertySheets(pPSUIInfo, lParam, m_pOEMHelp);
}

/**
	@param pPSUIInfo Pointer to the property pages data structure
	@param lParam Additional data (depends on values in pPSUIInfo)
	@return S_OK if all went well, E_FAIL if not
*/
HRESULT __stdcall IOemUI::DevicePropertySheets(PPROPSHEETUI_INFO pPSUIInfo, LPARAM lParam)
{
    VERBOSE(DLLTEXT("IOemUI:DevicePropertySheets entry.\r\n")); 

    return hrOEMDevicePropertySheets(pPSUIInfo, lParam, m_pOEMHelp);
}

/**
	@param poemuiobj Pointer to the printer data structure
	@param hPrinter Handle to the printer
	@param pDeviceName Name of the printer device
	@param wCapability Type of capability to check
	@param pOutput Buffer to receive additional information
	@param pPublicDM Pointer to the DevMode structure
	@param pOEMDM Pointer to the plugin's data structure
	@param dwOld The value returned from the previous plugins called
	@param dwResult Pointer to put the result number at
	@return S_OK
*/
HRESULT __stdcall IOemUI::DeviceCapabilities(POEMUIOBJ poemuiobj, HANDLE hPrinter, PWSTR pDeviceName, WORD wCapability, PVOID pOutput, PDEVMODE pPublicDM, PVOID pOEMDM, DWORD dwOld, DWORD *dwResult)
{
    VERBOSE(DLLTEXT("IOemUI:DeviceCapabilities entry.\r\n"));

	switch (wCapability)
	{
		case DC_COLORDEVICE:
			*dwResult = 1;
			break;
		case DC_COPIES:
			*dwResult = 1;
			break;
	}

    return S_OK;
}

/**
	@param poemuiobj Pointer to the printer data structure
	@param pDQPInfo Pointer to the print query data structure
	@param pPublicDM Pointer to the DevMode structure
	@param pOEMDM Pointer to the plugin's data structure
	@return S_OK 
*/
HRESULT __stdcall IOemUI::DevQueryPrintEx(POEMUIOBJ poemuiobj, PDEVQUERYPRINT_INFO pDQPInfo, PDEVMODE pPublicDM, PVOID pOEMDM)
{
    VERBOSE(DLLTEXT("IOemUI:DevQueryPrintEx entry.\r\n"));

    return S_OK;
}

/**
	@param dwLevel Level to upgrade to (always 1)
	@param pDriverUpgradeInfo Pointer to the upgrade information structure
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::UpgradePrinter(DWORD dwLevel, PBYTE pDriverUpgradeInfo)
{
    VERBOSE(DLLTEXT("IOemUI:UpgradePrinter entry.\r\n"));

    return E_NOTIMPL;
}

/**
	@param pPrinterName Name of printer
	@param iDriverEvent Type of printer event
	@param dwFlags Flags of the event
	@param lParam Additional parameter
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::PrinterEvent(PWSTR pPrinterName, INT iDriverEvent, DWORD dwFlags, LPARAM lParam)
{
    VERBOSE(DLLTEXT("IOemUI:PrinterEvent entry.\r\n"));

    return E_NOTIMPL;
}

/**
	@param dwDriverEvent Type of driver event
	@param dwLevel Level of driver event (defines pDriverInfo)
	@param pDriverInfo Pointer to driver information structure
	@param lParam Additional data
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::DriverEvent(DWORD dwDriverEvent, DWORD dwLevel, LPBYTE pDriverInfo, LPARAM lParam)
{
    VERBOSE(DLLTEXT("IOemUI:DriverEvent entry.\r\n"));

    return E_NOTIMPL;
};

/**
	@param hPrinter Handle to the printer
	@param poemuiobj Pointer to the printer data structure
	@param pPublicDM Pointer to the DevMode structure
	@param pOEMDM Pointer to the plugin's data structure
	@param ulQueryMode Type of profile to retrieve
	@param pvProfileData Buffer to receive profile data
	@param pcbProfileData Size of provide data buffer
	@param pflProfileData Profile data flags
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::QueryColorProfile(HANDLE hPrinter, POEMUIOBJ poemuiobj, PDEVMODE pPublicDM, PVOID pOEMDM, ULONG ulQueryMode, VOID *pvProfileData, ULONG *pcbProfileData, FLONG *pflProfileData)
{
    VERBOSE(DLLTEXT("IOemUI:QueryColorProfile entry.\r\n"));

	return E_NOTIMPL;
};

/**
	@param hWnd Handle to the font install dialog
	@param usMsg Message send to the window
	@param wParam First parameter of the message
	@param lParam Second parameter of the message
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::FontInstallerDlgProc(HWND hWnd, UINT usMsg, WPARAM wParam, LPARAM lParam) 
{
    VERBOSE(DLLTEXT("IOemUI:FontInstallerDlgProc entry.\r\n"));

    return E_NOTIMPL;
};

/**
	@param hPrinter Handle ot the printer
	@param hHeap Handle to the heap
	@param pwstrCartridges Array of names of cartridges
	@return E_NOTIMPL
*/
HRESULT __stdcall IOemUI::UpdateExternalFonts(HANDLE hPrinter, HANDLE hHeap, PWSTR pwstrCartridges)
{
    VERBOSE(DLLTEXT("IOemUI:UpdateExternalFonts entry.\r\n"));

    return E_NOTIMPL;
}

/**
    @brief OEM class factory
*/
class IOemCF : public IClassFactory
{
public:
    // *** IUnknown methods ***
	/// This function returns the requested interface for the object, if available
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR* ppvObj);
	/// This function adds a reference to the object
    STDMETHOD_(ULONG,AddRef)  (THIS);
	/// This function removes a reference of the object, deleting it if necessary
    STDMETHOD_(ULONG,Release) (THIS);

    // *** IClassFactory methods ***
	/// Called to create an IOemUI object
    STDMETHOD(CreateInstance) (THIS_ LPUNKNOWN pUnkOuter, REFIID riid, LPVOID FAR* ppvObject);
	/// Called to lock the IOemUI DLL (so it will not unload itself when the last object is deleted)
    STDMETHOD(LockServer)     (THIS_ BOOL bLock);

    // Constructor
	/**
		Default constructor
	*/
    IOemCF(): m_cRef(1) { };
	/**
		Destructor: does nothing
	*/
    ~IOemCF() { };

protected:
	/// Number of class factory objects active
    LONG m_cRef;

};

/**
	@param iid ID of wanted interface
	@param ppv Pointer to a location to put the interface address at (if available)
	@return S_OK if the wanted interface is implemented by this object, E_NOINTERFACE otherwise
*/
HRESULT __stdcall IOemCF::QueryInterface(const IID& iid, void** ppv)
{
    if ((iid == IID_IUnknown) || (iid == IID_IClassFactory))
    {
		// We support the Unknown and Class Factory interfaces
        *ppv = static_cast<IOemCF*>(this) ;
    }
    else
    {
		// Nothing we know anything about
#if DBG && defined(USERMODE_DRIVER)
        TCHAR szOutput[80] = {0};
        StringFromGUID2(iid, szOutput, COUNTOF(szOutput)); // can not fail!
        WARNING(DLLTEXT("IOemCF::QueryInterface %s not supported.\r\n"), szOutput); 
#endif

        *ppv = NULL ;
        return E_NOINTERFACE ;
    }

	// OK, add a reference
    reinterpret_cast<IUnknown*>(*ppv)->AddRef() ;
    return S_OK ;
}

/**
	@return The current number of references for this object
*/
ULONG __stdcall IOemCF::AddRef()
{
    return InterlockedIncrement(&m_cRef) ;
}

/**
	@return The current number of references to this object
*/
ULONG __stdcall IOemCF::Release()
{
    if (InterlockedDecrement(&m_cRef) == 0)
    {
        delete this ;
        return 0 ;
    }
    return m_cRef ;
}

/**
	@param pUnknownOuter Pointer to whether object is or isn't part of an aggregate (must be NULL in this case)
	@param iid Reference to the interface ID for the created object
	@param ppv Pointer to the interface pointer to be filled
	@return S_OK if successfully created an object for this interface, error value otherwise
*/
HRESULT __stdcall IOemCF::CreateInstance(IUnknown* pUnknownOuter, const IID& iid, void** ppv)
{
    // Cannot aggregate.
    if (pUnknownOuter != NULL)
    {
        return CLASS_E_NOAGGREGATION ;
    }

    // Create component.
    IOemUI* pOemCB = new IOemUI ;
    if (pOemCB == NULL)
    {
        return E_OUTOFMEMORY ;
    }
    // Get the requested interface.
    HRESULT hr = pOemCB->QueryInterface(iid, ppv) ;

    // Release the IUnknown pointer.
    // (If QueryInterface failed, component will delete itself.)
    pOemCB->Release() ;
    return hr ;
}

// LockServer
/**
	@param bLock TRUE to lock the server, FALSE to release it
	@return S_OK
*/
HRESULT __stdcall IOemCF::LockServer(BOOL bLock)
{
    if (bLock)
    {
        InterlockedIncrement(&g_cServerLocks) ;
    }
    else
    {
        InterlockedDecrement(&g_cServerLocks) ;
    }
    return S_OK ;
}

///////////////////////////////////////////////////////////
//
// Exported functions
//


// Can DLL unload now?
//
/**
	@return S_OK if we can unload (not locked and has no created objects), S_FALSE otherwise
*/
STDAPI DllCanUnloadNow()
{
    if ((g_cComponents == 0) && (g_cServerLocks == 0))
    {
        return S_OK ;
    }
    else
    {
        return S_FALSE ;
    }
}

//
// Get class factory
//
/**
	@param clsid OLE class ID for the object (factory)
	@param iid Reference to the interface ID for the created object
	@param ppv Pointer to the interface pointer to be filled
	@return S_OK if created successfully, error code otherwise
*/
STDAPI DllGetClassObject(const CLSID& clsid, const IID& iid, void** ppv)
{
    VERBOSE(DLLTEXT("DllGetClassObject:Create class factory.\r\n"));

    // Can we create this component?
    if (clsid != CLSID_OEMUI)
    {
        return CLASS_E_CLASSNOTAVAILABLE ;
    }

    // Create class factory.
    IOemCF* pFontCF = new IOemCF ;  // Reference count set to 1
                                         // in constructor
    if (pFontCF == NULL)
    {
        return E_OUTOFMEMORY ;
    }

    // Get requested interface.
    HRESULT hr = pFontCF->QueryInterface(iid, ppv) ;
    pFontCF->Release() ;

    return hr ;
}


