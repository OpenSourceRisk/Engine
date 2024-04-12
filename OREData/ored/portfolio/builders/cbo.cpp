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

#include <ored/portfolio/builders/cbo.hpp>

#include <qle/pricingengines/cboengine.hpp>
#include <qle/pricingengines/cbomcengine.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/experimental/credit/onefactorgaussiancopula.hpp>
#include <ql/experimental/credit/randomdefaultmodel.hpp>

namespace ore {
namespace data {

static DefaultProbKey dummyDefaultProbKey() {
    Currency currency = QuantLib::EURCurrency();
    Seniority seniority = NoSeniority;
    QuantLib::ext::shared_ptr<DefaultType> defaultType(new DefaultType());
    vector<QuantLib::ext::shared_ptr<DefaultType>> defaultTypes(1, defaultType);
    DefaultProbKey key(defaultTypes, currency, seniority);
    return key;
}

QuantLib::ext::shared_ptr<PricingEngine> CboMCEngineBuilder::engine(const QuantLib::ext::shared_ptr<Pool>& pool) {

    // get parameter
    Size samples = parseInteger(engineParameter("Samples"));
    Size bins = parseInteger(engineParameter("Bins"));
    long seed = parseInteger(engineParameter("Seed"));
    double corr = parseReal(engineParameter("Correlation"));

    double errorTolerance = parseReal(engineParameter("ErrorTolerance", {}, false, "1.0e-6"));

    string lossDistributionPeriods_str = engineParameter("LossDistributionPeriods");
    std::vector<string> lossDistributionPeriods_vec = parseListOfValues(lossDistributionPeriods_str);
    std::vector<QuantLib::Period> lossDistributionPeriods;
    for (Size i = 0; i < lossDistributionPeriods_vec.size(); i++)
        lossDistributionPeriods.push_back(parsePeriod(lossDistributionPeriods_vec[i]));

    QuantLib::ext::shared_ptr<SimpleQuote> correlationQuote(new SimpleQuote(corr));
    Handle<Quote> correlationHandle = Handle<Quote>(correlationQuote);
    QuantLib::ext::shared_ptr<OneFactorCopula> gaussianCopula(new OneFactorGaussianCopula(correlationHandle));
    RelinkableHandle<OneFactorCopula> copula(gaussianCopula);

    vector<DefaultProbKey> keys(pool->size(), dummyDefaultProbKey());

    QuantLib::ext::shared_ptr<RandomDefaultModel> rdm(new GaussianRandomDefaultModel(pool, keys, copula, 1.e-6, seed));

    return QuantLib::ext::make_shared<QuantExt::MonteCarloCBOEngine>(rdm, samples, bins, errorTolerance,
                                                             lossDistributionPeriods);
};

} // namespace data
} // namespace ore
