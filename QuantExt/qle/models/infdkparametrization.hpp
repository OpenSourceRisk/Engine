/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file qle/models/infdkparametrization.hpp
    \brief Inflation Dodgson Kainth parametrization
*/

#ifndef quantextplus_infdklgm1f_parametrization_hpp
#define quantextplus_infdklgm1f_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace QuantExt {

typedef Lgm1fParametrization<ZeroInflationTermStructure> InfDkParametrization;

typedef Lgm1fConstantParametrization<ZeroInflationTermStructure> InfDkConstantParametrization;

typedef Lgm1fPiecewiseConstantHullWhiteAdaptor<ZeroInflationTermStructure> InfDkPiecewiseConstantHullWhiteAdaptor;

typedef Lgm1fPiecewiseConstantParametrization<ZeroInflationTermStructure> InfDkPiecewiseConstantParametrization;

typedef Lgm1fPiecewiseLinearParametrization<ZeroInflationTermStructure> InfDkPiecewiseLinearParametrization;

} // namespace QuantExt

#endif
