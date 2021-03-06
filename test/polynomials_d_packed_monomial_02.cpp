// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <mp++/integer.hpp>
#include <mp++/rational.hpp>

#include <obake/byte_size.hpp>
#include <obake/config.hpp>
#include <obake/detail/to_string.hpp>
#include <obake/detail/tuple_for_each.hpp>
#include <obake/key/key_evaluate.hpp>
#include <obake/key/key_trim.hpp>
#include <obake/key/key_trim_identify.hpp>
#include <obake/polynomials/d_packed_monomial.hpp>
#include <obake/polynomials/monomial_diff.hpp>
#include <obake/polynomials/monomial_integrate.hpp>
#include <obake/polynomials/monomial_subs.hpp>
#include <obake/s11n.hpp>
#include <obake/symbols.hpp>
#include <obake/type_traits.hpp>

#include "catch.hpp"
#include "test_utils.hpp"

using namespace obake;

using int_types = std::tuple<std::int32_t, std::uint32_t
#if defined(OBAKE_PACKABLE_INT64)
                             ,
                             std::int64_t, std::uint64_t
#endif
                             >;

// The packed sizes over which we will be testing for type T.
template <typename T>
using psizes
    = std::tuple<std::integral_constant<unsigned, polynomials::dpm_default_psize>, std::integral_constant<unsigned, 1>,
                 std::integral_constant<unsigned, 2>, std::integral_constant<unsigned, 3>,
                 std::integral_constant<unsigned, detail::kpack_max_size<T>()>>;

TEST_CASE("byte_size_test")
{
    obake_test::disable_slow_stack_traces();

    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(is_size_measurable_v<pm_t>);
            REQUIRE(is_size_measurable_v<pm_t &>);
            REQUIRE(is_size_measurable_v<const pm_t &>);

            // Not much to test at this point, as
            // boost::small_vector does not expose enough
            // info for an accurate assessment.
            REQUIRE(byte_size(pm_t{}) >= sizeof(pm_t));
            REQUIRE(byte_size(pm_t{1, 0, 1}) >= sizeof(pm_t));
        });
    });
}

TEST_CASE("key_evaluate_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(is_evaluable_key_v<pm_t, double>);
            REQUIRE(is_evaluable_key_v<pm_t &, double>);
            REQUIRE(is_evaluable_key_v<const pm_t &, double>);
            REQUIRE(is_evaluable_key_v<const pm_t, double>);
            REQUIRE(!is_evaluable_key_v<const pm_t, double &>);
            REQUIRE(!is_evaluable_key_v<const pm_t, void>);
            REQUIRE(std::is_same_v<double, decltype(key_evaluate(pm_t{}, symbol_idx_map<double>{}, symbol_set{}))>);

            if constexpr (bw <= 3u) {
                REQUIRE(key_evaluate(pm_t{}, symbol_idx_map<double>{}, symbol_set{}) == 1.);
                REQUIRE(key_evaluate(pm_t{2}, symbol_idx_map<double>{{0, 3.5}}, symbol_set{"x"}) == std::pow(3.5, 2.));
                REQUIRE(key_evaluate(pm_t{2, 3}, symbol_idx_map<double>{{0, 3.5}, {1, -4.6}}, symbol_set{"x", "y"})
                        == std::pow(3.5, 2.) * std::pow(-4.6, 3));

                if constexpr (is_signed_v<int_t>) {
                    REQUIRE(key_evaluate(pm_t{-2, 3}, symbol_idx_map<double>{{0, 3.5}, {1, -4.6}}, symbol_set{"x", "y"})
                            == std::pow(3.5, -2.) * std::pow(-4.6, 3));
                }

                REQUIRE(!is_evaluable_key_v<pm_t, int>);

                REQUIRE(is_evaluable_key_v<pm_t, mppp::integer<1>>);
                REQUIRE(
                    std::is_same_v<mppp::integer<1>,
                                   decltype(key_evaluate(pm_t{}, symbol_idx_map<mppp::integer<1>>{}, symbol_set{}))>);
                REQUIRE(key_evaluate(pm_t{}, symbol_idx_map<mppp::integer<1>>{}, symbol_set{}) == 1);
                REQUIRE(
                    key_evaluate(pm_t{2}, symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}}, symbol_set{"x"})
                    == mppp::pow(mppp::integer<1>{3}, 2));
                REQUIRE(
                    key_evaluate(pm_t{2, 3},
                                 symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}, {1, mppp::integer<1>{4}}},
                                 symbol_set{"x", "y"})
                    == 576);

                if constexpr (is_signed_v<int_t>) {
                    REQUIRE(key_evaluate(
                                pm_t{-2, 3},
                                symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}, {1, mppp::integer<1>{4}}},
                                symbol_set{"x", "y"})
                            == 0);
                }

                REQUIRE(is_evaluable_key_v<pm_t, mppp::rational<1>>);
                REQUIRE(
                    std::is_same_v<mppp::rational<1>,
                                   decltype(key_evaluate(pm_t{}, symbol_idx_map<mppp::rational<1>>{}, symbol_set{}))>);
            }
        });
    });
}

TEST_CASE("monomial_subs_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(!is_substitutable_monomial_v<pm_t, void>);
            REQUIRE(!is_substitutable_monomial_v<pm_t, int>);
            REQUIRE(is_substitutable_monomial_v<pm_t, mppp::integer<1>>);
            REQUIRE(is_substitutable_monomial_v<pm_t &, mppp::integer<1>>);
            REQUIRE(is_substitutable_monomial_v<const pm_t &, mppp::integer<1>>);
            REQUIRE(is_substitutable_monomial_v<const pm_t, mppp::integer<1>>);
            REQUIRE(!is_substitutable_monomial_v<const pm_t, mppp::integer<1> &>);

            REQUIRE(std::is_same_v<std::pair<mppp::integer<1>, pm_t>,
                                   decltype(monomial_subs(pm_t{}, symbol_idx_map<mppp::integer<1>>{}, symbol_set{}))>);
            REQUIRE(std::is_same_v<std::pair<double, pm_t>,
                                   decltype(monomial_subs(pm_t{}, symbol_idx_map<double>{}, symbol_set{}))>);

            if constexpr (bw <= 3u) {
                REQUIRE(monomial_subs(pm_t{}, symbol_idx_map<mppp::integer<1>>{}, symbol_set{})
                        == std::make_pair(mppp::integer<1>{1}, pm_t{}));
                REQUIRE(monomial_subs(pm_t{1, 2, 3}, symbol_idx_map<mppp::integer<1>>{}, symbol_set{"x", "y", "z"})
                        == std::make_pair(mppp::integer<1>{1}, pm_t{1, 2, 3}));
                REQUIRE(monomial_subs(pm_t{1, 2, 3}, symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}},
                                      symbol_set{"x", "y", "z"})
                        == std::make_pair(mppp::integer<1>{3}, pm_t{0, 2, 3}));
                REQUIRE(monomial_subs(pm_t{1, 2, 3}, symbol_idx_map<mppp::integer<1>>{{1, mppp::integer<1>{3}}},
                                      symbol_set{"x", "y", "z"})
                        == std::make_pair(mppp::integer<1>{9}, pm_t{1, 0, 3}));
                REQUIRE(monomial_subs(pm_t{1, 2, 3}, symbol_idx_map<mppp::integer<1>>{{2, mppp::integer<1>{3}}},
                                      symbol_set{"x", "y", "z"})
                        == std::make_pair(mppp::integer<1>{27}, pm_t{1, 2, 0}));
                REQUIRE(
                    monomial_subs(pm_t{1, 2, 3},
                                  symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}, {1, mppp::integer<1>{-2}}},
                                  symbol_set{"x", "y", "z"})
                    == std::make_pair(mppp::integer<1>{12}, pm_t{0, 0, 3}));
                REQUIRE(
                    monomial_subs(pm_t{1, 2, 3},
                                  symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{3}}, {2, mppp::integer<1>{-2}}},
                                  symbol_set{"x", "y", "z"})
                    == std::make_pair(mppp::integer<1>{-24}, pm_t{0, 2, 0}));
                REQUIRE(
                    monomial_subs(pm_t{1, 2, 3},
                                  symbol_idx_map<mppp::integer<1>>{{1, mppp::integer<1>{3}}, {2, mppp::integer<1>{-2}}},
                                  symbol_set{"x", "y", "z"})
                    == std::make_pair(mppp::integer<1>{-72}, pm_t{1, 0, 0}));
                REQUIRE(monomial_subs(pm_t{1, 2, 3},
                                      symbol_idx_map<mppp::integer<1>>{{0, mppp::integer<1>{-3}},
                                                                       {1, mppp::integer<1>{4}},
                                                                       {2, mppp::integer<1>{-5}}},
                                      symbol_set{"x", "y", "z"})
                        == std::make_pair(mppp::integer<1>{6000}, pm_t{0, 0, 0}));

                if constexpr (is_signed_v<int_t>) {
                    REQUIRE(
                        monomial_subs(pm_t{-2, 3}, symbol_idx_map<double>{{0, 3.5}, {1, -4.6}}, symbol_set{"x", "y"})
                        == std::make_pair(std::pow(3.5, -2.) * std::pow(-4.6, 3), pm_t{0, 0}));
                    REQUIRE(monomial_subs(pm_t{-2, 3}, symbol_idx_map<double>{{0, 3.5}}, symbol_set{"x", "y"})
                            == std::make_pair(std::pow(3.5, -2.), pm_t{0, 3}));
                }
            }
        });
    });
}

TEST_CASE("key_trim_identify_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(is_trim_identifiable_key_v<pm_t>);
            REQUIRE(is_trim_identifiable_key_v<pm_t &>);
            REQUIRE(is_trim_identifiable_key_v<const pm_t &>);
            REQUIRE(is_trim_identifiable_key_v<const pm_t>);

            if constexpr (bw <= 3u) {
                std::vector<int> v;
                key_trim_identify(v, pm_t{}, symbol_set{});
                REQUIRE(v.empty());

                v.resize(3, 1);
                key_trim_identify(v, pm_t{1, 2, 3}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{0, 0, 0});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{0, 2, 3}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{1, 0, 0});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{1, 0, 3}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{0, 1, 0});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{1, 2, 0}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{0, 0, 1});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{0, 2, 0}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{1, 0, 1});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{0, 0, 3}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{1, 1, 0});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{1, 0, 0}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{0, 1, 1});
                v.clear();

                v.resize(3, 1);
                key_trim_identify(v, pm_t{0, 0, 0}, symbol_set{"x", "y", "z"});
                REQUIRE(v == std::vector<int>{1, 1, 1});
                v.clear();
            }
        });
    });
}

TEST_CASE("key_trim_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(is_trimmable_key_v<pm_t>);
            REQUIRE(is_trimmable_key_v<pm_t &>);
            REQUIRE(is_trimmable_key_v<const pm_t &>);
            REQUIRE(is_trimmable_key_v<const pm_t>);

            if constexpr (bw <= 3u) {
                REQUIRE(key_trim(pm_t{}, symbol_idx_set{}, symbol_set{}) == pm_t{});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{}, symbol_set{"x", "y", "z"}) == pm_t{1, 2, 3});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{0}, symbol_set{"x", "y", "z"}) == pm_t{2, 3});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{1}, symbol_set{"x", "y", "z"}) == pm_t{1, 3});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{2}, symbol_set{"x", "y", "z"}) == pm_t{1, 2});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{0, 1}, symbol_set{"x", "y", "z"}) == pm_t{3});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{0, 2}, symbol_set{"x", "y", "z"}) == pm_t{2});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{1, 2}, symbol_set{"x", "y", "z"}) == pm_t{1});
                REQUIRE(key_trim(pm_t{1, 2, 3}, symbol_idx_set{0, 1, 2}, symbol_set{"x", "y", "z"}) == pm_t{});
            }
        });
    });
}

TEST_CASE("monomial_diff_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(is_differentiable_monomial_v<pm_t>);
            REQUIRE(is_differentiable_monomial_v<pm_t &>);
            REQUIRE(is_differentiable_monomial_v<const pm_t &>);
            REQUIRE(is_differentiable_monomial_v<const pm_t>);
            REQUIRE(std::is_same_v<decltype(monomial_diff(pm_t{}, 0, symbol_set{})), std::pair<int_t, pm_t>>);

            if constexpr (bw <= 3u) {
                REQUIRE(monomial_diff(pm_t{0}, 0, symbol_set{"x"}) == std::make_pair(int_t(0), pm_t{0}));
                REQUIRE(monomial_diff(pm_t{1}, 0, symbol_set{"x"}) == std::make_pair(int_t(1), pm_t{0}));
                REQUIRE(monomial_diff(pm_t{2}, 0, symbol_set{"x"}) == std::make_pair(int_t(2), pm_t{1}));
                REQUIRE(monomial_diff(pm_t{3}, 0, symbol_set{"x"}) == std::make_pair(int_t(3), pm_t{2}));

                REQUIRE(monomial_diff(pm_t{0, 0}, 0, symbol_set{"x", "y"}) == std::make_pair(int_t(0), pm_t{0, 0}));
                REQUIRE(monomial_diff(pm_t{0, 1}, 0, symbol_set{"x", "y"}) == std::make_pair(int_t(0), pm_t{0, 1}));
                REQUIRE(monomial_diff(pm_t{0, 0}, 1, symbol_set{"x", "y"}) == std::make_pair(int_t(0), pm_t{0, 0}));
                REQUIRE(monomial_diff(pm_t{1, 0}, 1, symbol_set{"x", "y"}) == std::make_pair(int_t(0), pm_t{1, 0}));
                REQUIRE(monomial_diff(pm_t{2, 1}, 0, symbol_set{"x", "y"}) == std::make_pair(int_t(2), pm_t{1, 1}));
                REQUIRE(monomial_diff(pm_t{3, 1}, 0, symbol_set{"x", "y"}) == std::make_pair(int_t(3), pm_t{2, 1}));
                REQUIRE(monomial_diff(pm_t{3, 2}, 1, symbol_set{"x", "y"}) == std::make_pair(int_t(2), pm_t{3, 1}));
                REQUIRE(monomial_diff(pm_t{3, 3}, 1, symbol_set{"x", "y"}) == std::make_pair(int_t(3), pm_t{3, 2}));

                REQUIRE(monomial_diff(pm_t{1, 2, 3}, 0, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(1), pm_t{0, 2, 3}));
                REQUIRE(monomial_diff(pm_t{1, 2, 3}, 1, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(2), pm_t{1, 1, 3}));
                REQUIRE(monomial_diff(pm_t{1, 2, 3}, 2, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(3), pm_t{1, 2, 2}));

                if constexpr (is_signed_v<int_t>) {
                    REQUIRE(monomial_diff(pm_t{-1}, 0, symbol_set{"x"}) == std::make_pair(int_t(-1), pm_t{-2}));
                    REQUIRE(monomial_diff(pm_t{-2}, 0, symbol_set{"x"}) == std::make_pair(int_t(-2), pm_t{-3}));
                    REQUIRE(monomial_diff(pm_t{-3}, 0, symbol_set{"x"}) == std::make_pair(int_t(-3), pm_t{-4}));

                    REQUIRE(monomial_diff(pm_t{-2, -1}, 0, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-2), pm_t{-3, -1}));
                    REQUIRE(monomial_diff(pm_t{-3, -1}, 0, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-3), pm_t{-4, -1}));
                    REQUIRE(monomial_diff(pm_t{-3, -2}, 1, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-2), pm_t{-3, -3}));
                    REQUIRE(monomial_diff(pm_t{-3, -3}, 1, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-3), pm_t{-3, -4}));
                }
            }
        });
    });
}

TEST_CASE("monomial_integrate_test")
{
    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_monomial<int_t, bw>;

            if constexpr (bw <= 3u) {
                REQUIRE(is_integrable_monomial_v<pm_t>);
                REQUIRE(is_integrable_monomial_v<pm_t &>);
                REQUIRE(is_integrable_monomial_v<const pm_t &>);
                REQUIRE(is_integrable_monomial_v<const pm_t>);
                REQUIRE(std::is_same_v<decltype(monomial_integrate(pm_t{}, 0, symbol_set{})), std::pair<int_t, pm_t>>);

                REQUIRE(monomial_integrate(pm_t{0}, 0, symbol_set{"x"}) == std::make_pair(int_t(1), pm_t{1}));
                REQUIRE(monomial_integrate(pm_t{1}, 0, symbol_set{"x"}) == std::make_pair(int_t(2), pm_t{2}));
                REQUIRE(monomial_integrate(pm_t{2}, 0, symbol_set{"x"}) == std::make_pair(int_t(3), pm_t{3}));
                REQUIRE(monomial_integrate(pm_t{3}, 0, symbol_set{"x"}) == std::make_pair(int_t(4), pm_t{4}));

                REQUIRE(monomial_integrate(pm_t{0, 0}, 0, symbol_set{"x", "y"})
                        == std::make_pair(int_t(1), pm_t{1, 0}));
                REQUIRE(monomial_integrate(pm_t{0, 1}, 0, symbol_set{"x", "y"})
                        == std::make_pair(int_t(1), pm_t{1, 1}));
                REQUIRE(monomial_integrate(pm_t{0, 0}, 1, symbol_set{"x", "y"})
                        == std::make_pair(int_t(1), pm_t{0, 1}));
                REQUIRE(monomial_integrate(pm_t{1, 0}, 1, symbol_set{"x", "y"})
                        == std::make_pair(int_t(1), pm_t{1, 1}));
                REQUIRE(monomial_integrate(pm_t{2, 1}, 0, symbol_set{"x", "y"})
                        == std::make_pair(int_t(3), pm_t{3, 1}));
                REQUIRE(monomial_integrate(pm_t{3, 1}, 0, symbol_set{"x", "y"})
                        == std::make_pair(int_t(4), pm_t{4, 1}));
                REQUIRE(monomial_integrate(pm_t{3, 2}, 1, symbol_set{"x", "y"})
                        == std::make_pair(int_t(3), pm_t{3, 3}));
                REQUIRE(monomial_integrate(pm_t{3, 3}, 1, symbol_set{"x", "y"})
                        == std::make_pair(int_t(4), pm_t{3, 4}));

                REQUIRE(monomial_integrate(pm_t{1, 2, 3}, 0, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(2), pm_t{2, 2, 3}));
                REQUIRE(monomial_integrate(pm_t{1, 2, 3}, 1, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(3), pm_t{1, 3, 3}));
                REQUIRE(monomial_integrate(pm_t{1, 2, 3}, 2, symbol_set{"x", "y", "z"})
                        == std::make_pair(int_t(4), pm_t{1, 2, 4}));

                if constexpr (is_signed_v<int_t>) {
                    REQUIRE(monomial_integrate(pm_t{-2}, 0, symbol_set{"x"}) == std::make_pair(int_t(-1), pm_t{-1}));
                    REQUIRE(monomial_integrate(pm_t{-3}, 0, symbol_set{"x"}) == std::make_pair(int_t(-2), pm_t{-2}));

                    REQUIRE(monomial_integrate(pm_t{-2, -1}, 0, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-1), pm_t{-1, -1}));
                    REQUIRE(monomial_integrate(pm_t{-3, -1}, 0, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-2), pm_t{-2, -1}));
                    REQUIRE(monomial_integrate(pm_t{-3, -3}, 1, symbol_set{"x", "y"})
                            == std::make_pair(int_t(-2), pm_t{-3, -2}));

                    OBAKE_REQUIRES_THROWS_CONTAINS(monomial_integrate(pm_t{-1}, 0, symbol_set{"x"}), std::domain_error,
                                                   "Cannot integrate a dynamic packed monomial: the exponent of the "
                                                   "integration variable ('x') is -1, and the "
                                                   "integration would generate a logarithmic term");
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
            using pm_t = d_packed_monomial<int_t, bw>;

            REQUIRE(boost::serialization::tracking_level<pm_t>::value == boost::serialization::track_never);

            if constexpr (bw <= 3u) {
                std::stringstream ss;
                pm_t tmp;

                {
                    boost::archive::binary_oarchive oarchive(ss);
                    oarchive << pm_t{1, 2, 3};
                }
                {
                    boost::archive::binary_iarchive iarchive(ss);
                    iarchive >> tmp;
                }
                REQUIRE(tmp == pm_t{1, 2, 3});
                ss.str("");

                {
                    boost::archive::binary_oarchive oarchive(ss);
                    oarchive << pm_t{};
                }
                {
                    boost::archive::binary_iarchive iarchive(ss);
                    iarchive >> tmp;
                }
                REQUIRE(tmp == pm_t{});
                ss.str("");

                if constexpr (is_signed_v<int_t>) {
                    {
                        boost::archive::binary_oarchive oarchive(ss);
                        oarchive << pm_t{-1, 2, -3};
                    }
                    {
                        boost::archive::binary_iarchive iarchive(ss);
                        iarchive >> tmp;
                    }
                    REQUIRE(tmp == pm_t{-1, 2, -3});
                    ss.str("");
                }
            }
        });
    });
}
