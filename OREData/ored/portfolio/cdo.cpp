/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/cdo.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/cdo.hpp>

#include <qle/instruments/indexcreditdefaultswap.hpp>
#include <qle/instruments/syntheticcdo.hpp>
#include <qle/models/inhomogeneouspooldef.hpp>
#include <qle/pricingengines/midpointcdoengine.hpp>

#include <ored/utilities/marketdata.hpp>
#include <ored/portfolio/legdata.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <algorithm>
#include <iterator>
#include <qle/pricingengines/midpointindexcdsengine.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <qle/termstructures/interpolatedhazardratecurve.hpp>
#include <qle/termstructures/interpolatedsurvivalprobabilitycurve.hpp>
#include <qle/termstructures/multisectiondefaultcurve.hpp>
#include <qle/termstructures/spreadedsurvivalprobabilitytermstructure.hpp>
#include <qle/termstructures/survivalprobabilitycurve.hpp>
#include <qle/utilities/time.hpp>
#include <ored/utilities/to_string.hpp>

using namespace QuantLib;
using namespace QuantExt;

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

void SyntheticCDO::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("SyntheticCDO::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Index Tranche");
    QuantLib::ext::shared_ptr<ReferenceDataManager> refData = engineFactory->referenceData();
    if (refData && refData->hasData("CreditIndex", qualifier_)) {
        auto refDatum = refData->getData("CreditIndex", qualifier_);
        QuantLib::ext::shared_ptr<CreditIndexReferenceDatum> creditIndexRefDatum =
            QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(refDatum);
        additionalData_["isdaSubProduct"] = creditIndexRefDatum->indexFamily();
        if (creditIndexRefDatum->indexFamily() == "") {
            ALOG("IndexFamily is blank in credit index reference data for entity " << qualifier_);
        }
    } else {
        ALOG("Credit index reference data missing for entity " << qualifier_ << ", isdaSubProduct left blank");
    }
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    Date protectionStartDate = protectionStart_ == "" ? Date() : parseDate(protectionStart_);
    Date upfrontDate = upfrontDate_ == "" ? Date() : parseDate(upfrontDate_);
    Leg leg = makeFixedLeg(legData_);
    Protection::Side side = legData_.isPayer() ? Protection::Buyer : Protection::Seller;
    Schedule schedule = makeSchedule(legData_.schedule());
    Real fixedRecovery = recoveryRate_;
    auto fixedLegData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(legData_.concreteLegData());
    QL_REQUIRE(fixedLegData, "Expected FixedLegData but got " << legData_.legType());
    Real runningRate = fixedLegData->rates().front();
    DayCounter dayCounter = parseDayCounter(legData_.dayCounter());
    BusinessDayConvention bdc = parseBusinessDayConvention(legData_.paymentConvention());
    Currency ccy = parseCurrency(legData_.currency());

    // In general for CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    Actual360 standardDayCounter;
    DayCounter lastPeriodDayCounter = dayCounter == standardDayCounter ? Actual360(true) : dayCounter;


    // Set some trade variables
    npvCurrency_ = legData_.currency();
    maturity_ = leg.back()->date();
    notionalCurrency_ = legData_.currency();
    legs_ = {leg};
    legPayers_ = {legData_.isPayer()};
    legCurrencies_ = {legData_.currency()};

    // Checks for upfront date and upfront fee
    QL_REQUIRE(upfrontDate == Date() || upfrontFee_ != Null<Real>(),
               "If upfront date is given (" << upfrontDate << "), upfront fee must be given.");
    QL_REQUIRE(upfrontDate != Date() || upfrontFee_ == Null<Real>() || close_enough(upfrontFee_, 0.0),
               "If no upfront date is given, no upfront fee should be given but got " << upfrontFee_ << ".");

    // Get the original total notional using the contractual attachment point and detachment point and the contractual
    // tranche notional. We will calculate the corresponding current amounts below from the basket or reference data.
    QL_REQUIRE(attachmentPoint_ < detachmentPoint_, "Detachment point should be greater than attachment point.");
    Real origTrancheNtl = legData_.notionals().front();
    Real origTotalNtl = origTrancheNtl / (detachmentPoint_ - attachmentPoint_);
    Real origEquityNtl = origTotalNtl * attachmentPoint_;
    Real origSeniorNtl = origTotalNtl * (1.0 - detachmentPoint_);

    DLOG("Original tranche notional: " << origTrancheNtl);
    DLOG("Original equity notional:  " << origEquityNtl);
    DLOG("Original senior notional:  " << origSeniorNtl);
    DLOG("Original attachment point: " << attachmentPoint_);
    DLOG("Original detachment point: " << detachmentPoint_);
    DLOG("Original total notional:   " << origTotalNtl);

    // There may have been credit events up to the valuation date. Record the cumulative lost and recovered notional.
    Real lostNotional = 0.0;
    Real recoveredNotional = 0.0;
    // clear the basketNotionals and credit names

    vector<Real> basketNotionals;

    vector<string> creditCurves;

    

    if (basketData_.constituents().size() > 0) {

        const auto& constituents = basketData_.constituents();
        DLOG("Building constituents from basket data containing " << constituents.size() << " elements.");

        Real totalRemainingNtl = 0.0;
        Real totalPriorNtl = 0.0;

        for (const auto& c : constituents) {
            Real ntl = Null<Real>(), priorNotional = Null<Real>();
            const auto& creditCurve = c.creditCurveId();
            if (c.weightInsteadOfNotional()) {
                ntl = c.weight() * origTotalNtl;
                priorNotional = c.priorWeight();
                if (priorNotional != Null<Real>()) {
                    priorNotional *= origTotalNtl;
                }
            } else {
                ntl = c.notional();
                priorNotional = c.priorNotional();
                QL_REQUIRE(c.currency() == npvCurrency_, "The currency of basket constituent "
                                                             << creditCurve << " is " << c.currency()
                                                             << " and does not equal the trade leg currency "
                                                             << npvCurrency_);
            }

            if (!close(0.0, ntl) && ntl > 0.0) {
                if (std::find(creditCurves.begin(), creditCurves.end(), creditCurve) == creditCurves.end()) {
                    DLOG("Adding underlying " << creditCurve << " with notional " << ntl);
                    creditCurves.push_back(creditCurve);
                    basketNotionals.push_back(ntl);
                    totalRemainingNtl += ntl;
                } else {
                    StructuredTradeErrorMessage(id(), "Synthetic CDO", "Error building trade",
                                                     ("Invalid Basket: found a duplicate credit curve " + creditCurve +
                                                      ", skip it. Check the basket data for possible errors.")
                                                    .c_str())
                        .log();
                }
            } else {
                DLOG("Underlying " << creditCurve << " notional is " << ntl << " so assuming a credit event occured.");
                QL_REQUIRE(priorNotional != Null<Real>(),
                           "Expecting a valid prior notional for name " << creditCurve << ".");
                auto recovery = c.recovery();
                QL_REQUIRE(recovery != Null<Real>(), "Expecting a valid recovery for name " << creditCurve << ".");
                validateWeightRec(recovery, creditCurve, "recovery");
                lostNotional += (1.0 - recovery) * priorNotional;
                recoveredNotional += recovery * priorNotional;
                totalPriorNtl += priorNotional;
            }
        }

        Real totalNtl = totalRemainingNtl + totalPriorNtl;
        DLOG("All Underlyings added, total remaining notional = " << totalRemainingNtl);
        DLOG("All Underlyings added, total prior notional = " << totalPriorNtl);
        DLOG("All Underlyings added, total notional = " << totalNtl);

        QL_REQUIRE(creditCurves.size() == basketNotionals.size(), "numbers of defaults curves ("
                                                                      << creditCurves.size() << ") and notionals ("
                                                                      << basketNotionals.size() << ") doesnt match");
        Real notionalCorrectionFactor = origTotalNtl / totalNtl;
        // Scaling to Notional if relative error is close less than 10^-4
        if (!close(totalNtl, origTotalNtl) && (abs(notionalCorrectionFactor - 1.0) <= 1e-4)) {
            ALOG("Trade " << id() << ", sum of notionals(" << totalNtl << ") is very close to total original notional ("
                          << origTotalNtl << "), will scale each notional by " << notionalCorrectionFactor
                          << ",  check the basket data for possible errors.");
            totalRemainingNtl = 0;
            for (Size i = 0; i < basketNotionals.size(); i++) {
                Real scaledNotional = basketNotionals[i] * notionalCorrectionFactor;
                TLOG("Trade " << id() << ", Issuer" << creditCurves[i] << " unscaled Notional: " << basketNotionals[i]
                              << ", scaled Notional: " << scaledNotional);
                basketNotionals[i] = scaledNotional;
                totalRemainingNtl += scaledNotional;
            }
            lostNotional *= notionalCorrectionFactor;
            recoveredNotional *= notionalCorrectionFactor;
            totalNtl *= notionalCorrectionFactor;
        }

        if (!close(totalRemainingNtl, origTotalNtl) && totalRemainingNtl > origTotalNtl) {
            StructuredTradeErrorMessage(id(), "Synthetic CDO", "Error building trade",
                                             ("Total remaining notional (" + std::to_string(totalRemainingNtl) +
                                              ") is greater than total original notional (" +
                                              std::to_string(origTotalNtl) +
                                              "),  check the basket data for possible errors.")
                                            .c_str())
                .log();
        }

        if (!close(totalNtl, origTotalNtl)) {
            StructuredTradeErrorMessage(id(), "Synthetic CDO", "Error building trade",
                                            ("Expected the total notional (" + std::to_string(totalNtl) + " = " +
                                             std::to_string(totalRemainingNtl) + " + " + std::to_string(totalPriorNtl) +
                                             ") to equal the total original notional (" + std::to_string(origTotalNtl) +
                                             "),  check the basket data for possible errors.")
                                            .c_str())
                .log();
        }

        DLOG("Finished building constituents using basket data.");

    } else {

        DLOG("Building constituents using CreditIndexReferenceDatum for ID " << qualifier_);

        QL_REQUIRE(engineFactory->referenceData(),
                   "Trade " << id() << " has no basket data and there is no reference data manager.");
        QL_REQUIRE(engineFactory->referenceData()->hasData(CreditIndexReferenceDatum::TYPE, qualifier_),
                   "Trade " << id() << " needs credit index reference data for ID " << qualifier_);
        auto crd = QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(
            engineFactory->referenceData()->getData(CreditIndexReferenceDatum::TYPE, qualifier_));

        Real totalRemainingWeight = 0.0;
        Real totalPriorWeight = 0.0;
        for (const auto& c : crd->constituents()) {

            const auto& name = c.name();
            auto weight = c.weight();
            validateWeightRec(weight, name, "weight");

            if (!close(0.0, weight)) {
                DLOG("Adding underlying " << name << " with weight " << weight);
                creditCurves.push_back(name);
                basketNotionals.push_back(weight * origTotalNtl);
                totalRemainingWeight += weight;
            } else {
                DLOG("Underlying " << name << " has weight " << weight << " so assuming a credit event occured.");
                auto priorWeight = c.priorWeight();
                QL_REQUIRE(priorWeight != Null<Real>(), "Expecting a valid prior weight for name " << name << ".");
                validateWeightRec(priorWeight, name, "prior weight");
                auto recovery = c.recovery();
                QL_REQUIRE(recovery != Null<Real>(), "Expecting a valid recovery for name " << name << ".");
                validateWeightRec(recovery, name, "recovery");
                lostNotional += (1.0 - recovery) * priorWeight * origTotalNtl;
                recoveredNotional += recovery * priorWeight * origTotalNtl;
                totalPriorWeight += priorWeight;
            }
        }

        Real totalWeight = totalRemainingWeight + totalPriorWeight;
        DLOG("All Underlyings added, total remaining weight = " << totalRemainingWeight);
        DLOG("All Underlyings added, total prior weight = " << totalPriorWeight);
        DLOG("All Underlyings added, total weight = " << totalWeight);

        if (!close(totalRemainingWeight, 1.0) && totalRemainingWeight > 1.0) {
            ALOG("Total remaining weight is greater than 1, possible error in CreditIndexReferenceDatum");
        }

        if (!close(totalWeight, 1.0)) {
            ALOG("Expected the total weight (" << totalWeight << " = " << totalRemainingWeight << " + "
                                               << totalPriorWeight
                                               << ") to equal 1, possible error in CreditIndexReferenceDatum");
        }

        DLOG("Finished building constituents using CreditIndexReferenceDatum for ID " << qualifier_);
    }

    // Lost notional eats into junior tranches and recovered amount reduces senior tranches.
    // Throw exception if tranche has been completely written down. Should be removed from portfolio or will be
    // ignored if continue on error is true. Maybe better to have instrument that gives an NPV of 0.
    Real currTotalNtl = accumulate(basketNotionals.begin(), basketNotionals.end(), 0.0);
    QL_REQUIRE(!close(currTotalNtl, 0.0), "Trade " << id() << " has a current total notional of 0.0.");
    Real currEquityNtl = max(origEquityNtl - lostNotional, 0.0);
    Real currSeniorNtl = max(origSeniorNtl - recoveredNotional, 0.0);
    Real currTrancheNtl = origTrancheNtl - max(min(recoveredNotional - origSeniorNtl, origTrancheNtl), 0.0) -
                          max(min(lostNotional - origEquityNtl, origTrancheNtl), 0.0);
    QL_REQUIRE(!close(currTrancheNtl, 0.0), "Trade " << id() << " has a current tranche notional of 0.0.");
    Real adjAttachPoint = currEquityNtl / currTotalNtl;
    Real adjDetachPoint = (currEquityNtl + currTrancheNtl) / currTotalNtl;
    notional_ = currTrancheNtl;

    DLOG("Current tranche notional: " << currTrancheNtl);
    DLOG("Current equity notional:  " << currEquityNtl);
    DLOG("Current senior notional:  " << currSeniorNtl);
    DLOG("Current attachment point: " << adjAttachPoint);
    DLOG("Current detachment point: " << adjDetachPoint);
    DLOG("Current total notional:   " << currTotalNtl);

    
    const auto& market = engineFactory->market();
    auto cdoEngineBuilder = QuantLib::ext::dynamic_pointer_cast<CdoEngineBuilder>(engineFactory->builder("SyntheticCDO"));
    QL_REQUIRE(cdoEngineBuilder, "Trade " << id() << " needs a valid CdoEngineBuilder.");
    const string& config = cdoEngineBuilder->configuration(MarketContext::pricing);
    
    
    std::vector<Handle<DefaultProbabilityTermStructure>> dpts;
    vector<Real> recoveryRates;

    if (fixedRecovery != Null<Real>()) {
        LOG("Set all recovery rates to " << fixedRecovery);
    } 
    for (Size i = 0; i < creditCurves.size(); ++i) {
        const string& cc = creditCurves[i];
        Real mktRecoveryRate = market->recoveryRate(cc, config)->value();
        recoveryRates.push_back(fixedRecovery != Null<Real>() ? fixedRecovery : mktRecoveryRate);
        auto originalCurve = market->defaultCurve(cc, config)->curve();
        dpts.push_back(originalCurve);
    }

    // TODO check if underlying is index cds

    // Calibrate the underlying constituent curves so that the index cds pricing with underlying curves matches the
    // prices of the index cds with flat index curve.
    
    QuantLib::ext::shared_ptr<SimpleQuote> calibrationFactor = QuantLib::ext::make_shared<SimpleQuote>(1.0);

    bool calibrateConstiuentCurves = cdoEngineBuilder->calibrateConstituentCurve() && isIndexTranche();

    if (calibrateConstiuentCurves) {
        // Adjustment factor is a simplified version of the O'Kane's Forward Default Probability Multiplier 
        // O'Kane 2008 - Modelling Single-name and Multi-name Credit Derivatives
        // Chapter 10.6
        LOG("Use calibrated constiuent curves");
        QuantLib::ext::shared_ptr<IndexCreditDefaultSwap> indexCDS;
        Real cdsFairSpreads = 0.0;
        Real cdsNpvs = 0.0;
        try {
            Handle<YieldTermStructure> yts =
                market->discountCurve(ccy.code(), cdoEngineBuilder->configuration(MarketContext::pricing));

            Date cdsStartDate = indexStartDateHint() == Date() ? schedule.dates().front() : indexStartDateHint();
            
            Schedule cdsSchedule(cdsStartDate, schedule.dates().back(),
                                 schedule.tenor(), schedule.calendar(), Following,
                                 schedule.terminationDateBusinessDayConvention(), schedule.rule(), false);
            indexCDS = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
                side, currTotalNtl, basketNotionals, 0.0, runningRate, cdsSchedule, bdc, dayCounter, settlesAccrual_,
                protectionPaymentTime_, Date(), Date(), QuantLib::ext::shared_ptr<Claim>(),
                lastPeriodDayCounter, rebatesAccrual_);
            Handle<DefaultProbabilityTermStructure> indexCreditCurve =
                indexCdsDefaultCurve(market, creditCurveIdWithTerm(), config)->curve();
            Handle<Quote> indexCdsRecovery = market->recoveryRate(creditCurveIdWithTerm(), config);
            auto indexPricingEngine =
                QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(indexCreditCurve, indexCdsRecovery->value(), yts);
            indexCDS->setPricingEngine(indexPricingEngine);
        } catch (const std::exception& e) {
            indexCDS = nullptr;
            WLOG("CDO constiuent calibration failed to build index cds.  Got "
                                                                     << e.what());
        }
        if (indexCDS) {
            cdsFairSpreads = indexCDS->fairSpreadClean();
            cdsNpvs = indexCDS->NPV();
        }

        auto it = engineFactory->engineData()->globalParameters().find("RunType");
        if (it != engineFactory->engineData()->globalParameters().end() && it->second != "PortfolioAnalyser") {
            
            Handle<YieldTermStructure> yts =
                market->discountCurve(ccy.code(), cdoEngineBuilder->configuration(MarketContext::pricing));
            
            std::vector<Handle<DefaultProbabilityTermStructure>> wrapperCurves;
            DLOG("Building wrapper curves for calibration");
            for (size_t i = 0; i < creditCurves.size(); ++i) {
                try {
                    const string& cc = creditCurves[i];
                    auto originalCurve = market->defaultCurve(cc, config)->curve();
                    wrapperCurves.push_back(
                        buildCalibratedConstiuentCurve(originalCurve, calibrationFactor));
                } catch (const std::exception& e) {

                    WLOG("CDO constiuent calibration failed during building wrapper curve for "
                         << creditCurves[i] << ", skip this curve. Got "
                                                                             << e.what());
                }
            }

            if (wrapperCurves.size() == dpts.size() && indexCDS) {
                LOG("Start bootstraping of the calibration factors");
                dpts.swap(wrapperCurves);

                auto cdsPricingEngineUnderlyingCurves =
                    QuantLib::ext::make_shared<QuantExt::MidPointIndexCdsEngine>(dpts, recoveryRates, yts);

                indexCDS->setPricingEngine(cdsPricingEngineUnderlyingCurves);

                try {
                    auto targetFunction = [&cdsNpvs, &calibrationFactor, &indexCDS](const double& factor) {
                        calibrationFactor->setValue(factor);
                        return cdsNpvs - indexCDS->NPV();
                    };

                    Brent solver;
                    double indexAdjustmentForUnderlyingCurves =
                        solver.solve(targetFunction, 1e-8, cdsFairSpreads / indexCDS->fairSpreadClean(), 0.001, 2);

                    DLOG("Calibration of indexterm " << io::iso_date(indexCDS->maturity())
                                                     << "successful, found solution "
                                                     << indexAdjustmentForUnderlyingCurves);
                    calibrationFactor->setValue(indexAdjustmentForUnderlyingCurves);
                } catch (const std::exception& e) {
                    WLOG("Calibration failed, at pillar " << io::iso_date(indexCDS->maturity())
                                                          << ", set calibration factor to 1 (uncalibrated), got "
                                                          << e.what());
                    calibrationFactor->setValue(1.0);
                }

                LOG("Calibration results for creditCurve:" << creditCurveIdWithTerm());
                LOG("Expiry \t CalibrationFactor \t NpvIntrinsic \t NpvIndexCurve \t NpvError \t "
                    "FairSpreadIntrinsic "
                    "\t FairSpreadIndexCurve \t FairSreadError");

                LOG(io::iso_date(indexCDS->maturity())
                    << "\t" << calibrationFactor->value() << "\t" << indexCDS->NPV() << "\t" << cdsNpvs << "\t"
                    << indexCDS->NPV() - cdsNpvs << "\t" << indexCDS->fairSpreadClean() << "\t" << cdsFairSpreads
                    << "\t" << indexCDS->fairSpreadClean() - cdsFairSpreads);
            }
        }
    }

    // Create the instruments.
    auto pool = QuantLib::ext::make_shared<Pool>();
    
    CreditPortfolioSensitivityDecomposition sensitivityDecomposition = cdoEngineBuilder->sensitivityDecomposition();
    useSensitivitySimplification_ = sensitivityDecomposition != CreditPortfolioSensitivityDecomposition::Underlying;
    Handle<DefaultProbabilityTermStructure> clientCurve;
    Handle<DefaultProbabilityTermStructure> baseCurve;
    vector<Time> baseCurveTimes;
    vector<Real> expLoss;

    if (cdoEngineBuilder->optimizedSensitivityCalculation()) {
        dpts = buildPerformanceOptimizedDefaultCurves(dpts);
    }

    for (Size i = 0; i < creditCurves.size(); ++i) {
        const string& cc = creditCurves[i];
        DefaultProbKey key = NorthAmericaCorpDefaultKey(ccy, SeniorSec, Period(), 1.0);
        Real recoveryRate = recoveryRates[i];
        auto defaultCurve = dpts[i];
        expLoss.push_back((1 - recoveryRate) * defaultCurve->defaultProbability(maturity_, true) * basketNotionals[i]);
        std::pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>> p(key, defaultCurve);
        vector<pair<DefaultProbKey, Handle<DefaultProbabilityTermStructure>>> probabilities(1, p);
        // Empty default set. Adjustments have been made above to account for existing credit events.
        Issuer issuer(probabilities, DefaultEventSet());
        pool->add(cc, issuer, key);
        DLOG("Issuer " << cc << " added to the pool.");
    }

    // If we use the simplification, we need a list of all credit curves and their weight to the basket

    if (sensitivityDecomposition == CreditPortfolioSensitivityDecomposition::LossWeighted) {
        basketConstituents_.clear();
        Real totalWeight = std::accumulate(expLoss.begin(), expLoss.end(), 0.0);
        for (Size basketIdx = 0; basketIdx < creditCurves.size(); basketIdx++) {
            string& creditCurve = creditCurves[basketIdx];
            Real weight = expLoss[basketIdx];
            if (basketConstituents_.count(creditCurve) == 0) {
                basketConstituents_[creditCurve] = weight / totalWeight;
            } else {
                basketConstituents_[creditCurve] += weight / totalWeight;
            }
        }
    } else if (sensitivityDecomposition == CreditPortfolioSensitivityDecomposition::NotionalWeighted) {
        basketConstituents_.clear();
        Real totalWeight = std::accumulate(basketNotionals.begin(), basketNotionals.end(), 0.0);
        for (Size basketIdx = 0; basketIdx < creditCurves.size(); basketIdx++) {
            string& creditCurve = creditCurves[basketIdx];
            Real weight = basketNotionals[basketIdx];
            if (basketConstituents_.count(creditCurve) == 0) {
                basketConstituents_[creditCurve] = weight / totalWeight;
            } else {
                basketConstituents_[creditCurve] += weight / totalWeight;
            }
        }
    } else if (sensitivityDecomposition == CreditPortfolioSensitivityDecomposition::DeltaWeighted) {
        basketConstituents_.clear();
        Real totalWeight = 0;
        for (Size basketIdx = 0; basketIdx < creditCurves.size(); basketIdx++) {
            string& creditCurve = creditCurves[basketIdx];
            Real notional = basketNotionals[basketIdx];
            auto defaultCurve = market->defaultCurve(creditCurve, config)->curve();
            Real constituentSurvivalProb = defaultCurve->survivalProbability(maturity_);
            Time t = defaultCurve->timeFromReference(maturity_);
            Real CR01 = t * constituentSurvivalProb * notional;
            if (basketConstituents_.find(creditCurve) == basketConstituents_.end())
                basketConstituents_.emplace(creditCurve, CR01);
            totalWeight += CR01;
        }
        // Normalize
        for (auto& decompWeight : basketConstituents_) {
            decompWeight.second /= totalWeight;
        }
    }

    // We will use a homogeneous pool loss model below if all notionals and recoveries are the same.
    bool homogeneous = all_of(basketNotionals.begin(), basketNotionals.end(),
                              [&basketNotionals](Real ntl) { return close_enough(ntl, basketNotionals[0]); });
    homogeneous = homogeneous && all_of(recoveryRates.begin(), recoveryRates.end(),
                                        [&recoveryRates](Real rr) { return close_enough(rr, recoveryRates[0]); });

    
    // vanilla holds the representation of the CDO, i.e. 1 * [0, Detach] instrument - 1 * [0, Attach] instrument,
    // without the upfront fee payment which is added below to create the full final instrument.
    QuantLib::ext::shared_ptr<Instrument> vanilla;

    // Tranche from 0 to detachment point.
    // If detachment point is 1.0, build an index CDS, i.e. [0, 100%] tranche. Otherwise an actual tranche.
    QuantLib::ext::shared_ptr<Instrument> cdoD;
    if (!close_enough(adjDetachPoint, 1.0)) {
        DLOG("Building detachment tranche [0," << adjDetachPoint << "].");
        // Set up the basket loss model.
        auto basket =
            QuantLib::ext::make_shared<QuantExt::Basket>(schedule[0], creditCurves, basketNotionals, pool, 0.0, adjDetachPoint);
        basket->setLossModel(
            cdoEngineBuilder->lossModel(qualifier(), recoveryRates, adjDetachPoint,
                                                         maturity_, homogeneous));

        auto cdoDetach =
            QuantLib::ext::make_shared<QuantExt::SyntheticCDO>(basket, side, schedule, 0.0, runningRate, dayCounter, bdc,
                                                       settlesAccrual_, protectionPaymentTime_, protectionStartDate, parseDate(upfrontDate_), boost::none, Null<Real>(), lastPeriodDayCounter);

        cdoDetach->setPricingEngine(
            cdoEngineBuilder->engine(ccy, false, {}, calibrationFactor, fixedRecovery));
        setSensitivityTemplate(*cdoEngineBuilder);
        cdoD = cdoDetach;

        DLOG("Detachment tranche [0," << adjDetachPoint << "] built.");

    } else {
        DLOG("Detachment point is 1.0 so building an index CDS for [0,1.0] 'tranche'.");

        /*auto cdsBuilder = QuantLib::ext::dynamic_pointer_cast<IndexCreditDefaultSwapEngineBuilder>(
            engineFactory->builder("IndexCreditDefaultSwap"));
        QL_REQUIRE(cdsBuilder, "Trade " << id() << " needs a IndexCreditDefaultSwapEngineBuilder.");
        */
        auto cds = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
            side, currTotalNtl, basketNotionals, 0.0, runningRate, schedule, bdc, dayCounter, settlesAccrual_,
            protectionPaymentTime_, protectionStartDate, Date(), QuantLib::ext::shared_ptr<Claim>(),
            lastPeriodDayCounter, rebatesAccrual_, protectionStartDate, 3);

        cds->setPricingEngine(
            cdoEngineBuilder->engine(ccy, true, creditCurves, calibrationFactor, fixedRecovery));
        setSensitivityTemplate(*cdoEngineBuilder);
        cdoD = cds;

        DLOG("Index CDS for [0,1.0] 'tranche' built.");
    }

    // Tranche from 0 to attachment point.
    // If attachment point is 0.0, the instrument is simply the 0 to detachment point CDO built above. If attachment
    // point is greater than 0, we build the 0 to attachment point tranche and the value of the CDO is the
    // difference between the detachment point CDO and attachment point CDO.
    if (close_enough(adjAttachPoint, 0.0)) {
        DLOG("Attachment point is 0 so the instrument is built.");
        vanilla = cdoD;
    } else {
        DLOG("Building attachment tranche [0," << adjAttachPoint << "].");

        // Set up the basket loss model.
        auto basket =
            QuantLib::ext::make_shared<QuantExt::Basket>(schedule[0], creditCurves, basketNotionals, pool, 0.0, adjAttachPoint);
        basket->setLossModel(cdoEngineBuilder->lossModel(qualifier(), recoveryRates, adjAttachPoint,
                                                         maturity_, homogeneous));

        auto cdoA =
            QuantLib::ext::make_shared<QuantExt::SyntheticCDO>(basket, side, schedule, 0.0, runningRate, dayCounter, bdc, settlesAccrual_, protectionPaymentTime_,
            protectionStartDate, parseDate(upfrontDate_), boost::none, fixedRecovery, lastPeriodDayCounter);

        cdoA->setPricingEngine(
            cdoEngineBuilder->engine(ccy, false, {}, calibrationFactor, fixedRecovery));
        setSensitivityTemplate(*cdoEngineBuilder);

        DLOG("Attachment tranche [0," << adjAttachPoint << "] built.");

        DLOG("Building attachment and detachment composite instrument.");

        auto composite = QuantLib::ext::make_shared<CompositeInstrument>();
        composite->add(cdoD);
        composite->subtract(cdoA);
        vanilla = composite;

        DLOG("Attachment and detachment composite instrument built.");
    }

    // Add the upfront fee payment here. Positive number indicates payment from buyer of protection to seller of
    // protection and a negative value indicates a payment from seller of protection to buyer of protection. The
    // upfront fee provided is interpreted as a fractional amount of the original tranche notional.
    if (upfrontDate != Date()) {
        vector<QuantLib::ext::shared_ptr<Instrument>> insts;
        vector<Real> mults;
        Real upfrontAmount = upfrontFee_ * origTrancheNtl;
        string configuration = cdoEngineBuilder->configuration(MarketContext::pricing);
        maturity_ =
            std::max(maturity_, addPremiums(insts, mults, 1.0, PremiumData(upfrontAmount, ccy.code(), upfrontDate),
                                            side == Protection::Buyer ? -1.0 : 1.0, ccy, engineFactory, configuration));

        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(vanilla, 1.0, insts, mults);

    } else {
        // If no upfront payment, the instrument is simply the vanilla instrument.
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(vanilla);
    }

    additionalData_["originalNotional"] = origTrancheNtl;
    additionalData_["currentNotional"] = currTrancheNtl;

    DLOG("CDO instrument built");
}

void SyntheticCDO::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cdoNode = XMLUtils::getChildNode(node, "CdoData");
    QL_REQUIRE(cdoNode, "No CdoData Node");
    qualifier_ = XMLUtils::getChildValue(cdoNode, "Qualifier", true);
    protectionStart_ = XMLUtils::getChildValue(cdoNode, "ProtectionStart", true);
    upfrontDate_ = XMLUtils::getChildValue(cdoNode, "UpfrontDate", false);

    // zero if empty or missing
    upfrontFee_ = Null<Real>();
    string strUpfrontFee = XMLUtils::getChildValue(cdoNode, "UpfrontFee", false);
    if (!strUpfrontFee.empty()) {
        upfrontFee_ = parseReal(strUpfrontFee);
    }
    settlesAccrual_ = XMLUtils::getChildValueAsBool(cdoNode, "SettlesAccrual", false);      // default = Y
    rebatesAccrual_ = XMLUtils::getChildValueAsBool(cdoNode, "RebatesAccrual", false);      // default = Y
    protectionPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault; // set default

    // Recovery rate is Null<Real>() on a standard CDO i.e. if "FixedRecoveryRate" field is not populated.
    recoveryRate_ = Null<Real>();
    string strRecoveryRate = XMLUtils::getChildValue(node, "FixedRecoveryRate", false);
    if (!strRecoveryRate.empty()) {
        recoveryRate_ = parseReal(strRecoveryRate);
    }

    // for backwards compatibility only
    if (auto c = XMLUtils::getChildNode(cdoNode, "PaysAtDefaultTime"))
        if (!parseBool(XMLUtils::getNodeValue(c)))
            protectionPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
    // new node overrides deprecated one, if both should be given
    if (auto c = XMLUtils::getChildNode(cdoNode, "ProtectionPaymentTime")) {
        if (XMLUtils::getNodeValue(c) == "atDefault")
            protectionPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault;
        else if (XMLUtils::getNodeValue(c) == "atPeriodEnd")
            protectionPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd;
        else if (XMLUtils::getNodeValue(c) == "atMaturity")
            protectionPaymentTime_ = QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity;
        else {
            QL_FAIL("protection payment time '" << XMLUtils::getNodeValue(c)
                                                << "' not known, expected atDefault, atPeriodEnd, atMaturity");
        }
    }
    attachmentPoint_ = XMLUtils::getChildValueAsDouble(cdoNode, "AttachmentPoint", true);
    detachmentPoint_ = XMLUtils::getChildValueAsDouble(cdoNode, "DetachmentPoint", true);
    XMLNode* legNode = XMLUtils::getChildNode(cdoNode, "LegData");
    legData_.fromXML(legNode);
    XMLNode* basketNode = XMLUtils::getChildNode(cdoNode, "BasketData");
    if (basketNode)
        basketData_.fromXML(basketNode);
}

XMLNode* SyntheticCDO::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* cdoNode = doc.allocNode("CdoData");
    XMLUtils::appendNode(node, cdoNode);
    XMLUtils::addChild(doc, cdoNode, "Qualifier", qualifier_);
    XMLUtils::addChild(doc, cdoNode, "ProtectionStart", protectionStart_);
    if (!upfrontDate_.empty()) {
        XMLUtils::addChild(doc, cdoNode, "UpfrontDate", upfrontDate_);
    }
    if (upfrontFee_ != Null<Real>()) {
        XMLUtils::addChild(doc, cdoNode, "UpfrontFee", upfrontFee_);
    }
    XMLUtils::addChild(doc, cdoNode, "SettlesAccrual", settlesAccrual_);
    if (!rebatesAccrual_)
        XMLUtils::addChild(doc, node, "RebatesAccrual", rebatesAccrual_);
    if (protectionPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atDefault)
        XMLUtils::addChild(doc, cdoNode, "ProtectionPaymentTime", "atDefault");
    else if (protectionPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd)
        XMLUtils::addChild(doc, cdoNode, "ProtectionPaymentTime", "atPeriodEnd");
    else if (protectionPaymentTime_ == QuantExt::CreditDefaultSwap::ProtectionPaymentTime::atMaturity)
        XMLUtils::addChild(doc, cdoNode, "ProtectionPaymentTime", "atMaturity");
    else {
        QL_FAIL("CreditDefaultSwapData::toXML(): unexpected protectionPaymentTime_");
    }
    if (recoveryRate_ != Null<Real>())
        XMLUtils::addChild(doc, node, "FixedRecoveryRate", recoveryRate_);
    XMLUtils::addChild(doc, cdoNode, "AttachmentPoint", attachmentPoint_);
    XMLUtils::addChild(doc, cdoNode, "DetachmentPoint", detachmentPoint_);
    XMLUtils::appendNode(cdoNode, legData_.toXML(doc));
    XMLUtils::appendNode(cdoNode, basketData_.toXML(doc));
    return node;
}
vector<Time>
SyntheticCDO::extractTimeGridDefaultCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure> &dpts) {
    if (auto c = QuantLib::ext::dynamic_pointer_cast<SpreadedSurvivalProbabilityTermStructure>(*dpts)) {
        return SyntheticCDO::extractTimeGridDefaultCurve(c->referenceCurve());
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedSurvivalProbabilityCurve<LogLinear>>(*dpts)) {
        return c->times();
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<InterpolatedHazardRateCurve<BackwardFlat>>(*dpts)) {
        return c->times();
    } else if (auto c = QuantLib::ext::dynamic_pointer_cast<SurvivalProbabilityCurve<LogLinear>>(*dpts)) {
        return c->times();
    } else {
        WLOG("Extraction of time points failed, unsupported default probability termstructure");
        return vector<double>();
    }
}

QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>
SyntheticCDO::buildCalibratedConstiuentCurve(const QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>& curve,
                                             const QuantLib::ext::shared_ptr<SimpleQuote>& calibrationFactor) {
    if (!calibrationFactor) {
        return curve;
    }
    auto curveTimes = extractTimeGridDefaultCurve(curve);
    vector<Handle<Quote>> spreads;
    for (size_t timeIdx = 0; timeIdx < curveTimes.size(); ++timeIdx) {
        auto sp = curve->survivalProbability(curveTimes[timeIdx]);
         auto compQuote = QuantLib::ext::make_shared<CompositeQuote<std::function<double(double, double)>>>(
            Handle<Quote>(calibrationFactor),
            Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(sp)),
            [](const double q1, const double q2) -> double { return std::exp(-(1 - q1) * std::log(q2)); });
        spreads.push_back(Handle<Quote>(compQuote));
    }
    Handle<DefaultProbabilityTermStructure> targetCurve = Handle<DefaultProbabilityTermStructure>(
        QuantLib::ext::make_shared<SpreadedSurvivalProbabilityTermStructure>(curve, curveTimes, spreads));
    if (curve->allowsExtrapolation()) {
        targetCurve->enableExtrapolation();
    }
    return targetCurve;   
}

std::string SyntheticCDO::creditCurveIdWithTerm() const {
    auto p = ore::data::splitCurveIdWithTenor(qualifier());
    if (p.second != 0 * Days || !isIndexTranche())
        return qualifier();

    QuantLib::Schedule s = makeSchedule(leg().schedule());
    if (s.dates().empty())
        return p.first;
    QuantLib::Period t = QuantExt::implyIndexTerm(
        indexStartDateHint_ == Date() ? s.dates().front() : indexStartDateHint_, s.dates().back());
    if (t != 0 * Days)
        return p.first + "_" + ore::data::to_string(t);
    return p.first;
}


} // namespace data
} // namespace ore
