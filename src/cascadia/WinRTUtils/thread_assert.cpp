// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "thread_assert.h"

#ifdef _DEBUG

static std::atomic<DWORD> foreground_thread_id;

__declspec(noinline) void throw_exception(const DWORD foreground_thread_id, const char* description)
{
    if (!foreground_thread_id)
    {
        description = "mark_as_foreground_thread must be called before any thread assertions";
    }

    throw std::exception(description);
}

void mark_as_foreground_thread() {
    foreground_thread_id.store(GetCurrentThreadId());
}

void assert_foreground_thread() {
    if (const auto id = foreground_thread_id.load(std::memory_order_relaxed); id != GetCurrentThreadId()) {
        throw_exception(id, "expected to be called from a foreground thread");
    }
}

void assert_background_thread() {
    if (const auto id = foreground_thread_id.load(std::memory_order_relaxed); id != GetCurrentThreadId()) {
        throw_exception(id, "expected to be called from a background thread");
    }
}

#endif // _DEBUG
