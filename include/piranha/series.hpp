// Copyright 2019 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the piranha library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef PIRANHA_SERIES_HPP
#define PIRANHA_SERIES_HPP

#include <type_traits>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include <piranha/config.hpp>
#include <piranha/math/pow.hpp>
#include <piranha/type_traits.hpp>

namespace piranha
{

template <typename Cf, typename Key, typename Tag>
class series
{
private:
    using container_t = ::std::vector<::absl::flat_hash_map<Key, Cf>>;

    container_t m_container;
};

namespace detail
{

template <typename T>
struct is_series_impl : ::std::false_type {
};

template <typename Cf, typename Key, typename Tag>
struct is_series_impl<series<Cf, Key, Tag>> : ::std::true_type {
};

} // namespace detail

template <typename T>
using is_cvr_series = detail::is_series_impl<remove_cvref_t<T>>;

template <typename T>
inline constexpr bool is_cvr_series_v = is_cvr_series<T>::value;

#if defined(PIRANHA_HAVE_CONCEPTS)

template <typename T>
PIRANHA_CONCEPT_DECL CvrSeries = is_cvr_series_v<T>;

#endif

namespace detail
{

template <typename>
struct series_cf_t_impl {
};

template <typename Cf, typename Key, typename Tag>
struct series_cf_t_impl<series<Cf, Key, Tag>> {
    using type = Cf;
};

} // namespace detail

template <typename T>
using series_cf_t = typename detail::series_cf_t_impl<::std::remove_cv_t<T>>::type;

namespace customisation::internal
{

template <typename T, typename U>
#if defined(PIRANHA_HAVE_CONCEPTS)
requires CvrSeries<T> &&Integral<::std::remove_reference_t<U>> inline constexpr auto pow<T, U>
#else
inline constexpr auto
    pow<T, U, ::std::enable_if_t<::std::conjunction_v<is_cvr_series<T>, is_integral<::std::remove_reference_t<U>>>>>
#endif
    = [](auto &&, auto &&) constexpr
{
    return 0;
};

} // namespace customisation::internal

} // namespace piranha

#endif
