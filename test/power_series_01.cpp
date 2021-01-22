// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include <obake/cf/cf_tex_stream_insert.hpp>
#include <obake/polynomials/d_packed_monomial.hpp>
#include <obake/polynomials/packed_monomial.hpp>
#include <obake/power_series/power_series.hpp>
#include <obake/symbols.hpp>
#include <obake/tex_stream_insert.hpp>

#include "catch.hpp"
#include "test_utils.hpp"

using namespace obake;

TEST_CASE("in place add")
{
    obake_test::disable_slow_stack_traces();

    using pm_t = packed_monomial<std::int32_t>;
    using ps_t = p_series<pm_t, double>;
    using ps2_t = p_series<pm_t, float>;

    {
        auto [x] = make_p_series<ps_t>("x");

        // Check that the primitive returns a reference.
        REQUIRE(std::is_reference_v<decltype(power_series::series_in_place_add(x, 1))>);
        REQUIRE(std::is_same_v<decltype(x += 2.), ps_t &>);
        REQUIRE(std::is_same_v<decltype(x += ps2_t{}), ps_t &>);
        ps2_t y;
        REQUIRE(std::is_same_v<decltype(y += ps_t{}), ps2_t &>);

        x += 2.;
        REQUIRE(x.size() == 2u);
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == 2.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0}; }));
        REQUIRE(obake::get_truncation(x).index() == 0u);
    }

    // Example with truncation.
    {
        auto [x] = make_p_series_t<ps_t>(-1, "x");

        REQUIRE(x.empty());
        x += 2.;
        REQUIRE(x.empty());
        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == -1);
    }

    // Same-rank series.
    {
        auto [x, y] = make_p_series_t<ps_t>(10, "x", "y");

        // Check that the primitive returns a reference.
        REQUIRE(std::is_reference_v<decltype(power_series::series_in_place_add(x, y))>);
        REQUIRE(std::is_reference_v<decltype(x += y)>);

        x += y;
        REQUIRE(x.size() == 2u);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::all_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
        // Check that the truncation level is perserved
        // in the return value.
        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 10);
    }

    // Check incompatible truncation levels.
    {
        auto [x] = make_p_series_t<ps_t>(10, "x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x += y, std::invalid_argument,
                                       "Unable to add two power series in place if "
                                       "their truncation levels do not match");
    }
    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"b"}, "x");
        auto [y] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x += y, std::invalid_argument,
                                       "Unable to add two power series in place if "
                                       "their truncation levels do not match");
    }
    // Truncation vs no truncation.
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        x += y;

        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 20);
        REQUIRE(x.size() == 2u);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::all_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        y += x;

        REQUIRE(obake::get_truncation(y).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(y)) == 20);
        REQUIRE(y.size() == 2u);
        REQUIRE(y.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::all_of(y.begin(), y.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(0, "y");

        x += y;

        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 0);
        REQUIRE(x.empty());
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(0, "y");

        y += x;

        REQUIRE(obake::get_truncation(y).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(y)) == 0);
        REQUIRE(y.empty());
        REQUIRE(y.get_symbol_set() == symbol_set{"x", "y"});
    }
    // Incompatible policies.
    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x += y, std::invalid_argument,
                                       "Unable to add two power series in place if "
                                       "their truncation policies do not match");
        OBAKE_REQUIRES_THROWS_CONTAINS(y += x, std::invalid_argument,
                                       "Unable to add two power series in place if "
                                       "their truncation policies do not match");
    }

    // Number on the left.
    {
        auto x = ps_t{5};
        double tmp = 5;
        tmp += x;
        REQUIRE(tmp == 10);
    }
}

TEST_CASE("in place sub")
{
    using pm_t = packed_monomial<std::int32_t>;
    using ps_t = p_series<pm_t, double>;
    using ps2_t = p_series<pm_t, float>;

    {
        auto [x] = make_p_series<ps_t>("x");

        // Check that the primitive returns a reference.
        REQUIRE(std::is_reference_v<decltype(power_series::series_in_place_sub(x, 1))>);
        REQUIRE(std::is_same_v<decltype(x -= 2.), ps_t &>);
        REQUIRE(std::is_same_v<decltype(x -= ps2_t{}), ps_t &>);
        ps2_t y;
        REQUIRE(std::is_same_v<decltype(y -= ps_t{}), ps2_t &>);

        x -= 2.;
        REQUIRE(x.size() == 2u);
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == -2.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0}; }));
    }

    // Example with truncation.
    {
        auto [x] = make_p_series_t<ps_t>(-1, "x");

        REQUIRE(x.empty());
        x -= 2.;
        REQUIRE(x.empty());
    }

    // Same-rank series.
    {
        auto [x, y] = make_p_series_t<ps_t>(10, "x", "y");

        // Check that the primitive returns a reference.
        REQUIRE(std::is_reference_v<decltype(power_series::series_in_place_sub(x, y))>);
        REQUIRE(std::is_reference_v<decltype(x -= y)>);

        x -= y;
        REQUIRE(x.size() == 2u);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == -1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
    }

    // Check incompatible truncation levels.
    {
        auto [x] = make_p_series_t<ps_t>(10, "x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x -= y, std::invalid_argument,
                                       "Unable to subtract two power series in place if "
                                       "their truncation levels do not match");
    }
    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"b"}, "x");
        auto [y] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x -= y, std::invalid_argument,
                                       "Unable to subtract two power series in place if "
                                       "their truncation levels do not match");
    }
    // Truncation vs no truncation.
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        x -= y;

        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 20);
        REQUIRE(x.size() == 2u);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.second == -1.; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(x.begin(), x.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        y -= x;

        REQUIRE(obake::get_truncation(y).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(y)) == 20);
        REQUIRE(y.size() == 2u);
        REQUIRE(y.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.second == 1.; }));
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.second == -1.; }));
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.first == pm_t{1, 0}; }));
        REQUIRE(std::any_of(y.begin(), y.end(), [](const auto &t) { return t.first == pm_t{0, 1}; }));
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(0, "y");

        x -= y;

        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 0);
        REQUIRE(x.empty());
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
    }
    {
        auto [x] = make_p_series<ps_t>("x");
        auto [y] = make_p_series_t<ps_t>(0, "y");

        y -= x;

        REQUIRE(obake::get_truncation(y).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(y)) == 0);
        REQUIRE(y.empty());
        REQUIRE(y.get_symbol_set() == symbol_set{"x", "y"});
    }
    // Incompatible policies.
    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "x");
        auto [y] = make_p_series_t<ps_t>(20, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x -= y, std::invalid_argument,
                                       "Unable to subtract two power series in place if "
                                       "their truncation policies do not match");
        OBAKE_REQUIRES_THROWS_CONTAINS(y -= x, std::invalid_argument,
                                       "Unable to subtract two power series in place if "
                                       "their truncation policies do not match");
    }

    // Number on the left.
    {
        auto x = ps_t{2};
        double tmp = 5;
        tmp -= x;
        REQUIRE(tmp == 3);
    }
}

TEST_CASE("tex stream insert")
{
    using pm_t = packed_monomial<std::int32_t>;
    using ps_t = p_series<pm_t, double>;

    {
        auto [x] = make_p_series<ps_t>("x");

        obake::tex_stream_insert(std::cout, x);

        std::cout << '\n';
    }

    {
        auto [x] = make_p_series_t<ps_t>(10, "x");

        obake::tex_stream_insert(std::cout, x);

        std::cout << '\n';
    }

    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"a", "b"}, "x");

        obake::tex_stream_insert(std::cout, x);

        std::cout << '\n';
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(10, symbol_set{"a", "b"}, "x", "y");

        obake::cf_tex_stream_insert(std::cout, x + y);

        std::cout << '\n';
    }
}

TEST_CASE("tex stream insert bug")
{
    using pm_t = packed_monomial<std::int32_t>;
    using dpm_t = d_packed_monomial<std::int32_t, 8>;
    using ps_t = p_series<pm_t, double>;
    using ps2_t = p_series<dpm_t, double>;

    std::ostringstream oss;
    obake::tex_stream_insert(oss, ps_t{1});
    REQUIRE(oss.str() == "1");
    oss.str("");

    obake::tex_stream_insert(oss, ps2_t{1});
    REQUIRE(oss.str() == "1");
    oss.str("");
}

TEST_CASE("multiplication")
{
    using pm_t = packed_monomial<std::int32_t>;
    using ps_t = p_series<pm_t, double>;

    {
        auto [x, y] = make_p_series<ps_t>("x", "y");

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.size() == 1u);
        REQUIRE(get_truncation(ret).index() == 0u);
        REQUIRE(ret.begin()->first == pm_t{1, 1});
        REQUIRE(ret.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(3, "x", "y");

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.size() == 1u);
        REQUIRE(get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(ret)) == 3);
        REQUIRE(ret.begin()->first == pm_t{1, 1});
        REQUIRE(ret.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(1, "x", "y");

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.empty());
        REQUIRE(get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(ret)) == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x", "y");

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.size() == 1u);
        REQUIRE(get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(ret)) == std::pair{std::int32_t(3), symbol_set{"a", "b"}});
        REQUIRE(ret.begin()->first == pm_t{1, 1});
        REQUIRE(ret.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(1, symbol_set{"x", "y", "z"}, "x", "y");

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.empty());
        REQUIRE(get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(ret)) == std::pair{std::int32_t(1), symbol_set{"x", "y", "z"}});
    }

    // Conflicting truncation levels.
    {
        auto [x] = make_p_series_t<ps_t>(3, "x");
        auto [y] = make_p_series_t<ps_t>(2, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x * y, std::invalid_argument,
                                       "Unable to multiply two power series if their truncation levels do not match");
    }
    {
        auto [x] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x");
        auto [y] = make_p_series_p<ps_t>(3, symbol_set{"a", "c"}, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x * y, std::invalid_argument,
                                       "Unable to multiply two power series if their truncation levels do not match");
    }

    // Truncation vs no truncation.
    {
        auto [x, y] = make_p_series_t<ps_t>(3, "x", "y");
        obake::unset_truncation(y);

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.size() == 1u);
        REQUIRE(get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(ret)) == 3);
        REQUIRE(ret.begin()->first == pm_t{1, 1});
        REQUIRE(ret.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(1, "x", "y");
        obake::unset_truncation(x);

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.empty());
        REQUIRE(get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(ret)) == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x", "y");
        obake::unset_truncation(x);

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.size() == 1u);
        REQUIRE(get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(ret)) == std::pair{std::int32_t(3), symbol_set{"a", "b"}});
        REQUIRE(ret.begin()->first == pm_t{1, 1});
        REQUIRE(ret.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(1, symbol_set{"x", "y", "z"}, "x", "y");
        obake::unset_truncation(y);

        auto ret = x * y;

        REQUIRE(std::is_same_v<decltype(ret), ps_t>);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(ret.empty());
        REQUIRE(get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(ret)) == std::pair{std::int32_t(1), symbol_set{"x", "y", "z"}});
    }

    // Test with different-rank operands.
    {
        auto [x] = make_p_series<ps_t>("x");

        auto ret = x * 5;
        REQUIRE(obake::get_truncation(ret).index() == 0u);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        ret = 5 * x;
        REQUIRE(obake::get_truncation(ret).index() == 0u);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        // Multiplication by zero.
        ret = x * 0;
        REQUIRE(obake::get_truncation(ret).index() == 0u);
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());

        ret = 0 * x;
        REQUIRE(obake::get_truncation(ret).index() == 0u);
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());
    }

    {
        auto [x] = make_p_series_t<ps_t>(10, "x");

        auto ret = x * 5;
        REQUIRE(obake::get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(ret)) == 10);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        ret = 5 * x;
        REQUIRE(obake::get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(ret)) == 10);
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        // Multiplication by zero.
        ret = x * 0;
        REQUIRE(obake::get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(ret)) == 10);
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());

        ret = 0 * x;
        REQUIRE(obake::get_truncation(ret).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(ret)) == 10);
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());
    }

    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "x");

        auto ret = x * 5;
        REQUIRE(obake::get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(ret)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        ret = 5 * x;
        REQUIRE(obake::get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(ret)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(ret.get_symbol_set() == symbol_set{"x"});
        REQUIRE(ret.begin()->first == pm_t{1});
        REQUIRE(ret.begin()->second == 5);

        // Multiplication by zero.
        ret = x * 0;
        REQUIRE(obake::get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(ret)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());

        ret = 0 * x;
        REQUIRE(obake::get_truncation(ret).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(ret)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(ret.get_symbol_set() == symbol_set{});
        REQUIRE(ret.empty());
    }
}

TEST_CASE("in-place multiplication")
{
    using pm_t = packed_monomial<std::int32_t>;
    using ps_t = p_series<pm_t, double>;

    {
        auto [x, y] = make_p_series<ps_t>("x", "y");

        x *= y;

        REQUIRE(std::is_same_v<decltype(x *= y), ps_t &>);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.size() == 1u);
        REQUIRE(get_truncation(x).index() == 0u);
        REQUIRE(x.begin()->first == pm_t{1, 1});
        REQUIRE(x.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(3, "x", "y");

        x *= y;

        REQUIRE(std::is_same_v<decltype(x *= y), ps_t &>);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.size() == 1u);
        REQUIRE(get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(x)) == 3);
        REQUIRE(x.begin()->first == pm_t{1, 1});
        REQUIRE(x.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(1, "x", "y");

        x *= y;

        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.empty());
        REQUIRE(get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(x)) == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x", "y");

        x *= y;

        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.size() == 1u);
        REQUIRE(get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(x)) == std::pair{std::int32_t(3), symbol_set{"a", "b"}});
        REQUIRE(x.begin()->first == pm_t{1, 1});
        REQUIRE(x.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(1, symbol_set{"x", "y", "z"}, "x", "y");

        x *= y;

        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.empty());
        REQUIRE(get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(x)) == std::pair{std::int32_t(1), symbol_set{"x", "y", "z"}});
    }

    // Conflicting truncation levels.
    {
        auto [x] = make_p_series_t<ps_t>(3, "x");
        auto [y] = make_p_series_t<ps_t>(2, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x *= y, std::invalid_argument,
                                       "Unable to multiply two power series if their truncation levels do not match");
    }
    {
        auto [x] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x");
        auto [y] = make_p_series_p<ps_t>(3, symbol_set{"a", "c"}, "y");

        OBAKE_REQUIRES_THROWS_CONTAINS(x *= y, std::invalid_argument,
                                       "Unable to multiply two power series if their truncation levels do not match");
    }

    // Truncation vs no truncation.
    {
        auto [x, y] = make_p_series_t<ps_t>(3, "x", "y");
        obake::unset_truncation(y);

        x *= y;

        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.size() == 1u);
        REQUIRE(get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(x)) == 3);
        REQUIRE(x.begin()->first == pm_t{1, 1});
        REQUIRE(x.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_t<ps_t>(1, "x", "y");
        obake::unset_truncation(x);

        x *= y;

        REQUIRE(std::is_same_v<decltype(x), ps_t>);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.empty());
        REQUIRE(get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(get_truncation(x)) == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(3, symbol_set{"a", "b"}, "x", "y");
        obake::unset_truncation(x);

        x *= y;

        REQUIRE(std::is_same_v<decltype(x), ps_t>);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.size() == 1u);
        REQUIRE(get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(x)) == std::pair{std::int32_t(3), symbol_set{"a", "b"}});
        REQUIRE(x.begin()->first == pm_t{1, 1});
        REQUIRE(x.begin()->second == 1);
    }

    {
        auto [x, y] = make_p_series_p<ps_t>(1, symbol_set{"x", "y", "z"}, "x", "y");
        obake::unset_truncation(y);

        x *= y;

        REQUIRE(std::is_same_v<decltype(x), ps_t>);
        REQUIRE(x.get_symbol_set() == symbol_set{"x", "y"});
        REQUIRE(x.empty());
        REQUIRE(get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(get_truncation(x)) == std::pair{std::int32_t(1), symbol_set{"x", "y", "z"}});
    }

    // Test with different-rank operands.
    {
        auto [x] = make_p_series<ps_t>("x");

        x *= 5;
        REQUIRE(obake::get_truncation(x).index() == 0u);
        REQUIRE(x.get_symbol_set() == symbol_set{"x"});
        REQUIRE(x.begin()->first == pm_t{1});
        REQUIRE(x.begin()->second == 5);

        // Multiplication by zero.
        x = make_p_series<ps_t>("x")[0];
        x *= 0;
        REQUIRE(obake::get_truncation(x).index() == 0u);
        REQUIRE(x.get_symbol_set() == symbol_set{});
        REQUIRE(x.empty());
    }

    {
        auto [x] = make_p_series_t<ps_t>(10, "x");

        x *= 5;
        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 10);
        REQUIRE(x.get_symbol_set() == symbol_set{"x"});
        REQUIRE(x.begin()->first == pm_t{1});
        REQUIRE(x.begin()->second == 5);

        // Multiplication by zero.
        x = make_p_series_t<ps_t>(10, "x")[0];
        x *= 0;
        REQUIRE(obake::get_truncation(x).index() == 1u);
        REQUIRE(std::get<1>(obake::get_truncation(x)) == 10);
        REQUIRE(x.get_symbol_set() == symbol_set{});
        REQUIRE(x.empty());
    }

    {
        auto [x] = make_p_series_p<ps_t>(10, symbol_set{"a"}, "x");

        x *= 5;
        REQUIRE(obake::get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(x)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(x.get_symbol_set() == symbol_set{"x"});
        REQUIRE(x.begin()->first == pm_t{1});
        REQUIRE(x.begin()->second == 5);

        // Multiplication by zero.
        x = make_p_series_p<ps_t>(10, symbol_set{"a"}, "x")[0];
        x *= 0;
        REQUIRE(obake::get_truncation(x).index() == 2u);
        REQUIRE(std::get<2>(obake::get_truncation(x)) == std::pair{std::int32_t(10), symbol_set{"a"}});
        REQUIRE(x.get_symbol_set() == symbol_set{});
        REQUIRE(x.empty());
    }

    // Non-series on the left.
    {
        auto x = ps_t{5};
        double tmp = 5;
        tmp *= x;
        REQUIRE(tmp == 25.);
    }
}
