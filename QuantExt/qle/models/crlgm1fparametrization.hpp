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

/*! \file crlgm1fparametrization.hpp
    \brief Credit Linear Gaussian Markov 1 factor parametrization
    \ingroup models
*/

#ifndef quantextplus_crlgm1f_parametrization_hpp
#define quantextplus_crlgm1f_parametrization_hpp

#include <ql/handle.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace QuantExt {

typedef Lgm1fParametrization<DefaultProbabilityTermStructure> CrLgm1fParametrization;

typedef Lgm1fConstantParametrization<DefaultProbabilityTermStructure> CrLgm1fConstantParametrization;

typedef Lgm1fPiecewiseConstantHullWhiteAdaptor<DefaultProbabilityTermStructure>
    CrLgm1fPiecewiseConstantHullWhiteADaptor;

typedef Lgm1fPiecewiseConstantParametrization<DefaultProbabilityTermStructure> CrLgm1fPiecewiseConstantParametrization;

typedef Lgm1fPiecewiseLinearParametrization<DefaultProbabilityTermStructure> CrLgm1fPiecewiseLinearParametrization;

} // namespace QuantExt

#endif
