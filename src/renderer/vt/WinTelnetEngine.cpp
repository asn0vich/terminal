// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WinTelnetEngine.hpp"
#include "..\..\inc\conattrs.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

WinTelnetEngine::WinTelnetEngine(_In_ wil::unique_hfile hPipe,
                                 const IDefaultColorProvider& colorProvider,
                                 const Viewport initialViewport,
                                 const std::basic_string_view<COLORREF> colorTable) :
    VtEngine(std::move(hPipe), colorProvider, initialViewport),
    _colorTable(colorTable)
{
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Only writes
//      16-color attributes.
// Arguments:
// - colorForeground: The RGB Color to use to paint the foreground text.
// - colorBackground: The RGB Color to use to paint the background of the text.
// - legacyColorAttribute: A console attributes bit field specifying the brush
//      colors we should use.
// - extendedAttrs - extended text attributes (italic, underline, etc.) to use.
// - isSettingDefaultBrushes: indicates if we should change the background color of
//      the window. Unused for VT
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT WinTelnetEngine::UpdateDrawingBrushes(const COLORREF colorForeground,
                                                            const COLORREF colorBackground,
                                                            const WORD /*legacyColorAttribute*/,
                                                            const ExtendedAttributes extendedAttrs,
                                                            const bool /*isSettingDefaultBrushes*/) noexcept
{
    return VtEngine::_16ColorUpdateDrawingBrushes(colorForeground,
                                                  colorBackground,
                                                  WI_IsFlagSet(extendedAttrs, ExtendedAttributes::Bold),
                                                  _colorTable);
}

// Routine Description:
// - Write a VT sequence to move the cursor to the specified coordinates. We
//      also store the last place we left the cursor for future optimizations.
// Arguments:
// - coord: location to move the cursor to.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT WinTelnetEngine::_MoveCursor(COORD const coord) noexcept
{
    HRESULT hr = S_OK;
    // don't try and be clever about moving the cursor.
    // Always just use the full sequence
    if (coord.X != _lastText.X || coord.Y != _lastText.Y)
    {
        hr = _CursorPosition(coord);
        if (SUCCEEDED(hr))
        {
            _lastText = coord;
        }
    }
    return hr;
}

// Routine Description:
// - Scrolls the existing data on the in-memory frame by the scroll region
//      deltas we have collectively received through the Invalidate methods
//      since the last time this was called.
//  Because win-telnet doesn't know how to do anything smart in response to
//      scrolling, we do nothing.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT WinTelnetEngine::ScrollFrame() noexcept
{
    // win-telnet doesn't know anything about scroll vt sequences
    // every frame, we're repainting everything, always.
    return S_OK;
}

// Routine Description:
// - Notifies us that the console is attempting to scroll the existing screen
//      area
// Arguments:
// - pcoordDelta - Pointer to character dimension (COORD) of the distance the
//      console would like us to move while scrolling.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT WinTelnetEngine::InvalidateScroll(const COORD* const /*pcoordDelta*/) noexcept
{
    // win-telnet assumes the client doesn't know anything about inserting or
    //  deleting lines.
    // So instead, just invalidate the entire viewport. Every line is going to
    //  have to move.
    return InvalidateAll();
}

// Method Description:
// - Wrapper for ITerminalOutputConnection. Write an ascii-only string to the pipe.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from either conversion or writing pipe.
[[nodiscard]] HRESULT WinTelnetEngine::WriteTerminalW(_In_ const std::wstring_view wstr) noexcept
{
    RETURN_IF_FAILED(VtEngine::_WriteTerminalAscii(wstr));
    // GH#4106, GH#2011 - WriteTerminalW is only ever called by the
    // StateMachine, when we've encountered a string we don't understand. When
    // this happens, we usually don't actually trigger another frame, but we
    // _do_ want this string to immediately be sent to the terminal. Since we
    // only flush our buffer on actual frames, this means that strings we've
    // decided to pass through would have gotten buffered here until the next
    // actual frame is triggered.
    //
    // To fix this, flush here, so this string is sent to the connected terminal
    // application.

    return _Flush();
}
