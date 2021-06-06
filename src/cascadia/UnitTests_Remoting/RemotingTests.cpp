// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../Remoting/Monarch.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;

#define MAKE_MONARCH(name, pid)                               \
    Remoting::implementation::Monarch _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Monarch> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup = wil::scope_exit([&]() { name.detach(); });

namespace RemotingUnitTests
{
    class RemotingTests
    {
        BEGIN_TEST_CLASS(RemotingTests)
        END_TEST_CLASS()

        TEST_METHOD(CreateMonarch);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }
    };

    void RemotingTests::CreateMonarch()
    {
        auto m1 = winrt::make_self<Remoting::implementation::Monarch>();
        VERIFY_IS_NOT_NULL(m1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         m1->GetPID(),
                         L"A Monarch without an explicit PID should use the current PID");

        Log::Comment(L"That's what we need for window process management, but for tests, it'll be more useful to fake the PIDs.");

        auto expectedFakePID = 1234u;
        MAKE_MONARCH(m2, expectedFakePID);

        VERIFY_IS_NOT_NULL(m2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         m2->GetPID(),
                         L"A Monarch with an explicit PID should use the one we provided");
    }

}
