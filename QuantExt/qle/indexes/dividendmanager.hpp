/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/dividendmanager.hpp
    \brief Dividend manager
*/

#ifndef quantext_divdendmanager_hpp
#define quantext_divdendmanager_hpp

#include <ql/patterns/singleton.hpp>
#include <ql/timeseries.hpp>
#include <ql/utilities/observablevalue.hpp>

namespace QuantExt {

struct Dividend {
    //! Ex dividend date
    QuantLib::Date exDate = QuantLib::Date();
    //! Index name
    std::string name = std::string();
    //! Dividend rate
    QuantLib::Real rate = QuantLib::Null<QuantLib::Real>();
    //! Dividend Payment date
    QuantLib::Date payDate = QuantLib::Date();

    //! Constructor
    Dividend() {}
    Dividend(const QuantLib::Date& ed, const std::string& s, const QuantLib::Real r, const QuantLib::Date& pd)
        : exDate(ed), name(s), rate(r), payDate(pd) {}
    bool empty() {
        return name.empty() && exDate == QuantLib::Date() && rate == QuantLib::Null<QuantLib::Real>() &&
               payDate == QuantLib::Date();
    }
};

//! Compare dividends
bool operator==(const Dividend& d1, const Dividend& d);
bool operator<(const Dividend& d1, const Dividend& d2);
std::ostream& operator<<(std::ostream&, Dividend);

//! Utility to write a set of dividends in the dividend manager's history
void applyDividends(const std::set<Dividend>& dividends);

//! global repository for past dividends
/*! \note index names are case insensitive */
class DividendManager : public QuantLib::Singleton<DividendManager> {
    friend class QuantLib::Singleton<DividendManager>;

private:
    DividendManager() = default;

public:
    //! returns whether historical fixings were stored for the index
    bool hasHistory(const std::string& name) const;
    //! returns the (possibly empty) history of the index fixings
    const std::set<Dividend>& getHistory(const std::string& name);
    //! stores the historical fixings of the index
    void setHistory(const std::string& name, const std::set<Dividend>&);
    //! observer notifying of changes in the index fixings
    QuantLib::ext::shared_ptr<QuantLib::Observable> notifier(const std::string& name);
    void clearHistory(const std::string& name);
    void clearHistories();

private:
    typedef std::map<std::string, QuantLib::ObservableValue<std::set<Dividend>>> history_map;
    history_map data_;
};

} // namespace QuantExt

#endif
