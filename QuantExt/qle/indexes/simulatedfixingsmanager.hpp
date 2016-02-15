/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file simulatedfixingsmanager.hpp
    \brief global respository for simulated fixings
*/

#ifndef quantlib_simulated_fixings_manager_hpp
#define quantlib_simulated_fixings_manager_hpp

#include <ql/settings.hpp>
#include <ql/timeseries.hpp>
#include <boost/algorithm/string/case_conv.hpp>

// #include <iostream> // only for debug

using boost::algorithm::to_upper_copy;

namespace QuantLib {

/*! global repository for simulated fixings */

template <class T = Real>
class SimulatedFixingsManager_t
    : public Singleton<SimulatedFixingsManager_t<T> > {
    friend class Singleton<SimulatedFixingsManager_t<T> >;

  private:
    SimulatedFixingsManager_t()
        : simulateFixings_(false), horizon_(0), referenceDate_(Null<Date>()) {}

  public:
    /*! If set, estimated fixings are recorded in engines
        that support simulated fixings. If estimated
        multiple times, one the last one is kept. This is
        the one with the greatest evaluation date only if
        a forward simulation is run (TODO, should we keep
        the fixing estimated on the greatest evaluation
        date instead? This would be less efficient though.
        Revisit if necessary).
        If a historic fixing is needed and not found in the
        IndexManager as a native fixing and if simulated
        fixings are enabled, a formerly estimated one will
        be used (no inter- or extrapolation is applied).
    */
    bool &simulateFixings();
    bool simulateFixings() const;

    /*! If set to a non-zero value, only fixing are
      stored that are not more than the given number
      of caledar days in the future. */
    BigInteger &horizon();
    BigInteger horizon() const;

    /*! clears recorded fixings and sets the reference
      date to the current evaluation date */
    void reset() const;
    /*! adds an estimated fixing */
    void addSimulatedFixing(const std::string &name, const Date &fixingDate,
                            const T &value) const;
    /*! returns an interpolated fixing */
    T simulatedFixing(const std::string &name, const Date &fixingDate) const;

  private:
    bool simulateFixings_;
    BigInteger horizon_;
    mutable Date referenceDate_;
    typedef std::map<std::string, TimeSeries<T> > SimulatedData;
    mutable SimulatedData data_;
};

typedef SimulatedFixingsManager_t<Real> SimulatedFixingsManager;

// inline

template <class T>
inline bool &SimulatedFixingsManager_t<T>::simulateFixings() {
    return simulateFixings_;
}

template <class T>
inline bool SimulatedFixingsManager_t<T>::simulateFixings() const {
    return simulateFixings_;
}

template <class T> inline BigInteger &SimulatedFixingsManager_t<T>::horizon() {
    return horizon_;
}

template <class T> inline BigInteger SimulatedFixingsManager_t<T>::horizon() const {
    return horizon_;
}

// implementation

template <class T> void SimulatedFixingsManager_t<T>::reset() const {
    data_.clear();
    referenceDate_ = Settings::instance().evaluationDate();
    // debug
    // std::clog << "reset simulated fixings reference date to " << referenceDate_
    //           << "\n";
    // end debug
}

template <class T>
void SimulatedFixingsManager_t<T>::addSimulatedFixing(const std::string &name,
                                                      const Date &fixingDate,
                                                      const T &value) const {
    QL_REQUIRE(referenceDate_ != Null<Date>(),
               "can not add estimation for simulated fixing for "
                   << name << " @ " << value << " on " << fixingDate
                   << ", need a reset first");

    QL_REQUIRE(referenceDate_ <= fixingDate,
               "can not add estimation for simulated fixing for "
                   << name << " @ " << value << " on " << fixingDate
                   << ", since reference date (" << referenceDate_
                   << ") is past fixing date");

    if (horizon_ == 0 ||
        fixingDate - Settings::instance().evaluationDate() <= horizon_) {
        std::string uname = boost::algorithm::to_upper_copy(name);
        data_[uname][fixingDate] = value;

        // debug
        // std::clog << "recording fixing for " << name << " on " << fixingDate
        //           << " @ " << value << " (evalDate,refDate) = ("
        //           << Settings::instance().evaluationDate() << ","
        //           << referenceDate_ << ")\n";
        // end debug
    }
}

template <class T>
T SimulatedFixingsManager_t<T>::simulatedFixing(const std::string &name,
                                                const Date &fixingDate) const {

    std::string uname = boost::algorithm::to_upper_copy(name);
    T tmp = data_[uname][fixingDate];
    // debug
    // std::clog << "retrieving fixing for " << name << " on " << fixingDate
    //           << " @ " << tmp << " (evalDate,refDate) = ("
    //           << Settings::instance().evaluationDate() << "," << referenceDate_
    //           << ")\n";
    // end debug
    return tmp;
}

} // namespace QuantLib

#endif
