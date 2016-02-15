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

#include <iostream> // only for debug

using boost::algorithm::to_upper_copy;

namespace QuantLib {

/*! global repository for simulated fixings */

template <class T = Real>
class SimulatedFixingsManager_t
    : public Singleton<SimulatedFixingsManager_t<T> > {
    friend class Singleton<SimulatedFixingsManager_t<T> >;

  private:
    SimulatedFixingsManager_t()
        : simulateFixings_(false),
          estimationMethod_(InterpolatedForwardBackward), horizon_(0),
          referenceDate_(Null<Date>()) {}

  public:
    // note that it is in the pricing engine's
    // responsibility to support both the
    // forward and backward method
    enum EstimationMethod {
        Forward,
        Backward,
        BestOfForwardBackward,
        InterpolatedForwardBackward
    };

    /*! If set, estimated fixings are recorded in engines
        that support simulated fixings.
        If a historic fixing is needed and not found in the
        IndexManager as a native fixing and if simulated
        fixings are enabled, a formerly estimated one will
        be used according to the Estimation method.
    */
    bool &simulateFixings();
    bool simulateFixings() const;
    EstimationMethod &estimationMethod();
    EstimationMethod estimationMethod() const;

    /*! If set to a non-zero value, only fixing are
      stored that are not more than the given number
      of caledar days in the future. A value greater
      than the greatest simulation step should be
      chosen. */
    BigInteger &horizon();
    BigInteger horizon() const;

    /*! clears recorded fixings and sets the reference
      date to the current evaluation date */
    void reset() const;
    /*! adds a projected fixing (forward method) */
    void addForwardFixing(const std::string &name, const Date &fixingDate,
                          const T &value) const;
    /*! adds a fixing as of evaluation date (backward method) */
    void addBackwardFixing(const std::string &name, const Date &fixingDate,
                           const T &value) const;
    /*! returns a simulated fixing, might be Null<T>, if data innsufficient */
    T simulatedFixing(const std::string &name, const Date &fixingDate) const;

  private:
    bool simulateFixings_;
    EstimationMethod estimationMethod_;
    BigInteger horizon_;
    mutable Date referenceDate_;
    // forward and backward data
    struct CompHlp {
        bool operator()(const std::pair<Date, T> &o1,
                        const std::pair<Date, T> &o2) {
            return o1.first < o2.first;
        }
    };
    typedef std::map<Date, std::pair<T, Date> > ForwardData;
    typedef std::map<Date, T> BackwardData;
    mutable std::map<std::string, ForwardData> forwardData_;
    mutable std::map<std::string, BackwardData> backwardData_;
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

template <class T>
inline typename SimulatedFixingsManager_t<T>::EstimationMethod &
SimulatedFixingsManager_t<T>::estimationMethod() {
    return estimationMethod_;
}

template <class T>
inline typename SimulatedFixingsManager_t<T>::EstimationMethod
SimulatedFixingsManager_t<T>::estimationMethod() const {
    return estimationMethod_;
}

template <class T> inline BigInteger &SimulatedFixingsManager_t<T>::horizon() {
    return horizon_;
}

template <class T>
inline BigInteger SimulatedFixingsManager_t<T>::horizon() const {
    return horizon_;
}

// implementation

template <class T> void SimulatedFixingsManager_t<T>::reset() const {
    forwardData_.clear();
    backwardData_.clear();
    referenceDate_ = Settings::instance().evaluationDate();
    // debug
    std::clog << "reset simulated fixings reference date to " << referenceDate_
              << "\n";
    // end debug
}

template <class T>
void SimulatedFixingsManager_t<T>::addForwardFixing(const std::string &name,
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
        forwardData_[uname][fixingDate] =
            std::make_pair(value, Settings::instance().evaluationDate());

        // debug
        std::clog << "recording forward fixing for " << name << " on "
                  << fixingDate << " @ " << value << " (evalDate,refDate) = ("
                  << Settings::instance().evaluationDate() << ","
                  << referenceDate_ << ")\n";
        // end debug
    }
}

template <class T>
void SimulatedFixingsManager_t<T>::addBackwardFixing(const std::string &name,
                                                     const Date &fixingDate,
                                                     const T &value) const {

    QL_REQUIRE(referenceDate_ != Null<Date>(),
               "can not add estimation for simulated fixing for "
                   << name << " @ " << value << " on " << fixingDate
                   << ", need a reset first");

    QL_REQUIRE(referenceDate_ <= fixingDate &&
                   fixingDate == Settings::instance().evaluationDate(),
               "can not add estimation for simulated fixing for "
                   << name << " @ " << value << " on " << fixingDate
                   << ", since reference date (" << referenceDate_
                   << ") <= fixing date = evaluationDate ("
                   << Settings::instance().evaluationDate() << ") required.");

    std::string uname = boost::algorithm::to_upper_copy(name);
    backwardData_[uname][fixingDate] = value;

    // debug
    std::clog << "recording backward fixing for " << name << " on "
              << fixingDate << " @ " << value << " (evalDate,refDate) = ("
              << Settings::instance().evaluationDate() << "," << referenceDate_
              << ")\n";
    // end debug
}

template <class T>
T SimulatedFixingsManager_t<T>::simulatedFixing(const std::string &name,
                                                const Date &fixingDate) const {

    std::string uname = boost::algorithm::to_upper_copy(name);

    T bwdTmp = Null<T>();
    Date bwdDate = Null<Date>();
    if (estimationMethod_ == Backward ||
        estimationMethod_ == BestOfForwardBackward ||
        estimationMethod_ == InterpolatedForwardBackward) {
        typename BackwardData::const_iterator it = std::lower_bound(
            backwardData_[uname].begin(), backwardData_[uname].end(),
            std::make_pair(fixingDate, T(0.0)), CompHlp());
        // fixing date not > all backward data
        if (it != backwardData_[uname].end()) {
            bwdDate = it->first;
            bwdTmp = it->second;
            // debug
            std::clog << "retrieving bwd-fixing for " << name << " on "
                      << bwdDate << " @ " << bwdTmp
                      << " (evalDate,refDate,fixingDate) = ("
                      << Settings::instance().evaluationDate() << ","
                      << referenceDate_ << "," << fixingDate << ")\n";
            // end debug
        }
        if (estimationMethod_ == Backward) {
            // might be Null<T>
            return bwdTmp;
        }
    }

    T fwdTmp = Null<T>();
    Date fwdDate = Null<Date>();
    typename ForwardData::const_iterator it =
        forwardData_[uname].find(fixingDate);
    if (it != forwardData_[uname].end()) {
        fwdTmp = it->second.first;
        fwdDate = it->second.second;
    }
    // debug
    std::clog << "retrieving forward fixing for " << name << " on "
              << fixingDate << " @ " << fwdTmp << " (evalDate,refDate) = ("
              << Settings::instance().evaluationDate() << "," << referenceDate_
              << ")\n";
    // end debug

    if (estimationMethod_ == Forward) {
        // might be Null<T>
        return fwdTmp;
    }

    // remaining methods are requiring both fwd and bwd estimate
    // if only one is available, we fall back on the respective
    // method
    if (bwdTmp == Null<T>()) {
        // debug
        std::clog << "only fwd estimate available, returning " << fwdTmp
                  << std::endl;
        // end debug
        return fwdTmp;
    }
    if (fwdTmp == Null<T>()) {
        // debug
        std::clog << "only bwd estimate available, returning " << fwdTmp
                  << std::endl;
        // end debug
        return bwdTmp;
    }

    // we have both estimates
    BigInteger fwdDistance = fixingDate - fwdDate;
    BigInteger bwdDistance = bwdDate - fixingDate;
    // debug
    std::clog << "best of or interpolated method, forward distance = "
              << fwdDistance << ", backward distance = " << bwdDistance
              << " => ";
    // end debug
    if (estimationMethod_ == BestOfForwardBackward) {
        if (fwdDistance <= bwdDistance) {
            // debug
            std::clog << fwdTmp << std::endl;
            // end debug
            return fwdTmp;
        } else {
            // debug
            std::clog << bwdTmp << std::endl;
            // end debug
            return bwdTmp;
        }
    }

    if (estimationMethod_ == InterpolatedForwardBackward) {
        T tmp = (fwdTmp * static_cast<T>(bwdDistance) +
                 bwdTmp * static_cast<T>(fwdDistance)) /
                (static_cast<T>(bwdDistance) + static_cast<T>(fwdDistance));
        // debug
        std::clog << tmp << std::endl;
        // end debug
        return tmp;
    }

    QL_FAIL("unexpected estimation method (" << estimationMethod_ << ")");
}

} // namespace QuantLib

#endif
