/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <ored/portfolio/builders/cbo.hpp>

#include <qle/pricingengines/cboengine.hpp>
#include <qle/pricingengines/cbomcengine.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/experimental/credit/onefactorgaussiancopula.hpp>
#include <ql/experimental/credit/randomdefaultmodel.hpp>

namespace oreplus {
namespace data {

static DefaultProbKey dummyDefaultProbKey() {
    Currency currency = QuantLib::EURCurrency();
    Seniority seniority = NoSeniority;
    boost::shared_ptr<DefaultType> defaultType(new DefaultType());
    vector<boost::shared_ptr<DefaultType>> defaultTypes(1, defaultType);
    DefaultProbKey key(defaultTypes, currency, seniority);
    return key;
}

boost::shared_ptr<PricingEngine> CboMCEngineBuilder::engine(const boost::shared_ptr<Pool>& pool) {

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

    boost::shared_ptr<SimpleQuote> correlationQuote(new SimpleQuote(corr));
    Handle<Quote> correlationHandle = Handle<Quote>(correlationQuote);
    boost::shared_ptr<OneFactorCopula> gaussianCopula(new OneFactorGaussianCopula(correlationHandle));
    RelinkableHandle<OneFactorCopula> copula(gaussianCopula);

    vector<DefaultProbKey> keys(pool->size(), dummyDefaultProbKey());

    boost::shared_ptr<RandomDefaultModel> rdm(new GaussianRandomDefaultModel(pool, keys, copula, 1.e-6, seed));

    return boost::make_shared<QuantExt::MonteCarloCBOEngine>(rdm, samples, bins, errorTolerance,
                                                             lossDistributionPeriods);
};

} // namespace data
} // namespace oreplus
