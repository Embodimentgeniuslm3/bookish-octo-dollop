#include "pch.h"
#include "TermControlHost.h"
#include "TermControlHost.g.cpp"

namespace winrt::TerminalApp::implementation
{
    TermControlHost::TermControlHost(Microsoft::Terminal::TerminalControl::TermControl control) :
        _control{ control }
    {
        // TODO: Hookup the control's titlechanged event, closed event
    }

    // Windows::UI::Xaml::Controls::Control TermControlHost::GetControl()
    // {
    //     return _control.GetControl();
    // }

    Windows::UI::Xaml::UIElement TermControlHost::GetRoot()
    {
        return _control.GetControl();
    }

    void TermControlHost::Close()
    {
        _control.Close();
    }

    hstring TermControlHost::GetTitle()
    {
        return _control.Title();
    }

    Microsoft::Terminal::TerminalControl::TermControl TermControlHost::Terminal()
    {
        return _control;
    }

    winrt::Windows::Foundation::Size TermControlHost::MinimumSize() const
    {
        return _control.MinimumSize();
    }

    bool TermControlHost::IsFocused()
    {
        return _control.GetControl().FocusState() != winrt::Windows::UI::Xaml::FocusState::Unfocused;
    }
    void TermControlHost::Focus()
    {
        _control.GetControl().Focus(winrt::Windows::UI::Xaml::FocusState::Programmatic);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TermControlHost, CloseRequested, _closeRequestedHandlers, TerminalApp::IControlHost, TerminalApp::ClosedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TermControlHost, TitleChanged, _titleChangedHandlers, TerminalApp::IControlHost, Microsoft::Terminal::TerminalControl::TitleChangedEventArgs);
}
