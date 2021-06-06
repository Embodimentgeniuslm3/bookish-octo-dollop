﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// pch.h
// Header for platform projection include files
//

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOHELP
#define NOCOMM

#include <LibraryIncludes.h>
// This is inexplicable, but for whatever reason, cppwinrt conflicts with the
//      SDK definition of this function, so the only fix is to undef it.
// from WinBase.h
// Windows::UI::Xaml::Media::Animation::IStoryboard::GetCurrentTime
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <unknwn.h>
#include <wil/cppwinrt.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.h>

#include <Shobjidl.h>
#include <shlwapi.h>

#include <wrl.h>
#include <wrl/module.h>
