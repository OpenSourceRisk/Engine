/*
 Copyright (C) 2020 Quaternion Risk Management Ltd.
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

/*! \file qle/instruments/bondbasket.hpp
    \brief Basket of defaultable bonds
*/

#pragma once

#include <ql/experimental/credit/pool.hpp>
#include <qle/indexes/fxindex.hpp>
#include <ql/instruments/bond.hpp>

namespace QuantLib {

/*! \relates Currency */
bool operator<(const Currency&, const Currency&);
bool operator>(const Currency&, const Currency&);

inline bool operator<(const Currency& c1, const Currency& c2) { return c1.name() < c2.name(); }

inline bool operator>(const Currency& c1, const Currency& c2) { return c1.name() > c2.name(); }

} // namespace QuantLib

namespace QuantExt {
using namespace QuantLib;

class Cash {
public:
    Cash(Real flow = 0.0, Real discountedFlow = 0.0) : flow_(flow), discountedFlow_(discountedFlow) {}
    Real flow_;
    Real discountedFlow_;
};

// Sum the undiscounted values of two cash objects
Real sum(const Cash& c, const Cash& d);

// Sum the discounted values of two cash objects
Real sumDiscounted(const Cash& c, const Cash& d);

//! Bond Basket
/*!
  This class holds a basket of defaultable bonds along with the pool of
  relevant names. There may be more bonds than names involved, e.g.
  several different bonds with same issuer.

  The class provides tools for evaluating basket cash flows of different
  kinds (interest, principal) for scenarios of default times stored  for
  all names involved in the Pool structure.

  For further information refer to the detailed QuantExt documentation.

  \ingroup instruments
 */
class BondBasket {
public:
    // BondBasket() {}
    BondBasket(
        //! map of QuantLib bonds
        const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::Bond>>& qlBonds,
        //! recoveries per bonds
        const std::map<std::string, double>& recoveries,
        //! multipliers per bonds
        const std::map<std::string, double>& multipliers,
        //! discount curves per bonds
        const std::map<std::string, QuantLib::Handle<QuantLib::YieldTermStructure>>& yieldTermStructures,
        //! currencies per bonds
        const std::map<std::string, Currency>& currencies,
        //! Pool storing default time for all names involved in the basekt above
        const QuantLib::ext::shared_ptr<QuantLib::Pool> pool,
        //! Base currency
        Currency baseCcy,
        //! Forex structure to compute spot and forward FX rates
        const std::map <std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndexMap,
        //! end of reinvestment period
        const QuantLib::Date & reinvestmentEndDate,
        //! scalar for reinvestment period per bonds
        const std::map<std::string, std::vector<double>>& reinvestmentScalar,
        //! flow types of cashflows per bonds
        const std::map<std::string, std::vector<std::string>>& flowType);
    //! Inspectors
    //@{
    /*! Vector of risky bonds */
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::Bond>>& bonds() const { return qlBonds_; }
    /*! Pool of names with associated default times */
    const QuantLib::ext::shared_ptr<QuantLib::Pool>& pool() const;
    /*! Forex structure */
    const std::map <std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>>& fxIndexMap() const { return fxIndexMap_;}
    /*! Unique currencies involved */
    const std::set<QuantLib::Currency> unique_currencies() const { return unique_currencies_; }
    /*! Recovery rate for given name */
    const double recoveryRate (const std::string& name) const;
    /*! Multiplier for given name */
    const double multiplier (const std::string& name) const ;

    /*! FX conversion */
    Real convert(Real amount, Currency ccy, Date date = Date());
    //@}
    /*!
      Set the date grid for mapping cash flows.
      Store for each bond cash flow date the associated date grid bucket.
     */
    void setGrid(std::vector<Date> dates);
    std::map<Currency, std::vector<Cash>> scenarioCashflow(std::vector<Date> dates);
    std::map<Currency, std::vector<Cash>> scenarioInterestflow(std::vector<Date> dates);
    std::map<Currency, std::vector<Cash>> scenarioPrincipalflow(std::vector<Date> dates);
    std::map<Currency, std::vector<Real>> scenarioRemainingNotional(std::vector<Date> dates);
    std::map<Currency, std::vector<Cash>> scenarioLossflow(std::vector<Date> dates);
    std::map<Currency, std::vector<Cash>> scenarioFeeflow(const std::vector<QuantLib::Date>& dates);

    void fillFlowMaps();

private:

    const double getScalar(const std::string& name, const QuantLib::Date& currentDate) const;
    const QuantLib::Handle<QuantLib::YieldTermStructure> yts(const std::string& name) const;
    const Currency currency(const std::string& name) const;
    const std::vector<double> reinvestmentScalar(const std::string& name) const;
    const std::string flowType(const std::string& name, int idx) const;
    const QuantLib::ext::shared_ptr<QuantExt::FxIndex> fxIndex(const std::string& name) const;

    //members filled by input arguments
    const std::map<std::string, QuantLib::ext::shared_ptr<QuantLib::Bond>> qlBonds_;
    const std::map<std::string, double> recoveries_;
    const std::map<std::string, double> multipliers_;
    const std::map<std::string, QuantLib::Handle<QuantLib::YieldTermStructure>> yieldTermStructures_;
    const std::map<std::string, QuantLib::Currency> currencies_;
    const QuantLib::ext::shared_ptr<QuantLib::Pool> pool_;
    const Currency baseCcy_;
    const std::map <std::string, QuantLib::ext::shared_ptr<QuantExt::FxIndex>> fxIndexMap_;
    const Date reinvestmentEndDate_;
    const std::map<std::string, std::vector<double>> reinvestmentScalar_;
    const std::map<std::string, std::vector<std::string>> flowType_;

    std::set<QuantLib::Currency> unique_currencies_;
    std::vector<Date> grid_;

    std::map<std::string, std::vector<int>> cashflow2grid_;
    std::map<std::string, std::vector<int>> interestflow2grid_;
    std::map<std::string, std::vector<int>> notionalflow2grid_;
    std::map<std::string, std::vector<int>> feeflow2grid_;

    std::map<std::string, std::vector<ext::shared_ptr<QuantLib::CashFlow>>> cashflows_;         // full leg_
    std::map<std::string, std::vector<ext::shared_ptr<QuantLib::CashFlow>>> interestFlows_;     // interestLeg_
    std::map<std::string, std::vector<ext::shared_ptr<QuantLib::CashFlow>>> notionalFlows_;     // redemptionLeg_
    std::map<std::string, std::vector<ext::shared_ptr<QuantLib::CashFlow>>> feeFlows_;          // feeLeg_

};

} // namespace QuantExt
