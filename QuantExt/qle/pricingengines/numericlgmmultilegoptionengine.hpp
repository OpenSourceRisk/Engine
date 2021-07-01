/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#pragma once

#include <qle/models/lgmconvolutionsolver2.hpp>
#include <qle/models/lgmvectorised.hpp>

#include <ql/instruments/nonstandardswaption.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantExt {

class NumericLgmMultilegOptionEngine
    : public public QuantLib::GenericEngine<MultiLegOption::arguments, MultiLegOption::results>,
      protected LgmConvolutionSolver2 {
protected:
    NumericLgmMultilegOptionEngine(const boost::shared_ptr<LinearGaussMarkovModel>& model, const Real sy, const Size ny,
                                   const Real sx, const Size nx, const Handle<YieldTermStructure>& discountCurve);

    Real calculate() const override;
    std::map<std::string, boost::any> additionalResults() const;

    const Handle<YieldTermStructure> discountCurve_;
};

} // namespace QuantExt
