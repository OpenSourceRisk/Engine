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

/*! \file qle/pricingengines/indexcdstrancheengine.hpp
    \brief Index CDS tranche pricing engine
*/

#ifndef quantext_index_cds_tranche_engine_hpp
#define quantext_index_cds_tranche_engine_hpp

#include <ql/qldefines.hpp>

#ifndef QL_PATCH_SOLARIS

#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/syntheticcdo.hpp>

namespace QuantExt {

/*! Index tranche pricing engine

    The engine obtains the index CDS reference basket from its arguments and it is expecting it to have a default 
    model assigned.

    This engine prices standard index CDS tranches. The mechanics of such tranches is outlined in <em>Markit Credit 
    Indices A Primer, 2014</em> for example available on the Markit website.

    \warning We do not cover the possibility that recovery amounts decrease the tranche notional on which the premium 
             is paid. For tranche detachment points met in practice, it is rare that recovery amounts exceed the 
             notional of the super-senior tranche and thus erode the notional of the other tranches. If we want to 
             cover this possibility we would need to extend the basket loss model algorithms so that they account for 
             losses on a tranche notional due to recovery amounts in addition to the losses due to default. In 
             summary, do not expect this pricing engine to work well for tranches with high detachment points which 
             are likely to be breached by the sum of recovered amounts as the premium leg will be over-estimated in 
             those situations.
*/
class IndexCdsTrancheEngine : public QuantExt::SyntheticCDO::engine {
public:
    IndexCdsTrancheEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
        boost::optional<bool> includeSettlementDateFlows = boost::none);

    void calculate() const override;

protected:
    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
};

}

#endif

#endif
