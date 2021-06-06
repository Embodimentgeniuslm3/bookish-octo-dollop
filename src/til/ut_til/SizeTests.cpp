// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "til/size.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class SizeTests
{
    TEST_CLASS(SizeTests);

    TEST_METHOD(DefaultConstruct)
    {
        const til::size sz;
        VERIFY_ARE_EQUAL(0, sz._width);
        VERIFY_ARE_EQUAL(0, sz._height);
    }

    TEST_METHOD(RawConstruct)
    {
        const til::size sz{ 5, 10 };
        VERIFY_ARE_EQUAL(5, sz._width);
        VERIFY_ARE_EQUAL(10, sz._height);
    }

    TEST_METHOD(UnsignedConstruct)
    {
        Log::Comment(L"0.) Normal unsigned construct.");
        {
            const size_t width = 5;
            const size_t height = 10;

            const til::size sz{ width, height };
            VERIFY_ARE_EQUAL(5, sz._width);
            VERIFY_ARE_EQUAL(10, sz._height);
        }

        Log::Comment(L"1.) Unsigned construct overflow on width.");
        {
            constexpr size_t width = std::numeric_limits<size_t>().max();
            const size_t height = 10;

            auto fn = [&]()
            {
                til::size sz{ width, height };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Unsigned construct overflow on height.");
        {
            constexpr size_t height = std::numeric_limits<size_t>().max();
            const size_t width = 10;

            auto fn = [&]()
            {
                til::size sz{ width, height };
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(SignedConstruct)
    {
        const ptrdiff_t width = -5;
        const ptrdiff_t height = -10;

        const til::size sz{ width, height };
        VERIFY_ARE_EQUAL(width, sz._width);
        VERIFY_ARE_EQUAL(height, sz._height);
    }

    TEST_METHOD(CoordConstruct)
    {
        COORD coord{ -5, 10 };

        const til::size sz{ coord };
        VERIFY_ARE_EQUAL(coord.X, sz._width);
        VERIFY_ARE_EQUAL(coord.Y, sz._height);
    }

    TEST_METHOD(SizeConstruct)
    {
        SIZE size{ 5, -10 };

        const til::size sz{ size };
        VERIFY_ARE_EQUAL(size.cx, sz._width);
        VERIFY_ARE_EQUAL(size.cy, sz._height);
    }

    TEST_METHOD(Equality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 == s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::size s1{ 4, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 6, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::size s1{ 5, 9 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 == s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 11 };
            VERIFY_IS_FALSE(s1 == s2);
        }
    }

    TEST_METHOD(Inequality)
    {
        Log::Comment(L"0.) Equal.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_FALSE(s1 != s2);
        }

        Log::Comment(L"1.) Left Width changed.");
        {
            const til::size s1{ 4, 10 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"2.) Right Width changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 6, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"3.) Left Height changed.");
        {
            const til::size s1{ 5, 9 };
            const til::size s2{ 5, 10 };
            VERIFY_IS_TRUE(s1 != s2);
        }

        Log::Comment(L"4.) Right Height changed.");
        {
            const til::size s1{ 5, 10 };
            const til::size s2{ 5, 11 };
            VERIFY_IS_TRUE(s1 != s2);
        }
    }

    TEST_METHOD(Width)
    {
        const til::size sz{ 5,10 };
        VERIFY_ARE_EQUAL(sz._width, sz.width());
    }

    TEST_METHOD(Height)
    {
        const til::size sz{ 5,10 };
        VERIFY_ARE_EQUAL(sz._height, sz.height());
    }

    TEST_METHOD(Area)
    {
        Log::Comment(L"0.) Area of two things that should be in bounds.");
        {
            const til::size sz{ 5,10 };
            VERIFY_ARE_EQUAL(sz._width * sz._height, sz.area());
        }

        Log::Comment(L"1.) Area is out of bounds on multiplication.");
        {
            constexpr ptrdiff_t bigSize = std::numeric_limits<ptrdiff_t>().max();
            const til::size sz{ bigSize, bigSize};

            auto fn = [&]()
            {
                sz.area();
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToCoord)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            COORD val = sz;
            VERIFY_ARE_EQUAL(5, val.X);
            VERIFY_ARE_EQUAL(10, val.Y);
        }

        Log::Comment(L"1.) Overflow on width.");
        {
            constexpr ptrdiff_t width = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t height = 10;
            const til::size sz{ width, height };

            auto fn = [&]()
            {
                COORD val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on height.");
        {
            constexpr ptrdiff_t height = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t width = 10;
            const til::size sz{ width, height };

            auto fn = [&]()
            {
                COORD val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToSize)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            SIZE val = sz;
            VERIFY_ARE_EQUAL(5, val.cx);
            VERIFY_ARE_EQUAL(10, val.cy);
        }

        Log::Comment(L"1.) Overflow on width.");
        {
            constexpr ptrdiff_t width = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t height = 10;
            const til::size sz{ width, height };

            auto fn = [&]()
            {
                SIZE val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }

        Log::Comment(L"2.) Overflow on height.");
        {
            constexpr ptrdiff_t height = std::numeric_limits<ptrdiff_t>().max();
            const ptrdiff_t width = 10;
            const til::size sz{ width, height };

            auto fn = [&]()
            {
                SIZE val = sz;
            };

            VERIFY_THROWS_SPECIFIC(fn(), wil::ResultException, [](wil::ResultException& e) { return e.GetErrorCode() == E_ABORT; });
        }
    }

    TEST_METHOD(CastToD2D1SizeF)
    {
        Log::Comment(L"0.) Typical situation.");
        {
            const til::size sz{ 5, 10 };
            D2D1_SIZE_F val = sz;
            VERIFY_ARE_EQUAL(5, val.width);
            VERIFY_ARE_EQUAL(10, val.height);
        }

        // All ptrdiff_ts fit into a float, so there's no exception tests.
    }
};
