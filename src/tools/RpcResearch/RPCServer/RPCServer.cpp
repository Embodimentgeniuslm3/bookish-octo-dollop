/* file: hellos.c */
#include "pch.h"

#include <conio.h>
#include "WindowBroker.h"

using namespace Microsoft::WRL;

// Holds the wwinmain open until COM tells us there are no more server connections
wil::unique_event _comServerExitEvent;
// Routine Description:
// - Called back when COM says there is nothing left for our server to do and we can tear down.
void _releaseNotifier() noexcept
{
    printf("Top of _releaseNotifier()\n");
    _comServerExitEvent.SetEvent();
}

int main()
{
    auto hr = S_OK;
    printf("Top of main()\n");

    // Set up OutOfProc COM server stuff in case we become one.
    // WRL Module gets going right before winmain is called, so if we don't
    // set this up appropriately... other things using WRL that aren't us
    // could get messed up by the singleton module and cause unexpected errors.
    _comServerExitEvent.create();
    printf("create'd event\n");

    // hr = CoInitialize(nullptr);
    // printf("CoInitialize -> %d\n", hr);

    auto comScope{ wil::CoInitializeEx(COINIT_MULTITHREADED) };
    printf("initialized COM\n");

    auto& module{ Module<OutOfProc>::Create(&_releaseNotifier) };
    printf("Create'd module\n");

    RETURN_IF_FAILED(module.RegisterObjects());
    printf("RegisterObjects\n");

    _comServerExitEvent.wait();
    printf("after wait()\n");

    RETURN_IF_FAILED(module.UnregisterObjects());
    printf("UnregisterObjects()\n");

    return hr;
}
