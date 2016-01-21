/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
 All rights reserved
*/

/*! \file crossccybasisswap.hpp
    \brief Cross currency basis swap instrument
*/

#ifndef quantext_cross_ccy_basis_swap_hpp
#define quantext_cross_ccy_basis_swap_hpp

#include <ql/time/schedule.hpp>
#include <ql/indexes/iborindex.hpp>

#include <qle/instruments/crossccyswap.hpp>

namespace QuantExt {
    
    //! Cross currency basis swap
    /*! The first leg holds the pay currency cashflows and second leg holds 
        the receive currency cashflows.

        \ingroup instruments
    */
    class CrossCcyBasisSwap : public CrossCcySwap {
      public:
        class arguments;
        class results;
        //! \name Constructors
		//@{
        /*! First leg holds the pay currency cashflows and the second leg 
            holds the receive currency cashflows.
        */
        CrossCcyBasisSwap(Real payNominal,
            const Currency& payCurrency,
            const Schedule& paySchedule,
            const boost::shared_ptr<IborIndex>& payIndex,
            Spread paySpread,
            Real recNominal,
            const Currency& recCurrency,
            const Schedule& recSchedule,
            const boost::shared_ptr<IborIndex>& recIndex,
            Spread recSpread);
        //@}
        //! \name Instrument interface
        //@{
        void setupArguments(PricingEngine::arguments* args) const;
        void fetchResults(const PricingEngine::results*) const;
        //@}
        //! \name Inspectors
        //@{
        Real payNominal() const { return payNominal_; }
        const Currency& payCurrency() const { return payCurrency_; }
        const Schedule& paySchedule() const { return paySchedule_; }
        const boost::shared_ptr<IborIndex>& payIndex() const {
            return payIndex_;
        } 
        Spread paySpread() const { return paySpread_; }

        Real recNominal() const { return recNominal_; }
        const Currency& recCurrency() const { return recCurrency_; }
        const Schedule& recSchedule() const { return recSchedule_; }
        const boost::shared_ptr<IborIndex>& recIndex() const {
            return recIndex_;
        }
        Spread recSpread() const { return recSpread_; }
        //@}

        //! \name Additional interface
        //@{
        Spread fairPaySpread() const {
            calculate();
            QL_REQUIRE(fairPaySpread_ != Null<Real>(),
                "Fair pay spread is not available");
            return fairPaySpread_;
        }
        Spread fairRecSpread() const {
            calculate();
            QL_REQUIRE(fairRecSpread_ != Null<Real>(),
                "Fair pay spread is not available");
            return fairRecSpread_;
        }
        //@}
      
      protected:
        //! \name Instrument interface
        //@{
        void setupExpired() const;
        //@}

      private:
        void initialize();

        Real payNominal_;
        Currency payCurrency_;
        Schedule paySchedule_;
        boost::shared_ptr<IborIndex> payIndex_;
        Spread paySpread_;
        
        Real recNominal_;
        Currency recCurrency_;
        Schedule recSchedule_;
        boost::shared_ptr<IborIndex> recIndex_;
        Spread recSpread_;
        
        mutable Spread fairPaySpread_;
        mutable Spread fairRecSpread_;
    };

    class CrossCcyBasisSwap::arguments : public CrossCcySwap::arguments {
      public:
        Spread paySpread;
        Spread recSpread;
        void validate() const;
    };

    class CrossCcyBasisSwap::results : public CrossCcySwap::results {
      public:
        Spread fairPaySpread;
        Spread fairRecSpread;
        void reset();
    };

}

#endif
