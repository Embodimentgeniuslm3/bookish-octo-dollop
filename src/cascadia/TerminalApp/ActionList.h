// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "ActionList.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionList : ActionListT<ActionList>
    {
        ActionList();

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> FilteredActions();
        void SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Action> const& actions);
        void ToggleVisibility();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(Closed, _closeHandlers, TerminalApp::ActionList, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Action> _allActions{ nullptr };
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _FilterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _KeyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _UpdateFilteredActions();
        static bool _FilterMatchesName(winrt::hstring searchText, winrt::hstring name);
        void _Close();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ActionList : ActionListT<ActionList, implementation::ActionList>
    {
    };
}
