// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OBAKE_MATH_SUBS_HPP
#define OBAKE_MATH_SUBS_HPP

#include <type_traits>
#include <utility>

#include <obake/detail/not_implemented.hpp>
#include <obake/detail/priority_tag.hpp>
#include <obake/detail/ss_func_forward.hpp>
#include <obake/symbols.hpp>
#include <obake/type_traits.hpp>

namespace obake
{

namespace customisation
{

// External customisation point for obake::subs().
struct subs_t {
};

} // namespace customisation

namespace detail
{

// Highest priority: explicit user override in the external customisation namespace.
template <typename T, typename U>
constexpr auto subs_impl(T &&x, const symbol_map<U> &sm, priority_tag<2>)
    OBAKE_SS_FORWARD_FUNCTION(subs(customisation::subs_t{}, ::std::forward<T>(x), sm));

// Unqualified function call implementation.
template <typename T, typename U>
constexpr auto subs_impl(T &&x, const symbol_map<U> &sm, priority_tag<1>)
    OBAKE_SS_FORWARD_FUNCTION(subs(::std::forward<T>(x), sm));

// Lowest-priority: forward-construct the input value.
template <typename T, typename U>
constexpr auto subs_impl(T &&x, const symbol_map<U> &, priority_tag<0>)
    OBAKE_SS_FORWARD_FUNCTION(remove_cvref_t<T>(::std::forward<T>(x)));

} // namespace detail

// NOTE: in principle we could use here the template lambda syntax
// + concept check instead of this helper and enable_if. However
// this does not work in GCC 9 due to a compiler issue, thus we
// keep for now this legacy implementation.
struct subs_functor {
    // NOTE: place an enabling condition here on U, otherwise
    // if we invoke the type-trait/concept with pathological types
    // for U (e.g., void) then the attempted instantiation of
    // symbol_map<void> in the impl functions above will trigger
    // a hard error. Also, it's easier to just require a semi
    // regular type here for meta-programming purposes.
    template <typename T, typename U, ::std::enable_if_t<is_semi_regular_v<U>, int> = 0>
    constexpr auto operator()(T &&x, const symbol_map<U> &sm) const
        OBAKE_SS_FORWARD_MEMBER_FUNCTION(detail::subs_impl(::std::forward<T>(x), sm, detail::priority_tag<2>{}))
};

inline constexpr auto subs = subs_functor{};

namespace detail
{

template <typename T, typename U>
using subs_t = decltype(::obake::subs(::std::declval<T>(), ::std::declval<const symbol_map<U> &>()));

}

template <typename T, typename U>
using is_substitutable = is_detected<detail::subs_t, T, U>;

template <typename T, typename U>
inline constexpr bool is_substitutable_v = is_substitutable<T, U>::value;

template <typename T, typename U>
concept Substitutable = requires(T &&x, const symbol_map<U> &sm)
{
    ::obake::subs(::std::forward<T>(x), sm);
};

} // namespace obake

#endif
