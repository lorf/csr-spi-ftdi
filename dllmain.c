#include <windows.h>
#include "spi.h"

typedef uint32_t (pttrans_get_version_t)(void);

uint32_t spifns_detect_api_version(void)
{
    static uint32_t pttrans_api_version = 0;

    HMODULE pttdll;
    pttrans_get_version_t *pttrans_get_version;

    if (!pttrans_api_version) {
        /* Detect SPI API version by calling a function from pttransport.dll */
        pttdll = GetModuleHandle("pttransport.dll");
        if (pttdll) {
            pttrans_get_version = (pttrans_get_version_t *)GetProcAddress(pttdll,
                            "pttrans_get_version");
            if (pttrans_get_version)
                pttrans_api_version = pttrans_get_version();
        }
    }
    return pttrans_api_version;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            spi_deinit();
            break;
    }

    return TRUE;
}
