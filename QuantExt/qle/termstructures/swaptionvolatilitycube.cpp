/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Ferdinando Ametrano
 Copyright (C) 2006 Katiuscia Manzoni

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/termstructures/swaptionvolatilitycube.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/termstructures/volatility/interpolatedsmilesection.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <ql/math/interpolations/flatextrapolation2d.hpp>


namespace QuantExt {

    SwaptionVolatilityCube::SwaptionVolatilityCube(
            const std::vector<Period>& optionTenors,
            const std::vector<Period>& swapTenors,
            const std::vector<Spread>& strikeSpreads,
            const std::vector<std::vector<std::vector<Handle<Quote> > > >& vols,
            const boost::shared_ptr<QuantLib::SwapIndex>& swapIndexBase,
            const boost::shared_ptr<QuantLib::SwapIndex>& shortSwapIndexBase,
            bool flatExtrapolation,
            VolatilityType volatilityType,
            BusinessDayConvention businessDayConvention,
            const DayCounter& dayCounter,
            const Calendar& calendar,
            Natural settlementDays,
            const std::vector<std::vector<Real> >& shifts
            )

    : SwaptionVolatilityDiscrete(optionTenors, swapTenors, 0,
                                 calendar,
                                 businessDayConvention,
                                 dayCounter),
      nStrikes_(strikeSpreads.size()),
      strikeSpreads_(strikeSpreads),
      localStrikes_(nStrikes_),
      localSmile_(nStrikes_),
      vols_(vols),
      swapIndexBase_(swapIndexBase),
      shortSwapIndexBase_(shortSwapIndexBase),
      flatExtrapolation_(flatExtrapolation),
      calendar_(calendar),
      settlementDays_(settlementDays),
      maxTenor_(100*Years),
      shifts_(optionTenors.size(), swapTenors.size(), 0.0),
      volsInterpolator_(nStrikes_),
      volatilityType_(volatilityType),
      volsMatrix_(nStrikes_, Matrix(optionTenors.size(), swapTenors.size(), 0.0))
    {
        QL_REQUIRE(swapIndexBase,"swapIndex empty");
        QL_REQUIRE(shortSwapIndexBase,"swapIndex empty");
        QL_REQUIRE(std::find(strikeSpreads_.begin(), strikeSpreads_.end(), 0) != strikeSpreads_.end(), 
            "strikeSpreads must contain 0.0 for atm vols");
        
        for (Size i=1; i<nStrikes_; ++i)
            QL_REQUIRE(strikeSpreads_[i-1]<strikeSpreads_[i],
                       "non increasing strike spreads: " <<
                       io::ordinal(i) << " is " << strikeSpreads_[i-1] << ", " <<
                       io::ordinal(i+1) << " is " << strikeSpreads_[i]);

        QL_REQUIRE(!vols_.empty(), "empty vol spreads matrix");

        QL_REQUIRE(strikeSpreads_.size()==vols_.size(),
            "mismatch between number of strikeSpreads (" <<
            strikeSpreads_.size() << ") and number of rows (" <<
            vols_.size() << ")");
        QL_REQUIRE(nOptionTenors_==vols_[0].size(),
            "mismatch between number of swap tenors (" <<
            nSwapTenors_ << ") and number of rows (" <<
            vols_[0].size() << ")");
            
        registerWith(swapIndexBase_);
        registerWith(shortSwapIndexBase_);

        QL_REQUIRE(shortSwapIndexBase_->tenor()<swapIndexBase_->tenor(),
                   "short index tenor (" << shortSwapIndexBase_->tenor() <<
                   ") is not less than index tenor (" <<
                   swapIndexBase_->tenor() << ")");

        registerWithVolatility();
        registerWith(Settings::instance().evaluationDate());
        evaluationDate_ = Settings::instance().evaluationDate();

        if(shifts != std::vector<std::vector<Real> >()) {
            for (Size i=0; i<optionTenors.size(); ++i) {
                for (Size j=0; j<swapTenors.size(); ++j) {
                        shifts_[i][j] = shifts[i][j];
                }
            }
        }
        
        QL_REQUIRE(optionTimes_.size()>0,"optionTimes empty");
        QL_REQUIRE(swapLengths_.size()>0,"swapLengths empty");
        if (flatExtrapolation) {
            interpolationShifts_ =
                FlatExtrapolator2D(boost::make_shared<BilinearInterpolation>(
                    swapLengths_.begin(), swapLengths_.end(),
                    optionTimes_.begin(), optionTimes_.end(), shifts_));
        } else {
            interpolationShifts_ = BilinearInterpolation(
                swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(),
                optionTimes_.end(), shifts_);
        }        
        
    }

    void SwaptionVolatilityCube::performCalculations() const {
        QL_REQUIRE(nStrikes_ >= requiredNumberOfStrikes(),
                    "too few strikes (" << nStrikes_
                                        << ") required are at least "
                                        << requiredNumberOfStrikes());
        SwaptionVolatilityDiscrete::performCalculations();
            //! set volSpreadsMatrix_ by volSpreads_ quotes
        for (Size i = 0; i < nStrikes_; i++)
            for (Size j = 0; j < nOptionTenors_; j++)
                for (Size k = 0; k < nSwapTenors_; k++) {
                    volsMatrix_[i][j][k] = vols_[i][j][k]->value();
                }

        //! create volSpreadsInterpolator_
        for (Size i = 0; i < nStrikes_; i++) {
            volsInterpolator_[i] = BilinearInterpolation(
                swapLengths_.begin(), swapLengths_.end(), optionTimes_.begin(), optionTimes_.end(), volsMatrix_[i]);
            volsInterpolator_[i].enableExtrapolation();
        }
    }
    
    void SwaptionVolatilityCube::registerWithVolatility()
    {
        for (Size i=0; i<nStrikes_; i++)
            for (Size j=0; j<nOptionTenors_; j++)
                for (Size k=0; k<nSwapTenors_; k++)
                    registerWith(vols_[i][j][k]);
    }

    Rate SwaptionVolatilityCube::atmStrike(const Date& optionD,
                                           const Period& swapTenor) const {
        // FIXME use a familyName-based index factory
        if (swapTenor > shortSwapIndexBase_->tenor()) {
            if (swapIndexBase_->exogenousDiscount()) {
                return SwapIndex(swapIndexBase_->familyName(),
                                 swapTenor,
                                 swapIndexBase_->fixingDays(),
                                 swapIndexBase_->currency(),
                                 swapIndexBase_->fixingCalendar(),
                                 swapIndexBase_->fixedLegTenor(),
                                 swapIndexBase_->fixedLegConvention(),
                                 swapIndexBase_->dayCounter(),
                                 swapIndexBase_->iborIndex(),
                                 swapIndexBase_->discountingTermStructure())
                    .fixing(optionD);
            } else {
                return SwapIndex(swapIndexBase_->familyName(),
                                 swapTenor,
                                 swapIndexBase_->fixingDays(),
                                 swapIndexBase_->currency(),
                                 swapIndexBase_->fixingCalendar(),
                                 swapIndexBase_->fixedLegTenor(),
                                 swapIndexBase_->fixedLegConvention(),
                                 swapIndexBase_->dayCounter(),
                                 swapIndexBase_->iborIndex())
                    .fixing(optionD);
            }
        } else {
            if (shortSwapIndexBase_->exogenousDiscount()) {
                return SwapIndex(shortSwapIndexBase_->familyName(),
                                 swapTenor,
                                 shortSwapIndexBase_->fixingDays(),
                                 shortSwapIndexBase_->currency(),
                                 shortSwapIndexBase_->fixingCalendar(),
                                 shortSwapIndexBase_->fixedLegTenor(),
                                 shortSwapIndexBase_->fixedLegConvention(),
                                 shortSwapIndexBase_->dayCounter(),
                                 shortSwapIndexBase_->iborIndex(),
                                 shortSwapIndexBase_->discountingTermStructure())
                    .fixing(optionD);
            } else {
                return SwapIndex(shortSwapIndexBase_->familyName(),
                                 swapTenor,
                                 shortSwapIndexBase_->fixingDays(),
                                 shortSwapIndexBase_->currency(),
                                 shortSwapIndexBase_->fixingCalendar(),
                                 shortSwapIndexBase_->fixedLegTenor(),
                                 shortSwapIndexBase_->fixedLegConvention(),
                                 shortSwapIndexBase_->dayCounter(),
                                 shortSwapIndexBase_->iborIndex())
                    .fixing(optionD);
            }
        }
    }
    boost::shared_ptr<SmileSection> SwaptionVolatilityCube::smileSectionImpl(Time optionTime, Time swapLength) const {
        calculate();
        Date optionDate = optionDateFromTime(optionTime);
        Rounding rounder(0);
        Period swapTenor(static_cast<Integer>(rounder(swapLength * 12.0)), Months);
        // ensure that option date is valid fixing date
        optionDate = swapTenor > shortSwapIndexBase_->tenor()
                         ? swapIndexBase_->fixingCalendar().adjust(optionDate, Following)
                         : shortSwapIndexBase_->fixingCalendar().adjust(optionDate, Following);
        return smileSectionImpl(optionDate, swapTenor);
    }

    boost::shared_ptr<SmileSection> SwaptionVolatilityCube::smileSectionImpl(const Date& optionDate,
                                                                   const Period& swapTenor) const {

        calculate();
        Rate atmForward = atmStrike(optionDate, swapTenor);
        Time optionTime = timeFromReference(optionDate);
        Real exerciseTimeSqrt = std::sqrt(optionTime);
        std::vector<Real> strikes, stdDevs;
        strikes.reserve(nStrikes_);
        stdDevs.reserve(nStrikes_);
        Time length = swapLength(swapTenor);
        for (Size i = 0; i < nStrikes_; ++i) {
            strikes.push_back(atmForward + strikeSpreads_[i]);
            stdDevs.push_back(exerciseTimeSqrt * (volsInterpolator_[i](length, optionTime)));
        }

        Real shift = interpolationShifts_(optionTime, length, true);
        boost::shared_ptr<SmileSection> tmp;
        if (!flatExtrapolation_)
            tmp = boost::shared_ptr<SmileSection>(new InterpolatedSmileSection<Linear>(
                optionTime, strikes, stdDevs, atmForward, Linear(), Actual365Fixed(), volatilityType(), shift));
        else
            tmp = boost::shared_ptr<SmileSection>(new InterpolatedSmileSection<LinearFlat>(
                optionTime, strikes, stdDevs, atmForward, LinearFlat(), Actual365Fixed(), volatilityType(), shift));

        QL_REQUIRE(tmp, "smile building failed");

        return tmp;
}

}
