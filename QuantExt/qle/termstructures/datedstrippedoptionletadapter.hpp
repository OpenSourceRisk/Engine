/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file qle/termstructures/datedstrippedoptionletadapter.hpp
    \brief StrippedOptionlet Adapter
*/

#pragma once

#include <qle/termstructures/datedstrippedoptionletbase.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/math/interpolation.hpp>
#include <ql/math/interpolations/sabrinterpolation.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Adapter class for turning a DatedStrippedOptionletBase object into an OptionletVolatilityStructure
    /*! Takes a DatedStrippedOptionletBase and converts it into an OptionletVolatilityStructure with a fixed 
        reference date
    */
    class DatedStrippedOptionletAdapter : public OptionletVolatilityStructure, public LazyObject {
      public:
        DatedStrippedOptionletAdapter(const boost::shared_ptr<DatedStrippedOptionletBase>& s);

        //! \name TermStructure interface
        //@{
        Date maxDate() const;
        //@}
        //! \name VolatilityTermStructure interface
        //@{
        Rate minStrike() const;
        Rate maxStrike() const;
        //@}
        //! \name LazyObject interface
        //@{
        void update();
        void performCalculations() const;
        //@}

        VolatilityType volatilityType() const;
        Real displacement() const;

      protected:
        //! \name OptionletVolatilityStructure interface
        //@{
        boost::shared_ptr<SmileSection> smileSectionImpl(Time optionTime) const;
        Volatility volatilityImpl(Time length, Rate strike) const;
        //@}

      private:
        const boost::shared_ptr<DatedStrippedOptionletBase> optionletStripper_;
        Size nInterpolations_;
        mutable vector<boost::shared_ptr<Interpolation>> strikeInterpolations_;
    };

    inline void DatedStrippedOptionletAdapter::update() {
        TermStructure::update();
        LazyObject::update();
    }
}
