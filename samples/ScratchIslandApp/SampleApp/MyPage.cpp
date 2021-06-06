// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include "MySettings.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "..\..\..\src\cascadia\UnitTests_Control\MockControlSettings.h"
#include "..\..\..\src\types\inc\utils.hpp"

using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::SampleApp::implementation
{
    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create()
    {
        auto settings = winrt::make_self<implementation::MySettings>();

        // winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.EchoConnection" };

        TerminalConnection::ConptyConnectionSettings connectionSettings{ L"cmd.exe /k echo This TermControl is hosted in-proc...",
                                                                         winrt::hstring{},
                                                                         L"",
                                                                         nullptr,
                                                                         32,
                                                                         80,
                                                                         winrt::guid() };
        winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.ConptyConnection" };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, connectionSettings };

        TerminalConnection::ITerminalConnection conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectInfo) };
        Control::TermControl control{ *settings, conn };

        InProcContent().Children().Append(control);

        // // Once the control loads (and not before that), write some text for debugging:
        // control.Initialized([conn](auto&&, auto&&) {
        //     conn.WriteInput(L"This TermControl is hosted in-proc...");
        // });
    }

    static wil::unique_process_information _createHostClassProcess(const winrt::guid& g)
    {
        auto guidStr{ ::Microsoft::Console::Utils::GuidToString(g) };
        std::wstring commandline{ fmt::format(L"windowsterminal.exe --content {}", guidStr) };
        STARTUPINFO siOne{ 0 };
        siOne.cb = sizeof(STARTUPINFOW);
        wil::unique_process_information piOne;
        auto succeeded = CreateProcessW(
            nullptr,
            commandline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
            CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            nullptr, // startingDirectory
            &siOne, // lpStartupInfo
            &piOne // lpProcessInformation
        );
        THROW_IF_WIN32_BOOL_FALSE(succeeded);
        // if (!succeeded)
        // {
        //     printf("Failed to create host process\n");
        //     return;
        // }

        // Ooof this is dumb, but we need a sleep here to make the server starts.
        // That's _sub par_. Maybe we could use the host's stdout to have them emit
        // a byte when they're set up?
        Sleep(2000);

        // TODO MONDAY - It seems like it takes conhost too long to start up to
        // host the ScratchWinRTServer that even a sleep 100 is too short. However,
        // any longer, and XAML will just crash, because some frame took too long.
        // So we _need_ to do the "have the server explicitly tell us it's ready"
        // thing, and maybe also do it on a bg thread (and signal to the UI thread
        // that it can attach now)

        return std::move(piOne);
    }

    winrt::fire_and_forget MyPage::CreateClicked(const IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& eventArgs)
    {
        auto guidString = GuidInput().Text();

        // Capture calling context.
        winrt::apartment_context ui_thread;
        co_await winrt::resume_background();

        auto canConvert = guidString.size() == 38 && guidString.front() == '{' && guidString.back() == '}';
        bool attached = false;
        winrt::guid contentGuid{ ::Microsoft::Console::Utils::CreateGuid() };

        if (canConvert)
        {
            GUID result{};
            if (SUCCEEDED(IIDFromString(guidString.c_str(), &result)))
            {
                contentGuid = result;
                attached = true;
            }
        }

        if (!attached)
        {
            // 2. Spawn a Server.exe, with the guid on the commandline
            auto piContent{ std::move(_createHostClassProcess(contentGuid)) };
        }

        Control::ContentProcess content = create_instance<Control::ContentProcess>(contentGuid, CLSCTX_LOCAL_SERVER);

        // TerminalConnection::ITerminalConnection conn{ nullptr };
        TerminalConnection::ConnectionInformation connectInfo{ nullptr };
        Control::IControlSettings settings{ nullptr };

        settings = *winrt::make_self<implementation::MySettings>();

        // if (!attached)
        // {
        //     conn = TerminalConnection::EchoConnection{};
        //     // settings = *winrt::make_self<implementation::MySettings>();
        //     content.Initialize(settings, conn);
        // }

        // When creating a terminal for the first time, pass it a connection info
        // otherwise, when attaching to an existing one, just pass null, because we don't need the connection info.
        if (!attached)
        {
            // winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.EchoConnection" };
            // connectInfo = TerminalConnection::ConnectionInformation(myClass, nullptr);

            TerminalConnection::ConptyConnectionSettings connectionSettings{ L"cmd.exe",
                                                                             winrt::hstring{},
                                                                             L"",
                                                                             nullptr,
                                                                             32,
                                                                             80,
                                                                             winrt::guid() };
            // winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.ConptyConnection" };
            winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
            connectInfo = TerminalConnection::ConnectionInformation(myClass, connectionSettings);

            if (!content.Initialize(settings, connectInfo))
            {
                co_return;
            }
        }
        else
        {
        }

        // Switch back to the UI thread.
        co_await ui_thread;
        // Control::TermControl control{ nullptr };
        // if (!attached)
        // {
        //     control = Control::TermControl::FromConnectionInfo(settings, connectInfo);
        // }
        // else
        // {
        //     // Control::TermControl control{ contentGuid, settings, conn };
        //     control = Control::TermControl(contentGuid, settings, nullptr);
        // }
        Control::TermControl control{ contentGuid, settings, nullptr };

        OutOfProcContent().Children().Append(control);

        if (!attached)
        {
            auto guidStr{ ::Microsoft::Console::Utils::GuidToString(contentGuid) };
            GuidInput().Text(guidStr);
        }
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring MyPage::Title()
    {
        return { L"Sample Application" };
    }

}
