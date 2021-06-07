// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)::polynomials
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OBAKE_POLYNOMIALS_D_PACKED_MONOMIAL_HPP
#define OBAKE_POLYNOMIALS_D_PACKED_MONOMIAL_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/container/container_fwd.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/split_member.hpp>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <tbb/blocked_range.h>
#include <tbb/parallel_invoke.h>
#include <tbb/parallel_reduce.h>

#include <mp++/integer.hpp>

#include <obake/config.hpp>
#include <obake/detail/ignore.hpp>
#include <obake/detail/limits.hpp>
#include <obake/detail/mppp_utils.hpp>
#include <obake/detail/safe_integral_arith.hpp>
#include <obake/detail/to_string.hpp>
#include <obake/detail/type_c.hpp>
#include <obake/detail/visibility.hpp>
#include <obake/exceptions.hpp>
#include <obake/key/key_is_compatible.hpp>
#include <obake/kpack.hpp>
#include <obake/math/pow.hpp>
#include <obake/math/safe_cast.hpp>
#include <obake/math/safe_convert.hpp>
#include <obake/polynomials/monomial_homomorphic_hash.hpp>
#include <obake/ranges.hpp>
#include <obake/s11n.hpp>
#include <obake/symbols.hpp>
#include <obake/type_traits.hpp>

namespace obake
{

namespace polynomials
{

namespace detail
{

// Small helper to determine the container size
// we need to store n exponents in a dynamic packed
// monomial of type T. U must be an
// unsigned integral type.
template <typename T>
inline constexpr auto dpm_n_expos_to_vsize = []<typename U>(const U &n) constexpr noexcept
{
    static_assert(is_integral_v<U> && !is_signed_v<U>);
    return n / T::psize + static_cast<U>(n % T::psize != 0u);
};

} // namespace detail

// Max psize for d_packed_monomial.
template <kpackable T>
inline constexpr unsigned dpm_max_psize = ::obake::detail::kpack_max_size<T>();

// Dynamic packed monomial.
template <kpackable T, unsigned PSize>
requires(PSize > 0u) && (PSize <= dpm_max_psize<T>)class d_packed_monomial
{
    friend class ::boost::serialization::access;

public:
    // Alias for PSize
    static constexpr unsigned psize = PSize;

    // Alias for T.
    using value_type = T;

    // The container type.
    using container_t = ::boost::container::small_vector<T, 1>;

    // Default constructor.
    d_packed_monomial() = default;

    // Constructor from symbol set.
    explicit d_packed_monomial(const symbol_set &ss)
        : m_container(::obake::safe_cast<typename container_t::size_type>(
            detail::dpm_n_expos_to_vsize<d_packed_monomial>(ss.size())))
    {
    }

    // Constructor from input iterator and size.
    template <typename It>
    requires InputIterator<It> && SafelyCastable<typename ::std::iterator_traits<It>::reference, T>
    explicit d_packed_monomial(It it, ::std::size_t n)
        // LCOV_EXCL_START
        : m_container(
            ::obake::safe_cast<typename container_t::size_type>(detail::dpm_n_expos_to_vsize<d_packed_monomial>(n)),
            // NOTE: avoid value-init of the elements, as we will
            // be setting all of them to some value in the loop below.
            ::boost::container::default_init_t{})
    // LCOV_EXCL_STOP
    {
        ::std::size_t counter = 0;
        for (auto &out : m_container) {
            kpacker<T> kp(psize);

            // Keep packing until we get to psize or we have
            // exhausted the input values.
            for (auto j = 0u; j < psize && counter < n; ++j, ++counter, ++it) {
                kp << ::obake::safe_cast<T>(*it);
            }

            out = kp.get();
        }
    }

private:
    struct input_it_ctor_tag {
    };
    // Implementation of the ctor from input iterators.
    // NOTE: a possible optimisation here is to detect
    // random-access iterators and delegate to the
    // ctor from input iterator and size.
    template <typename It>
    explicit d_packed_monomial(input_it_ctor_tag, It b, It e)
    {
        while (b != e) {
            kpacker<T> kp(psize);

            for (auto j = 0u; j < psize && b != e; ++j, ++b) {
                kp << ::obake::safe_cast<T>(*b);
            }

            m_container.push_back(kp.get());
        }
    }

public:
    // Ctor from a pair of input iterators.
    template <typename It>
    requires InputIterator<It> && SafelyCastable<typename ::std::iterator_traits<It>::reference, T>
    explicit d_packed_monomial(It b, It e) : d_packed_monomial(input_it_ctor_tag{}, b, e) {}

    // Ctor from input range.
    template <typename Range>
    requires InputRange<Range> && SafelyCastable<typename ::std::iterator_traits<range_begin_t<Range>>::reference, T>
    explicit d_packed_monomial(Range &&r)
        : d_packed_monomial(input_it_ctor_tag{}, ::obake::begin(::std::forward<Range>(r)),
                            ::obake::end(::std::forward<Range>(r)))
    {
    }

    // Ctor from init list.
    template <typename U>
    requires SafelyCastable<const U &, T>
    explicit d_packed_monomial(::std::initializer_list<U> l)
        : d_packed_monomial(input_it_ctor_tag{}, l.begin(), l.end())
    {
    }

    container_t &_container()
    {
        return m_container;
    }
    const container_t &_container() const
    {
        return m_container;
    }

private:
    // Serialisation.
    template <class Archive>
    void save(Archive &ar, unsigned) const
    {
        ar << m_container.size();

        for (const auto &n : m_container) {
            ar << n;
        }
    }
    template <class Archive>
    void load(Archive &ar, unsigned)
    {
        decltype(m_container.size()) size;
        ar >> size;
        m_container.resize(size);

        for (auto &n : m_container) {
            ar >> n;
        }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()

private:
    container_t m_container;
};

// Default PSize for d_packed_monomial.
inline constexpr unsigned dpm_default_psize =
#if defined(OBAKE_PACKABLE_INT64)
    8
#else
    4
#endif
    ;

// Default signed type for the exponents.
using dpm_default_s_t =
#if defined(OBAKE_PACKABLE_INT64)
    ::std::int64_t
#else
    ::std::int32_t
#endif
    ;

// Default unsigned type for the exponents.
using dpm_default_u_t =
#if defined(OBAKE_PACKABLE_INT64)
    ::std::uint64_t
#else
    ::std::uint32_t
#endif
    ;

// Implementation of key_is_zero(). A monomial is never zero.
template <typename T, unsigned PSize>
inline bool key_is_zero(const d_packed_monomial<T, PSize> &, const symbol_set &)
{
    return false;
}

// Implementation of key_is_one(). A monomial is one if all its exponents are zero.
template <typename T, unsigned PSize>
inline bool key_is_one(const d_packed_monomial<T, PSize> &d, const symbol_set &)
{
    return ::std::all_of(d._container().cbegin(), d._container().cend(), [](const T &n) { return n == T(0); });
}

// Comparisons.
template <typename T, unsigned PSize>
inline bool operator==(const d_packed_monomial<T, PSize> &d1, const d_packed_monomial<T, PSize> &d2)
{
    return d1._container() == d2._container();
}

template <typename T, unsigned PSize>
inline bool operator!=(const d_packed_monomial<T, PSize> &d1, const d_packed_monomial<T, PSize> &d2)
{
    return !(d1 == d2);
}

// Hash implementation.
template <typename T, unsigned PSize>
inline ::std::size_t hash(const d_packed_monomial<T, PSize> &d)
{
    // NOTE: the idea is that we will mix the individual
    // hashes for every pack of exponents via addition.
    ::std::size_t ret = 0;
    for (const auto &n : d._container()) {
        ret += static_cast<::std::size_t>(n);
    }
    return ret;
}

namespace detail
{

// Implementation of symbol set compatibility check.
// NOTE: factored out for re-use.
template <typename T, typename F>
inline bool dpm_key_is_compatible(const T &d, const symbol_set &s, const F &f, unsigned psize)
{
    const auto s_size = s.size();
    const auto &c = d._container();

    // Determine the size the container must have in order
    // to be able to represent s_size exponents.
    const auto exp_size = f(s_size);

    // Check if c has the expected size.
    if (c.size() != exp_size) {
        return false;
    }

    // We need to check if the encoded values in the container
    // are within the limits.
    const auto [klim_min, klim_max] = ::obake::detail::kpack_get_klims<typename T::value_type>(psize);
    for (const auto &n : c) {
        if (n < klim_min || n > klim_max) {
            return false;
        }
    }

    return true;
}

} // namespace detail

// Symbol set compatibility implementation.
template <typename T, unsigned PSize>
inline bool key_is_compatible(const d_packed_monomial<T, PSize> &d, const symbol_set &s)
{
    return detail::dpm_key_is_compatible(d, s, detail::dpm_n_expos_to_vsize<d_packed_monomial<T, PSize>>, PSize);
}

// Implementation of stream insertion.
// NOTE: requires that d is compatible with s.
template <typename T, unsigned PSize>
inline void key_stream_insert(::std::ostream &os, const d_packed_monomial<T, PSize> &d, const symbol_set &s)
{
    assert(polynomials::key_is_compatible(d, s));

    const auto &c = d._container();
    auto s_it = s.cbegin();
    const auto s_end = s.cend();

    T tmp;
    bool wrote_something = false;
    for (const auto &n : c) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && s_it != s_end; ++j, ++s_it) {
            ku >> tmp;

            if (tmp != T(0)) {
                // The exponent of the current variable
                // is nonzero.
                if (wrote_something) {
                    // We already printed something
                    // earlier, make sure we put
                    // the multiplication sign
                    // in front of the variable
                    // name.
                    os << '*';
                }
                // Print the variable name.
                os << *s_it;
                wrote_something = true;
                if (tmp != T(1)) {
                    // The exponent is not unitary,
                    // print it.
                    using namespace ::fmt::literals;
                    os << "**{}"_format(tmp);
                }
            }
        }
    }

    if (!wrote_something) {
        // We did not write anything to the stream.
        // It means that all variables have zero
        // exponent, thus we print only "1".
        os << '1';
    }
}

extern template void key_stream_insert(::std::ostream &, const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &,
                                       const symbol_set &);

extern template void key_stream_insert(::std::ostream &, const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &,
                                       const symbol_set &);

// Implementation of tex stream insertion.
// NOTE: requires that d is compatible with s.
template <typename T, unsigned PSize>
inline void key_tex_stream_insert(::std::ostream &os, const d_packed_monomial<T, PSize> &d, const symbol_set &s)
{
    assert(polynomials::key_is_compatible(d, s));

    using namespace ::fmt::literals;

    const auto &c = d._container();
    auto s_it = s.cbegin();
    const auto s_end = s.cend();

    // Use separate streams for numerator and denominator
    // (the denominator is used only in case of negative powers).
    ::std::ostringstream oss_num, oss_den, *cur_oss;
    oss_num.exceptions(::std::ios_base::failbit | ::std::ios_base::badbit);
    oss_num.flags(os.flags());
    oss_den.exceptions(::std::ios_base::failbit | ::std::ios_base::badbit);
    oss_den.flags(os.flags());

    T tmp;
    // Go through a multiprecision integer for the stream
    // insertion. This allows us not to care about potential
    // overflow conditions when manipulating the exponents
    // below.
    ::mppp::integer<1> tmp_mp;
    for (const auto &n : c) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && s_it != s_end; ++j, ++s_it) {
            // Extract the current exponent into
            // tmp_mp.
            ku >> tmp;
            tmp_mp = tmp;

            const auto sgn = tmp_mp.sgn();
            if (sgn != 0) {
                // Non-zero exponent, we will write something.
                if (sgn == 1) {
                    // Positive exponent, we will write
                    // to the numerator stream.
                    cur_oss = &oss_num;
                } else {
                    // Negative exponent: take the absolute value
                    // and write to the denominator stream.
                    tmp_mp.neg();
                    cur_oss = &oss_den;
                }

                // Print the symbol name.
                *cur_oss << "{{{}}}"_format(*s_it);

                // Raise to power, if the exponent is not one.
                if (!tmp_mp.is_one()) {
                    *cur_oss << "^{{{}}}"_format(tmp_mp);
                }
            }
        }
    }

    const auto num_str = oss_num.str(), den_str = oss_den.str();

    if (!num_str.empty() && !den_str.empty()) {
        // We have both negative and positive exponents,
        // print them both in a fraction.
        os << "\\frac{{{}}}{{{}}}"_format(num_str, den_str);
    } else if (!num_str.empty() && den_str.empty()) {
        // Only positive exponents.
        os << num_str;
    } else if (num_str.empty() && !den_str.empty()) {
        // Only negative exponents, display them as 1/something.
        os << "\\frac{{1}}{{{}}}"_format(den_str);
    } else {
        // We did not write anything to the stream.
        // It means that all variables have zero
        // exponent, thus we print only "1".
        assert(::std::all_of(c.begin(), c.end(), [](const T &n) { return n == T(0); }));
        os << '1';
    }
}

extern template void key_tex_stream_insert(::std::ostream &,
                                           const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &,
                                           const symbol_set &);

extern template void key_tex_stream_insert(::std::ostream &,
                                           const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &,
                                           const symbol_set &);

namespace detail
{

// Implementation of symbols merging.
// NOTE: requires that d is compatible with s, and ins_map consistent with s.
// NOTE: factored out for re-use.
template <typename T>
inline T dpm_key_merge_symbols(const T &d, const symbol_idx_map<symbol_set> &ins_map, const symbol_set &s)
{
    assert(::obake::key_is_compatible(d, s));
    // The last element of the insertion map must be at most s.size(), which means that there
    // are symbols to be appended at the end.
    assert(ins_map.empty() || ins_map.rbegin()->first <= s.size());

    using value_type = typename T::value_type;

    const auto &c = d._container();
    symbol_idx idx = 0;
    const auto s_size = s.size();
    auto map_it = ins_map.begin();
    const auto map_end = ins_map.end();
    value_type tmp;
    // NOTE: store the merged monomial in a temporary
    // vector and then pack it at the end.
    thread_local ::std::vector<value_type> tmp_v;
    tmp_v.clear();
    for (const auto &n : c) {
        kunpacker<value_type> ku(n, T::psize);

        for (auto j = 0u; j < T::psize && idx < s_size; ++j, ++idx) {
            if (map_it != map_end && map_it->first == idx) {
                // We reached an index at which we need to
                // insert new elements. Insert as many
                // zeroes as necessary in the temporary vector.
                tmp_v.insert(tmp_v.end(), ::obake::safe_cast<decltype(tmp_v.size())>(map_it->second.size()),
                             value_type(0));
                // Move to the next element in the map.
                ++map_it;
            }

            // Add the existing element to tmp_v.
            ku >> tmp;
            tmp_v.push_back(tmp);
        }
    }

    assert(idx == s_size);

    // We could still have symbols which need to be appended at the end.
    if (map_it != map_end) {
        tmp_v.insert(tmp_v.end(), ::obake::safe_cast<decltype(tmp_v.size())>(map_it->second.size()), value_type(0));
        assert(map_it + 1 == map_end);
    }

    return T(tmp_v);
}

} // namespace detail

// Implementation of symbols merging.
template <typename T, unsigned PSize>
inline d_packed_monomial<T, PSize> key_merge_symbols(const d_packed_monomial<T, PSize> &d,
                                                     const symbol_idx_map<symbol_set> &ins_map, const symbol_set &s)
{
    return detail::dpm_key_merge_symbols(d, ins_map, s);
}

extern template d_packed_monomial<dpm_default_s_t, dpm_default_psize>
key_merge_symbols(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &, const symbol_idx_map<symbol_set> &,
                  const symbol_set &);

extern template d_packed_monomial<dpm_default_u_t, dpm_default_psize>
key_merge_symbols(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &, const symbol_idx_map<symbol_set> &,
                  const symbol_set &);

// Implementation of monomial_mul().
// NOTE: requires a, b and out to be compatible with ss.
template <typename T, unsigned PSize>
inline void monomial_mul(d_packed_monomial<T, PSize> &out, const d_packed_monomial<T, PSize> &a,
                         const d_packed_monomial<T, PSize> &b, [[maybe_unused]] const symbol_set &ss)
{
    // Verify the inputs.
    assert(polynomials::key_is_compatible(a, ss));
    assert(polynomials::key_is_compatible(b, ss));
    assert(polynomials::key_is_compatible(out, ss));

    // NOTE: check whether using pointers + restrict helps here
    // (in which case we'd have to add the requirement to monomial_mul()
    // that out must be distinct from a/b).
    ::std::transform(a._container().cbegin(), a._container().cend(), b._container().cbegin(), out._container().begin(),
                     [](const T &x, const T &y) { return x + y; });

    // Verify the output as well.
    assert(polynomials::key_is_compatible(out, ss));
}

namespace detail
{

// Small helper to detect if 2 types
// are the same d_packed_monomial type.
template <typename, typename>
struct same_d_packed_monomial : ::std::false_type {
};

template <typename T, unsigned PSize>
struct same_d_packed_monomial<d_packed_monomial<T, PSize>, d_packed_monomial<T, PSize>> : ::std::true_type {
};

template <typename T, typename U>
inline constexpr bool same_d_packed_monomial_v = same_d_packed_monomial<T, U>::value;

} // namespace detail

// Monomial overflow checking.
// NOTE: this assumes that all the monomials in the 2 ranges
// are compatible with ss.
// NOTE: this will check both that the components
// of the product are within the kpack limits,
// and that the degrees of the product monomials
// are all computable without overflows.
// NOTE: this may be sped up by using safe integral
// arithmetics rather than mppp::integer. However,
// we would need to deal with the fact that safe
// arithmetics throws in case of overflows, whereas
// here we want to return a boolean. Not sure if it
// is worth it to change the safe arithmetics API
// for this.
template <typename R1, typename R2>
requires InputRange<R1> && InputRange<R2> && detail::same_d_packed_monomial_v<
    remove_cvref_t<typename ::std::iterator_traits<range_begin_t<R1>>::reference>,
    remove_cvref_t<typename ::std::iterator_traits<range_begin_t<R2>>::reference>>
inline bool monomial_range_overflow_check(R1 &&r1, R2 &&r2, const symbol_set &ss)
{
    using pm_t = remove_cvref_t<typename ::std::iterator_traits<range_begin_t<R1>>::reference>;
    using value_type = typename pm_t::value_type;
    using int_t = ::mppp::integer<1>;

    const auto s_size = ss.size();

    if (s_size == 0u) {
        // If the monomials have zero variables,
        // there cannot be overflow.
        return true;
    }

    // Get out the begin/end iterators.
    auto b1 = ::obake::begin(::std::forward<R1>(r1));
    const auto e1 = ::obake::end(::std::forward<R1>(r1));
    auto b2 = ::obake::begin(::std::forward<R2>(r2));
    const auto e2 = ::obake::end(::std::forward<R2>(r2));

    if (b1 == e1 || b2 == e2) {
        // If either range is empty, there will be no overflow.
        return true;
    }

    // Prepare the component limits and the degree limits
    // for each range.
    // For signed integrals, the component limits are vectors
    // of min/max pairs for each exponent, and the degree limits
    // are pairs of min/max degree represented as multiprecision
    // integers.
    // For unsigned integrals, the component limits are vectors
    // of max exponents, and the degree limits are max degrees
    // represented as multiprecision integers.
    auto [limits1, limits2, dlimits1, dlimits2] = [s_size]() {
        if constexpr (is_signed_v<value_type>) {
            ::std::vector<::std::pair<value_type, value_type>> minmax1, minmax2;
            minmax1.reserve(static_cast<decltype(minmax1.size())>(s_size));
            minmax2.reserve(static_cast<decltype(minmax2.size())>(s_size));

            return ::std::make_tuple(::std::move(minmax1), ::std::move(minmax2), ::std::make_pair(int_t{}, int_t{}),
                                     ::std::make_pair(int_t{}, int_t{}));
        } else {
            ::std::vector<value_type> max1, max2;
            max1.reserve(static_cast<decltype(max1.size())>(s_size));
            max2.reserve(static_cast<decltype(max2.size())>(s_size));

            return ::std::make_tuple(::std::move(max1), ::std::move(max2), int_t{}, int_t{});
        }
    }();

    // Fetch the packed size.
    constexpr auto psize = pm_t::psize;

    // Init the limits with the first elements of the ranges.
    {
        // NOTE: if the iterators return copies
        // of the monomials, rather than references,
        // capturing them via const
        // ref will extend their lifetimes.
        const auto &init1 = *b1;
        const auto &init2 = *b2;

        assert(polynomials::key_is_compatible(init1, ss));
        assert(polynomials::key_is_compatible(init2, ss));

        const auto &c1 = init1._container();
        const auto &c2 = init2._container();
        assert(c1.size() == c2.size());

        symbol_idx idx = 0;
        value_type tmp1, tmp2;

        for (decltype(c1.size()) i = 0; i < c1.size(); ++i) {
            kunpacker<value_type> ku1(c1[i], psize), ku2(c2[i], psize);

            for (auto j = 0u; j < psize && idx < s_size; ++j, ++idx) {
                ku1 >> tmp1;
                ku2 >> tmp2;

                if constexpr (is_signed_v<value_type>) {
                    limits1.emplace_back(tmp1, tmp1);
                    limits2.emplace_back(tmp2, tmp2);

                    // Accumulate the min/max degrees
                    // (initially inited to the same values).
                    dlimits1.first += tmp1;
                    dlimits1.second += tmp1;
                    dlimits2.first += tmp2;
                    dlimits2.second += tmp2;
                } else {
                    limits1.emplace_back(tmp1);
                    limits2.emplace_back(tmp2);

                    // Accumulate the max degrees.
                    dlimits1 += tmp1;
                    dlimits2 += tmp2;
                }
            }
        }
    }

    // Serial implementation.
    auto serial_impl = [&ss, s_size, b1, e1, &l1 = limits1, &dl1 = dlimits1, b2, e2, &l2 = limits2, &dl2 = dlimits2
#if defined(_MSC_VER) && !defined(__clang__)
                        ,
                        psize
#endif
    ]() {
        // Helper to examine the rest of the ranges.
        auto update_minmax = [&ss, s_size
#if defined(_MSC_VER) && !defined(__clang__)
                              ,
                              psize
#endif
        ](auto b, auto e, auto &limits, auto &dl) {
            ::obake::detail::ignore(ss);

            for (++b; b != e; ++b) {
                const auto &cur = *b;

                assert(polynomials::key_is_compatible(cur, ss));

                symbol_idx idx = 0;
                value_type tmp;
                int_t deg;
                for (const auto &n : cur._container()) {
                    kunpacker<value_type> ku(n, psize);

                    for (auto j = 0u; j < psize && idx < s_size; ++j, ++idx) {
                        ku >> tmp;
                        // Accumulate the degree.
                        deg += tmp;

                        if constexpr (is_signed_v<value_type>) {
                            limits[idx].first = ::std::min(limits[idx].first, tmp);
                            limits[idx].second = ::std::max(limits[idx].second, tmp);
                        } else {
                            limits[idx] = ::std::max(limits[idx], tmp);
                        }
                    }
                }

                if constexpr (is_signed_v<value_type>) {
                    // Update the min/max degrees.
                    dl.first = ::std::min(dl.first, deg);
                    dl.second = ::std::max(dl.second, deg);
                } else {
                    // Update the max degree.
                    dl = ::std::max(dl, deg);
                }
            }
        };
        // Examine the rest of the ranges.
        update_minmax(b1, e1, l1, dl1);
        update_minmax(b2, e2, l2, dl2);
    };

    if constexpr (::std::conjunction_v<is_random_access_iterator<decltype(b1)>,
                                       is_random_access_iterator<decltype(b2)>>) {
        // If both ranges are random-access, we have the option of running a parallel
        // overflow check.
        const auto size1 = ::std::distance(b1, e1);
        const auto size2 = ::std::distance(b2, e2);

        // NOTE: run the parallel implementation only if
        // at least one of the sizes is large enough.
        if (size1 > 5000 || size2 > 5000) {
            auto par_functor = [&ss, s_size
#if defined(_MSC_VER) && !defined(__clang__)
                                ,
                                psize
#endif
            ](auto b, auto e, const auto &limits, const auto &dl) {
                return ::tbb::parallel_reduce(
                    // NOTE: the ranges are guaranteed to be non-empty,
                    // thus b + 1 is always well-defined.
                    ::tbb::blocked_range<decltype(b)>(b + 1, e), ::std::make_pair(limits, dl),
                    [&ss, s_size
#if defined(_MSC_VER) && !defined(__clang__)
                     ,
                     psize
#endif
                ](const auto &range, auto cur) {
                        ::obake::detail::ignore(ss);

                        for (const auto &m : range) {
                            assert(polynomials::key_is_compatible(m, ss));

                            symbol_idx idx = 0;
                            value_type tmp;
                            int_t deg;
                            for (const auto &n : m._container()) {
                                kunpacker<value_type> ku(n, psize);

                                for (auto j = 0u; j < psize && idx < s_size; ++j, ++idx) {
                                    ku >> tmp;
                                    // Accumulate the degree.
                                    deg += tmp;

                                    if constexpr (is_signed_v<value_type>) {
                                        cur.first[idx].first = ::std::min(cur.first[idx].first, tmp);
                                        cur.first[idx].second = ::std::max(cur.first[idx].second, tmp);
                                    } else {
                                        cur.first[idx] = ::std::max(cur.first[idx], tmp);
                                    }
                                }
                            }

                            if constexpr (is_signed_v<value_type>) {
                                // Update the min/max degrees.
                                cur.second.first = ::std::min(cur.second.first, deg);
                                cur.second.second = ::std::max(cur.second.second, deg);
                            } else {
                                // Update the max degree.
                                cur.second = ::std::max(cur.second, deg);
                            }
                        }

                        return cur;
                    },
                    [s_size](const auto &l1, const auto &l2) {
                        assert(l1.first.size() == s_size);
                        assert(l2.first.size() == s_size);

                        remove_cvref_t<decltype(l1)> ret;
                        ret.first.reserve(s_size);

                        for (auto i = 0u; i < s_size; ++i) {
                            if constexpr (is_signed_v<value_type>) {
                                ret.first.emplace_back(::std::min(l1.first[i].first, l2.first[i].first),
                                                       ::std::max(l1.first[i].second, l2.first[i].second));
                            } else {
                                ret.first.emplace_back(::std::max(l1.first[i], l2.first[i]));
                            }
                        }

                        if constexpr (is_signed_v<value_type>) {
                            // Compute the min/max degrees.
                            ret.second.first = ::std::min(l1.second.first, l2.second.first);
                            ret.second.second = ::std::max(l1.second.second, l2.second.second);
                        } else {
                            // Compute the max degree.
                            ret.second = ::std::max(l1.second, l2.second);
                        }

                        return ret;
                    });
            };

            ::tbb::parallel_invoke(
                [par_functor, b1, e1, &l1 = limits1, &dl1 = dlimits1]() {
                    auto ret = par_functor(b1, e1, l1, dl1);
                    l1 = ::std::move(ret.first);
                    dl1 = ::std::move(ret.second);
                },
                [par_functor, b2, e2, &l2 = limits2, &dl2 = dlimits2]() {
                    auto ret = par_functor(b2, e2, l2, dl2);
                    l2 = ::std::move(ret.first);
                    dl2 = ::std::move(ret.second);
                });
        } else {
            serial_impl();
        }
    } else {
        serial_impl();
    }

    // Now add the component limits via interval arithmetics
    // and check for overflow. Use mppp::integer for the check.
    const auto [lim_min, lim_max] = ::obake::detail::kpack_get_lims<value_type>(psize);

    for (decltype(limits1.size()) i = 0; i < s_size; ++i) {
        if constexpr (is_signed_v<value_type>) {
            const auto add_min = int_t{limits1[i].first} + limits2[i].first;
            const auto add_max = int_t{limits1[i].second} + limits2[i].second;

            // NOTE: an overflow condition will likely result in an exception
            // or some other error handling. Optimise for the non-overflow case.
            if (obake_unlikely(add_min < lim_min || add_max > lim_max)) {
                return false;
            }
        } else {
            const auto add_max = int_t{limits1[i]} + limits2[i];

            if (obake_unlikely(add_max > lim_max)) {
                return false;
            }
        }
    }

    // Do the same check for the degrees.
    if constexpr (is_signed_v<value_type>) {
        const auto deg_min = dlimits1.first + dlimits2.first;
        const auto deg_max = dlimits1.second + dlimits2.second;

        if (obake_unlikely(
                deg_min
                    < ::obake::detail::limits_min<value_type> || deg_max > ::obake::detail::limits_max<value_type>)) {
            return false;
        }
    } else {
        const auto deg_max = dlimits1 + dlimits2;

        if (obake_unlikely(deg_max > ::obake::detail::limits_max<value_type>)) {
            return false;
        }
    }

    return true;
}

// Implementation of key_degree().
// NOTE: this assumes that d is compatible with ss.
template <typename T, unsigned PSize>
inline T key_degree(const d_packed_monomial<T, PSize> &d, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));

    const auto s_size = ss.size();

    symbol_idx idx = 0;
    T tmp, retval(0);
    for (const auto &n : d._container()) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && idx < s_size; ++j, ++idx) {
            ku >> tmp;
            retval = ::obake::detail::safe_int_add(retval, tmp);
        }
    }

    return static_cast<T>(retval);
}

extern template dpm_default_s_t key_degree(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &,
                                           const symbol_set &);

extern template dpm_default_u_t key_degree(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &,
                                           const symbol_set &);

// Implementation of key_p_degree().
// NOTE: this assumes that d and si are compatible with ss.
template <typename T, unsigned PSize>
inline T key_p_degree(const d_packed_monomial<T, PSize> &d, const symbol_idx_set &si, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    assert(si.empty() || *(si.end() - 1) < ss.size());

    const auto s_size = ss.size();

    symbol_idx idx = 0;
    T tmp, retval(0);
    auto si_it = si.begin();
    const auto si_it_end = si.end();
    for (const auto &n : d._container()) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && idx < s_size && si_it != si_it_end; ++j, ++idx) {
            ku >> tmp;

            if (idx == *si_it) {
                retval = ::obake::detail::safe_int_add(retval, tmp);
                ++si_it;
            }
        }
    }

    assert(si_it == si_it_end);

    return static_cast<T>(retval);
}

extern template dpm_default_s_t key_p_degree(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &,
                                             const symbol_idx_set &, const symbol_set &);

extern template dpm_default_u_t key_p_degree(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &,
                                             const symbol_idx_set &, const symbol_set &);

// Monomial exponentiation.
// NOTE: this assumes that d is compatible with ss.
template <typename T, unsigned PSize, typename U,
          ::std::enable_if_t<::std::disjunction_v<::obake::detail::is_mppp_integer<U>,
                                                  is_safely_convertible<const U &, ::mppp::integer<1> &>>,
                             int> = 0>
inline d_packed_monomial<T, PSize> monomial_pow(const d_packed_monomial<T, PSize> &d, const U &n, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));

    // NOTE: exp will be a const ref if n is already
    // an mppp integer, a new value otherwise.
    decltype(auto) exp = [&n]() -> decltype(auto) {
        if constexpr (::obake::detail::is_mppp_integer_v<U>) {
            return n;
        } else {
            ::mppp::integer<1> ret;

            if (obake_unlikely(!::obake::safe_convert(ret, n))) {
                if constexpr (is_stream_insertable_v<const U &>) {
                    // Provide better error message if U is ostreamable.
                    using namespace ::fmt::literals;
                    obake_throw(::std::invalid_argument, "Invalid exponent for monomial exponentiation: the exponent "
                                                         "({}) cannot be converted into an integral value"_format(n));
                } else {
                    obake_throw(::std::invalid_argument, "Invalid exponent for monomial exponentiation: the exponent "
                                                         "cannot be converted into an integral value");
                }
            }

            return ret;
        }
    }();

    const auto s_size = ss.size();

    // Prepare the return value.
    const auto &c_in = d._container();
    d_packed_monomial<T, PSize> retval;
    auto &c_out = retval._container();
    c_out.reserve(c_in.size());

    // Unpack, multiply in arbitrary-precision arithmetic, re-pack.
    T tmp;
    symbol_idx idx = 0;
    remove_cvref_t<decltype(exp)> tmp_int;
    for (const auto &np : c_in) {
        kunpacker<T> ku(np, PSize);
        kpacker<T> kp(PSize);

        for (auto j = 0u; j < PSize && idx < s_size; ++j, ++idx) {
            ku >> tmp;
            tmp_int = tmp;
            tmp_int *= exp;
            kp << static_cast<T>(tmp_int);
        }

        c_out.push_back(kp.get());
    }

    return retval;
}

// Specialise byte_size().
// NOTE: currently there does not seem to be
// a way to ask boost::small_vector whether
// dynamic or static storage is being used.
// Thus, this function will slightly overestimate
// the actual byte size of d.
template <typename T, unsigned PSize>
inline ::std::size_t byte_size(const d_packed_monomial<T, PSize> &d)
{
    return sizeof(d) + d._container().capacity() * sizeof(T);
}

namespace detail
{

// Metaprogramming for the key_evaluate() implementation for d_packed_monomial.
template <typename T, typename U>
constexpr auto dpm_key_evaluate_algorithm_impl()
{
    [[maybe_unused]] constexpr auto failure = ::std::make_pair(0, ::obake::detail::type_c<void>{});

    // Test exponentiability via const lvalue refs.
    if constexpr (is_exponentiable_v<::std::add_lvalue_reference_t<const U>, ::std::add_lvalue_reference_t<const T>>) {
        using ret_t = ::obake::detail::pow_t<const U &, const T &>;

        // We will need to construct the return value from 1,
        // multiply it in-place and then return it.
        // NOTE: require also a semi-regular type,
        // it's just easier to reason about.
        if constexpr (::std::conjunction_v<::std::is_constructible<ret_t, int>,
                                           // NOTE: we will be multiplying an
                                           // lvalue by an rvalue.
                                           is_in_place_multipliable<::std::add_lvalue_reference_t<ret_t>, ret_t>,
                                           is_semi_regular<ret_t>, is_returnable<ret_t>>) {
            return ::std::make_pair(1, ::obake::detail::type_c<ret_t>{});
        } else {
            return failure;
        }
    } else {
        return failure;
    }
}

// Shortcuts.
template <typename T, typename U>
inline constexpr auto dpm_key_evaluate_algorithm = detail::dpm_key_evaluate_algorithm_impl<T, U>();

template <typename T, typename U>
inline constexpr int dpm_key_evaluate_algo = dpm_key_evaluate_algorithm<T, U>.first;

template <typename T, typename U>
using dpm_key_evaluate_ret_t = typename decltype(dpm_key_evaluate_algorithm<T, U>.second)::type;

} // namespace detail

// Evaluation of a dynamic packed monomial.
// NOTE: this requires that d is compatible with ss,
// and that sm is consistent with ss.
template <typename T, unsigned PSize, typename U, ::std::enable_if_t<detail::dpm_key_evaluate_algo<T, U> != 0, int> = 0>
inline detail::dpm_key_evaluate_ret_t<T, U> key_evaluate(const d_packed_monomial<T, PSize> &d,
                                                         const symbol_idx_map<U> &sm, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    // sm and ss must have the same size, and the last element
    // of sm must have an index equal to the last index of ss.
    assert(sm.size() == ss.size() && (sm.empty() || (sm.cend() - 1)->first == ss.size() - 1u));

    // Init the return value.
    detail::dpm_key_evaluate_ret_t<T, U> retval(1);
    T tmp;
    auto sm_it = sm.begin();
    const auto sm_end = sm.end();
    // Accumulate the result.
    for (const auto &n : d._container()) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && sm_it != sm_end; ++j, ++sm_it) {
            ku >> tmp;
            retval *= ::obake::pow(sm_it->second, ::std::as_const(tmp));
        }
    }

    return retval;
}

namespace detail
{

// NOTE: the metaprogramming for the monomial_subs() implementation for d_packed_monomial
// is identical to the key_evaluate() implementation.

// Shortcuts.
template <typename T, typename U>
inline constexpr auto dpm_monomial_subs_algorithm = detail::dpm_key_evaluate_algorithm_impl<T, U>();

template <typename T, typename U>
inline constexpr int dpm_monomial_subs_algo = dpm_key_evaluate_algorithm<T, U>.first;

template <typename T, typename U>
using dpm_monomial_subs_ret_t = typename decltype(dpm_key_evaluate_algorithm<T, U>.second)::type;

} // namespace detail

// Substitution of symbols in a dynamic packed monomial.
// NOTE: this requires that d is compatible with ss,
// and that sm is consistent with ss.
template <typename T, unsigned PSize, typename U,
          ::std::enable_if_t<detail::dpm_monomial_subs_algo<T, U> != 0, int> = 0>
inline ::std::pair<detail::dpm_monomial_subs_ret_t<T, U>, d_packed_monomial<T, PSize>>
monomial_subs(const d_packed_monomial<T, PSize> &d, const symbol_idx_map<U> &sm, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    // sm must not be larger than ss, and the last element
    // of sm must have an index smaller than the size of ss.
    assert(sm.size() <= ss.size() && (sm.empty() || (sm.cend() - 1)->first < ss.size()));

    const auto s_size = ss.size();

    // Init the return values.
    const auto &in_c = d._container();
    d_packed_monomial<T, PSize> out_dpm;
    auto &out_c = out_dpm._container();
    out_c.reserve(in_c.size());
    detail::dpm_monomial_subs_ret_t<T, U> retval(1);

    symbol_idx idx = 0;
    auto sm_it = sm.begin();
    const auto sm_end = sm.end();
    T tmp;
    for (const auto &n : in_c) {
        kunpacker<T> ku(n, PSize);
        kpacker<T> kp(PSize);

        for (auto j = 0u; j < PSize && idx < s_size; ++j, ++idx) {
            ku >> tmp;

            if (sm_it != sm_end && sm_it->first == idx) {
                // The current exponent is in the subs map,
                // accumulate the result of the substitution.
                retval *= ::obake::pow(sm_it->second, ::std::as_const(tmp));

                // Set the exponent to zero in the output
                // monomial.
                kp << T(0);

                // Move to the next item in the map.
                ++sm_it;
            } else {
                // Either the current exponent is not in the subs map,
                // or we already reached the end of the map.
                // Just copy the original exponent into the output monomial.
                kp << tmp;
            }
        }

        out_c.push_back(kp.get());
    }
    assert(sm_it == sm_end);

    return ::std::make_pair(::std::move(retval), ::std::move(out_dpm));
}

// Identify non-trimmable exponents in d.
// NOTE: this requires that d is compatible with ss,
// and that v has the same size as ss.
template <typename T, unsigned PSize>
inline void key_trim_identify(::std::vector<int> &v, const d_packed_monomial<T, PSize> &d, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    assert(v.size() == ss.size());

    const auto s_size = ss.size();

    T tmp;
    symbol_idx idx = 0;
    for (const auto &n : d._container()) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && idx < s_size; ++j, ++idx) {
            ku >> tmp;

            if (tmp != T(0)) {
                // The current exponent is nonzero,
                // thus it must not be trimmed.
                v[idx] = 0;
            }
        }
    }
}

extern template void key_trim_identify(::std::vector<int> &,
                                       const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &,
                                       const symbol_set &);

extern template void key_trim_identify(::std::vector<int> &,
                                       const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &,
                                       const symbol_set &);

// Eliminate from d the exponents at the indices
// specifed by si.
// NOTE: this requires that d is compatible with ss,
// and that si is consistent with ss.
template <typename T, unsigned PSize>
inline d_packed_monomial<T, PSize> key_trim(const d_packed_monomial<T, PSize> &d, const symbol_idx_set &si,
                                            const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    // NOTE: si cannot be larger than ss, and its last element must be smaller
    // than the size of ss.
    assert(si.size() <= ss.size() && (si.empty() || *(si.cend() - 1) < ss.size()));

    const auto s_size = ss.size();

    // NOTE: store the trimmed monomial in a temporary vector and then pack it
    // at the end.
    // NOTE: perhaps we could use a small_vector here with a static size
    // equal to psize, for the case in which everything fits in a single
    // packed value.
    // NOTE: here we also know that the output number of exponents is
    // s_size - si.size(), thus we can probably pre-allocate storage
    // in tmp_v and/or the output monomial.
    // NOTE: perhaps better to make this a thread local variable.
    ::std::vector<T> tmp_v;

    symbol_idx idx = 0;
    T tmp;
    auto si_it = si.cbegin();
    const auto si_end = si.cend();
    for (const auto &n : d._container()) {
        kunpacker<T> ku(n, PSize);

        for (auto j = 0u; j < PSize && idx < s_size; ++j, ++idx) {
            ku >> tmp;

            if (si_it != si_end && *si_it == idx) {
                // The current exponent must be trimmed. Don't
                // add it to tmp_v, and move to the next item in the trim set.
                ++si_it;
            } else {
                // The current exponent must be kept.
                tmp_v.push_back(tmp);
            }
        }
    }
    assert(si_it == si_end);

    return d_packed_monomial<T, PSize>(tmp_v);
}

extern template d_packed_monomial<dpm_default_s_t, dpm_default_psize>
key_trim(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &, const symbol_idx_set &, const symbol_set &);

extern template d_packed_monomial<dpm_default_u_t, dpm_default_psize>
key_trim(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &, const symbol_idx_set &, const symbol_set &);

// Monomial differentiation.
// NOTE: this requires that d is compatible with ss,
// and idx is within ss.
template <typename T, unsigned PSize>
inline ::std::pair<T, d_packed_monomial<T, PSize>> monomial_diff(const d_packed_monomial<T, PSize> &d,
                                                                 const symbol_idx &idx, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    assert(idx < ss.size());

    const auto s_size = ss.size();

    // Init the return value.
    const auto &in_c = d._container();
    d_packed_monomial<T, PSize> out_dpm;
    auto &out_c = out_dpm._container();
    out_c.reserve(in_c.size());

    symbol_idx i = 0;
    T tmp, ret_exp(0);
    for (const auto &n : in_c) {
        kunpacker<T> ku(n, PSize);
        kpacker<T> kp(PSize);

        for (auto j = 0u; j < PSize && i < s_size; ++j, ++i) {
            ku >> tmp;

            if (i == idx && tmp != T(0)) {
                // NOTE: the exponent of the differentiation variable
                // is not zero. Take the derivative.
                // NOTE: if the exponent is zero, ret_exp will remain to
                // its initial value (0) and the output monomial
                // will be the same as p.
                // NOTE: no need for overflow checking here
                // due to the way we create the kpack deltas
                // and consequently the limits.
                ret_exp = tmp--;
            }

            kp << tmp;
        }

        out_c.push_back(kp.get());
    }

    return ::std::make_pair(ret_exp, ::std::move(out_dpm));
}

extern template ::std::pair<dpm_default_s_t, d_packed_monomial<dpm_default_s_t, dpm_default_psize>>
monomial_diff(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &, const symbol_idx &, const symbol_set &);

extern template ::std::pair<dpm_default_u_t, d_packed_monomial<dpm_default_u_t, dpm_default_psize>>
monomial_diff(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &, const symbol_idx &, const symbol_set &);

// Monomial integration.
// NOTE: this requires that d is compatible with ss,
// and idx is within ss.
template <typename T, unsigned PSize>
inline ::std::pair<T, d_packed_monomial<T, PSize>> monomial_integrate(const d_packed_monomial<T, PSize> &d,
                                                                      const symbol_idx &idx, const symbol_set &ss)
{
    assert(polynomials::key_is_compatible(d, ss));
    assert(idx < ss.size());

    const auto s_size = ss.size();

    // Init the return value.
    const auto &in_c = d._container();
    d_packed_monomial<T, PSize> out_dpm;
    auto &out_c = out_dpm._container();
    out_c.reserve(in_c.size());

    symbol_idx i = 0;
    T tmp, ret_exp(0);
    for (const auto &n : in_c) {
        kunpacker<T> ku(n, PSize);
        kpacker<T> kp(PSize);

        for (auto j = 0u; j < PSize && i < s_size; ++j, ++i) {
            ku >> tmp;

            if (i == idx) {
                if constexpr (is_signed_v<T>) {
                    // For signed integrals, make sure
                    // we are not integrating x**-1.
                    if (obake_unlikely(tmp == T(-1))) {
                        using namespace ::fmt::literals;

                        obake_throw(
                            ::std::domain_error,
                            "Cannot integrate a dynamic packed monomial: the exponent of the integration variable "
                            "('{}') is -1, and the integration would generate a logarithmic term"_format(
                                *ss.nth(static_cast<decltype(ss.size())>(i))));
                    }
                }

                // NOTE: no need for overflow checking here
                // due to the way we create the kpack deltas
                // and consequently the limits.
                ret_exp = ++tmp;
            }

            kp << tmp;
        }

        out_c.push_back(kp.get());
    }
    // We must have written some nonzero value to ret_exp.
    assert(ret_exp != T(0));

    return ::std::make_pair(ret_exp, ::std::move(out_dpm));
}

extern template ::std::pair<dpm_default_s_t, d_packed_monomial<dpm_default_s_t, dpm_default_psize>>
monomial_integrate(const d_packed_monomial<dpm_default_s_t, dpm_default_psize> &, const symbol_idx &,
                   const symbol_set &);

extern template ::std::pair<dpm_default_u_t, d_packed_monomial<dpm_default_u_t, dpm_default_psize>>
monomial_integrate(const d_packed_monomial<dpm_default_u_t, dpm_default_psize> &, const symbol_idx &,
                   const symbol_set &);

} // namespace polynomials

// Lift to the obake namespace.
template <typename T, unsigned PSize>
using d_packed_monomial = polynomials::d_packed_monomial<T, PSize>;

// Definition of the default dynamically-packed monomial type.
using d_monomial = d_packed_monomial<polynomials::dpm_default_u_t, polynomials::dpm_default_psize>;

// Definition of the default dynamically-packed Laurent monomial type.
using d_laurent_monomial = d_packed_monomial<polynomials::dpm_default_s_t, polynomials::dpm_default_psize>;

// Specialise monomial_has_homomorphic_hash.
template <typename T, unsigned PSize>
inline constexpr bool monomial_hash_is_homomorphic<d_packed_monomial<T, PSize>> = true;

} // namespace obake

namespace boost::serialization
{

// Disable tracking for d_packed_monomial.
template <typename T, unsigned PSize>
struct tracking_level<::obake::d_packed_monomial<T, PSize>>
    : ::obake::detail::s11n_no_tracking<::obake::d_packed_monomial<T, PSize>> {
};

} // namespace boost::serialization

#endif
