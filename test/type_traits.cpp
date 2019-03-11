// Copyright 2019 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the piranha library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <piranha/type_traits.hpp>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <string>

#include <piranha/config.hpp>

using namespace piranha;

TEST_CASE("is_cpp_integral")
{
    REQUIRE(is_cpp_integral_v<int>);
    REQUIRE(is_cpp_integral_v<const int>);
    REQUIRE(is_cpp_integral_v<const volatile int>);
    REQUIRE(is_cpp_integral_v<volatile int>);
    REQUIRE(!is_cpp_integral_v<int &>);
    REQUIRE(!is_cpp_integral_v<const int &>);
    REQUIRE(!is_cpp_integral_v<int &&>);

    REQUIRE(is_cpp_integral_v<long>);
    REQUIRE(is_cpp_integral_v<const long>);
    REQUIRE(is_cpp_integral_v<const volatile long>);
    REQUIRE(is_cpp_integral_v<volatile long>);
    REQUIRE(!is_cpp_integral_v<long &>);
    REQUIRE(!is_cpp_integral_v<const long &>);
    REQUIRE(!is_cpp_integral_v<long &&>);

    REQUIRE(!is_cpp_integral_v<float>);
    REQUIRE(!is_cpp_integral_v<const double>);
    REQUIRE(!is_cpp_integral_v<const volatile long double>);
    REQUIRE(!is_cpp_integral_v<volatile float>);

    REQUIRE(!is_cpp_integral_v<std::string>);
    REQUIRE(!is_cpp_integral_v<void>);

#if defined(PIRANHA_HAVE_GCC_INT128)
    REQUIRE(is_cpp_integral_v<__int128_t>);
    REQUIRE(is_cpp_integral_v<const __int128_t>);
    REQUIRE(is_cpp_integral_v<const volatile __int128_t>);
    REQUIRE(is_cpp_integral_v<volatile __int128_t>);
    REQUIRE(!is_cpp_integral_v<__int128_t &>);
    REQUIRE(!is_cpp_integral_v<const __int128_t &>);
    REQUIRE(!is_cpp_integral_v<__int128_t &&>);

    REQUIRE(is_cpp_integral_v<__uint128_t>);
    REQUIRE(is_cpp_integral_v<const __uint128_t>);
    REQUIRE(is_cpp_integral_v<const volatile __uint128_t>);
    REQUIRE(is_cpp_integral_v<volatile __uint128_t>);
    REQUIRE(!is_cpp_integral_v<__uint128_t &>);
    REQUIRE(!is_cpp_integral_v<const __uint128_t &>);
    REQUIRE(!is_cpp_integral_v<__uint128_t &&>);
#endif

#if defined(PIRANHA_HAVE_CONCEPTS)
    REQUIRE(CppIntegral<int>);
    REQUIRE(CppIntegral<const int>);
    REQUIRE(CppIntegral<const volatile int>);
    REQUIRE(CppIntegral<volatile int>);
    REQUIRE(!CppIntegral<int &>);
    REQUIRE(!CppIntegral<const int &>);
    REQUIRE(!CppIntegral<int &&>);
#endif
}

TEST_CASE("is_cpp_floating_point")
{
    REQUIRE(is_cpp_floating_point_v<float>);
    REQUIRE(is_cpp_floating_point_v<const float>);
    REQUIRE(is_cpp_floating_point_v<const volatile float>);
    REQUIRE(is_cpp_floating_point_v<volatile float>);
    REQUIRE(!is_cpp_floating_point_v<float &>);
    REQUIRE(!is_cpp_floating_point_v<const float &>);
    REQUIRE(!is_cpp_floating_point_v<float &&>);

    REQUIRE(is_cpp_floating_point_v<double>);
    REQUIRE(is_cpp_floating_point_v<const double>);
    REQUIRE(is_cpp_floating_point_v<const volatile double>);
    REQUIRE(is_cpp_floating_point_v<volatile double>);
    REQUIRE(!is_cpp_floating_point_v<double &>);
    REQUIRE(!is_cpp_floating_point_v<const double &>);
    REQUIRE(!is_cpp_floating_point_v<double &&>);

    REQUIRE(!is_cpp_floating_point_v<int>);
    REQUIRE(!is_cpp_floating_point_v<const long>);
    REQUIRE(!is_cpp_floating_point_v<const volatile long long>);
    REQUIRE(!is_cpp_floating_point_v<volatile short>);

    REQUIRE(!is_cpp_floating_point_v<std::string>);
    REQUIRE(!is_cpp_floating_point_v<void>);

#if defined(PIRANHA_HAVE_CONCEPTS)
    REQUIRE(CppFloatingPoint<float>);
    REQUIRE(CppFloatingPoint<const float>);
    REQUIRE(CppFloatingPoint<const volatile float>);
    REQUIRE(CppFloatingPoint<volatile float>);
    REQUIRE(!CppFloatingPoint<float &>);
    REQUIRE(!CppFloatingPoint<const float &>);
    REQUIRE(!CppFloatingPoint<float &&>);
#endif
}

TEST_CASE("is_cpp_arithmetic")
{
    REQUIRE(is_cpp_arithmetic_v<int>);
    REQUIRE(is_cpp_arithmetic_v<const bool>);
    REQUIRE(is_cpp_arithmetic_v<const volatile float>);
    REQUIRE(is_cpp_arithmetic_v<volatile double>);
    REQUIRE(!is_cpp_arithmetic_v<float &>);
    REQUIRE(!is_cpp_arithmetic_v<const float &>);
    REQUIRE(!is_cpp_arithmetic_v<float &&>);

    REQUIRE(!is_cpp_arithmetic_v<std::string>);
    REQUIRE(!is_cpp_arithmetic_v<void>);

#if defined(PIRANHA_HAVE_CONCEPTS)
    REQUIRE(CppArithmetic<float>);
    REQUIRE(CppArithmetic<const bool>);
    REQUIRE(CppArithmetic<const volatile long>);
    REQUIRE(CppArithmetic<volatile char>);
    REQUIRE(!CppArithmetic<float &>);
    REQUIRE(!CppArithmetic<const int &>);
    REQUIRE(!CppArithmetic<short &&>);
#endif
}