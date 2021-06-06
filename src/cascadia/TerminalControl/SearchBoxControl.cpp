﻿//
// SearchBoxControl.cpp
// Implementation of the SearchBoxControl class
//

#include "pch.h"
#include "SearchBoxControl.h"
#include "SearchBoxControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    SearchBoxControl::SearchBoxControl() :
        _goForward(false),
        _isCaseSensitive(false)
    {
        InitializeComponent();

        _goForwardButton = this->FindName(L"SetGoForwardButton").try_as<Controls::Button>();
        _goBackwardButton = this->FindName(L"SetGoBackwardButton").try_as<Controls::Button>();
    }

    bool SearchBoxControl::getGoForward()
    {
        return _goForward;
    }

    bool SearchBoxControl::getIsCaseSensitive()
    {
        return _isCaseSensitive;
    }

    void SearchBoxControl::AutoSuggestBox_QuerySubmitted(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs const& e)
    {
    }

    void SearchBoxControl::_GoBackwardClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _goForward = false;

        // If button is clicked, we show the blue border around the clicked button, and eliminate
        // the border on the other direction control button
        Thickness thickness = ThicknessHelper::FromUniformLength(1);
        _goBackwardButton.BorderThickness(thickness);

        thickness = ThicknessHelper::FromUniformLength(0);
        _goForwardButton.BorderThickness(thickness);
    }

    void SearchBoxControl::_GoForwardClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _goForward = true;

        // If button is clicked, we show the blue border around the clicked button, and eliminate
        // the border on the other direction control button
        Thickness thickness = ThicknessHelper::FromUniformLength(1);
        _goForwardButton.BorderThickness(thickness);

        thickness = ThicknessHelper::FromUniformLength(0);
        _goBackwardButton.BorderThickness(thickness);
    }

    void SearchBoxControl::_CaseSensitivityChecked(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _isCaseSensitive = true;
    }

    void SearchBoxControl::_CaseSensitivityUnChecked(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _isCaseSensitive = false;
    }

    void SearchBoxControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
    }

    void SearchBoxControl::Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
    }
}
