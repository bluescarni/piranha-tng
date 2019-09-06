// Copyright 2019 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the piranha library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef PIRANHA_BYTE_SIZE_HPP
#define PIRANHA_BYTE_SIZE_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

#include <mp++/config.hpp>
#include <mp++/integer.hpp>
#include <mp++/rational.hpp>

#if defined(MPPP_WITH_MPFR)

#include <mp++/real.hpp>

#endif

#include <piranha/config.hpp>
#include <piranha/detail/not_implemented.hpp>
#include <piranha/detail/priority_tag.hpp>
#include <piranha/detail/ss_func_forward.hpp>
#include <piranha/type_traits.hpp>

namespace piranha
{

namespace customisation
{

// External customisation point for piranha::byte_size().
template <typename T
#if !defined(PIRANHA_HAVE_CONCEPTS)
          ,
          typename = void
#endif
          >
inline constexpr auto byte_size = not_implemented;

namespace internal
{

// Internal customisation point for piranha::byte_size().
template <typename T
#if !defined(PIRANHA_HAVE_CONCEPTS)
          ,
          typename = void
#endif
          >
inline constexpr auto byte_size = not_implemented;

} // namespace internal

} // namespace customisation

namespace detail
{

template <::std::size_t SSize>
inline ::std::size_t byte_size(const ::mppp::integer<SSize> &n)
{
    const auto s = n.size();

    auto retval = sizeof(n);
    if (s > SSize) {
        retval += s * sizeof(::mp_limb_t);
    }
    return retval;
}

template <::std::size_t SSize>
inline ::std::size_t byte_size(const ::mppp::rational<SSize> &q)
{
    return detail::byte_size(q.get_num()) + detail::byte_size(q.get_den());
}

#if defined(MPPP_WITH_MPFR)

inline ::std::size_t byte_size(const ::mppp::real &r)
{
    return sizeof(r) + mpfr_custom_get_size(r.get_prec());
}

#endif

// Highest priority: explicit user override in the external customisation namespace.
template <typename T>
constexpr auto byte_size_impl(T &&x, priority_tag<3>)
    PIRANHA_SS_FORWARD_FUNCTION((customisation::byte_size<T &&>)(::std::forward<T>(x)));

// Unqualified function call implementation.
template <typename T>
constexpr auto byte_size_impl(T &&x, priority_tag<2>) PIRANHA_SS_FORWARD_FUNCTION(byte_size(::std::forward<T>(x)));

// Explicit override in the internal customisation namespace.
template <typename T>
constexpr auto byte_size_impl(T &&x, priority_tag<1>)
    PIRANHA_SS_FORWARD_FUNCTION((customisation::internal::byte_size<T &&>)(::std::forward<T>(x)));

// Lowest priority: use sizeof().
template <typename T>
constexpr auto byte_size_impl(T &&, priority_tag<0>) PIRANHA_SS_FORWARD_FUNCTION(sizeof(remove_cvref_t<T>));

// Machinery to enable the byte_size implementation only if the return
// type is std::size_t.
template <typename T>
using byte_size_impl_ret_t = decltype(detail::byte_size_impl(::std::declval<T>(), priority_tag<3>{}));

template <typename T, ::std::enable_if_t<::std::is_same_v<detected_t<byte_size_impl_ret_t, T>, ::std::size_t>, int> = 0>
constexpr auto byte_size_impl_with_ret_check(T &&x)
    PIRANHA_SS_FORWARD_FUNCTION(detail::byte_size_impl(::std::forward<T>(x), priority_tag<3>{}));

} // namespace detail

#if defined(_MSC_VER)

struct byte_size_msvc {
    template <typename T>
    constexpr auto operator()(T &&x) const
        PIRANHA_SS_FORWARD_MEMBER_FUNCTION(detail::byte_size_impl_with_ret_check(::std::forward<T>(x)))
};

inline constexpr auto byte_size = byte_size_msvc{};

#else

inline constexpr auto byte_size =
    [](auto &&x) PIRANHA_SS_FORWARD_LAMBDA(detail::byte_size_impl_with_ret_check(::std::forward<decltype(x)>(x)));

#endif

namespace detail
{

template <typename T>
using byte_size_t = decltype(::piranha::byte_size(::std::declval<T>()));

}

template <typename T>
using is_size_measurable = is_detected<detail::byte_size_t, T>;

template <typename T>
inline constexpr bool is_size_measurable_v = is_size_measurable<T>::value;

#if defined(PIRANHA_HAVE_CONCEPTS)

template <typename T>
PIRANHA_CONCEPT_DECL SizeMeasurable = requires(T &&x)
{
    ::piranha::byte_size(::std::forward<T>(x));
};

#endif

} // namespace piranha

#endif
