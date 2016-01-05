/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/parametrization.hpp>

namespace QuantExt {

Parametrization::Parametrization(const Currency &currency)
    : h_(1.0E-6), h2_(1.0E-4), currency_(currency),
      emptyParameter_(boost::make_shared<NullParameter>()) {}

} // namespace QuantExt
