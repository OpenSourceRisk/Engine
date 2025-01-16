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

#include <ored/marketdata/basecorrelationcurve.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>

#include <ored/portfolio/builders/cdo.hpp>
#include <ored/utilities/to_string.hpp>
#include <qle/instruments/makecds.hpp>
#include <qle/instruments/syntheticcdo.hpp>
#include <qle/math/flatextrapolation2d.hpp>
#include <qle/models/basket.hpp>
#include <qle/pricingengines/indexcdstrancheengine.hpp>
#include <qle/utilities/interpolation.hpp>
#include <qle/utilities/time.hpp>

using namespace QuantLib;
using namespace std;

namespace {

using std::string;

// Check that weight, prior Weight or recovery is in [0, 1]
void validateWeightRec(Real value, const string& name, const string& varName) {
    QL_REQUIRE(value <= 1.0,
               "The " << varName << " value (" << value << ") for name " << name << " should not be greater than 1.0.");
    QL_REQUIRE(value >= 0.0,
               "The " << varName << " value (" << value << ") for name " << name << " should not be less than 0.0.");
}

} // namespace

namespace ore {
namespace data {

BaseCorrelationCurve::QuoteData BaseCorrelationCurve::loadQuotes(const Date& asof,
                                                                 const BaseCorrelationCurveConfig& config,
                                                                 const Loader& loader) const {
    BaseCorrelationCurve::QuoteData res;
    const auto& termStrs = config.terms();
    QL_REQUIRE(!termStrs.empty(), "BaseCorrelationCurve: need at least one term.");

    bool termWc = find(termStrs.begin(), termStrs.end(), "*") != termStrs.end();
    if (termWc) {
        QL_REQUIRE(termStrs.size() == 1, "BaseCorrelationCurve: only one wild card term can be specified.");
        DLOG("Have term wildcard pattern " << termStrs[0]);
    } else {
        for (const string& termStr : termStrs) {
            res.terms.insert(ore::data::parsePeriod(termStr));
        }
        DLOG("Parsed " << res.terms.size() << " unique configured term(s).");
    }
    // Detachment points
    const vector<string>& dpStrs = config.detachmentPoints();
    bool dpsWc = find(dpStrs.begin(), dpStrs.end(), "*") != dpStrs.end();
    if (dpsWc) {
        QL_REQUIRE(dpStrs.size() == 1,
                   "BaseCorrelationCurve: only one wild card " << "detachment point can be specified.");
        DLOG("Have detachment point wildcard pattern " << dpStrs[0]);
    } else {
        for (const string& dpStr : dpStrs) {
            res.dps.insert(parseReal(dpStr));
        }
        DLOG("Parsed " << res.dps.size() << " unique configured detachment points.");
        QL_REQUIRE(res.dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
    }

    // Read in quotes relevant for the base correlation surface. The points that will be used are stored in data
    // where the key is the term, detachment point pair and value is the base correlation quote.
    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::CDS_INDEX << "/" << config.quoteType() << "/*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {

        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

        auto q = QuantLib::ext::dynamic_pointer_cast<BaseCorrelationQuote>(md);
        QL_REQUIRE(q, "Internal error: could not downcast MarketDatum '" << md->name() << "' to BaseCorrelationQuote");

        // Go to next quote if index name in the quote does not match the cds vol configuration name.
        if (config.quoteName() != q->cdsIndexName())
            continue;

        TLOG("Processing quote " << q->name() << ": (" << q->term() << "," << fixed << setprecision(9)
                                 << q->detachmentPoint() << "," << q->quote()->value() << ")");

        // If we have been given a list of explicit terms, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!termWc) {
            auto it = res.terms.find(q->term());
            if (it == res.terms.end())
                continue;
        }

        // If we have been given a list of explicit detachment points, check that the quote matches one of them.
        // Move to the next quote if it does not.
        if (!dpsWc) {
            auto it = res.dps.find(q->detachmentPoint());
            if (it == res.dps.end())
                continue;
        }

        // Skip if we have already added a quote for the given term and detachment point.
        auto key = make_pair(q->term(), q->detachmentPoint());
        if (res.data.find(key) != res.data.end()) {
            DLOG("Already added base correlation with term " << q->term() << " and detachment point " << fixed
                                                             << setprecision(9) << q->detachmentPoint()
                                                             << " so skipping quote " << q->name());
            continue;
        }

        // If we have wildcards, update the set so that dps and terms are populated.
        if (termWc)
            res.terms.insert(q->term());
        if (dpsWc)
            res.dps.insert(q->detachmentPoint());

        // Add to the data that we will use.
        res.data[key] = q->quote();

        TLOG("Added quote " << q->name() << ": (" << q->term() << "," << fixed << setprecision(9)
                            << q->detachmentPoint() << "," << q->quote()->value() << ")");
    }
    return res;
}

BaseCorrelationCurve::BaseCorrelationCurve(
    Date asof, BaseCorrelationCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
    const std::map<std::string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
    const std::map<std::string, QuantLib::ext::shared_ptr<DefaultCurve>>& creditCurves,
    const std::map<std::string, std::string>& creditNameMapping)
    : spec_(spec), yieldCurves_(yieldCurves), creditCurves_(creditCurves), creditNameMapping_(creditNameMapping),
      referenceData_(referenceData) {

    DLOG("BaseCorrelationCurve: start building base correlation structure with ID " << spec_.curveConfigID());

    try {
        const auto& config = *curveConfigs.baseCorrelationCurveConfig(spec_.curveConfigID());
        const auto qData = loadQuotes(asof, config, loader);

        if (config.quoteType() == MarketDatum::QuoteType::BASE_CORRELATION) {
            buildFromCorrelations(config, qData);
        } else if (config.quoteType() == MarketDatum::QuoteType::TRANCHE_UPFRONT) {
            buildFromUpfronts(asof, config, qData);
        } else {
            QL_FAIL("Unexpected quoteType, expect BASE_CORRELATION or TRANCHE_UPFRONT");
        }
    } catch (std::exception& e) {
        QL_FAIL("BaseCorrelationCurve: curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("BaseCorrelationCurve: curve building failed: unknown error");
    }

    DLOG("BaseCorrelationCurve: finished building base correlation structure with ID " << spec_.curveConfigID());
}

BaseCorrelationCurve::AdjustForLossResults
BaseCorrelationCurve::adjustForLosses(const vector<Real>& detachPoints) const {

    AdjustForLossResults results;
    const auto& cId = spec_.curveConfigID();

    const auto& [qualifier, period] = splitCurveIdWithTenor(cId);

    DLOG("BaseCorrelationCurve::adjustForLosses: start adjusting for losses for base correlation " << qualifier);

    if (!referenceData_) {
        DLOG("Reference data manager is null so cannot adjust for losses.");
        results.indexFactor = 1;
        results.adjDetachmentPoints = detachPoints;
        return results;
    }

    if (!referenceData_->hasData(CreditIndexReferenceDatum::TYPE, qualifier)) {
        DLOG("Reference data manager does not have index credit data for " << qualifier
                                                                           << " so cannot adjust for losses.");
        results.indexFactor = 1;
        results.adjDetachmentPoints = detachPoints;
        return results;
    }

    auto crd = QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(
        referenceData_->getData(CreditIndexReferenceDatum::TYPE, qualifier));
    if (!crd) {
        DLOG("Index credit data for " << qualifier << " is not of correct type so cannot adjust for losses.");
        results.indexFactor = 1;
        results.adjDetachmentPoints = detachPoints;
        return results;
    }

    // Process the credit index reference data
    Real totalRemainingWeight = 0.0;
    Real totalPriorWeight = 0.0;
    Real lost = 0.0;
    Real recovered = 0.0;

    for (const auto& c : crd->constituents()) {

        const auto& name = c.name();
        auto weight = c.weight();
        validateWeightRec(weight, name, "weight");

        if (!close(0.0, weight)) {
            totalRemainingWeight += weight;
            results.remainingNames.push_back(name);
            results.remainingWeights.push_back(weight);
        } else {
            auto priorWeight = c.priorWeight();
            QL_REQUIRE(priorWeight != Null<Real>(), "Expecting a valid prior weight for name " << name << ".");
            validateWeightRec(priorWeight, name, "prior weight");
            auto recovery = c.recovery();
            QL_REQUIRE(recovery != Null<Real>(), "Expecting a valid recovery for name " << name << ".");
            validateWeightRec(recovery, name, "recovery");
            lost += (1.0 - recovery) * priorWeight;
            recovered += recovery * priorWeight;
            totalPriorWeight += priorWeight;
        }
    }

    Real totalWeight = totalRemainingWeight + totalPriorWeight;
    TLOG("Total remaining weight = " << totalRemainingWeight);
    TLOG("Total prior weight = " << totalPriorWeight);
    TLOG("Total weight = " << totalWeight);

    results.indexFactor = totalRemainingWeight;
    results.adjDetachmentPoints = detachPoints;

    if (!close(totalRemainingWeight, 1.0) && totalRemainingWeight > 1.0) {
        ALOG("Total remaining weight is greater than 1, possible error in CreditIndexReferenceDatum for " << qualifier);
    }

    if (!close(totalWeight, 1.0)) {
        ALOG("Expected the total weight (" << totalWeight << " = " << totalRemainingWeight << " + " << totalPriorWeight
                                           << ") to equal 1, possible error in CreditIndexReferenceDatum for "
                                           << qualifier);
    }

    if (close(totalRemainingWeight, 0.0)) {
        ALOG("The total remaining weight is 0 so cannot adjust for losses.");
        return results;
    }

    if (close(totalRemainingWeight, 1.0)) {
        DLOG("Index factor for " << qualifier << " is 1 so adjustment for losses not required.");
        return results;
    }

    // Index factor is less than 1 so need to adjust each of the detachment points.
    vector<Real> adjDetachmentPoints;
    for (Size i = 0; i < detachPoints.size(); ++i) {

        // Amounts below, for tranche and above for original (quoted) attachment and detachment points.
        Real below = i == 0 ? 0.0 : detachPoints[i - 1];
        Real tranche = detachPoints[i] - below;
        Real above = 1.0 - detachPoints[i];
        Real newBelow = max(below - lost, 0.0);
        Real newTranche = tranche - max(min(recovered - above, tranche), 0.0) - max(min(lost - below, tranche), 0.0);
        Real newDetach = (newBelow + newTranche) / totalRemainingWeight;

        TLOG("Quoted detachment point " << detachPoints[i] << " adjusted to " << newDetach << ".");

        if (i > 0 && (newDetach < adjDetachmentPoints.back() || close(newDetach, adjDetachmentPoints.back()))) {
            ALOG("The " << io::ordinal(i + 1) << " adjusted detachment point is not greater than the previous "
                        << "adjusted detachment point so cannot adjust for losses.");
            return results;
        }

        adjDetachmentPoints.push_back(newDetach);
    }
    results.adjDetachmentPoints = adjDetachmentPoints;

    DLOG("BaseCorrelationCurve::adjustForLosses: finished adjusting for losses for base correlation " << qualifier);

    return results;
}

void BaseCorrelationCurve::buildFromCorrelations(const BaseCorrelationCurveConfig& config,
                                                 const QuoteData& qData) const {
    // The base correlation surface is of
    // the form term x detachment point. We
    // need at least two detachment points
    // and at least one term. The list of terms may be explicit or contain a single wildcard character '*'.
    // Similarly, the list of detachment points may be explicit or contain a single wildcard character '*'.
    const auto& terms = qData.terms;
    const auto& dps = qData.dps;
    const auto& data = qData.data;

    DLOG("After processing the quotes, we have " << terms.size() << " unique term(s), " << dps.size()
                                                 << " unique detachment points and " << data.size() << " quotes.");
    QL_REQUIRE(dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
    QL_REQUIRE(dps.size() * terms.size() == data.size(),
               "BaseCorrelationCurve: number of quotes ("
                   << data.size() << ") should equal number of detachment points (" << dps.size()
                   << ") times the number of terms (" << terms.size() << ").");

    vector<vector<Handle<Quote>>> quotes;

    // Need a vector of terms and detachment points for ctor below
    vector<Period> tmpTerms(terms.begin(), terms.end());
    vector<Real> tmpDps(dps.begin(), dps.end());

    if (config.indexTerm() != 0 * Days) {
        vector<Time> termTimes;
        for (auto const& h : tmpTerms) {
            termTimes.push_back(QuantExt::periodToTime(h));
        }

        Real t = QuantExt::periodToTime(config.indexTerm());
        Size termIndex_m, termIndex_p;
        Real alpha;
        std::tie(termIndex_m, termIndex_p, alpha) = QuantExt::interpolationIndices(termTimes, t);

        for (const auto& dp : dps) {
            quotes.push_back({});
            for (const auto& term : terms) {
                auto key_m = make_pair(tmpTerms[termIndex_m], dp);
                auto it_m = data.find(key_m);
                auto key_p = make_pair(tmpTerms[termIndex_p], dp);
                auto it_p = data.find(key_p);
                QL_REQUIRE(it_m != data.end() && it_p != data.end(),
                           "BaseCorrelationCurve: do not have a quote for term "
                               << term << " and detachment point " << fixed << setprecision(9) << dp << ".");
                quotes.back().push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(
                    alpha * it_m->second->value() + (1 - alpha) * it_p->second->value())));
            }
        }
    } else {

        // Need to now fill _completely_ the (number of dps) x (number of terms) quotes surface.

        for (const auto& dp : dps) {
            quotes.push_back({});
            for (const auto& term : terms) {
                auto key = make_pair(term, dp);
                auto it = data.find(key);
                QL_REQUIRE(it != data.end(), "BaseCorrelationCurve: do not have a quote for term "
                                                 << term << " and detachment point " << fixed << setprecision(9) << dp
                                                 << ".");
                quotes.back().push_back(it->second);
            }
        }
    }

    if (config.adjustForLosses()) {
        DLOG("Adjust for losses is true for base correlation " << spec_.curveConfigID());
        DLOG("Detachment points before: " << Array(tmpDps.begin(), tmpDps.end()));
        auto res = adjustForLosses(tmpDps);
        tmpDps = res.adjDetachmentPoints;
        DLOG("Detachment points after: " << Array(tmpDps.begin(), tmpDps.end()));
    }

    // The QuantLib interpolator expects at least two terms, so add a column, copying the last
    QL_REQUIRE(!tmpTerms.empty(), "No terms found");
    tmpTerms.push_back(tmpTerms.back() + 1 * tmpTerms.back().units());
    for (Size i = 0; i < tmpDps.size(); ++i)
        quotes[i].push_back(quotes[i][tmpTerms.size() - 2]);

    baseCorrelation_ =
        QuantLib::ext::make_shared<QuantExt::InterpolatedBaseCorrelationTermStructure<QuantExt::BilinearFlat>>(
            config.settlementDays(), config.calendar(), config.businessDayConvention(), tmpTerms, tmpDps, quotes,
            config.dayCounter(), config.startDate(), config.rule());

    baseCorrelation_->enableExtrapolation(config.extrapolate());
}

void BaseCorrelationCurve::buildFromUpfronts(const Date& asof, const BaseCorrelationCurveConfig& config,
                                             const QuoteData& qData) const {
    LOG("Building from upfronts" << config.curveID());
    const auto& terms = qData.terms;
    const auto& dps = qData.dps;
    const auto& data = qData.data;

    DLOG("After processing the quotes, we have " << terms.size() << " unique term(s), " << dps.size()
                                                 << " unique detachment points and " << data.size() << " quotes.");
    QL_REQUIRE(referenceData_ != nullptr,
               "can not imply base correlations from upfront, no reference data manager found");
    QL_REQUIRE(config.indexSpread() != Null<Real>(),
               "can not imply base correlations from upfronts, missing index spread in curve config");
    QL_REQUIRE(config.startDate() != Null<Date>(),
               "can not imply base correlations from upfronts, missing index start date in curve config");
    QL_REQUIRE(dps.size() > 1, "BaseCorrelationCurve: need at least 2 unique detachment points.");
    QL_REQUIRE(dps.size() * terms.size() == data.size(),
               "BaseCorrelationCurve: number of quotes ("
                   << data.size() << ") should equal number of detachment points (" << dps.size()
                   << ") times the number of terms (" << terms.size() << ").");

    QuoteData newQuoteData;
    newQuoteData.dps = dps;
    for (const auto& term : terms) {
        try{
        vector<Real> tmpDps(dps.begin(), dps.end());
        auto basketData = adjustForLosses(tmpDps);
        std::vector<std::pair<double, double>> attachPoints = {{0.0, 0.0}};
        std::vector<std::pair<double, double>> detachPoints;
        for (size_t i = 0; i < tmpDps.size(); ++i) {
            attachPoints.push_back(std::make_pair(tmpDps[i], basketData.adjDetachmentPoints[i]));
            detachPoints.push_back(std::make_pair(tmpDps[i], basketData.adjDetachmentPoints[i]));
        }
        attachPoints.pop_back();
        QL_REQUIRE(attachPoints.size() == dps.size(), "Mismatch size");
        QL_REQUIRE(detachPoints.size() == dps.size(), "Mismatch size");

        std::vector<double> trancheNpvs;

        std::string indexNameWithTerm = config.curveID() + "_" + to_string(term);
        auto mappedIndexCurveName = creditCurveNameMapping(indexNameWithTerm);
        auto indexCreditCurve = getDefaultProbCurveAndRecovery(mappedIndexCurveName);
        QL_REQUIRE(indexCreditCurve != nullptr, "can not imply base correlation from upfront, index cds curve for "
                                                    << indexNameWithTerm << " missing");

        const auto discountCurve = indexCreditCurve->rateCurve();
        const auto indexCurve = indexCreditCurve->curve();
        const auto indexRecovery = indexCreditCurve->recovery();
        QL_REQUIRE(!discountCurve.empty(), "can not imply base correlation curve from upfront, discount curve missing");
        QL_REQUIRE(!indexCurve.empty(),
                   "can not imply base correlation curve from upfronts, index credit curve missing");
        QL_REQUIRE(!indexRecovery.empty(),
                   "can not imply base correlation curve from upfronts, index recovery missing");

        auto pool = QuantLib::ext::make_shared<Pool>();
        std::vector<double> recoveryRates;
        Currency ccy = parseCurrency(config.currency());
        auto curveCalibration = ext::make_shared<QuantExt::IndexConstituentDefaultCurveCalibration>(
            config.startDate(), term, config.indexSpread(), indexRecovery, indexCurve, discountCurve);
        std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
        for (const auto& name : basketData.remainingNames) {
            auto mappedName = creditCurveNameMapping(name);
            const auto creditCurve = getDefaultProbCurveAndRecovery(mappedName);
            QL_REQUIRE(creditCurve != nullptr, "buildFromUpfronts, credit curve for " << name << " missing");
            recoveryRates.push_back(creditCurve->recovery()->value());
            dpts.push_back(creditCurve->curve());
        }

        auto calibrationResults = curveCalibration->calibratedCurves(basketData.remainingWeights, dpts, recoveryRates);

        LOG("Maturity CalibrationFacotr MarketNPV ImpliedNPV Error");
        LOG("Expiry;CalibrationFactor;MarketNpv;ImpliedNpv;Error");
        for (size_t i = 0; i < calibrationResults.cdsMaturity.size(); ++i) {
            LOG(io::iso_date(calibrationResults.cdsMaturity[i])
                << ";" << std::fixed << std::setprecision(8) << calibrationResults.calibrationFactor[i] << ";"
                << calibrationResults.marketNpv[i] << ";" << calibrationResults.impliedNpv[i] << ";"
                << calibrationResults.marketNpv[i] - calibrationResults.impliedNpv[i]);
        }
        auto uncalibratedCurve = dpts.front();
        auto debugCurve = calibrationResults.curves.front();
        
        for(const auto& time : {0.5, 1., 1.5, 2., 3.5, 4., 5., 6.}){
            LOG("Time: " << time << " Uncalibrated: " << uncalibratedCurve->defaultProbability(time, true) << " Calibrated: " << debugCurve->defaultProbability(time, true));
            auto hazardRateUncalibrated = uncalibratedCurve->hazardRate(time, true);
            auto hazardRateCalibrated = debugCurve->hazardRate(time, true);
            LOG("Time: " << time << " Uncalibrated: " << hazardRateUncalibrated << " Calibrated: " << hazardRateCalibrated << " alpha " << hazardRateCalibrated / hazardRateUncalibrated);
        }


        

        for (size_t i = 0; i < basketData.remainingNames.size(); i++) {
            std::string name = basketData.remainingNames[i];
            Handle<DefaultProbabilityTermStructure> curve =
                calibrationResults.success ? calibrationResults.curves[i] : dpts[i];
            DefaultProbKey key = NorthAmericaCorpDefaultKey(ccy, SeniorSec, Period(), 1.0);
            std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>> p(key, curve);
            vector<pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities(1, p);
            // Empty default set. Adjustments have been made above to account for existing credit events.
            Issuer issuer(probabilities, DefaultEventSet());
            pool->add(name, issuer, key);
        }

        std::vector<double> trancheNPV;
        std::vector<double> baseCorrelations;

        Schedule schedule = MakeSchedule()
                                .from(config.startDate())
                                .to(cdsMaturity(config.startDate(), term, DateGeneration::CDS2015))
                                .withTenor(3 * Months)
                                .withCalendar(WeekendsOnly())
                                .withConvention(Unadjusted)
                                .withTerminationDateConvention(Unadjusted)
                                .withRule(DateGeneration::CDS2015);

        for (size_t i = 0; i < dps.size(); i++) {
            double inceptionAttachPoint = attachPoints[i].first;
            double adjustedAttachPoint = attachPoints[i].second;
            double inceptionDetachPoint = detachPoints[i].first;
            double adjustedDetachPoint = detachPoints[i].second;
            double trancheWidth = (adjustedDetachPoint - adjustedAttachPoint) * basketData.indexFactor;
            double inceptionTrancheWidth = (inceptionDetachPoint - inceptionAttachPoint);
            double previousTrancheCleanNPV = (i == 0) ? 0 : trancheNPV.back();
            //LOG("previous Tranche clean NPV" << previousTrancheCleanNPV);
            // Build Tranche
            auto baseCorrelQuote = QuantLib::ext::make_shared<SimpleQuote>(0.5);
            RelinkableHandle<Quote> baseCorrelation = RelinkableHandle<Quote>(baseCorrelQuote);
            GaussCopulaBucketingLossModelBuilder modelBuilder{
                -5., 5, 64, false, 372, false, true, {0.35, 0.3, 0.35}, "Markit2020"};
            auto lossModel = modelBuilder.lossModel(recoveryRates, baseCorrelation, false);

            auto basket = QuantLib::ext::make_shared<QuantExt::Basket>(
                config.startDate(), basketData.remainingNames, basketData.remainingWeights, pool, 0.0,
                adjustedDetachPoint, QuantLib::ext::shared_ptr<Claim>(new FaceValueClaim()));
            // Build model with model builder
            basket->setLossModel(lossModel);

            auto cdo = ext::make_shared<QuantExt::SyntheticCDO>(
                basket, Protection::Side::Buyer, schedule, 0.0, config.indexSpread(), Actual360(), Following, true,
                QuantLib::CreditDefaultSwap::ProtectionPaymentTime::atDefault, asof, Date(), boost::none, Null<Real>(),
                Actual360(true));

            auto pricingEngine = QuantLib::ext::make_shared<QuantExt::IndexCdsTrancheEngine>(discountCurve);
            cdo->setPricingEngine(pricingEngine);
            auto key = make_pair(term, inceptionDetachPoint);
            auto it = data.find(key);
            double mktUpfront = it->second->value();
            auto targetFunction = [&cdo, &mktUpfront, &baseCorrelQuote, &previousTrancheCleanNPV,
                                   &trancheWidth](const double correlation) {
                baseCorrelQuote->setValue(correlation);
                double implyUpfront = (cdo->cleanNPV() - previousTrancheCleanNPV) / trancheWidth;
                return mktUpfront - implyUpfront;
            };

            Brent solver;

            double targetCorrelation = baseCorrelations.empty() ? 0.5 : baseCorrelations.back();
            if (inceptionDetachPoint < 1.0 || !close_enough(inceptionDetachPoint, 1.0)) {
                targetCorrelation = solver.solve(targetFunction, 0.000001, 0.5,
                    0 + QL_EPSILON, 1 - QL_EPSILON);
                baseCorrelations.push_back(targetCorrelation);
            }
            double error = targetFunction(targetCorrelation);
            double impliedUpfront = (cdo->cleanNPV() - previousTrancheCleanNPV) / trancheWidth;
            LOG("CurveId" << "," << "term" << "," << "inceptionAttachPoint" << "," << "inceptionDetachPoint" << "," << "inceptionTrancheWidth" << ","
                            << "adjustedAttachPoint" << "," << "adjustedDetachPoint" << "," << "trancheWidth"
                            << "," << "mktUpfront" << "," << "impliedUpfront"  << "," << "error"<< "," << "targetCorrelation");
            LOG(config.curveID() << "," << term << "," << inceptionAttachPoint << "," << inceptionDetachPoint << "," << inceptionTrancheWidth << ","
                << adjustedAttachPoint << "," << adjustedDetachPoint << "," << trancheWidth
                << "," << mktUpfront << "," << impliedUpfront  << "," << error<< "," << targetCorrelation);
            trancheNPV.push_back(cdo->cleanNPV());
            newQuoteData.data[key] = Handle<Quote>(ext::make_shared<SimpleQuote>(targetCorrelation));
        }
        newQuoteData.terms.insert(term);
        
        
        baseCorrelations.push_back(1.0);
        } catch (std::exception& e) {
            ALOG("Error building base correlation curve from upfronts for term " << term << ": " << e.what());
        } catch (...) {
            ALOG("Error building base correlation curve from upfronts for term " << term << ": unknown error");
        }
    }

    buildFromCorrelations(config, newQuoteData);
}

} // namespace data
} // namespace ore
