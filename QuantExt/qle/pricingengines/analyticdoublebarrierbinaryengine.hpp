/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/pricingengines/analyticdoublebarrierbinaryengine.hpp
    \brief Wrapper of QuantLib analytic double barrier binary engine to allow for flipping back some of the additional
   results in the case of FX instruments where the trade builder may have inverted the underlying pair
*/

#pragma once

#include <ql/pricingengines/barrier/analyticdoublebarrierbinaryengine.hpp>
#include <ql/instruments/doublebarrieroption.hpp>

namespace QuantExt {

    using namespace QuantLib;

    //! Analytic pricing engine for double barrier binary options
    class AnalyticDoubleBarrierBinaryEngine : public QuantLib::AnalyticDoubleBarrierBinaryEngine {
      public:
        explicit AnalyticDoubleBarrierBinaryEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> gbsp, const Date& payDate,
                                                   const bool flipResults = false)
              : QuantLib::AnalyticDoubleBarrierBinaryEngine(gbsp), process_(std::move(gbsp)), payDate_(payDate),
                flipResults_(flipResults) {
              registerWith(process_);
          }
        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Date payDate_;
        bool flipResults_;
    };

}
