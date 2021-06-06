// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef _DEBUG
void mark_as_foreground_thread();
void assert_foreground_thread();
void assert_background_thread();
#else
constexpr void mark_as_foreground_thread() {}
constexpr void assert_foreground_thread() {}
constexpr void assert_background_thread() {}
#endif
