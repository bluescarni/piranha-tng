// Copyright 2019-2020 Francesco Biscani (bluescarni@gmail.com)
//
// This file is part of the obake library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OBAKE_TYPE_NAME_HPP
#define OBAKE_TYPE_NAME_HPP

#include <mp++/type_name.hpp>

#include <obake/config.hpp>

namespace obake
{

// Wrapper around mppp::type_name() for getting
// the name of T at runtime.

template <typename T>
inline constexpr auto type_name = []() { return ::mppp::type_name<T>(); };

} // namespace obake

#endif
