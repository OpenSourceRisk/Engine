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

/*! \file commodityschwartzfutureoptionengine.hpp
    \brief commodity future options priced in the Schwartz model
    \ingroup engines
*/

#ifndef quantext_com_schwartz_futureoption_engine_hpp
#define quantext_com_schwartz_futureoption_engine_hpp

#include <ql/instruments/vanillaoption.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/commodityschwartzmodel.hpp>

namespace QuantExt {

//! Commodity options on prompt future (with maturity=expiry) priced in the Schwartz model
/*! \ingroup engines
 */
class CommoditySchwartzFutureOptionEngine : public VanillaOption::engine {
public:
    CommoditySchwartzFutureOptionEngine(const QuantLib::ext::shared_ptr<CommoditySchwartzModel>& model);
    void calculate() const override;

private:
    QuantLib::ext::shared_ptr<CommoditySchwartzModel> model_;
};

} // namespace QuantExt

#endif
