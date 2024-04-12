/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/models/infjyparameterization.hpp
    \brief Jarrow Yildrim inflation parameterization
*/

#ifndef quantext_inf_jy_parameterization_hpp
#define quantext_inf_jy_parameterization_hpp

#include <ql/shared_ptr.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {

class InfJyParameterization : public Parametrization {
public:
    InfJyParameterization(QuantLib::ext::shared_ptr<Lgm1fParametrization<QuantLib::ZeroInflationTermStructure>> realRate,
                          QuantLib::ext::shared_ptr<FxBsParametrization> index,
                          QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> inflationIndex);

    //! \name Parametrization interface
    //@{
    const QuantLib::Array& parameterTimes(const QuantLib::Size i) const override;
    const QuantLib::ext::shared_ptr<QuantLib::Parameter> parameter(const QuantLib::Size i) const override;
    void update() const override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<Lgm1fParametrization<QuantLib::ZeroInflationTermStructure>> realRate() const;
    QuantLib::ext::shared_ptr<FxBsParametrization> index() const;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> inflationIndex() const;
    //@}

    Size numberOfParameters() const override { return 3; }

protected:
    //! \name Parametrization interface
    //@{
    QuantLib::Real direct(const QuantLib::Size i, const QuantLib::Real x) const override;
    QuantLib::Real inverse(const QuantLib::Size i, const QuantLib::Real y) const override;
    //@}

private:
    QuantLib::ext::shared_ptr<Lgm1fParametrization<QuantLib::ZeroInflationTermStructure>> realRate_;
    QuantLib::ext::shared_ptr<FxBsParametrization> index_;
    QuantLib::ext::shared_ptr<QuantLib::ZeroInflationIndex> inflationIndex_;

    //! Check that the indexing.
    void checkIndex(QuantLib::Size i) const;
};

} // namespace QuantExt

#endif
