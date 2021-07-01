/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
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
