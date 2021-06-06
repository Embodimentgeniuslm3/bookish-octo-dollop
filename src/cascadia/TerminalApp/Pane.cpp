// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;

static const int PaneSeparatorSize = 4;
static const float Half = 0.50f;

Pane::Pane(const GUID& profile, const TermControl& control, Pane* const rootPane, const bool lastFocused) :
    _control{ control },
    _lastFocused{ lastFocused },
    _profile{ profile }
{
    _rootPane = rootPane ? rootPane : this;

    _root.Children().Append(_control);
    _connectionClosedToken = _control.ConnectionClosed({ this, &Pane::_ControlClosedHandler });
    _fontSizeChangedToken = _control.FontSizeChanged({ this, &Pane::_FontSizeChangedHandler });

    // Set the background of the pane to match that of the theme's default grid
    // background. This way, we'll match the small underline under the tabs, and
    // the UI will be consistent on bot light and dark modes.
    const auto res = Application::Current().Resources();
    const auto key = winrt::box_value(L"BackgroundGridThemeStyle");
    if (res.HasKey(key))
    {
        const auto g = res.Lookup(key);
        const auto style = g.try_as<winrt::Windows::UI::Xaml::Style>();
        // try_as fails by returning nullptr
        if (style)
        {
            _root.Style(style);
        }
    }
}

// Method Description:
// - Update the size of this pane. Resizes each of our columns so they have the
//   same relative sizes, given the newSize.
// - Because we're just manually setting the row/column sizes in pixels, we have
//   to be told our new size, we can't just use our own OnSized event, because
//   that _won't fire when we get smaller_.
// Arguments:
// - newSize: the amount of space that this pane has to fill now.
// Return Value:
// - <none>
void Pane::ResizeContent(const Size& newSize)
{
    const auto width = newSize.Width;
    const auto height = newSize.Height;

    _CreateRowColDefinitions(newSize);

    if (_splitState == SplitState::Vertical)
    {
        const auto paneSizes = _GetPaneSizes(width);

        const Size firstSize{ paneSizes.first, height };
        const Size secondSize{ paneSizes.second, height };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        const auto paneSizes = _GetPaneSizes(height);

        const Size firstSize{ width, paneSizes.first };
        const Size secondSize{ width, paneSizes.second };
        _firstChild->ResizeContent(firstSize);
        _secondChild->ResizeContent(secondSize);
    }
}

// Method Description:
// - Adjust our child percentages to increase the size of one of our children
//   and decrease the size of the other.
// - Adjusts the separation amount by 5%
// - Does nothing if the direction doesn't match our current split direction
// Arguments:
// - direction: the direction to move our separator. If it's down or right,
//   we'll be increasing the size of the first of our children. Else, we'll be
//   decreasing the size of our first child.
// Return Value:
// - false if we couldn't resize this pane in the given direction, else true.
bool Pane::_Resize(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    float amount = .05f;
    if (direction == Direction::Right || direction == Direction::Down)
    {
        amount = -amount;
    }

    // Make sure we're not making a pane explode here by resizing it to 0 characters.
    const bool changeWidth = _splitState == SplitState::Vertical;

    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };
    // actualDimension is the size in DIPs of this pane in the direction we're
    // resizing.
    const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

    _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition - amount, actualDimension);

    // Resize our columns to match the new percentages.
    ResizeContent(actualSize);

    return true;
}

// Method Description:
// - Moves the separator between panes, as to resize each child on either size
//   of the separator. Tries to move a separator in the given direction. The
//   separator moved is the separator that's closest depth-wise to the
//   currently focused pane, that's also in the correct direction to be moved.
//   If there isn't such a separator, then this method returns false, as we
//   couldn't handle the resize.
// Arguments:
// - direction: The direction to move the separator in.
// Return Value:
// - true if we or a child handled this resize request.
bool Pane::ResizePane(const Direction& direction)
{
    // If we're a leaf, do nothing. We can't possibly have a descendant with a
    // separator the correct direction.
    if (_IsLeaf())
    {
        return false;
    }

    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested resize direction matches our separator, then
    // we're the pane that needs to adjust its separator.
    // If our separator is the wrong direction, then we can't handle it.
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastFocused;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastFocused;
    if (firstIsFocused || secondIsFocused)
    {
        return _Resize(direction);
    }

    // If neither of our children were the focused leaf, then recurse into
    // our children and see if they can handle the resize.
    // For each child, if it has a focused descendant, try having that child
    // handle the resize.
    // If the child wasn't able to handle the resize, it's possible that
    // there were no descendants with a separator the correct direction. If
    // our separator _is_ the correct direction, then we should be the pane
    // to resize. Otherwise, just return false, as we couldn't handle it
    // either.
    if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
    {
        return _firstChild->ResizePane(direction) || _Resize(direction);
    }

    if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
    {
        return _secondChild->ResizePane(direction) || _Resize(direction);
    }

    return false;
}

// Method Description:
// - Attempts to handle moving focus to one of our children. If our split
//   direction isn't appropriate for the move direction, then we'll return
//   false, to try and let our parent handle the move. If our child we'd move
//   focus to is already focused, we'll also return false, to again let our
//   parent try and handle the focus movement.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - true if we handled this focus move request.
bool Pane::_NavigateFocus(const Direction& direction)
{
    if (!DirectionMatchesSplit(direction, _splitState))
    {
        return false;
    }

    const bool focusSecond = (direction == Direction::Right) || (direction == Direction::Down);

    const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;

    // If the child we want to move focus to is _already_ focused, return false,
    // to try and let our parent figure it out.
    if (newlyFocusedChild->WasLastFocused())
    {
        return false;
    }

    // Transfer focus to our child, and update the focus of our tree.
    newlyFocusedChild->_FocusFirstChild();
    UpdateFocus();

    return true;
}

// Method Description:
// - Attempts to move focus to one of our children. If we have a focused child,
//   we'll try to move the focus in the direction requested.
//   - If there isn't a pane that exists as a child of this pane in the correct
//     direction, we'll return false. This will indicate to our parent that they
//     should try and move the focus themselves. In this way, the focus can move
//     up and down the tree to the correct pane.
// - This method is _very_ similar to ResizePane. Both are trying to find the
//   right separator to move (focus) in a direction.
// Arguments:
// - direction: The direction to move the focus in.
// Return Value:
// - true if we or a child handled this focus move request.
bool Pane::NavigateFocus(const Direction& direction)
{
    // If we're a leaf, do nothing. We can't possibly have a descendant with a
    // separator the correct direction.
    if (_IsLeaf())
    {
        return false;
    }

    // Check if either our first or second child is the currently focused leaf.
    // If it is, and the requested move direction matches our separator, then
    // we're the pane that needs to handle this focus move.
    const bool firstIsFocused = _firstChild->_IsLeaf() && _firstChild->_lastFocused;
    const bool secondIsFocused = _secondChild->_IsLeaf() && _secondChild->_lastFocused;
    if (firstIsFocused || secondIsFocused)
    {
        return _NavigateFocus(direction);
    }

    // If neither of our children were the focused leaf, then recurse into
    // our children and see if they can handle the focus move.
    // For each child, if it has a focused descendant, try having that child
    // handle the focus move.
    // If the child wasn't able to handle the focus move, it's possible that
    // there were no descendants with a separator the correct direction. If
    // our separator _is_ the correct direction, then we should be the pane
    // to move focus into our other child. Otherwise, just return false, as
    // we couldn't handle it either.
    if ((!_firstChild->_IsLeaf()) && _firstChild->_HasFocusedChild())
    {
        return _firstChild->NavigateFocus(direction) || _NavigateFocus(direction);
    }

    if ((!_secondChild->_IsLeaf()) && _secondChild->_HasFocusedChild())
    {
        return _secondChild->NavigateFocus(direction) || _NavigateFocus(direction);
    }

    return false;
}

// Method Description:
// - Called when our attached control is closed. Triggers listeners to our close
//   event, if we're a leaf pane.
// - If this was called, and we became a parent pane (due to work on another
//   thread), this function will do nothing (allowing the control's new parent
//   to handle the event instead).
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_ControlClosedHandler()
{
    std::unique_lock lock{ _createCloseLock };
    // It's possible that this event handler started being executed, then before
    // we got the lock, another thread created another child. So our control is
    // actually no longer _our_ control, and instead could be a descendant.
    //
    // When the control's new Pane takes ownership of the control, the new
    // parent will register it's own event handler. That event handler will get
    // fired after this handler returns, and will properly cleanup state.
    if (!_IsLeaf())
    {
        return;
    }

    if (_control.ShouldCloseOnExit())
    {
        // Fire our Closed event to tell our parent that we should be removed.
        _closedHandlers();
    }
}

// Method Description:
// - Called when our terminal changes its font size or sets it for the first time
//   (because when we just create terminal via its ctor it has invalid font size).
//   On the latter event, we tell the root pane to resize itself so that its
//   descendants (including ourself) can properly snap to character grids. In future,
//   we may also want to do that on regular font changes.
// Arguments:
// - fontWidth - new font width in pixels
// - fontHeight - new font height in pixels
// - isInitialChange - whether terminal just got its proper font size.
// Return Value:
// - <none>
void Pane::_FontSizeChangedHandler(const int /* fontWidth */, const int /* fontHeight */, const bool isInitialChange)
{
    if (isInitialChange)
    {
        _rootPane->ResizeContent(_rootPane->_root.ActualSize());
    }
}

// Method Description:
// - Fire our Closed event to tell our parent that we should be removed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::Close()
{
    _control.FontSizeChanged(_fontSizeChangedToken);
    _fontSizeChangedToken.value = 0;

    // Fire our Closed event to tell our parent that we should be removed.
    _closedHandlers();
}

// Method Description:
// - Get the root UIElement of this pane. There may be a single TermControl as a
//   child, or an entire tree of grids and panes as children of this element.
// Arguments:
// - <none>
// Return Value:
// - the Grid acting as the root of this pane.
Controls::Grid Pane::GetRootElement()
{
    return _root;
}

// Method Description:
// - If this is the last focused pane, returns itself. Returns nullptr if this
//   is a leaf and it's not focused. If it's a parent, it returns nullptr if no
//   children of this pane were the last pane to be focused, or the Pane that
//   _was_ the last pane to be focused (if there was one).
// - This Pane's control might not currently be focused, if the tab itself is
//   not currently focused.
// Return Value:
// - nullptr if we're a leaf and unfocused, or no children were marked
//   `_lastFocused`, else returns this
std::shared_ptr<Pane> Pane::GetFocusedPane()
{
    if (_IsLeaf())
    {
        return _lastFocused ? shared_from_this() : nullptr;
    }

    auto firstFocused = _firstChild->GetFocusedPane();
    if (firstFocused != nullptr)
    {
        return firstFocused;
    }
    return _secondChild->GetFocusedPane();
}

// Method Description:
// - Returns nullptr if no children of this pane were the last control to be
//   focused, or the TermControl that _was_ the last control to be focused (if
//   there was one).
// - This control might not currently be focused, if the tab itself is not
//   currently focused.
// Arguments:
// - <none>
// Return Value:
// - nullptr if no children were marked `_lastFocused`, else the TermControl
//   that was last focused.
TermControl Pane::GetFocusedTerminalControl()
{
    auto lastFocused = GetFocusedPane();
    return lastFocused ? lastFocused->_control : nullptr;
}

// Method Description:
// - Returns nullopt if no children of this pane were the last control to be
//   focused, or the GUID of the profile of the last control to be focused (if
//   there was one).
// Arguments:
// - <none>
// Return Value:
// - nullopt if no children of this pane were the last control to be
//   focused, else the GUID of the profile of the last control to be focused
std::optional<GUID> Pane::GetFocusedProfile()
{
    auto lastFocused = GetFocusedPane();
    return lastFocused ? lastFocused->_profile : std::nullopt;
}

// Method Description:
// - Returns true if this pane was the last pane to be focused in a tree of panes.
// Arguments:
// - <none>
// Return Value:
// - true iff we were the last pane focused in this tree of panes.
bool Pane::WasLastFocused() const noexcept
{
    return _lastFocused;
}

// Method Description:
// - Returns true iff this pane has no child panes.
// Arguments:
// - <none>
// Return Value:
// - true iff this pane has no child panes.
bool Pane::_IsLeaf() const noexcept
{
    return _splitState == SplitState::None;
}

// Method Description:
// - Returns true if this pane is currently focused, or there is a pane which is
//   a child of this pane that is actively focused
// Arguments:
// - <none>
// Return Value:
// - true if the currently focused pane is either this pane, or one of this
//   pane's descendants
bool Pane::_HasFocusedChild() const noexcept
{
    // We're intentionally making this one giant expression, so the compiler
    // will skip the following lookups if one of the lookups before it returns
    // true
    return (_control && _control.FocusState() != FocusState::Unfocused) ||
           (_firstChild && _firstChild->_HasFocusedChild()) ||
           (_secondChild && _secondChild->_HasFocusedChild());
}

// Method Description:
// - Update the focus state of this pane, and all its descendants.
//   * If this is a leaf node, and our control is actively focused, we'll mark
//     ourselves as the _lastFocused.
//   * If we're not a leaf, we'll recurse on our children to check them.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::UpdateFocus()
{
    if (_IsLeaf())
    {
        const auto controlFocused = _control &&
                                    _control.FocusState() != FocusState::Unfocused;

        _lastFocused = controlFocused;
    }
    else
    {
        _lastFocused = false;
        _firstChild->UpdateFocus();
        _secondChild->UpdateFocus();
    }
}

// Method Description:
// - Focuses this control if we're a leaf, or attempts to focus the first leaf
//   of our first child, recursively.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_FocusFirstChild()
{
    if (_IsLeaf())
    {
        _control.Focus(FocusState::Programmatic);
    }
    else
    {
        _firstChild->_FocusFirstChild();
    }
}

// Method Description:
// - Attempts to update the settings of this pane or any children of this pane.
//   * If this pane is a leaf, and our profile guid matches the parameter, then
//     we'll apply the new settings to our control.
//   * If we're not a leaf, we'll recurse on our children.
// Arguments:
// - settings: The new TerminalSettings to apply to any matching controls
// - profile: The GUID of the profile these settings should apply to.
// Return Value:
// - <none>
void Pane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    if (!_IsLeaf())
    {
        _firstChild->UpdateSettings(settings, profile);
        _secondChild->UpdateSettings(settings, profile);
    }
    else
    {
        if (profile == _profile)
        {
            _control.UpdateSettings(settings);
        }
    }
}

// Method Description:
// - Closes one of our children. In doing so, takes the control from the other
//   child, and makes this pane a leaf node again.
// Arguments:
// - closeFirst: if true, the first child should be closed, and the second
//   should be preserved, and vice-versa for false.
// Return Value:
// - <none>
void Pane::_CloseChild(const bool closeFirst)
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // If we're a leaf, then chances are both our children closed in close
    // succession. We waited on the lock while the other child was closed, so
    // now we don't have a child to close anymore. Return here. When we moved
    // the non-closed child into us, we also set up event handlers that will be
    // triggered when we return from this.
    if (_IsLeaf())
    {
        return;
    }

    auto closedChild = closeFirst ? _firstChild : _secondChild;
    auto remainingChild = closeFirst ? _secondChild : _firstChild;

    // If the only child left is a leaf, that means we're a leaf now.
    if (remainingChild->_IsLeaf())
    {
        // take the control and profile of the pane that _wasn't_ closed.
        _control = remainingChild->_control;
        _profile = remainingChild->_profile;

        // Add our new event handler before revoking the old one.
        _connectionClosedToken = _control.ConnectionClosed({ this, &Pane::_ControlClosedHandler });

        // Revoke the old event handlers. Remove both the handlers for the panes
        // themselves closing, and remove their handlers for their controls
        // closing. At this point, if the remaining child's control is closed,
        // they'll trigger only our event handler for the control's close.
        _firstChild->Closed(_firstClosedToken);
        _secondChild->Closed(_secondClosedToken);
        closedChild->_control.ConnectionClosed(closedChild->_connectionClosedToken);
        remainingChild->_control.ConnectionClosed(remainingChild->_connectionClosedToken);

        // If either of our children was focused, we want to take that focus from
        // them.
        _lastFocused = _firstChild->_lastFocused || _secondChild->_lastFocused;

        // Remove all the ui elements of our children. This'll make sure we can
        // re-attach the TermControl to our Grid.
        _firstChild->_root.Children().Clear();
        _secondChild->_root.Children().Clear();

        // Reset our UI:
        _root.Children().Clear();
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();
        _separatorRoot = { nullptr };

        // Reattach the TermControl to our grid.
        _root.Children().Append(_control);

        if (_lastFocused)
        {
            _control.Focus(FocusState::Programmatic);
        }

        _splitState = SplitState::None;

        // Release our children.
        _firstChild = nullptr;
        _secondChild = nullptr;
    }
    else
    {
        // First stash away references to the old panes and their tokens
        const auto oldFirstToken = _firstClosedToken;
        const auto oldSecondToken = _secondClosedToken;
        const auto oldFirst = _firstChild;
        const auto oldSecond = _secondChild;

        // Steal all the state from our child
        _splitState = remainingChild->_splitState;
        _separatorRoot = remainingChild->_separatorRoot;
        _firstChild = remainingChild->_firstChild;
        _secondChild = remainingChild->_secondChild;

        // Set up new close handlers on the children
        _SetupChildCloseHandlers();

        // Revoke the old event handlers on our new children
        _firstChild->Closed(remainingChild->_firstClosedToken);
        _secondChild->Closed(remainingChild->_secondClosedToken);

        // Revoke event handlers on old panes and controls
        oldFirst->Closed(oldFirstToken);
        oldSecond->Closed(oldSecondToken);
        closedChild->_control.ConnectionClosed(closedChild->_connectionClosedToken);

        // Reset our UI:
        _root.Children().Clear();
        _root.ColumnDefinitions().Clear();
        _root.RowDefinitions().Clear();

        // Copy the old UI over to our grid.
        // Start by copying the row/column definitions. Iterate over the
        // rows/cols, and remove each one from the old grid, and attach it to
        // our grid instead.
        while (remainingChild->_root.ColumnDefinitions().Size() > 0)
        {
            auto col = remainingChild->_root.ColumnDefinitions().GetAt(0);
            remainingChild->_root.ColumnDefinitions().RemoveAt(0);
            _root.ColumnDefinitions().Append(col);
        }
        while (remainingChild->_root.RowDefinitions().Size() > 0)
        {
            auto row = remainingChild->_root.RowDefinitions().GetAt(0);
            remainingChild->_root.RowDefinitions().RemoveAt(0);
            _root.RowDefinitions().Append(row);
        }

        // Remove the child's UI elements from the child's grid, so we can
        // attach them to us instead.
        remainingChild->_root.Children().Clear();

        _root.Children().Append(_firstChild->GetRootElement());
        _root.Children().Append(_separatorRoot);
        _root.Children().Append(_secondChild->GetRootElement());

        // If the closed child was focused, transfer the focus to it's first sibling.
        if (closedChild->_lastFocused)
        {
            _FocusFirstChild();
        }

        // Release the pointers that the child was holding.
        remainingChild->_firstChild = nullptr;
        remainingChild->_secondChild = nullptr;
        remainingChild->_separatorRoot = { nullptr };
    }
}

// Method Description:
// - Adds event handlers to our children to handle their close events.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_SetupChildCloseHandlers()
{
    _firstClosedToken = _firstChild->Closed([this]() {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]() {
            _CloseChild(true);
        });
    });

    _secondClosedToken = _secondChild->Closed([this]() {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=]() {
            _CloseChild(false);
        });
    });
}

// Method Description:
// - Sets up row/column definitions for this pane. There are three total
//   row/cols. The middle one is for the separator. The first and third are for
//   each of the child panes, and are given a size in pixels, based off the
//   availiable space, and the percent of the space they respectively consume,
//   which is stored in _desiredSplitPosition
// - Does nothing if our split state is currently set to SplitState::None
// Arguments:
// - rootSize: The dimensions in pixels that this pane (and its children should consume.)
// Return Value:
// - <none>
void Pane::_CreateRowColDefinitions(const Size& rootSize)
{
    if (_splitState == SplitState::Vertical)
    {
        _root.ColumnDefinitions().Clear();

        // Create three columns in this grid: one for each pane, and one for the separator.
        auto separatorColDef = Controls::ColumnDefinition();
        separatorColDef.Width(GridLengthHelper::Auto());

        const auto paneSizes = _GetPaneSizes(rootSize.Width);

        auto firstColDef = Controls::ColumnDefinition();
        firstColDef.Width(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondColDef = Controls::ColumnDefinition();
        secondColDef.Width(GridLengthHelper::FromPixels(paneSizes.second));

        _root.ColumnDefinitions().Append(firstColDef);
        _root.ColumnDefinitions().Append(separatorColDef);
        _root.ColumnDefinitions().Append(secondColDef);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        _root.RowDefinitions().Clear();

        // Create three rows in this grid: one for each pane, and one for the separator.
        auto separatorRowDef = Controls::RowDefinition();
        separatorRowDef.Height(GridLengthHelper::Auto());

        const auto paneSizes = _GetPaneSizes(rootSize.Height);

        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromPixels(paneSizes.first));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromPixels(paneSizes.second));

        _root.RowDefinitions().Append(firstRowDef);
        _root.RowDefinitions().Append(separatorRowDef);
        _root.RowDefinitions().Append(secondRowDef);
    }
}

// Method Description:
// - Initializes our UI for a new split in this pane. Sets up row/column
//   definitions, and initializes the separator grid. Does nothing if our split
//   state is currently set to SplitState::None
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_CreateSplitContent()
{
    Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                     gsl::narrow_cast<float>(_root.ActualHeight()) };

    _CreateRowColDefinitions(actualSize);

    if (_splitState == SplitState::Vertical)
    {
        // Create the pane separator
        _separatorRoot = Controls::Grid{};
        _separatorRoot.Width(PaneSeparatorSize);
        // NaN is the special value XAML uses for "Auto" sizing.
        _separatorRoot.Height(NAN);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        // Create the pane separator
        _separatorRoot = Controls::Grid{};
        _separatorRoot.Height(PaneSeparatorSize);
        // NaN is the special value XAML uses for "Auto" sizing.
        _separatorRoot.Width(NAN);
    }
}

// Method Description:
// - Sets the row/column of our child UI elements, to match our current split type.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Pane::_ApplySplitDefinitions()
{
    if (_splitState == SplitState::Vertical)
    {
        Controls::Grid::SetColumn(_firstChild->GetRootElement(), 0);
        Controls::Grid::SetColumn(_separatorRoot, 1);
        Controls::Grid::SetColumn(_secondChild->GetRootElement(), 2);
    }
    else if (_splitState == SplitState::Horizontal)
    {
        Controls::Grid::SetRow(_firstChild->GetRootElement(), 0);
        Controls::Grid::SetRow(_separatorRoot, 1);
        Controls::Grid::SetRow(_secondChild->GetRootElement(), 2);
    }
}

// Method Description:
// - Determines whether the pane can be split
// Arguments:
// - splitType: what type of split we want to create.
// Return Value:
// - True if the pane can be split. False otherwise.
bool Pane::CanSplit(SplitState splitType)
{
    if (_IsLeaf())
    {
        return _CanSplit(splitType);
    }

    if (_firstChild->_HasFocusedChild())
    {
        return _firstChild->CanSplit(splitType);
    }

    if (_secondChild->_HasFocusedChild())
    {
        return _secondChild->CanSplit(splitType);
    }

    return false;
}

// Method Description:
// - Split the focused pane in our tree of panes, and place the given
//   TermControl into the newly created pane. If we're the focused pane, then
//   we'll create two new children, and place them side-by-side in our Grid.
// Arguments:
// - splitType: what type of split we want to create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Pane::Split(SplitState splitType, const GUID& profile, const TermControl& control)
{
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            _firstChild->Split(splitType, profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            _secondChild->Split(splitType, profile, control);
        }

        return;
    }

    _Split(splitType, profile, control);
}

// Method Description:
// - Determines whether the pane can be split.
// Arguments:
// - splitType: what type of split we want to create.
// Return Value:
// - True if the pane can be split. False otherwise.
bool Pane::_CanSplit(SplitState splitType)
{
    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };

    const Size minSize = _GetMinSize();

    if (splitType == SplitState::Vertical)
    {
        const auto widthMinusSeparator = actualSize.Width - PaneSeparatorSize;
        const auto newWidth = widthMinusSeparator * Half;

        return newWidth > minSize.Width;
    }

    if (splitType == SplitState::Horizontal)
    {
        const auto heightMinusSeparator = actualSize.Height - PaneSeparatorSize;
        const auto newHeight = heightMinusSeparator * Half;

        return newHeight > minSize.Height;
    }

    return false;
}

// Method Description:
// - Does the bulk of the work of creating a new split. Initializes our UI,
//   creates a new Pane to host the control, registers event handlers.
// Arguments:
// - splitType: what type of split we should create.
// - profile: The profile GUID to associate with the newly created pane.
// - control: A TermControl to use in the new pane.
// Return Value:
// - <none>
void Pane::_Split(SplitState splitType, const GUID& profile, const TermControl& control)
{
    // Lock the create/close lock so that another operation won't concurrently
    // modify our tree
    std::unique_lock lock{ _createCloseLock };

    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);
    _connectionClosedToken.value = 0;

    _control.FontSizeChanged(_fontSizeChangedToken);
    _fontSizeChangedToken.value = 0;

    _splitState = splitType;
    _desiredSplitPosition = Half;

    // Remove any children we currently have. We can't add the existing
    // TermControl to a new grid until we do this.
    _root.Children().Clear();

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control, _rootPane);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control, _rootPane);

    _CreateSplitContent();

    _root.Children().Append(_firstChild->GetRootElement());
    _root.Children().Append(_separatorRoot);
    _root.Children().Append(_secondChild->GetRootElement());

    _ApplySplitDefinitions();

    // Register event handlers on our children to handle their Close events
    _SetupChildCloseHandlers();

    _lastFocused = false;
}

// Method Description:
// - Gets the size in pixels of each of our children, given the full size they
//   should fill. If specified size is lower than required then children will be
//   of minimum size. Snaps first child to grid but not the second. Accounts for
//   the size of the separator that should be between them as well.
// Arguments:
// - fullSize: the amount of space in pixels that should be filled by our
//   children and their separator. Can be arbitrarily low.
// Return Value:
// - a pair with the size of our first child and the size of our second child,
//   respectively.
std::pair<float, float> Pane::_GetPaneSizes(const float fullSize) const
{
    const auto snapToWidth = _splitState == SplitState::Vertical;
    const auto snappedSizes = _CalcSnappedPaneDimensions(snapToWidth, fullSize, nullptr);

    // Keep the first pane snapped and give the second pane all remaining size
    return {
        snappedSizes.first.lower,
        fullSize - PaneSeparatorSize - snappedSizes.first.lower
    };
}

// Method Description:
// - Gets the size in pixels of each of our children, given the full size they should
//   fill. Each is snapped to char grid. If called multiple times with fullSize
//   argument growing, then both returned sizes are guaranteed to be non-decreasing.
//   This is important so that user doesn't get any pane shrinked when they actually
//   increase the window/parent pane size. That's also required by the layout algorithm.
// Arguments:
// - snapToWidth: if true, operates on width, otherwise on height.
// - fullSize: the amount of space in pixels that should be filled by our children and
//   their separator. Can be arbitrarily low.
// - next: if not null, it will be assigned the next possible snapped sizes (see
//   'Return value' below), unless the children fit fullSize without any remaining space,
//   in which case it is equal to returned value.
// Return Value:
// - a pair with the size of our first child and the size of our second child,
//   respectively. Since they are snapped to grid, their sum might be (and usually is)
//   lower than the specified full size.
ChildrenSnapBounds Pane::_CalcSnappedPaneDimensions(const bool snapToWidth,
                                                    const float fullSize /*,
                                                         std::pair<float, float>* next*/
                                                    ) const
{
    if (_IsLeaf())
    {
        THROW_HR(E_FAIL);
    }

    auto sizeTree = _GetMinSizeTree(snapToWidth);
    LayoutSizeNode lastSizeTree{ sizeTree };

    // MG: Continually attempt to snap our children upwards, until we find a
    // size larger than the given size. This will let us find the nearest snap
    // size both up and downwards for the given size.
    while (sizeTree.size < fullSize)
    {
        lastSizeTree = sizeTree;
        _SnapSizeUpwards(snapToWidth, sizeTree);

        // MG: If by snapping upwards we exactly match the given size, then
        // great! return that pair of sizes as both the lower and upper bound.
        if (sizeTree.size == fullSize)
        {
            ChildrenSnapBounds bounds;
            bounds.first{ sizeTree.firstChild->size, sizeTree.firstChild->size };
            bounds.second{ sizeTree.secondChild->size, sizeTree.secondChild->size };
            return bounds;
        }
    }

    // MG: We're out of the loop. lastSizeTree has the size before the snap that
    // would take us to a size larger than the given size, and sizeTree has the
    // size of the sanp above the given size. Return these values.
    ChildrenSnapBounds bounds;
    bounds.first{ lastSizeTree.firstChild->size, sizeTree.firstChild->size };
    bounds.second{ lastSizeTree.secondChild->size, sizeTree.secondChild->size };
    return bounds;
}

// Method Description:
// - Adjusts given dimension (width or height) so that all descendant terminals
//   align with their character grids as close as possible. Snaps to closes match
//   (either upward or downward). Also makes sure to fit in minimal sizes of the panes.
// Arguments:
// - snapToWidth: if true operates on width, otherwise on height
// - dimension: a dimension (width or height) to snap
// Return Value:
// - calculated dimension
float Pane::SnapDimension(const bool snapToWidth, const float dimension) const
{
    const auto snapPossibilites = _GetProposedSnapSizes(snapToWidth, dimension);
    const auto lower = snapPossibilites.lower;
    const auto higher = snapPossibilites.higher;
    return dimension - lower < higher - dimension ? lower : higher;
}

// Method Description:
// - Adjusts given dimension (width or height) so that all descendant terminals
//   align with their character grids as close as possible. Also makes sure to
//   fit in minimal sizes of the panes.
// Arguments:
// - snapToWidth: if true operates on width, otherwise on height
// - dimension: a dimension (width or height) to be snapped
// Return Value:
// - pair of floats, where first value is the size snapped downward (not greater then
//   requested size) and second is the size snapped upward (not lower than requested size).
//   If requested size is already snapped, then both returned values equal this value.
SnapSizeBounds Pane::_GetProposedSnapSizes(const bool snapToWidth, const float dimension) const
{
    if (_IsLeaf())
    {
        // If we're a leaf pane, alight to the grid of controlling terminal

        const auto minSize = _GetMinSize();
        const auto minDimension = snapToWidth ? minSize.Width : minSize.Height;

        // MG: If the proposed size is smaller than our minimum size, just return our min size. We can't snpa smaller.
        if (dimension <= minDimension)
        {
            return { minDimension, minDimension };
        }

        // MG: Ask our control what it would snap to for this size. This is always downwards.
        const float lower = _control.SnapDimensionToGrid(snapToWidth, dimension);
        // MG: If it would exactly snap downwards to the proposed size, great! Just return that size.
        if (lower == dimension)
        {
            return { lower, lower };
        }
        else
        {
            // MG: Otherwise, calculate the next upwards snap size by adding one
            // character to the control's "snap down" size. This will give us
            // the next upwards snap size.
            const auto cellSize = _control.CharacterDimensions();
            const auto higher = lower + (snapToWidth ? cellSize.Width : cellSize.Height);
            return { lower, higher };
        }
    }
    else if (SnapDirectionIsParallelToSplit(snapToWidth, _splitState))
    {
        // If we're resizes along separator axis, snap to the closes possibility
        // given by our children panes.

        const auto firstSnapped = _firstChild->_GetProposedSnapSizes(snapToWidth, dimension);
        const auto secondSnapped = _secondChild->_GetProposedSnapSizes(snapToWidth, dimension);
        return {
            std::max(firstSnapped.lower, secondSnapped.lower),
            std::min(firstSnapped.higher, secondSnapped.higher)
        };
    }
    else
    {
        // If we're resizes perpendicularly to separator axis, calculate the sizes
        // of child panes that would fit the given size. We use same algorithm that
        // is used for real resize routine, but exclude the remaining empty space that
        // would appear after the second pane. This will be the 'downward' snap possibility,
        // while the 'upward' will be given as a side product of the layout function.

        const auto bounds = _CalcSnappedPaneDimensions(snapToWidth, dimension);
        return {
            bounds.first.lower + PaneSeparatorSize + bounds.second.lower,
            bounds.first.higher + PaneSeparatorSize + bounds.second.higher
        };
    }
}

// Method Description:
// - Increases size of given LayoutSizeNode to match next possible 'snap'. In case of leaf
//   pane this means the next cell of the terminal. Otherwise it means that one of its children
//   advances (recursively). It expects the given node and its descendants to have either
//   already snapped or minimum size.
// Arguments:
// - snapToWidth: if true operates on width, otherwise on height.
// - sizeNode: a layouting node that corresponds to this pane.
// Return Value:
// - <none>
void Pane::_SnapSizeUpwards(const bool snapToWidth, LayoutSizeNode& sizeNode) const
{
    if (_IsLeaf())
    {
        if (sizeNode.isMinimumSize)
        {
            // If the node is of its minimum size, this size might not be snapped,
            // so snap it upward. It might however be snapped, so add 1 to make
            // sure it really increases (not really required but to avoid surprises).
            sizeNode.size = _GetProposedSnapSizes(snapToWidth, sizeNode.size + 1).higher;
        }
        else
        {
            const auto cellSize = _control.CharacterDimensions();
            sizeNode.size += snapToWidth ? cellSize.Width : cellSize.Height;
        }
    }
    else
    {
        // The given node often has next possible (advanced) values already
        // cached by the previous advance operation. If we're the first one,
        // we need to calculate them now.
        if (sizeNode.nextFirstChild == nullptr)
        {
            sizeNode.nextFirstChild.reset(new LayoutSizeNode(*sizeNode.firstChild));
            _firstChild->_SnapSizeUpwards(snapToWidth, *sizeNode.nextFirstChild);
        }
        if (sizeNode.nextSecondChild == nullptr)
        {
            sizeNode.nextSecondChild.reset(new LayoutSizeNode(*sizeNode.secondChild));
            _secondChild->_SnapSizeUpwards(snapToWidth, *sizeNode.nextSecondChild);
        }

        const auto nextFirstSize = sizeNode.nextFirstChild->size;
        const auto nextSecondSize = sizeNode.nextSecondChild->size;

        bool advanceFirst; // Whether to advance first or second child
        if (SnapDirectionIsParallelToSplit(snapToWidth, _splitState))
        {
            // If we're growing along separator axis, choose the child that
            // wants to be smaller than the other.
            advanceFirst = nextFirstSize < nextSecondSize;
        }
        else
        {
            // If we're growing perpendicularly to separator axis, choose
            // the child so that their size ratio is closer to the currently
            // maintained (so that the relative separator position is closer
            // to the _desiredSplitPosition field).

            const auto firstSize = sizeNode.firstChild->size;
            const auto secondSize = sizeNode.secondChild->size;

            // Because we relay on equality check these calculations have to be
            // immune to floating point errors.
            const auto deviation1 = nextFirstSize - (nextFirstSize + secondSize) * _desiredSplitPosition;
            const auto deviation2 = -1 * (firstSize - (firstSize + nextSecondSize) * _desiredSplitPosition);
            advanceFirst = deviation1 <= deviation2;
        }

        // MG: Here, we're taking the value from the child we decided to snap
        // upwards on, and calculating a new upwards snap size for that child?
        // MG: I'm very unsure.
        if (advanceFirst)
        {
            *sizeNode.firstChild = *sizeNode.nextFirstChild;
            _firstChild->_SnapSizeUpwards(snapToWidth, *sizeNode.nextFirstChild);
        }
        else
        {
            *sizeNode.secondChild = *sizeNode.nextSecondChild;
            _secondChild->_SnapSizeUpwards(snapToWidth, *sizeNode.nextSecondChild);
        }

        // MG: If we're resizing parallel to the split, then our new size is the
        // size from the largest child. If we're resizing perpendicularly, then
        // our new size is the sum of the sizes of our children, plus the size
        // of the separator.
        if (SnapDirectionIsParallelToSplit(snapToWidth, _splitState))
        {
            sizeNode.size = std::max(sizeNode.firstChild->size, sizeNode.secondChild->size);
        }
        else
        {
            sizeNode.size = sizeNode.firstChild->size + PaneSeparatorSize + sizeNode.secondChild->size;
        }
    }

    sizeNode.isMinimumSize = false;
}

// Method Description:
// - Get the absolute minimum size that this pane can be resized to and still
//   have 1x1 character visible, in each of its children. This includes the
//   space needed for the separator.
// Arguments:
// - <none>
// Return Value:
// - The minimum size that this pane can be resized to and still have a visible
//   character.
Size Pane::_GetMinSize() const
{
    if (_IsLeaf())
    {
        return _control.MinimumSize();
    }

    const auto firstSize = _firstChild->_GetMinSize();
    const auto secondSize = _secondChild->_GetMinSize();

    const auto minWidth = _splitState == SplitState::Vertical ?
                              firstSize.Width + PaneSeparatorSize + secondSize.Width :
                              std::max(firstSize.Width, secondSize.Width);
    const auto minHeight = _splitState == SplitState::Horizontal ?
                               firstSize.Height + PaneSeparatorSize + secondSize.Height :
                               std::max(firstSize.Height, secondSize.Height);

    return { minWidth, minHeight };
}

// Method Description:
// - Builds a tree of LayoutSizeNode that matches the tree of panes. Each node
//   has minimum size that the corresponding pane can have.
// Arguments:
// - snapToWidth: if true operates on width, otherwise on height
// Return Value:
// - Root node of built tree that matches this pane.
Pane::LayoutSizeNode Pane::_GetMinSizeTree(const bool snapToWidth) const
{
    const auto size = _GetMinSize();
    LayoutSizeNode node(snapToWidth ? size.Width : size.Height);
    if (!_IsLeaf())
    {
        node.firstChild.reset(new LayoutSizeNode(_firstChild->_GetMinSizeTree(snapToWidth)));
        node.secondChild.reset(new LayoutSizeNode(_secondChild->_GetMinSizeTree(snapToWidth)));
    }

    return node;
}

// Method Description:
// - Adjusts split position so that no child pane is smaller then its
//   minimum size
// Arguments:
// - snapToWidth: if true, operates on width, otherwise on height.
// - requestedValue: split position value to be clamped
// - totalSize: size (width or height) of the parent pane
// Return Value:
// - split position (value in range <0.0, 1.0>)
float Pane::_ClampSplitPosition(const bool snapToWidth, const float requestedValue, const float totalSize) const
{
    const auto firstMinSize = _firstChild->_GetMinSize();
    const auto secondMinSize = _secondChild->_GetMinSize();

    const auto firstMinDimension = snapToWidth ? firstMinSize.Width : firstMinSize.Height;
    const auto secondMinDimension = snapToWidth ? secondMinSize.Width : secondMinSize.Height;

    const auto minSplitPosition = firstMinDimension / (totalSize - PaneSeparatorSize);
    const auto maxSplitPosition = 1.0f - (secondMinDimension / (totalSize - PaneSeparatorSize));

    return std::clamp(requestedValue, minSplitPosition, maxSplitPosition);
}

DEFINE_EVENT(Pane, Closed, _closedHandlers, ConnectionClosedEventArgs);

Pane::LayoutSizeNode::LayoutSizeNode(const float minSize) :
    size{ minSize },
    isMinimumSize{ true },
    firstChild{ nullptr },
    secondChild{ nullptr },
    nextFirstChild{ nullptr },
    nextSecondChild{ nullptr }
{
}

Pane::LayoutSizeNode::LayoutSizeNode(const LayoutSizeNode& other) :
    size{ other.size },
    isMinimumSize{ other.isMinimumSize },
    firstChild{ other.firstChild ? std::make_unique<LayoutSizeNode>(*other.firstChild) : nullptr },
    secondChild{ other.secondChild ? std::make_unique<LayoutSizeNode>(*other.secondChild) : nullptr },
    nextFirstChild{ other.nextFirstChild ? std::make_unique<LayoutSizeNode>(*other.nextFirstChild) : nullptr },
    nextSecondChild{ other.nextSecondChild ? std::make_unique<LayoutSizeNode>(*other.nextSecondChild) : nullptr }
{
}

// Method Description:
// - Makes sure that this node and all its descendants equal the supplied node.
//   This may be more efficient that copy construction since it will reuse its
//   allocated children.
// Arguments:
// - other: Node to take the values from.
// Return Value:
// - itself
Pane::LayoutSizeNode& Pane::LayoutSizeNode::operator=(const LayoutSizeNode& other)
{
    size = other.size;
    isMinimumSize = other.isMinimumSize;

    _AssignChildNode(firstChild, other.firstChild.get());
    _AssignChildNode(secondChild, other.secondChild.get());
    _AssignChildNode(nextFirstChild, other.nextFirstChild.get());
    _AssignChildNode(nextSecondChild, other.nextSecondChild.get());

    return *this;
}

// Method Description:
// - Performs assignment operation on a single child node reusing
// - current one if present.
// Arguments:
// - nodeField: Reference to our field holding concerned node.
// - other: Node to take the values from.
// Return Value:
// - <none>
void Pane::LayoutSizeNode::_AssignChildNode(std::unique_ptr<LayoutSizeNode>& nodeField, const LayoutSizeNode* const newNode)
{
    if (newNode)
    {
        if (nodeField)
        {
            *nodeField = *newNode;
        }
        else
        {
            nodeField.reset(new LayoutSizeNode(*newNode));
        }
    }
    else
    {
        nodeField.release();
    }
}
