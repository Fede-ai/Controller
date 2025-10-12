#include "attacker.hpp"

#include <comdef.h>
#include <Wbemidl.h>
#include <iostream>

#pragma comment(lib, "wbemuuid.lib")

static std::string getHardwareId() {
    std::string hardwareId = "";

    DWORD volumeSerialNumber = 0;
    BOOL result = GetVolumeInformationA("C:\\", 0, 0, &volumeSerialNumber, 0, 0, 0, 0);
    if (result) {
        char serialNumber[9];
        sprintf_s(serialNumber, "%08X", volumeSerialNumber);

        hardwareId += serialNumber;
    }
    else {
        std::cerr << "failed to retrieve volume serial number, error: " << GetLastError();
        std::exit(-1);
    }

    HRESULT hres;

    //initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cerr << "failed to initialize COM library, error: 0x" << std::hex << hres;
        std::exit(-1);
    }

    //set general COM security levels
    hres = CoInitializeSecurity(0, -1, 0, 0, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE, 0);
    if (FAILED(hres)) {
        std::cerr << "failed to initialize security, error: 0x" << std::hex << hres;
        CoUninitialize();
        std::exit(-1);
    }

    //obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        std::cerr << "failed to create IWbemLocator object, error: 0x" << std::hex << hres;
        CoUninitialize();
        std::exit(-1);
    }

    //connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), 0, 0, 0, 0, 0, 0, &pSvc);
    if (FAILED(hres)) {
        std::cerr << "could not connect to WMI, error: 0x" << std::hex << hres;
        pLoc->Release();
        CoUninitialize();
        std::exit(-1);
    }

    //set security levels on the proxy
    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, 0,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);
    if (FAILED(hres)) {
        std::cerr << "could not set proxy blanket, error: 0x" << std::hex << hres;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        std::exit(-1);
    }

    //use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 0, &pEnumerator);
    if (FAILED(hres)) {
        std::cerr << "query for UUID failed, error: 0x" << std::hex << hres;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        std::exit(-1);
    }

    //get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0)
            break;

        //get the value of the UUID property
        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = pclsObj->Get(L"UUID", 0, &vtProp, 0, 0);

        std::wstring ws(vtProp.bstrVal, SysStringLen(vtProp.bstrVal));
        VariantClear(&vtProp);
        pclsObj->Release();

        std::string uuid = "";
        std::transform(ws.begin(), ws.end(), std::back_inserter(uuid), [](wchar_t c) {
            return (char)c;
            });
        uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
        hardwareId += uuid;
    }

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return hardwareId;
}

int main() {
	std::string hId = getHardwareId();
	std::atomic_bool has_tui_stopped = false;
    ftxui::Tui tui;

    auto priv = sf::IpAddress::getLocalAddress().value_or(sf::IpAddress::Any).toString();
    auto publ = sf::IpAddress::getPublicAddress().value_or(sf::IpAddress::Any).toString();
    tui.setTitle(" " + hId + " - " + priv + " / " + publ);

    std::thread tui_thread([&tui, &has_tui_stopped] {
        tui.run();
		has_tui_stopped.store(true);
        });

    Attacker attacker(hId, tui);
    int status = 0;
    while (status == 0) {
        if (has_tui_stopped.load())
			break;

        status = attacker.update(); 
    }

    tui.stop();
	tui_thread.join();

	return (status > 0) ? 0 : status;
}