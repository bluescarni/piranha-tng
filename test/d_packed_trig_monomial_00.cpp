// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <obake/config.hpp>
#include <obake/detail/tuple_for_each.hpp>
#include <obake/hash.hpp>
#include <obake/key/key_is_one.hpp>
#include <obake/key/key_is_zero.hpp>
#include <obake/kpack.hpp>
#include <obake/poisson_series/d_packed_trig_monomial.hpp>
#include <obake/symbols.hpp>
#include <obake/type_name.hpp>
#include <obake/type_traits.hpp>

#include "catch.hpp"
#include "test_utils.hpp"

using namespace obake;

using int_types = std::tuple<std::int32_t
#if defined(OBAKE_PACKABLE_INT64)
                             ,
                             std::int64_t
#endif
                             >;

// The packed sizes over which we will be testing for type T.
template <typename T>
using psizes
    = std::tuple<std::integral_constant<unsigned, poisson_series::dptm_default_psize>,
                 std::integral_constant<unsigned, 1>, std::integral_constant<unsigned, 2>,
                 std::integral_constant<unsigned, 3>, std::integral_constant<unsigned, detail::kpack_max_size<T>()>>;

std::mt19937 rng;

TEST_CASE("basic_test")
{
    obake_test::disable_slow_stack_traces();

    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            REQUIRE(!std::is_constructible_v<pm_t, void>);
            REQUIRE(!std::is_constructible_v<pm_t, int>);
            REQUIRE(!std::is_constructible_v<pm_t, const double &>);

            pm_t t00;

            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == true);

            t00 = pm_t(symbol_set{});
            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == true);

            t00 = pm_t(symbol_set{"x"});
            REQUIRE(t00._container().size() == 1u);
            REQUIRE(t00._container()[0] == 0);
            REQUIRE(t00.type() == true);

            t00 = pm_t(symbol_set{"x"}, false);
            REQUIRE(t00._container().size() == 1u);
            REQUIRE(t00._container()[0] == 0);
            REQUIRE(t00.type() == false);

            t00 = pm_t(symbol_set{"x", "y"});
            REQUIRE(t00._container().size() == (bw == 1u ? 2u : 1u));
            REQUIRE(std::all_of(t00._container().begin(), t00._container().end(), [](auto x) { return x == 0; }));
            REQUIRE(t00.type() == true);

            t00 = pm_t(symbol_set{"x", "y"}, false);
            REQUIRE(t00._container().size() == (bw == 1u ? 2u : 1u));
            REQUIRE(std::all_of(t00._container().begin(), t00._container().end(), [](auto x) { return x == 0; }));
            REQUIRE(t00.type() == false);

            std::vector<int_t> upack_v;
            auto upack = [&upack_v](const pm_t &t, unsigned size) {
                upack_v.clear();

                int_t tmp;

                auto i = 0u;
                for (const auto &n : t._container()) {
                    kunpacker<int_t> ku(n, pm_t::psize);

                    for (auto j = 0u; j < pm_t::psize && i != size; ++j, ++i) {
                        ku >> tmp;

                        upack_v.push_back(tmp);
                    }
                }
            };

            t00 = pm_t(static_cast<int_t *>(nullptr), 0);
            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == true);
            t00 = pm_t(static_cast<int_t *>(nullptr), static_cast<int_t *>(nullptr));
            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == true);

            t00 = pm_t(static_cast<int_t *>(nullptr), 0, false);
            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == false);
            t00 = pm_t(static_cast<int_t *>(nullptr), static_cast<int_t *>(nullptr), false);
            REQUIRE(t00._container().empty());
            REQUIRE(t00.type() == false);

            upack_v = std::vector<int_t>{1};
            t00 = pm_t(upack_v.data(), 1);
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{1});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v.begin(), upack_v.end());
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{1});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v);
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{1});
            REQUIRE(t00.type() == true);

            upack_v = std::vector<int_t>{2};
            t00 = pm_t(upack_v.data(), 1, false);
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{2});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v.begin(), upack_v.end(), false);
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{2});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v, false);
            upack(t00, 1);
            REQUIRE(upack_v == std::vector<int_t>{2});
            REQUIRE(t00.type() == false);

            upack_v = std::vector<int_t>{1, -1, 3, 3};
            t00 = pm_t(upack_v.data(), 4);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{1, -1, 3, 3});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v.begin(), upack_v.end());
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{1, -1, 3, 3});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{1, -1, 3, 3});
            REQUIRE(t00.type() == true);

            upack_v = std::vector<int_t>{0, 0, 3, 3};
            t00 = pm_t(upack_v.data(), 4, false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v.begin(), upack_v.end(), false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v, false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);

            upack_v = std::vector<int_t>{0, 0, 3, 3};
            t00 = pm_t(upack_v.data(), 4);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v.begin(), upack_v.end());
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == true);
            t00 = pm_t(upack_v);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == true);

            upack_v = std::vector<int_t>{0, 0, 3, 3};
            t00 = pm_t(upack_v.data(), 4, false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v.begin(), upack_v.end(), false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);
            t00 = pm_t(upack_v, false);
            upack(t00, 4);
            REQUIRE(upack_v == std::vector<int_t>{0, 0, 3, 3});
            REQUIRE(t00.type() == false);

            upack_v = std::vector<int_t>{-1, 0, 3, 3};
            OBAKE_REQUIRES_THROWS_CONTAINS(pm_t(upack_v.data(), 4), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-1) is negative");
            OBAKE_REQUIRES_THROWS_CONTAINS(pm_t(upack_v.begin(), upack_v.end()), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-1) is negative");
            OBAKE_REQUIRES_THROWS_CONTAINS(t00 = pm_t(upack_v), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-1) is negative");

            upack_v = std::vector<int_t>{0, 0, -3, 3};
            OBAKE_REQUIRES_THROWS_CONTAINS(pm_t(upack_v.data(), 4, false), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-3) is negative");
            OBAKE_REQUIRES_THROWS_CONTAINS(pm_t(upack_v.begin(), upack_v.end()), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-3) is negative");
            OBAKE_REQUIRES_THROWS_CONTAINS(t00 = pm_t(upack_v), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-3) is negative");

            // Ctor from init list.
            t00 = pm_t{1, 2, 3};
            upack(t00, 3);
            REQUIRE(upack_v == std::vector<int_t>{1, 2, 3});
            REQUIRE(t00.type() == true);

            t00 = pm_t{0, 2, -3};
            upack(t00, 3);
            REQUIRE(upack_v == std::vector<int_t>{0, 2, -3});
            REQUIRE(t00.type() == true);

            OBAKE_REQUIRES_THROWS_CONTAINS((t00 = pm_t{0, 0, -2}), std::invalid_argument,
                                           "Cannot construct a trigonometric monomial whose first nonzero "
                                           "exponent (-2) is negative");

            // Random testing.
            if constexpr (bw <= 3u) {
                using idist_t = std::uniform_int_distribution<detail::make_dependent_t<int_t, decltype(b)>>;
                using param_t = typename idist_t::param_type;
                idist_t dist;

                std::vector<int_t> copy;

                for (auto i = 0u; i < 1000u; ++i) {
                    upack_v.resize(i);

                    bool first_nz_found = false;
                    for (auto j = 0u; j < i; ++j) {
                        auto tmp = dist(rng, param_t{-10, 10});

                        if (!first_nz_found && tmp < 0) {
                            tmp = -tmp;
                        }

                        upack_v[j] = tmp;

                        first_nz_found = (first_nz_found || tmp != 0);
                    }

                    // Construct the monomial.
                    const bool type = (dist(rng, param_t{0, 1}) == 0);
                    pm_t pm(upack_v.data(), i, type);

                    copy = upack_v;

                    // Unpack it.
                    upack(pm, i);

                    REQUIRE(upack_v == copy);
                    REQUIRE(pm.type() == type);

                    // Do the same with input iterators.
                    pm = pm_t(upack_v.begin(), upack_v.end(), type);
                    upack(pm, i);
                    REQUIRE(upack_v == copy);
                    REQUIRE(pm.type() == type);

                    // Do the same with input range.
                    pm = pm_t(upack_v, type);
                    upack(pm, i);
                    REQUIRE(upack_v == copy);
                    REQUIRE(pm.type() == type);
                }
            }
        });
    });
}

TEST_CASE("s11n_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            auto test_s11n = [](const pm_t &t) {
                std::stringstream ss;

                {
                    boost::archive::binary_oarchive oa(ss);
                    oa << t;
                }

                pm_t out;

                {
                    boost::archive::binary_iarchive ia(ss);
                    ia >> out;
                }

                REQUIRE(t == out);
                REQUIRE(!(t != out));
            };

            pm_t t;
            test_s11n(t);

            t._type() = false;
            test_s11n(t);

            t = pm_t{1, -2, 3};
            test_s11n(t);

            t._type() = false;
            test_s11n(t);

            t = pm_t{0, 0, 1, -2, -3, 2};
            test_s11n(t);

            t._type() = false;
            test_s11n(t);
        });
    });
}

TEST_CASE("comparison")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            REQUIRE(pm_t{} == pm_t{});
            REQUIRE(!(pm_t{} != pm_t{}));

            REQUIRE(pm_t{1, 2, -3} == pm_t{1, 2, -3});
            REQUIRE(pm_t{1, 2, -3} != pm_t{1, -2, -3});
            REQUIRE(pm_t{0, 2, -3} == pm_t{0, 2, -3});
            REQUIRE(pm_t{0, 2, -3} != pm_t{1, -2, -3});

            // Checks on the type.
            pm_t t0, t1;
            t1._type() = false;
            REQUIRE(t0 != t1);
            REQUIRE(t1 != t0);

            t0 = pm_t{1, -2, 3, 0};
            t1 = t0;
            t1._type() = false;
            REQUIRE(t0 != t1);
            REQUIRE(t1 != t0);
        });
    });
}

TEST_CASE("is zero one")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            REQUIRE(obake::key_is_one(pm_t{}, symbol_set{}));
            REQUIRE(!obake::key_is_zero(pm_t{}, symbol_set{}));

            pm_t t00;
            t00._type() = false;
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(obake::key_is_zero(t00, symbol_set{}));

            t00 = pm_t{0, 2, 3};
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(!obake::key_is_zero(t00, symbol_set{}));

            t00._type() = false;
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(!obake::key_is_zero(t00, symbol_set{}));

            t00 = pm_t{1, -2, 3};
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(!obake::key_is_zero(t00, symbol_set{}));

            t00._type() = false;
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(!obake::key_is_zero(t00, symbol_set{}));

            t00 = pm_t{0, 0, 0};
            REQUIRE(obake::key_is_one(t00, symbol_set{}));
            REQUIRE(!obake::key_is_zero(t00, symbol_set{}));

            t00._type() = false;
            REQUIRE(!obake::key_is_one(t00, symbol_set{}));
            REQUIRE(obake::key_is_zero(t00, symbol_set{}));
        });
    });
}

TEST_CASE("hash")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            REQUIRE(obake::hash(pm_t{}) == 1u);

            pm_t t00;
            t00._type() = false;
            REQUIRE(obake::hash(t00) == 0u);

            t00 = pm_t{1, -2, 3, 0, 1};
            std::cout << "Sample hash for cos: " << obake::hash(t00) << '\n';
            t00._type() = false;
            std::cout << "Sample hash for sin: " << obake::hash(t00) << '\n';
        });
    });
}