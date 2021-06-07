// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <ostream>

#include <obake/detail/visibility.hpp>
#include <obake/poisson_series/d_packed_trig_monomial.hpp>
#include <obake/symbols.hpp>

namespace obake::poisson_series
{

template OBAKE_DLL_PUBLIC void key_stream_insert(::std::ostream &,
                                                 const d_packed_trig_monomial<dptm_default_t, dptm_default_psize> &,
                                                 const symbol_set &);

template OBAKE_DLL_PUBLIC void key_tex_stream_insert(::std::ostream &,
                                                     const d_packed_trig_monomial<dptm_default_t, dptm_default_psize> &,
                                                     const symbol_set &);

template OBAKE_DLL_PUBLIC d_packed_trig_monomial<dptm_default_t, dptm_default_psize>
key_merge_symbols(const d_packed_trig_monomial<dptm_default_t, dptm_default_psize> &,
                  const symbol_idx_map<symbol_set> &, const symbol_set &);

} // namespace obake::poisson_series
