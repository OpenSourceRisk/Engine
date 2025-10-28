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

#pragma once
#include <orea/engine/saccrcalculator.hpp>
#include <orea/engine/saccrtradedata.hpp>
#include <ored/utilities/timer.hpp>

namespace ore {
namespace analytics {

//! Class for calculating Basic Approach CVA capital charge
class BaCvaCalculator {
public:
    BaCvaCalculator(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio, const QuantLib::ext::shared_ptr<ore::data::NettingSetManager>& nettingSetManager,
                    const QuantLib::ext::shared_ptr<ore::data::CounterpartyManager>& counterpartyManager,
                    const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& calculationCcy,
                    const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& collateralBalances,
                    const QuantLib::ext::shared_ptr<ore::data::CollateralBalances>& calculatedCollateralBalances,
                    const QuantLib::ext::shared_ptr<SimmBasicNameMapper>& nameMapper,
                    const QuantLib::ext::shared_ptr<SimmBucketMapper>& bucketMapper,
                    const QuantLib::ext::shared_ptr<ReferenceDataManager>& refDataManager = nullptr,
        QuantLib::Real rho = 0.5, QuantLib::Real alpha = 1.4, QuantLib::Real discount = 0.65);

    BaCvaCalculator(const QuantLib::ext::shared_ptr<SaccrCalculator>& saccrCalculator,
                    const QuantLib::ext::shared_ptr<SaccrTradeData>& saccrTradeData, const std::string& calculationCcy,
                    QuantLib::Real rho = 0.5, QuantLib::Real alpha = 1.4);

    void calculate();

    QuantLib::Real effectiveMaturity(std::string nettingSet);
    QuantLib::Real discountFactor(std::string nettingSet);
    QuantLib::Real counterpartySCVA(std::string counterparty);
    QuantLib::Real EAD(std::string nettingSet) { return saccrCalculator_->EAD(nettingSet); }
    map<string, set<string>> counterpartyNettingSets() { return counterpartyNettingSets_; }
    QuantLib::Real riskWeight(std::string counterparty);
    const ore::data::Timer& timer() const { return timer_; }

    //! Give back the CVA results container for the given \p portfolioId
    QuantLib::Real cvaResult() const { return cvaResult_; }

    //! Return the calculator's calculation currency
    const std::string& calculationCurrency() const { return calculationCcy_; }

protected:
    void calculateEffectiveMaturity();

private:
    QuantLib::ext::shared_ptr<SaccrCalculator> saccrCalculator_;
    QuantLib::ext::shared_ptr<SaccrTradeData> saccrTradeData_;
    
    QuantLib::Real rho_ = 0.5;
    QuantLib::Real alpha_ = 1.4;
    QuantLib::Real discount_ = 0.65;

    // hold intermediary results
    std::map<std::string, QuantLib::Real> effectiveMaturityMap_;
    std::map<std::string, QuantLib::Real> counterpartySCVA_;
    std::map<std::string, std::set<std::string>> counterpartyNettingSets_;
    std::map<std::string, QuantLib::Real> riskWeights_;
    std::map<std::string, QuantLib::Real> discountFactor_;

    ore::data::Timer timer_;

    //! The SIMM calculation currency i.e. the currency of the SIMM results
    std::string calculationCcy_;

    //! Container with a CvaResults object for each portfolio ID
    QuantLib::Real cvaResult_;
};
} // analytics
} // ore
