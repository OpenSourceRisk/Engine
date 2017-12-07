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

/*! \file analyticdkcpicapfloorengine.hpp
    \brief analytic dk cpi cap floor engine
 \ingroup engines
*/

#ifndef quantext_dk_cpicapfloorengine_hpp
#define quantext_dk_cpicapfloorengine_hpp

#include <ql/instruments/cpicapfloor.hpp>
#include <qle/models/crossassetmodel.hpp>

namespace QuantExt {
//! Analytic dk cpi cap floor engine
/*! \ingroup engines
 */
class AnalyticDkCpiCapFloorEngine : public CPICapFloor::engine {
public:
    AnalyticDkCpiCapFloorEngine(const boost::shared_ptr<CrossAssetModel>& model, const Size index, const Real baseCPI);
    void calculate() const;

private:
    const boost::shared_ptr<CrossAssetModel> model_;
    const Size index_;
    const Real baseCPI_;
};

} // namespace QuantExt

#endif
