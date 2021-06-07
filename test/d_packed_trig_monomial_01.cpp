// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdint>
#include <initializer_list>
#include <tuple>
#include <type_traits>

#include <obake/config.hpp>
#include <obake/detail/tuple_for_each.hpp>
#include <obake/key/key_merge_symbols.hpp>
#include <obake/kpack.hpp>
#include <obake/poisson_series/d_packed_trig_monomial.hpp>
#include <obake/symbols.hpp>
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

TEST_CASE("key_merge_symbols_test")
{
    obake_test::disable_slow_stack_traces();

    detail::tuple_for_each(int_types{}, [](const auto &n) {
        using int_t = remove_cvref_t<decltype(n)>;

        detail::tuple_for_each(psizes<int_t>{}, [](auto b) {
            constexpr auto bw = decltype(b)::value;
            using pm_t = d_packed_trig_monomial<int_t, bw>;

            REQUIRE(is_symbols_mergeable_key_v<pm_t>);
            REQUIRE(is_symbols_mergeable_key_v<pm_t &>);
            REQUIRE(is_symbols_mergeable_key_v<pm_t &&>);
            REQUIRE(is_symbols_mergeable_key_v<const pm_t &>);

            if constexpr (bw <= 3u) {
                REQUIRE(key_merge_symbols(pm_t{}, symbol_idx_map<symbol_set>{}, symbol_set{}) == pm_t{});
                REQUIRE(key_merge_symbols(pm_t({}, false), symbol_idx_map<symbol_set>{}, symbol_set{})
                        == pm_t({}, false));

                REQUIRE(key_merge_symbols(pm_t{}, symbol_idx_map<symbol_set>{{0, {"x"}}}, symbol_set{}) == pm_t{0});
                REQUIRE(key_merge_symbols(pm_t({}, false), symbol_idx_map<symbol_set>{{0, {"x"}}}, symbol_set{})
                        == pm_t({0}, false));

                REQUIRE(key_merge_symbols(pm_t{1}, symbol_idx_map<symbol_set>{}, symbol_set{"x"}) == pm_t{1});
                REQUIRE(key_merge_symbols(pm_t({1}, false), symbol_idx_map<symbol_set>{}, symbol_set{"x"})
                        == pm_t({1}, false));

                REQUIRE(key_merge_symbols(pm_t{1}, symbol_idx_map<symbol_set>{{0, {"y"}}}, symbol_set{"x"})
                        == pm_t{0, 1});
                REQUIRE(key_merge_symbols(pm_t({1}, false), symbol_idx_map<symbol_set>{{0, {"y"}}}, symbol_set{"x"})
                        == pm_t({0, 1}, false));

                REQUIRE(key_merge_symbols(pm_t{1}, symbol_idx_map<symbol_set>{{1, {"y"}}}, symbol_set{"x"})
                        == pm_t{1, 0});
                REQUIRE(key_merge_symbols(pm_t({1}, false), symbol_idx_map<symbol_set>{{1, {"y"}}}, symbol_set{"x"})
                        == pm_t({1, 0}, false));

                REQUIRE(key_merge_symbols(pm_t{1, 2, 3},
                                          symbol_idx_map<symbol_set>{{0, {"a", "b"}}, {1, {"c"}}, {3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{0, 0, 1, 0, 2, 3, 0, 0});
                REQUIRE(key_merge_symbols(pm_t({1, 2, 3}, false),
                                          symbol_idx_map<symbol_set>{{0, {"a", "b"}}, {1, {"c"}}, {3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({0, 0, 1, 0, 2, 3, 0, 0}, false));

                REQUIRE(key_merge_symbols(pm_t{1, 2, 3}, symbol_idx_map<symbol_set>{{3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{1, 2, 3, 0, 0});
                REQUIRE(key_merge_symbols(pm_t({1, 2, 3}, false), symbol_idx_map<symbol_set>{{3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({1, 2, 3, 0, 0}, false));

                REQUIRE(key_merge_symbols(pm_t{1, 2, 3}, symbol_idx_map<symbol_set>{{0, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{0, 0, 1, 2, 3});
                REQUIRE(key_merge_symbols(pm_t({1, 2, 3}, false), symbol_idx_map<symbol_set>{{0, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({0, 0, 1, 2, 3}, false));

                REQUIRE(key_merge_symbols(pm_t{1, 2, 3}, symbol_idx_map<symbol_set>{{1, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{1, 0, 0, 2, 3});
                REQUIRE(key_merge_symbols(pm_t({1, 2, 3}, false), symbol_idx_map<symbol_set>{{1, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({1, 0, 0, 2, 3}, false));

                REQUIRE(key_merge_symbols(pm_t{-1, -2, 3},
                                          symbol_idx_map<symbol_set>{{0, {"a", "b"}}, {1, {"c"}}, {3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{0, 0, -1, 0, -2, 3, 0, 0});
                REQUIRE(key_merge_symbols(pm_t({-1, -2, 3}, false),
                                          symbol_idx_map<symbol_set>{{0, {"a", "b"}}, {1, {"c"}}, {3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({0, 0, -1, 0, -2, 3, 0, 0}, false));

                REQUIRE(key_merge_symbols(pm_t{-1, -2, 3}, symbol_idx_map<symbol_set>{{3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{-1, -2, 3, 0, 0});
                REQUIRE(key_merge_symbols(pm_t({-1, -2, 3}, false), symbol_idx_map<symbol_set>{{3, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({-1, -2, 3, 0, 0}, false));

                REQUIRE(key_merge_symbols(pm_t{-1, -2, 3}, symbol_idx_map<symbol_set>{{0, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{0, 0, -1, -2, 3});
                REQUIRE(key_merge_symbols(pm_t({-1, -2, 3}, false), symbol_idx_map<symbol_set>{{0, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({0, 0, -1, -2, 3}, false));

                REQUIRE(key_merge_symbols(pm_t{-1, -2, 3}, symbol_idx_map<symbol_set>{{1, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t{-1, 0, 0, -2, 3});
                REQUIRE(key_merge_symbols(pm_t({-1, -2, 3}, false), symbol_idx_map<symbol_set>{{1, {"d", "e"}}},
                                          symbol_set{"x", "y", "z"})
                        == pm_t({-1, 0, 0, -2, 3}, false));
            }
        });
    });
}
