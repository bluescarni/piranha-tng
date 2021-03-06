// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <string>
#include <type_traits>

#include <obake/key/key_is_zero.hpp>
#include <obake/symbols.hpp>
#include <obake/type_traits.hpp>

#include "catch.hpp"

using namespace obake;

// ADL-based implementations.
namespace ns
{

struct zt00 {
};

bool key_is_zero(const zt00 &, const symbol_set &);

struct zt01 {
};

// Disable certain overloads.
bool key_is_zero(zt01 &, const symbol_set &);

struct nzt00 {
};

// Wrong return type.
std::string key_is_zero(nzt00 &, const symbol_set &);

} // namespace ns

struct ext_zt00 {
};

struct ext_zt01 {
};

struct ext_nzt00 {
};

namespace obake::customisation
{

template <typename T>
requires SameCvr<T, ext_zt00>
inline constexpr auto key_is_zero<T> = [](auto &&, const symbol_set &) constexpr noexcept
{
    return true;
};

template <typename T>
requires SameCvr<T, ext_zt01>
inline constexpr auto key_is_zero<T> = [](auto &, const symbol_set &) constexpr noexcept
{
    return true;
};

template <typename T>
requires SameCvr<T, ext_nzt00>
inline constexpr auto key_is_zero<T> = [](auto &&, const symbol_set &) constexpr noexcept
{
    return std::string{};
};

} // namespace obake::customisation

TEST_CASE("key_is_zero_test")
{
    REQUIRE(!is_zero_testable_key_v<void>);

    REQUIRE(!is_zero_testable_key_v<int>);
    REQUIRE(!is_zero_testable_key_v<int &>);
    REQUIRE(!is_zero_testable_key_v<const int &>);
    REQUIRE(!is_zero_testable_key_v<int &&>);

    REQUIRE(!is_zero_testable_key_v<std::string>);
    REQUIRE(!is_zero_testable_key_v<std::string &>);
    REQUIRE(!is_zero_testable_key_v<const std::string &>);
    REQUIRE(!is_zero_testable_key_v<std::string &&>);

    REQUIRE(is_zero_testable_key_v<ns::zt00>);
    REQUIRE(is_zero_testable_key_v<ns::zt00 &>);
    REQUIRE(is_zero_testable_key_v<const ns::zt00 &>);
    REQUIRE(is_zero_testable_key_v<ns::zt00 &&>);

    REQUIRE(!is_zero_testable_key_v<ns::zt01>);
    REQUIRE(is_zero_testable_key_v<ns::zt01 &>);
    REQUIRE(!is_zero_testable_key_v<const ns::zt01 &>);
    REQUIRE(!is_zero_testable_key_v<ns::zt01 &&>);

    REQUIRE(!is_zero_testable_key_v<const ns::nzt00 &>);

    REQUIRE(is_zero_testable_key_v<ext_zt00>);
    REQUIRE(is_zero_testable_key_v<ext_zt00 &>);
    REQUIRE(is_zero_testable_key_v<const ext_zt00 &>);
    REQUIRE(is_zero_testable_key_v<ext_zt00 &&>);

    REQUIRE(!is_zero_testable_key_v<ext_zt01>);
    REQUIRE(is_zero_testable_key_v<ext_zt01 &>);
    REQUIRE(is_zero_testable_key_v<const ext_zt01 &>);
    REQUIRE(!is_zero_testable_key_v<ext_zt01 &&>);

    REQUIRE(!is_zero_testable_key_v<const ext_nzt00 &>);

    REQUIRE(!ZeroTestableKey<void>);

    REQUIRE(!ZeroTestableKey<int>);
    REQUIRE(!ZeroTestableKey<int &>);
    REQUIRE(!ZeroTestableKey<const int &>);
    REQUIRE(!ZeroTestableKey<int &&>);

    REQUIRE(!ZeroTestableKey<std::string>);
    REQUIRE(!ZeroTestableKey<std::string &>);
    REQUIRE(!ZeroTestableKey<const std::string &>);
    REQUIRE(!ZeroTestableKey<std::string &&>);

    REQUIRE(ZeroTestableKey<ns::zt00>);
    REQUIRE(ZeroTestableKey<ns::zt00 &>);
    REQUIRE(ZeroTestableKey<const ns::zt00 &>);
    REQUIRE(ZeroTestableKey<ns::zt00 &&>);

    REQUIRE(!ZeroTestableKey<ns::zt01>);
    REQUIRE(ZeroTestableKey<ns::zt01 &>);
    REQUIRE(!ZeroTestableKey<const ns::zt01 &>);
    REQUIRE(!ZeroTestableKey<ns::zt01 &&>);

    REQUIRE(!ZeroTestableKey<const ns::nzt00 &>);

    REQUIRE(ZeroTestableKey<ext_zt00>);
    REQUIRE(ZeroTestableKey<ext_zt00 &>);
    REQUIRE(ZeroTestableKey<const ext_zt00 &>);
    REQUIRE(ZeroTestableKey<ext_zt00 &&>);

    REQUIRE(!ZeroTestableKey<ext_zt01>);
    REQUIRE(ZeroTestableKey<ext_zt01 &>);
    REQUIRE(ZeroTestableKey<const ext_zt01 &>);
    REQUIRE(!ZeroTestableKey<ext_zt01 &&>);

    REQUIRE(!ZeroTestableKey<const ext_nzt00 &>);
}
