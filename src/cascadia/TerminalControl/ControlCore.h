// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EventArgs.h"
#include "ControlCore.g.h"
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "cppwinrt_utils.h"
#include "ThrottledFunc.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct ControlCore : ControlCoreT<ControlCore>
    {
    public:
        ControlCore(IControlSettings settings, TerminalConnection::ITerminalConnection connection);

        ////////////////////////////////////////////////////////////////////////
        // These members are taken from TermControl

        bool _initializedTerminal;

        TerminalConnection::ITerminalConnection _connection;
        event_token _connectionOutputEventToken;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;
        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;

    private:
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;

        IControlSettings _settings; // ? Might be able to get away with only retrieving pieces

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

    public:
        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        bool _isReadOnly{ false }; // ?

        std::optional<COORD> _lastHoveredCell;
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId;

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval;

        ////////////////////////////////////////////////////////////////////////
        // These members are new
        double _panelWidth;
        double _panelHeight;
        double _compositionScaleX;
        double _compositionScaleY;
        til::color _backgroundColor; // This is _in_ Terminal already!
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // These methods are taken from TermControl
        bool InitializeTerminal(const double actualWidth,
                                const double actualHeight,
                                const double compositionScaleX,
                                const double compositionScaleY);

        void _SetFontSize(int fontSize);
        void _UpdateFont(const bool initialUpdate = false);
        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();
        void _RefreshSizeUnderLock();
        void _DoResizeUnderLock(const double newWidth,
                                const double newHeight);

        void _SendInputToConnection(const winrt::hstring& wstr);
        void _SendInputToConnection(std::wstring_view wstr);

        void SendInput(const winrt::hstring& wstr);
        void ToggleShaderEffects();
        void UpdatePatternLocations();
        void UpdateHoveredCell(const std::optional<COORD>& terminalPosition);

        void SetSelectionAnchor(winrt::Windows::Foundation::Point const& position);
        void SetEndSelectionPoint(winrt::Windows::Foundation::Point const& position);
        bool CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats);

        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;

        winrt::fire_and_forget _AsyncCloseConnection();
        void Close();

#pragma region ICoreState
        // existing
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() noexcept;
        const size_t TaskbarState() const noexcept;
        const size_t TaskbarProgress() const noexcept;
        hstring Title();
        hstring WorkingDirectory() const;
        TerminalConnection::ConnectionState ConnectionState() const;
        // new
        int ScrollOffset();
        int ViewHeight() const;
        int BufferHeight() const;
#pragma endregion

#pragma region TerminalCoreCallbacks
        void _TerminalCopyToClipboard(const std::wstring_view& wstr);
        void _TerminalWarningBell();
        void _TerminalTitleChanged(const std::wstring_view& wstr);
        void _TerminalTabColorChanged(const std::optional<til::color> color);
        void _TerminalBackgroundColorChanged(const COLORREF color);
        void _TerminalScrollPositionChanged(const int viewTop,
                                            const int viewHeight,
                                            const int bufferSize);
        void _TerminalCursorPositionChanged();
        void _TerminalTaskbarProgressChanged();
#pragma endregion

#pragma region RendererCallbacks
        void _RendererWarning(const HRESULT hr);
        void RenderEngineSwapChainChanged();
#pragma endregion

        TYPED_EVENT(CopyToClipboard, IInspectable, TerminalControl::CopyToClipboardEventArgs);

        TYPED_EVENT(TitleChanged, IInspectable, TerminalControl::TitleChangedEventArgs);
        TYPED_EVENT(WarningBell, IInspectable, IInspectable);
        TYPED_EVENT(TabColorChanged, IInspectable, IInspectable);
        TYPED_EVENT(BackgroundColorChanged, IInspectable, IInspectable);
        TYPED_EVENT(ScrollPositionChanged, IInspectable, TerminalControl::ScrollPositionChangedArgs);
        TYPED_EVENT(CursorPositionChanged, IInspectable, IInspectable);
        TYPED_EVENT(TaskbarProgressChanged, IInspectable, IInspectable);
        TYPED_EVENT(ConnectionStateChanged, IInspectable, IInspectable);

    public:
        ////////////////////////////////////////////////////////////////////////
        // These methods are new
        void UpdateSettings(const IControlSettings& settings);

        void SizeChanged(const double width, const double height);
        void ScaleChanged(const double scaleX, const double scaleY);
        float RendererScale() const;

        void _raiseHoveredHyperlinkChanged();
        winrt::hstring GetHyperlink(const til::point position) const;
        winrt::hstring GetHoveredUriText() const;
        std::optional<COORD> GetHoveredCell() const;

        void PasteText(const winrt::hstring& hstr);

        FontInfo GetFont() const;
        til::color BackgroundColor() const;

        bool HasSelection() const;
        std::vector<std::wstring> SelectedText(bool trimTrailingWhitespace) const;

        void Search(const winrt::hstring& text,
                    const bool goForward,
                    const bool caseSensitive);

        void SetBackgroundOpacity(const float opacity);

#pragma region ITerminalInputButNotReally
        bool TrySendKeyEvent(const WORD vkey,
                             const WORD scanCode,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const bool eitherWinPressed,
                             const bool keyDown);
        bool SendCharEvent(const wchar_t ch,
                           const WORD scanCode,
                           const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        bool SendMouseEvent(const COORD viewportPos,
                            const unsigned int uiButton,
                            const ::Microsoft::Terminal::Core::ControlKeyStates states,
                            const short wheelDelta,
                            const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state);
        void UserScrollViewport(const int viewTop);
#pragma endregion

        HANDLE GetSwapChainHandle() const;

        void BlinkAttributeTick();
        void BlinkCursor();
        bool CursorOn() const;
        void CursorOn(const bool isCursorOn);

        void ResumeRendering();
        bool IsVtMouseModeEnabled() const;
        til::point CursorPosition() const;

        void LeftClickOnTerminal(const til::point terminalPosition,
                                 const int numberOfClicks,
                                 const bool altEnabled,
                                 const bool shiftEnabled,
                                 const bool isOnOriginalPosition,
                                 bool& selectionNeedsToBeCopied);

        TYPED_EVENT(HoveredHyperlinkChanged, IInspectable, IInspectable);

        TYPED_EVENT(SwapChainChanged, IInspectable, IInspectable);
        TYPED_EVENT(RendererWarning, IInspectable, TerminalControl::RendererWarningArgs);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    BASIC_FACTORY(ControlCore);
}
