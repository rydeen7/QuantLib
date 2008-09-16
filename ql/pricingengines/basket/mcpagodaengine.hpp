/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Master IMAFA - Polytech'Nice Sophia - Université de Nice Sophia Antipolis

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

/*! \file mcpagodaengine.hpp
    \brief Monte Carlo engine for pagoda options
*/

#ifndef quantlib_mc_pagoda_engine_hpp
#define quantlib_mc_pagoda_engine_hpp

#include <ql/instruments/pagodaoption.hpp>
#include <ql/pricingengines/mcsimulation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/exercise.hpp>

namespace QuantLib {

    //! Pricing engine for pagoda options using Monte Carlo simulation
    template <class RNG = PseudoRandom, class S = Statistics>
    class MCPagodaEngine : public PagodaOption::engine,
                           public McSimulation<MultiVariate,RNG,S> {
      public:
        typedef typename McSimulation<MultiVariate,RNG,S>::path_generator_type
            path_generator_type;
        typedef typename McSimulation<MultiVariate,RNG,S>::path_pricer_type
            path_pricer_type;
        typedef typename McSimulation<MultiVariate,RNG,S>::stats_type
            stats_type;
        // constructor
        MCPagodaEngine(const boost::shared_ptr<StochasticProcessArray>&,
                       bool brownianBridge,
                       bool antitheticVariate,
                       bool controlVariate,
                       Size requiredSamples,
                       Real requiredTolerance,
                       Size maxSamples,
                       BigNatural seed);
        void calculate() const {
            McSimulation<MultiVariate,RNG,S>::calculate(requiredTolerance_,
                                                        requiredSamples_,
                                                        maxSamples_);
            results_.value = this->mcModel_->sampleAccumulator().mean();
            if (RNG::allowsErrorEstimate)
                results_.errorEstimate =
                    this->mcModel_->sampleAccumulator().errorEstimate();
        }
      private:
        // McSimulation implementation
        TimeGrid timeGrid() const;
        boost::shared_ptr<path_generator_type> pathGenerator() const {

            Size numAssets = processes_->size();

            TimeGrid grid = timeGrid();
            typename RNG::rsg_type gen =
                RNG::make_sequence_generator(numAssets*(grid.size()-1),seed_);

            return boost::shared_ptr<path_generator_type>(
                         new path_generator_type(processes_,
                                                 grid, gen, brownianBridge_));
        }
        boost::shared_ptr<path_pricer_type> pathPricer() const;

        // data members
        boost::shared_ptr<StochasticProcessArray> processes_;
        Size requiredSamples_;
        Size maxSamples_;
        Real requiredTolerance_;
        bool brownianBridge_;
        BigNatural seed_;
    };

    class PagodaMultiPathPricer : public PathPricer<MultiPath> {
      public:
        PagodaMultiPathPricer(Real roof, Real fraction,
                              DiscountFactor discount);
        Real operator()(const MultiPath& multiPath) const;
      private:
        DiscountFactor discount_;
        Real roof_, fraction_;
    };


    // template definitions

    template<class RNG, class S>
    inline MCPagodaEngine<RNG,S>::MCPagodaEngine(
                   const boost::shared_ptr<StochasticProcessArray>& processes,
                   bool brownianBridge,
                   bool antitheticVariate,
                   bool controlVariate,
                   Size requiredSamples,
                   Real requiredTolerance,
                   Size maxSamples,
                   BigNatural seed)
    : McSimulation<MultiVariate,RNG,S>(antitheticVariate, controlVariate),
      processes_(processes), requiredSamples_(requiredSamples),
      maxSamples_(maxSamples), requiredTolerance_(requiredTolerance),
      brownianBridge_(brownianBridge), seed_(seed) {
        registerWith(processes_);
    }

    template <class RNG, class S>
    inline TimeGrid MCPagodaEngine<RNG,S>::timeGrid() const {

        std::vector<Time> fixingTimes;
        for (Size i=0; i<arguments_.fixingDates.size(); i++) {
            Time t = processes_->time(arguments_.fixingDates[i]);
            QL_REQUIRE(t >= 0.0, "seasoned options are not handled");
            if (i > 0)
                QL_REQUIRE(t > fixingTimes.back(), "fixing dates not sorted");
            fixingTimes.push_back(t);
        }

        return TimeGrid(fixingTimes.begin(), fixingTimes.end());
    }


    template <class RNG, class S>
    inline
    boost::shared_ptr<typename MCPagodaEngine<RNG,S>::path_pricer_type>
    MCPagodaEngine<RNG,S>::pathPricer() const {

        boost::shared_ptr<GeneralizedBlackScholesProcess> process =
            boost::dynamic_pointer_cast<GeneralizedBlackScholesProcess>(
                                                      processes_->process(0));
        QL_REQUIRE(process, "Black-Scholes process required");

        return boost::shared_ptr<
                         typename MCPagodaEngine<RNG,S>::path_pricer_type>(
            new PagodaMultiPathPricer(arguments_.roof, arguments_.fraction,
                                      process->riskFreeRate()->discount(
                                           arguments_.exercise->lastDate())));
    }

}



#endif