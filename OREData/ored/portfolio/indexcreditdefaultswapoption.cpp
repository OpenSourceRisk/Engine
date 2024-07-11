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

#include <ored/portfolio/optionwrapper.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswap.hpp>
#include <ored/portfolio/builders/indexcreditdefaultswapoption.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <qle/instruments/indexcdsoption.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/utilities/time.hpp>

using ore::data::Envelope;
using ore::data::OptionData;
using ore::data::ReferenceDataManager;
using ore::data::to_string;
using namespace QuantLib;
using namespace QuantExt;
using std::string;

namespace ore {
namespace data {

IndexCreditDefaultSwapOption::IndexCreditDefaultSwapOption()
    : Trade("IndexCreditDefaultSwapOption"), strike_(Null<Real>()) {}

IndexCreditDefaultSwapOption::IndexCreditDefaultSwapOption(const Envelope& env, const IndexCreditDefaultSwapData& swap,
                                                           const OptionData& option, Real strike,
                                                           const string& indexTerm, const string& strikeType,
                                                           const Date& tradeDate, const Date& fepStartDate)
    : Trade("IndexCreditDefaultSwapOption", env), swap_(swap), option_(option), strike_(strike),
      indexTerm_(indexTerm), strikeType_(strikeType), tradeDate_(tradeDate), fepStartDate_(fepStartDate) {}

void IndexCreditDefaultSwapOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("IndexCreditDefaultSwapOption::build() called for trade " << id());

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Credit");
    additionalData_["isdaBaseProduct"] = string("Swaptions");
    string entity = swap_.creditCurveId();
    QuantLib::ext::shared_ptr<ReferenceDataManager> refData = engineFactory->referenceData();
    if (refData && refData->hasData("CreditIndex", entity)) {
        auto refDatum = refData->getData("CreditIndex", entity);
        QuantLib::ext::shared_ptr<CreditIndexReferenceDatum> creditIndexRefDatum =
            QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(refDatum);
        additionalData_["isdaSubProduct"] = creditIndexRefDatum->indexFamily();
        if (creditIndexRefDatum->indexFamily() == "") {
            ALOG("IndexFamily is blank in credit index reference data for entity " << entity);
        }
    } else {
        ALOG("Credit index reference data missing for entity " << entity << ", isdaSubProduct left blank");
    }
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");  

    // Dates
    const QuantLib::ext::shared_ptr<Market>& market = engineFactory->market();
    Date asof = market->asofDate();
    if (asof == Null<Date>() || asof == Date()) {
        asof = Settings::instance().evaluationDate();
    }

    if (tradeDate_ == Date()) {
        tradeDate_ = asof;
    } else {
        QL_REQUIRE(tradeDate_ <= asof, "Trade date (" << io::iso_date(tradeDate_) << ") should be on or "
                                                      << "before the valuation date (" << io::iso_date(asof) << ")");
    }

    if (fepStartDate_ == Date()) {
        fepStartDate_ = tradeDate_;
    } else {
        QL_REQUIRE(fepStartDate_ <= tradeDate_, "Front end protection start date ("
                                                    << io::iso_date(fepStartDate_)
                                                    << ") should be on or before the trade date ("
                                                    << io::iso_date(tradeDate_) << ")");
    }

    // Option trade notional. This is the full notional of the index that is being traded not reduced by any
    // defaults. The notional on the trade date will be a fraction of this if there are defaults by trade date.
    auto legData = swap_.leg();
    const auto& ntls = legData.notionals();
    QL_REQUIRE(ntls.size() == 1, "IndexCreditDefaultSwapOption requires a single notional.");
    notionals_ = Notionals();
    notionals_.full = ntls.front();
    notionalCurrency_ = legData.currency();
    npvCurrency_ = legData.currency();

    // Need fixed leg data with one rate. This should be the standard running coupon on the index CDS e.g.
    // 100bp for CDX IG and 500bp for CDX HY.
    QL_REQUIRE(legData.legType() == "Fixed", "Index CDS option " << id() << " requires fixed leg.");
    auto fixedLegData = QuantLib::ext::dynamic_pointer_cast<FixedLegData>(legData.concreteLegData());
    QL_REQUIRE(fixedLegData->rates().size() == 1, "Index CDS option " << id() << " requires single fixed rate.");
    auto runningCoupon = fixedLegData->rates().front();
    Real upfrontFee = swap_.upfrontFee();

    // Usually, we expect a Strike and StrikeType. However, for backwards compatibility we also allow for
    // empty values and populate Strike, StrikeType from the underlying upfront and coupon.
    QL_REQUIRE(strikeType_ == "" || strikeType_ == "Spread" || strikeType_ == "Price",
               "invalid StrikeType (" << strikeType_ << "), expected 'Spread' or 'Price' or empty value");
    if (strike_ == Null<Real>() && strikeType_ == "" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = runningCoupon;
        effectiveStrikeType_ = "Spread";
    } else if (strike_ == Null<Real>() && strikeType_ == "Spread" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = runningCoupon;
        effectiveStrikeType_ = "Spread";
    } else if (strike_ == Null<Real>() && strikeType_ == "Price" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = 1.0;
        effectiveStrikeType_ = "Price";
    } else if (strike_ != Null<Real>() && strikeType_ == "" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = strike_;
        effectiveStrikeType_ = "Spread";
    } else if (strike_ != Null<Real>() && strikeType_ == "Spread" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = strike_;
        effectiveStrikeType_ = "Spread";
    } else if (strike_ != Null<Real>() && strikeType_ == "Price" && upfrontFee == Null<Real>()) {
        effectiveStrike_ = strike_;
        effectiveStrikeType_ = "Price";
    } else if (strike_ == Null<Real>() && strikeType_ == "" && upfrontFee != Null<Real>()) {
        effectiveStrike_ = 1.0 - upfrontFee;
        effectiveStrikeType_ = "Price";
    } else if (strike_ == Null<Real>() && strikeType_ == "Spread" && upfrontFee != Null<Real>()) {
        if (close_enough(upfrontFee, 0.0)) {
            effectiveStrike_ = runningCoupon;
            effectiveStrikeType_ = "Spread";
        } else {
            QL_FAIL("StrikeType 'Spread' and non-zero upfront fee can not be combined.");
        }
    } else if (strike_ == Null<Real>() && strikeType_ == "Price" && upfrontFee != Null<Real>()) {
        effectiveStrike_ = 1.0 - upfrontFee;
        effectiveStrikeType_ = "Price";
    } else if (strike_ != Null<Real>() && strikeType_ == "" && upfrontFee != Null<Real>()) {
        if (close_enough(upfrontFee, 0.0)) {
            effectiveStrike_ = strike_;
            effectiveStrikeType_ = "Spread";
        } else {
            QL_FAIL("Strike and non-zero upfront can not be combined.");
        }
    } else if (strike_ != Null<Real>() && strikeType_ == "Spread" && upfrontFee != Null<Real>()) {
        if (close_enough(upfrontFee, 0.0)) {
            effectiveStrike_ = strike_;
            effectiveStrikeType_ = "Spread";
        } else {
            QL_FAIL("Strike and non-zero upfront can not be combined.");
        }
    } else if (strike_ != Null<Real>() && strikeType_ == "Price" && upfrontFee != Null<Real>()) {
        if (close_enough(upfrontFee, 0.0)) {
            effectiveStrike_ = strike_;
            effectiveStrikeType_ = "Price";
        } else {
            QL_FAIL("Strike and non-zero upfront can not be combined.");
        }
    } else {
        QL_FAIL("internal error, impossible branch in strike / strike type deduction.");
    }
    DLOG("Will use strike = " << effectiveStrike_ << ", strikeType = " << effectiveStrikeType_);

    // Payer (Receiver) swaption if the leg is paying (receiving).
    auto side = legData.isPayer() ? Protection::Side::Buyer : Protection::Side::Seller;

    // Populate the constituents and determine the various notional amounts.
    constituents_.clear();
    if (swap_.basket().constituents().size() > 1) {
        fromBasket(asof, constituents_);
    } else {
        fromReferenceData(asof, constituents_, engineFactory->referenceData());
    }

    // Transfer to vectors for ctors below
    vector<string> constituentIds;
    constituentIds.reserve(constituents_.size());
    vector<Real> constituentNtls;
    constituentNtls.reserve(constituents_.size());
    for (const auto& kv : constituents_) {
        constituentIds.push_back(kv.first);
        constituentNtls.push_back(kv.second);
    }

    // Day counter. In general for CDS and CDS index trades, the standard day counter is Actual/360 and the final
    // period coupon accrual includes the maturity date.
    DayCounter dc = parseDayCounter(legData.dayCounter());
    Actual360 standardDayCounter;
    DayCounter lastPeriodDayCounter = dc == standardDayCounter ? Actual360(true) : dc;

    // Checks on the option data
    QL_REQUIRE(option_.style() == "European", "IndexCreditDefaultSwapOption option style must"
                                                  << " be European but got " << option_.style() << ".");
    QL_REQUIRE(option_.exerciseFees().empty(), "IndexCreditDefaultSwapOption cannot handle exercise fees.");

    // Exercise must be European
    const auto& exerciseDates = option_.exerciseDates();
    QL_REQUIRE(exerciseDates.size() == 1, "IndexCreditDefaultSwapOption expects one exercise date"
                                              << " but got " << exerciseDates.size() << " exercise dates.");
    Date exerciseDate = parseDate(exerciseDates.front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);

    QL_REQUIRE(parseDate(legData.schedule().rules().front().endDate()) > exerciseDate, 
        "IndexCreditDefaultSwapOption: ExerciseDate must be before EndDate");

    // We apply an automatic correction to a common mistake in the input data, where the full index underlying
    // is provided and not only the part of the underlying into which we exercise.
    if (legData.schedule().rules().size() == 1 && legData.schedule().dates().empty()) {
        // The start date should be >= exercise date, this will produce correct coupons for both
        // - post big bang rules CDS, CDS2015 (full first coupon) and
        // - pre big bang rules (short first coupon)
        if (parseDate(legData.schedule().rules().front().startDate()) < exerciseDate) {
            legData.schedule().modifyRules().front().modifyStartDate() = ore::data::to_string(exerciseDate);
        }
    }

    // Schedule
    Schedule schedule = makeSchedule(legData.schedule());
    BusinessDayConvention payConvention = parseBusinessDayConvention(legData.paymentConvention());

    // Populate trade date and protection start date of underlying swap
    QL_REQUIRE(!schedule.dates().empty(),
               "IndexCreditDefaultSwapOption: underlying swap schedule does not contain any dates");
    Date underlyingTradeDate =
        swap_.tradeDate() == Date() ? std::max(exerciseDate, schedule.dates().front()) : swap_.tradeDate();
    Date underlyingProtectionStart;
    if (swap_.protectionStart() != Date()) {
        underlyingProtectionStart = swap_.protectionStart();
    } else if (legData.schedule().rules().size() == 1 && legData.schedule().dates().empty()) {
        auto rule = parseDateGenerationRule(legData.schedule().rules().front().rule());
        if (rule == DateGeneration::CDS || rule == DateGeneration::CDS2015) {
            underlyingProtectionStart = std::max(exerciseDate, schedule.dates().front());
        } else {
            underlyingProtectionStart = schedule.dates().front();
        }
    } else {
        underlyingProtectionStart = std::max(exerciseDate, schedule.dates().front());
    }

    // get engine builders for option and underlying swap
    auto iCdsOptionEngineBuilder = QuantLib::ext::dynamic_pointer_cast<IndexCreditDefaultSwapOptionEngineBuilder>(
        engineFactory->builder("IndexCreditDefaultSwapOption"));
    QL_REQUIRE(iCdsOptionEngineBuilder,
               "IndexCreditDefaultSwapOption: internal error, expected IndexCreditDefaultSwapOptionEngineBuilder");
    auto iCdsEngineBuilder = QuantLib::ext::dynamic_pointer_cast<IndexCreditDefaultSwapEngineBuilder>(
        engineFactory->builder("IndexCreditDefaultSwap"));
    QL_REQUIRE(iCdsEngineBuilder,
               "IndexCreditDefaultSwap: internal error, expected IndexCreditDefaultSwapEngineBuilder");

    // The underlying index CDS as it looks on the valuation date i.e. outstanding notional is the valuation
    // date notional and the basket of notionals contains only those reference entities not defaulted (i.e.
    // those with auction date in the future to be more precise).
    auto cds = QuantLib::ext::make_shared<QuantExt::IndexCreditDefaultSwap>(
        side, notionals_.valuationDate, constituentNtls, runningCoupon, schedule, payConvention, dc,
        swap_.settlesAccrual(), swap_.protectionPaymentTime(), underlyingProtectionStart, QuantLib::ext::shared_ptr<Claim>(),
        lastPeriodDayCounter, true, underlyingTradeDate, swap_.cashSettlementDays());

    // Set engine on the underlying CDS.
    auto ccy = parseCurrency(npvCurrency_);
    std::string overrideCurve = iCdsOptionEngineBuilder->engineParameter("Curve", {}, false, "Underlying");

    auto creditCurveId = this->creditCurveId();
    // warn if that is not possible, except for trades on custom baskets
    if (swap_.basket().constituents().empty() && splitCurveIdWithTenor(creditCurveId).second == 0 * Days) {
        StructuredTradeWarningMessage(id(), tradeType(), "Could not imply Index CDS term.",
                                           "Index CDS term could not be derived from start, end date, are these "
                                           "dates correct (credit curve id is '" +
                                          swap_.creditCurveId() + "')")
            .log();
    }

    // for cash settlement build the underlying swap with the inccy discount curve
    Settlement::Type settleType = parseSettlementType(option_.settlement());
    cds->setPricingEngine(iCdsEngineBuilder->engine(ccy, creditCurveId, constituentIds, overrideCurve,
                                                    swap_.recoveryRate(), settleType == Settlement::Cash));
    setSensitivityTemplate(*iCdsEngineBuilder);

    // Strike may be in terms of spread or price
    auto strikeType = parseCdsOptionStrikeType(effectiveStrikeType_);

    // Determine the index term;
    effectiveIndexTerm_ = 5 * Years;
    if (!indexTerm_.empty()) {
        // if the option has an explicit index term set, we use that
        effectiveIndexTerm_ = parsePeriod(indexTerm_);
    } else {
        // otherwise we derive the index term from the start date (or an externally set hint for that)
        effectiveIndexTerm_ = QuantExt::implyIndexTerm(swap_.indexStartDateHint() == Date() ? schedule.dates().front()
                                                                                        : swap_.indexStartDateHint(),
                                                   schedule.dates().back());
    }

    // Build the option
    auto option = QuantLib::ext::make_shared<QuantExt::IndexCdsOption>(cds, exercise, effectiveStrike_, strikeType, settleType,
                                                               notionals_.tradeDate, notionals_.realisedFep,
                                                               effectiveIndexTerm_);
    // the vol curve id is the credit curve id stripped by a term, if the credit curve id should contain one
    auto p = splitCurveIdWithTenor(swap_.creditCurveId());
    volCurveId_ = p.first;
    option->setPricingEngine(iCdsOptionEngineBuilder->engine(ccy, creditCurveId, volCurveId_, constituentIds));
    setSensitivityTemplate(*iCdsOptionEngineBuilder);

    // Keep this comment about the maturity date being the underlying maturity instead of the option expiry.
    // [RL] Align option product maturities with ISDA AANA/GRID guidance as of November 2020.
    maturity_ = cds->coupons().back()->date();

    // Set Trade members _before_ possibly adding the premium payment below.
    legs_ = {cds->coupons()};
    legCurrencies_ = {npvCurrency_};
    legPayers_ = {legData.isPayer()};

    // Long or short the option
    Position::Type positionType = parsePositionType(option_.longShort());
    Real indicatorLongShort = positionType == Position::Long ? 1.0 : -1.0;

    // Include premium if enough information is provided
    vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    string configuration = iCdsOptionEngineBuilder->configuration(MarketContext::pricing);
    maturity_ =
        std::max(maturity_, addPremiums(additionalInstruments, additionalMultipliers, indicatorLongShort,
                                        option_.premiumData(), -indicatorLongShort, ccy, engineFactory, configuration));

    // Instrument wrapper depends on the settlement type.
    // The instrument build should be indpednent of the evaluation date. However, the general behavior
    // in ORE (e.g. IR swaptions) for normal pricing runs is that the option is considered expired on
    // the expiry date with no assumptions on an (automatic) exercise. Therefore we build a vanilla
    // instrument if the exercise date is <= the eval date at build time.
    if (settleType == Settlement::Cash || exerciseDate <= Settings::instance().evaluationDate()) {
        instrument_ = QuantLib::ext::make_shared<VanillaInstrument>(option, indicatorLongShort, additionalInstruments,
                                                            additionalMultipliers);
    } else {
        bool isLong = positionType == Position::Long;
        bool isPhysical = settleType == Settlement::Physical;
        instrument_ = QuantLib::ext::make_shared<EuropeanOptionWrapper>(option, isLong, exerciseDate, isPhysical, cds, 1.0, 1.0,
                                                                additionalInstruments, additionalMultipliers);
    }

    sensitivityDecomposition_ = iCdsOptionEngineBuilder->sensitivityDecomposition();
}

Real IndexCreditDefaultSwapOption::notional() const {

    if (notionals_.valuationDate == Null<Real>()) {
        ALOG("Error retrieving current notional for index credit default swap option "
             << id() << " as of " << Settings::instance().evaluationDate());
    }

    return notionals_.valuationDate;
}

void IndexCreditDefaultSwapOption::fromXML(XMLNode* node) {

    Trade::fromXML(node);

    XMLNode* iCdsOptionData = XMLUtils::getChildNode(node, "IndexCreditDefaultSwapOptionData");
    QL_REQUIRE(iCdsOptionData, "Expected IndexCreditDefaultSwapOptionData node on trade " << id() << ".");
    strike_ = XMLUtils::getChildValueAsDouble(iCdsOptionData, "Strike", false, Null<Real>());
    indexTerm_ = XMLUtils::getChildValue(iCdsOptionData, "IndexTerm", false);
    strikeType_ = XMLUtils::getChildValue(iCdsOptionData, "StrikeType", false);
    tradeDate_ = Date();
    if (auto n = XMLUtils::getChildNode(iCdsOptionData, "TradeDate")) {
        tradeDate_ = parseDate(XMLUtils::getNodeValue(n));
    }
    fepStartDate_ = Date();
    if (auto n = XMLUtils::getChildNode(iCdsOptionData, "FrontEndProtectionStartDate")) {
        fepStartDate_ = parseDate(XMLUtils::getNodeValue(n));
    }

    XMLNode* iCdsData = XMLUtils::getChildNode(iCdsOptionData, "IndexCreditDefaultSwapData");
    QL_REQUIRE(iCdsData, "Expected IndexCreditDefaultSwapData node on trade " << id() << ".");
    swap_.fromXML(iCdsData);

    XMLNode* optionData = XMLUtils::getChildNode(iCdsOptionData, "OptionData");
    QL_REQUIRE(optionData, "Expected OptionData node on trade " << id() << ".");
    option_.fromXML(optionData);
}

XMLNode* IndexCreditDefaultSwapOption::toXML(XMLDocument& doc) const {

    // Trade node
    XMLNode* node = Trade::toXML(doc);

    // IndexCreditDefaultSwapOptionData node
    XMLNode* iCdsOptionData = doc.allocNode("IndexCreditDefaultSwapOptionData");
    if (strike_ != Null<Real>())
        XMLUtils::addChild(doc, iCdsOptionData, "Strike", strike_);
    if (!indexTerm_.empty())
        XMLUtils::addChild(doc, iCdsOptionData, "IndexTerm", indexTerm_);
    if (strikeType_ != "")
        XMLUtils::addChild(doc, iCdsOptionData, "StrikeType", strikeType_);
    if (tradeDate_ != Date())
        XMLUtils::addChild(doc, iCdsOptionData, "TradeDate", to_string(tradeDate_));
    if (fepStartDate_ != Date())
        XMLUtils::addChild(doc, iCdsOptionData, "FrontEndProtectionStartDate", to_string(fepStartDate_));

    XMLUtils::appendNode(iCdsOptionData, swap_.toXML(doc));
    XMLUtils::appendNode(iCdsOptionData, option_.toXML(doc));

    // Add IndexCreditDefaultSwapOptionData node to Trade node
    XMLUtils::appendNode(node, iCdsOptionData);

    return node;
}

const IndexCreditDefaultSwapData& IndexCreditDefaultSwapOption::swap() const { return swap_; }

const OptionData& IndexCreditDefaultSwapOption::option() const { return option_; }

const std::string& IndexCreditDefaultSwapOption::indexTerm() const { return indexTerm_; }

Real IndexCreditDefaultSwapOption::strike() const { return strike_; }

QuantLib::Option::Type IndexCreditDefaultSwapOption::callPut() const {
    if (swap().leg().isPayer())
        return QuantLib::Option::Type::Call;
    else
        return QuantLib::Option::Type::Put;
}

const string& IndexCreditDefaultSwapOption::strikeType() const { return strikeType_; }

const Date& IndexCreditDefaultSwapOption::tradeDate() const { return tradeDate_; }

const Date& IndexCreditDefaultSwapOption::fepStartDate() const { return fepStartDate_; }

void IndexCreditDefaultSwapOption::fromBasket(const Date& asof, map<string, Real>& outConstituents) {

    const auto& constituents = swap_.basket().constituents();
    DLOG("Building constituents from basket data containing " << constituents.size() << " elements.");

    Real totalNtl = 0.0;
    Real fullNtl = notionals_.full;
    for (const auto& c : constituents) {
        Real ntl = Null<Real>();
        Real priorNotional = Null<Real>();

        const auto& creditCurve = c.creditCurveId();

        if (c.weightInsteadOfNotional()) {
            ntl = c.weight() * fullNtl;
        } else {
            ntl = c.notional();
        }

        if (ntl == 0.0 || close(0.0, ntl)) {
            if (c.weightInsteadOfNotional()) {
                priorNotional = c.priorWeight();
                if (priorNotional != Null<Real>()) {
                    priorNotional *= fullNtl;
                }
            } else {
                priorNotional = c.priorNotional();
            }
            // Entity is not in the index. Its auction date is in the past.
            QL_REQUIRE(priorNotional != Null<Real>(), "Constituent "
                                                          << creditCurve << " in index CDS option trade " << id()
                                                          << " has defaulted so expecting a prior notional.");
            QL_REQUIRE(c.recovery() != Null<Real>(), "Constituent " << creditCurve << " in index CDS option trade "
                                                                    << id()
                                                                    << " has defaulted so expecting a recovery.");
            QL_REQUIRE(c.auctionDate() != Date(), "Constituent " << creditCurve << " in index CDS option trade " << id()
                                                                 << " has defaulted so expecting an auction date.");
            QL_REQUIRE(c.auctionDate() <= asof, "Constituent " << creditCurve << " in index CDS option trade " << id()
                                                               << " has defaulted so expecting the auction date ("
                                                               << io::iso_date(c.auctionDate())
                                                               << ") to be before or on the valuation date ("
                                                               << io::iso_date(asof) << ".");

            totalNtl += priorNotional * notional_;

            if (tradeDate_ < c.auctionDate()) {
                TLOG("Trade date (" << io::iso_date(tradeDate_) << ") is before auction date ("
                                    << io::iso_date(c.auctionDate()) << ") of " << creditCurve
                                    << " so updating trade date "
                                    << "notional by amount " << priorNotional);
                notionals_.tradeDate += priorNotional;
            }

            if (fepStartDate_ < c.auctionDate()) {
                Real recovery = swap_.recoveryRate() != Null<Real>() ? swap_.recoveryRate() : c.recovery();
                Real fepAmount = (1 - recovery) * priorNotional;
                TLOG("FEP start date (" << io::iso_date(fepStartDate_) << ") is before auction date ("
                                        << io::iso_date(c.auctionDate()) << ") of " << creditCurve
                                        << " so updating realised "
                                        << "FEP by amount " << fepAmount);
                notionals_.realisedFep += fepAmount;
            }

        } else if (ntl > 0.0) {

            // Entity is still in the index.
            // Note that it may have defaulted but its auction date is still in the future.
            auto it = outConstituents.find(creditCurve);
            if (it == outConstituents.end()) {
                outConstituents[creditCurve] = ntl;
                TLOG("Adding underlying " << creditCurve << " with notional " << ntl);
                totalNtl += ntl;
                notionals_.tradeDate += ntl;
                notionals_.valuationDate += ntl;
            } else {
                StructuredTradeErrorMessage(id(), "IndexCDSOption", "Error building trade",
                                                 ("Invalid Basket: found a duplicate credit curve " + creditCurve +
                                                  ".Skip it. Check the basket data for possible errors.")
                                                .c_str())
                    .log();
            }
        } else {
            QL_FAIL("Constituent " << creditCurve << " in index CDS option trade " << id()
                                   << " has a negative notional " << ntl << ".");
        }
    }

    Real correctionFactor = fullNtl / totalNtl;
    // Scaling to Notional if relative error is close less than 10^-4
    if (!close(fullNtl, totalNtl) && (std::abs(correctionFactor - 1.0) <= 1e-4)) {
        DLOG("Trade " << id() << ", sum of notionals (" << totalNtl << ") is very close to " << fullNtl
                      << ",will scale it by " << correctionFactor << ". Check the basket data for possible errors.");

        for (auto& name_notional : outConstituents) {
            TLOG("Trade " << id() << ", Issuer" << name_notional.first << " unscaled Notional: " << name_notional.second
                          << ", scaled Notional: " << name_notional.second * correctionFactor);
            name_notional.second *= correctionFactor;
        }

        totalNtl *= correctionFactor;
        notionals_.tradeDate *= correctionFactor;
        notionals_.valuationDate *= correctionFactor;
        notionals_.realisedFep *= correctionFactor;
    }

    DLOG("All underlyings added, total notional = " << totalNtl);
    if (!close(fullNtl, totalNtl) && totalNtl > fullNtl) {
        StructuredTradeErrorMessage(id(), "IndexCDSOption", "Error building trade",
                                         ("Sum of basket notionals (" + std::to_string(totalNtl) +
                                          ") is greater than trade notional (" + std::to_string(fullNtl) +
                                          "). Check the basket data for possible errors.")
                                        .c_str())
            .log();
    }

    DLOG("Finished building constituents using basket data.");
}

void IndexCreditDefaultSwapOption::fromReferenceData(const Date& asof, map<string, Real>& outConstituents,
                                                     const QuantLib::ext::shared_ptr<ReferenceDataManager>& refData) {

    const string& iCdsId = swap_.creditCurveId();
    DLOG("Start building constituents using credit reference data for " << iCdsId << ".");

    QL_REQUIRE(refData, "Building index CDS option " << id() << " ReferenceDataManager is null.");
    QL_REQUIRE(refData->hasData(CreditIndexReferenceDatum::TYPE, iCdsId),
               "No CreditIndex reference data for " << iCdsId);
    auto referenceData = QuantLib::ext::dynamic_pointer_cast<CreditIndexReferenceDatum>(
        refData->getData(CreditIndexReferenceDatum::TYPE, iCdsId));
    DLOG("Got CreditIndexReferenceDatum for id " << iCdsId);

    Real fullNtl = notionals_.full;
    Real totalWeight = 0.0;
    for (const auto& c : referenceData->constituents()) {

        const auto& name = c.name();
        auto weight = c.weight();

        if (weight == 0.0 || close(0.0, weight)) {

            // Entity is not in the index. Its auction date is in the past.
            QL_REQUIRE(c.priorWeight() != Null<Real>(), "Constituent "
                                                            << name << " in index CDS option trade " << id()
                                                            << " has defaulted so expecting a prior weight.");
            QL_REQUIRE(c.recovery() != Null<Real>(), "Constituent " << name << " in index CDS option trade " << id()
                                                                    << " has defaulted so expecting a recovery.");
            QL_REQUIRE(c.auctionDate() != Date(), "Constituent " << name << " in index CDS option trade " << id()
                                                                 << " has defaulted so expecting an auction date.");
            QL_REQUIRE(c.auctionDate() <= asof, "Constituent " << name << " in index CDS option trade " << id()
                                                               << " has defaulted so expecting the auction date ("
                                                               << io::iso_date(c.auctionDate())
                                                               << ") to be before or on the valuation date ("
                                                               << io::iso_date(asof) << ").");

            totalWeight += c.priorWeight();

            if (tradeDate_ < c.auctionDate()) {
                Real entityNtl = c.priorWeight() * fullNtl;
                TLOG("Trade date (" << io::iso_date(tradeDate_) << ") is before auction date ("
                                    << io::iso_date(c.auctionDate()) << ") of " << name << " so updating trade date "
                                    << "notional by amount " << entityNtl);
                notionals_.tradeDate += entityNtl;
            }

            if (fepStartDate_ < c.auctionDate()) {
                Real recovery = swap_.recoveryRate() != Null<Real>() ? swap_.recoveryRate() : c.recovery();
                Real fepAmount = (1 - recovery) * c.priorWeight() * fullNtl;
                TLOG("FEP start date (" << io::iso_date(fepStartDate_) << ") is before auction date ("
                                        << io::iso_date(c.auctionDate()) << ") of " << name << " so updating realised "
                                        << "FEP by amount " << fepAmount);
                notionals_.realisedFep += fepAmount;
            }

        } else if (weight > 0.0) {

            // Entity is still in the index.
            // Note that it may have defaulted but its auction date is still in the future.
            Real entityNtl = weight * fullNtl;
            auto it = outConstituents.find(name);
            if (it == outConstituents.end()) {
                outConstituents[name] = entityNtl;
                TLOG("Adding underlying " << name << " with weight " << weight << " (notional = " << entityNtl << ")");
            } else {
                it->second += entityNtl;
                TLOG("Updating underlying " << name << " with weight " << weight << " (notional = " << entityNtl
                                            << ")");
            }

            totalWeight += weight;
            notionals_.tradeDate += entityNtl;
            notionals_.valuationDate += entityNtl;

        } else {
            QL_FAIL("Constituent " << name << " in index CDS option trade " << id() << " has a negative weight "
                                   << weight << ".");
        }
    }

    DLOG("All underlyings added, total weight = " << totalWeight);
    if (!close(1.0, totalWeight) && totalWeight > 1.0) {
        ALOG("Total weight is greater than 1, possible error in CreditIndexReferenceDatum for "
             << iCdsId << " while building constituents for trade " << id() << ".");
    }

    DLOG("Finished building constituents using credit reference data.");
}

const CreditPortfolioSensitivityDecomposition IndexCreditDefaultSwapOption::sensitivityDecomposition() const {
    return sensitivityDecomposition_;
}

QuantLib::Real IndexCreditDefaultSwapOption::effectiveStrike() const { return effectiveStrike_; }

const std::string& IndexCreditDefaultSwapOption::effectiveStrikeType() const { return effectiveStrikeType_; }

const QuantLib::Period& IndexCreditDefaultSwapOption::effectiveIndexTerm() const { return effectiveIndexTerm_; }

std::string IndexCreditDefaultSwapOption::creditCurveId() const {
    auto p = splitCurveIdWithTenor(swap_.creditCurveId());
    if (p.second != 0 * Days) {
        // if the credit curve id has a suffix "_5Y" already, we use that
        return swap_.creditCurveId();
    } else if (!indexTerm_.empty()) {
        // if not and we have a term we use that
        return p.first + "_" + indexTerm_;
    } else {
        // if not we imply the term from the swap schedule
        return swap_.creditCurveIdWithTerm();
    }
}

const std::string& IndexCreditDefaultSwapOption::volCurveId() const { return volCurveId_; }

const map<string, Real>& IndexCreditDefaultSwapOption::constituents() const { return constituents_; };

} // namespace data
} // namespace ore
