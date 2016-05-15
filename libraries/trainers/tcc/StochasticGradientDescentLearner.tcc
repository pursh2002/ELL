////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Project:  Embedded Machine Learning Library (EMLL)
//  File:     StochasticGradientDescentLearner.tcc (trainers)
//  Authors:  Ofer Dekel
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BinaryClassificationEvaluator.h"
#include "SingleEpochTrainer.h"

// stl
#include <cmath>
#include <cassert>

// dataset
#include "RowDataset.h"

namespace trainers
{
    template<typename LossFunctionType>
    StochasticGradientDescentLearner<LossFunctionType>::StochasticGradientDescentLearner(uint64_t dim, const StochasticGradientDescentLearnerParameters& parameters, const LossFunctionType& lossFunction) :
        _parameters(parameters), _lossFunction(lossFunction), _total_iterations(1), _lastPredictor(dim), _averagedPredictor(dim) // iterations start from 1 to prevent divide-by-zero
    {}

    template<typename LossFunctionType>
    void StochasticGradientDescentLearner<LossFunctionType>::Update(dataset::GenericRowDataset::Iterator exampleIterator)
    {
        // get references to the vector and biases
        auto& vLast = _lastPredictor.GetVector();
        auto& vAvg = _averagedPredictor.GetVector();

        double& bLast = _lastPredictor.GetBias();
        double& bAvg = _averagedPredictor.GetBias();

        // define some constants
        const double T_prev = double(_total_iterations);
        const double T_next = T_prev + exampleIterator.NumIteratesLeft();
        const double eta = 1.0 / _parameters.regularization / T_prev;
        const double sigma = std::log(T_next) + 0.5 / T_next;

        // calulate the contribution of the old lastPredictor to the new avergedPredictor
        const double historyWeight = sigma - std::log(T_prev) - 0.5 / T_prev;
        vLast.AddTo(vAvg, historyWeight);
        bAvg += bLast * historyWeight;
        while(exampleIterator.IsValid())
        {
            ++_total_iterations;
            double t = (double)_total_iterations;

            // get the Next example
            const auto& example = exampleIterator.Get();
            double label = example.GetLabel();
            double weight = example.GetWeight();
            const auto& dataVector = example.GetDataVector();

            // calculate the prediction 
            double alpha = T_prev / (t-1) * _lastPredictor.Predict(dataVector);

            // calculate the loss derivative
            double beta = weight * _lossFunction.GetDerivative(alpha, label);

            // Update vLast and vAvg
            double lastCoeff = -eta*beta;
            dataVector.AddTo(vLast, lastCoeff);
            bLast += lastCoeff;

            double avgCoeff = lastCoeff*(sigma - std::log(t) - 0.5/t);
            dataVector.AddTo(vAvg, avgCoeff);
            bAvg += avgCoeff;

            // move on
            exampleIterator.Next();
        }
        
        assert((double)_total_iterations == T_next);

        // calculate w and w_avg
        double scale = T_prev / T_next;
        _lastPredictor.Scale(scale);
        _averagedPredictor.Scale(scale);
    }

    template<typename LossFunctionType>
    const predictors::LinearPredictor& StochasticGradientDescentLearner<LossFunctionType>::GetPredictor() const
    {
        return _averagedPredictor;
    }

    template<typename LossFunctionType>
    predictors::LinearPredictor StochasticGradientDescentLearner<LossFunctionType>::Reset()
    {
        _total_iterations = 1;
        _lastPredictor.Reset();
        predictors::LinearPredictor averagedPredictor(_averagedPredictor.GetDimension());
        predictors::LinearPredictor::Swap(_averagedPredictor, averagedPredictor);
        return averagedPredictor;
    }

    template <typename LossFunctionType>
    std::unique_ptr<ILearner<predictors::LinearPredictor>> MakeStochasticGradientDescentLearner(uint64_t dim, const StochasticGradientDescentLearnerParameters& parameters, const LossFunctionType& lossFunction)
    {
        return std::make_unique<StochasticGradientDescentLearner<LossFunctionType>>(dim, parameters, lossFunction);
    }

    template<typename LossFunctionType>
    std::unique_ptr<ITrainer<predictors::LinearPredictor>> trainers::MakeStochasticGradientDescentTrainer(uint64_t dim, const StochasticGradientDescentLearnerParameters& parameters, const LossFunctionType & lossFunction)
    {
        return std::unique_ptr<SingleEpochTrainer<predictors::LinearPredictor>>(MakeStochasticGradientDescentLearner(dim, parameters, lossfunction));
    }
}
