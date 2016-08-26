#include "CNTKLibrary.h"
#include <functional>
#include "Common.h"

using namespace CNTK;

using namespace std::placeholders;

void TrainSimpleFeedForwardClassifer(const DeviceDescriptor& device)
{
    const size_t inputDim = 2;
    const size_t numOutputClasses = 2;
    const size_t hiddenLayerDim = 50;
    const size_t numHiddenLayers = 2;

    const size_t minibatchSize = 25;
    const size_t numSamplesPerSweep = 10000;
    const size_t numSweepsToTrainWith = 2;
    const size_t numMinibatchesToTrain = (numSamplesPerSweep * numSweepsToTrainWith) / minibatchSize;

    auto minibatchSource = CreateTextMinibatchSource(L"SimpleDataTrain_cntk_text.txt", (size_t)2, (size_t)2, 0);
    auto streamInfos = minibatchSource->StreamInfos();
    auto featureStreamInfo = std::find_if(streamInfos.begin(), streamInfos.end(), [](const StreamInformation& streamInfo) { return (streamInfo.m_name == L"features"); });
    auto labelStreamInfo = std::find_if(streamInfos.begin(), streamInfos.end(), [](const StreamInformation& streamInfo) { return (streamInfo.m_name == L"labels"); });

    std::unordered_map<StreamInformation, std::pair<NDArrayViewPtr, NDArrayViewPtr>> inputMeansAndInvStdDevs = { { *featureStreamInfo, { nullptr, nullptr } } };
    ComputeInputPerDimMeansAndInvStdDevs(minibatchSource, inputMeansAndInvStdDevs);

    auto nonLinearity = std::bind(Sigmoid, _1, L"");
    Variable input({ inputDim }, DataType::Float, L"features");
    auto normalizedinput = PerDimMeanVarianceNormalize(input, inputMeansAndInvStdDevs[*featureStreamInfo].first, inputMeansAndInvStdDevs[*featureStreamInfo].second);
    auto classifierOutput = FullyConnectedDNNLayer(normalizedinput, hiddenLayerDim, device, nonLinearity);
    for (size_t i = 1; i < numHiddenLayers; ++i)
        classifierOutput = FullyConnectedDNNLayer(classifierOutput, hiddenLayerDim, device, nonLinearity);

    auto outputTimesParam = Parameter(NDArrayView::RandomUniform<float>({ numOutputClasses, hiddenLayerDim }, -0.05, 0.05, 1, device));
    auto outputBiasParam = Parameter(NDArrayView::RandomUniform<float>({ numOutputClasses }, -0.05, 0.05, 1, device));
    classifierOutput = Plus(outputBiasParam, Times(outputTimesParam, classifierOutput));

    Variable labels({ numOutputClasses }, DataType::Float, L"labels");
    auto trainingLoss = CNTK::CrossEntropyWithSoftmax(classifierOutput, labels, L"lossFunction");;
    auto prediction = CNTK::ClassificationError(classifierOutput, labels, L"classificationError");

    auto oneHiddenLayerClassifier = CNTK::Combine({ trainingLoss, prediction, classifierOutput }, L"classifierModel");

    double learningRatePerSample = 0.02;
    minibatchSource = CreateTextMinibatchSource(L"SimpleDataTrain_cntk_text.txt", (size_t)2, (size_t)2, SIZE_MAX);
    Trainer trainer(oneHiddenLayerClassifier, trainingLoss, { SGDLearner(oneHiddenLayerClassifier->Parameters(), learningRatePerSample) });
    size_t outputFrequencyInMinibatches = 20;
    for (size_t i = 0; i < numMinibatchesToTrain; ++i)
    {
        auto minibatchData = minibatchSource->GetNextMinibatch(minibatchSize, device);
        trainer.TrainMinibatch({ { input, minibatchData[*featureStreamInfo].m_data }, { labels, minibatchData[*labelStreamInfo].m_data } }, device);

        if ((i % outputFrequencyInMinibatches) == 0)
        {
            double trainLossValue = trainer.PreviousMinibatchAverageTrainingLoss();
            printf("Minibatch %d: CrossEntropy loss = %.8g\n", (int)i, trainLossValue);
        }
    }
}

void TrainMNISTClassifier(const DeviceDescriptor& device)
{
    const size_t inputDim = 784;
    const size_t numOutputClasses = 10;
    const size_t hiddenLayerDim = 200;

    Variable input({ inputDim }, DataType::Float, L"features");
    auto scaledInput = ElementTimes(Constant({}, 0.00390625f, device), input);
    auto classifierOutput = FullyConnectedDNNLayer(scaledInput, hiddenLayerDim, device, std::bind(Sigmoid, _1, L""));
    auto outputTimesParam = Parameter(NDArrayView::RandomUniform<float>({ numOutputClasses, hiddenLayerDim }, -0.05, 0.05, 1, device));
    auto outputBiasParam = Parameter(NDArrayView::RandomUniform<float>({ numOutputClasses }, -0.05, 0.05, 1, device));
    classifierOutput = Plus(outputBiasParam, Times(outputTimesParam, classifierOutput));

    Variable labels({ numOutputClasses }, DataType::Float, L"labels");
    auto trainingLoss = CNTK::CrossEntropyWithSoftmax(classifierOutput, labels, L"lossFunction");;
    auto prediction = CNTK::ClassificationError(classifierOutput, labels, L"classificationError");

    auto oneHiddenLayerClassifier = CNTK::Combine({ trainingLoss, prediction, classifierOutput }, L"classifierModel");

    const size_t minibatchSize = 32;
    const size_t numSamplesPerSweep = 60000;
    const size_t numSweepsToTrainWith = 3;
    const size_t numMinibatchesToTrain = (numSamplesPerSweep * numSweepsToTrainWith) / minibatchSize;

    auto minibatchSource = CreateTextMinibatchSource(L"Train-28x28_cntk_text.txt", (size_t)784, (size_t)10, SIZE_MAX);

    auto streamInfos = minibatchSource->StreamInfos();
    auto featureStreamInfo = std::find_if(streamInfos.begin(), streamInfos.end(), [](const StreamInformation& streamInfo) { return (streamInfo.m_name == L"features"); });
    auto labelStreamInfo = std::find_if(streamInfos.begin(), streamInfos.end(), [](const StreamInformation& streamInfo) { return (streamInfo.m_name == L"labels"); });

    double learningRatePerSample = 0.003125;
    Trainer trainer(oneHiddenLayerClassifier, trainingLoss, { SGDLearner(oneHiddenLayerClassifier->Parameters(), learningRatePerSample) });
    size_t outputFrequencyInMinibatches = 20;
    for (size_t i = 0; i < numMinibatchesToTrain; ++i)
    {
        auto minibatchData = minibatchSource->GetNextMinibatch(minibatchSize, device);
        trainer.TrainMinibatch({ { input, minibatchData[*featureStreamInfo].m_data }, { labels, minibatchData[*labelStreamInfo].m_data } }, device);

        if ((i % outputFrequencyInMinibatches) == 0)
        {
            double trainLossValue = trainer.PreviousMinibatchAverageTrainingLoss();
            printf("Minibatch %d: CrossEntropy loss = %.8g\n", (int)i, trainLossValue);
        }
    }
}

void TrainerTests()
{
    TrainSimpleFeedForwardClassifer(DeviceDescriptor::CPUDevice());
    TrainMNISTClassifier(DeviceDescriptor::GPUDevice(0));
}
