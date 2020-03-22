#include "NNModule/NeuroLabNet.hpp"
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <stdio.h>
#include <algorithm>
#include <random>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;
using namespace cv;

#define BATCH_SIZE 1

#define INPUT_SIZE 82
#define INPUT_MAPS 3

#define CONV_1_KERNEL_SIZE 7
#define CONV_1_STRIDE_SIZE 3
#define CONV_1_INPUT_SIZE 82
#define CONV_1_OUTPUT_MAPS 15
#define CONV_1_OUTPUT_SIZE 26

#define MAX_1_OUTPUT_SIZE 13
#define MAX_1_POOL_SIZE 2
#define MAX_1_STRIDE_SIZE 2

#define CONV_2_KERNEL_SIZE 4
#define CONV_2_STRIDE_SIZE 1
#define CONV_2_OUTPUT_SIZE 10
#define CONV_2_OUTPUT_MAPS 40

#define MAX_2_OUTPUT_SIZE 5
#define MAX_2_POOL_SIZE 2
#define MAX_2_STRIDE_SIZE 2

#define DENSE_NEURONS 6

#define EPOCHS 8

//in total at least 60.000 images are recommended

NeuroLabNet::NeuroLabNet(){
    initNeuroLabNet();
}

NeuroLabNet::~NeuroLabNet(){
    delete env;
    delete envCreator;
    delete layerCreator;

    delete conv1;
    delete relu1;
    delete max1;
    delete conv2;
    delete relu2;
    delete max2;
    delete dense;
    delete softmax;
}

void NeuroLabNet::initNeuroLabNet(){
    envCreator=new OpenCLEnvironmentCreator();
    env=envCreator->createOpenCLEnvironment(HardwareType::CPU);

    OpenCLLayerCreator* layerCreator=new OpenCLLayerCreator();

    conv1=layerCreator->createConvLayer(env, BATCH_SIZE, INPUT_MAPS, CONV_1_INPUT_SIZE, CONV_1_INPUT_SIZE, CONV_1_OUTPUT_MAPS, CONV_1_OUTPUT_SIZE, CONV_1_OUTPUT_SIZE, CONV_1_KERNEL_SIZE, CONV_1_KERNEL_SIZE, CONV_1_STRIDE_SIZE, CONV_1_STRIDE_SIZE);
    relu1=layerCreator->createReluLayer(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS, CONV_1_OUTPUT_SIZE, CONV_1_OUTPUT_SIZE);
    max1=layerCreator->createMaxPoolLayer(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS, CONV_1_OUTPUT_SIZE, CONV_1_OUTPUT_SIZE, MAX_1_OUTPUT_SIZE, MAX_1_OUTPUT_SIZE, MAX_1_POOL_SIZE, MAX_1_POOL_SIZE, MAX_1_STRIDE_SIZE, MAX_1_STRIDE_SIZE);
    conv2=layerCreator->createConvLayer(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS, MAX_1_OUTPUT_SIZE, MAX_1_OUTPUT_SIZE, CONV_2_OUTPUT_MAPS, CONV_2_OUTPUT_SIZE, CONV_2_OUTPUT_SIZE, CONV_2_KERNEL_SIZE, CONV_2_KERNEL_SIZE, CONV_2_STRIDE_SIZE, CONV_2_STRIDE_SIZE);
    relu2=layerCreator->createReluLayer(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS, CONV_2_OUTPUT_SIZE, CONV_2_OUTPUT_SIZE);
    max2=layerCreator->createMaxPoolLayer(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS, CONV_2_OUTPUT_SIZE, CONV_2_OUTPUT_SIZE, MAX_2_OUTPUT_SIZE, MAX_2_OUTPUT_SIZE, MAX_2_POOL_SIZE, MAX_2_POOL_SIZE, MAX_2_STRIDE_SIZE, MAX_2_STRIDE_SIZE);
    dense=layerCreator->createDenseLayer(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS, MAX_2_OUTPUT_SIZE, MAX_2_OUTPUT_SIZE, DENSE_NEURONS);
    softmax=layerCreator->createSoftmaxLayer(env, BATCH_SIZE, DENSE_NEURONS, 1, 1);

    relu1->setPreviousOpenCLLayer(conv1);
    max1->setPreviousOpenCLLayer(relu1);
    conv2->setPreviousOpenCLLayer(max1);
    relu2->setPreviousOpenCLLayer(conv2);
    max2->setPreviousOpenCLLayer(relu2);
    dense->setPreviousOpenCLLayer(max2);
    softmax->setPreviousOpenCLLayer(dense);
}

vector<Result> NeuroLabNet::classify(){
    vector<Result> results;

    loadWeightsAndBiases("team");

    for(int image=0;image<dataSet.size();image++){

        string path=dataSet[image];

        //calculate forward
        int length=INPUT_MAPS*INPUT_SIZE*INPUT_SIZE*BATCH_SIZE;
        float*inputValues=(float*)malloc(sizeof(float)*length);
        loadImageToArray(inputValues, path);
        conv1->setInputs(env, inputValues, length);

        conv1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
        relu1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
        max1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
        conv2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
        relu2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
        max2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
        dense->computeForward(env, BATCH_SIZE, DENSE_NEURONS);
        softmax->computeForward(env, BATCH_SIZE, DENSE_NEURONS);

        //calculate errors
        length=BATCH_SIZE*DENSE_NEURONS;
        float* outputs=softmax->getOutputs(env, BATCH_SIZE, DENSE_NEURONS, 1, 1, NULL);

        delete[]inputValues;
        delete[]outputs;

        Result result;
        result.setPath(path);
        result.setLabelsAndProb(getLabelWithProb(outputs));
        results.push_back(result);
    }

    return results;
}

void NeuroLabNet::train(string weightsDir, string dataSetDir, string newWeightsDir){
    conv1->setLearningRate(0.03);
    conv2->setLearningRate(0.03);
    dense->setLearningRate(0.03);

    loadWeightsAndBiases(weightsDir);

    vector<TrainingItem>trainingItems=getAllTrainingItems(dataSetDir);

    for(int epoch=0;epoch<EPOCHS;epoch++){
        if(epoch>0 && epoch%2==0){
            conv1->setLearningRate(0.03/4.0);
            conv2->setLearningRate(0.03/4.0);
            dense->setLearningRate(0.03/4.0);
        }

        trainingItems=shuffleDataset(trainingItems);

        int wrong=0;
        int correct=0;
        for(int image=0;image<trainingItems.size();image++){

            string path=trainingItems[image].getPath();
            int label=trainingItems[image].getLabel();

            //calculate forward
            int length=INPUT_MAPS*INPUT_SIZE*INPUT_SIZE*BATCH_SIZE;
            float*inputValues=(float*)malloc(sizeof(float)*length);
            loadImageToArray(inputValues, path);
            conv1->setInputs(env, inputValues, length);

            conv1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
            relu1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
            max1->computeForward(env, BATCH_SIZE, CONV_1_OUTPUT_MAPS);
            conv2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
            relu2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
            max2->computeForward(env, BATCH_SIZE, CONV_2_OUTPUT_MAPS);
            dense->computeForward(env, BATCH_SIZE, DENSE_NEURONS);
            softmax->computeForward(env, BATCH_SIZE, DENSE_NEURONS);

            //calculate errors
            length=BATCH_SIZE*DENSE_NEURONS;
            float* errors=(float*)malloc(sizeof(float)*length);
            float* outputs=softmax->getOutputs(env, BATCH_SIZE, DENSE_NEURONS, 1, 1, NULL);
            calculateOutputErrors(label, errors, outputs);
            softmax->setOutputErrors(env, errors, DENSE_NEURONS*BATCH_SIZE);

            softmax->computeErrorComp(env, BATCH_SIZE);
            dense->computeErrorComp(env, BATCH_SIZE);
            max2->computeErrorComp(env, BATCH_SIZE);
            relu2->computeErrorComp(env, BATCH_SIZE);
            conv2->computeErrorComp(env, BATCH_SIZE);
            max1->computeErrorComp(env, BATCH_SIZE);
            relu1->computeErrorComp(env, BATCH_SIZE);
            conv1->computeErrorComp(env, BATCH_SIZE);

            //calculate parameters update
            conv1->computeWeightsUpdate(env, CONV_1_OUTPUT_MAPS);
            conv2->computeWeightsUpdate(env, CONV_2_OUTPUT_MAPS);
            dense->computeWeightsUpdate(env, DENSE_NEURONS);

            if(image%50==0){
                float percentage=(float)image/(float)trainingItems.size()*100.0;
                float percentageWrong=(float)wrong/(float)(image%10000);
                float percentageCorrect=(float)correct/(float)(image%10000);
                cout<<"Epoch: "<<epoch<<"\tImage: "<<image<<"\tProgress: "<<percentage<<"%\tCorrect: "<<percentageCorrect<<"%\tWrong: "<<percentageWrong<<"%"<<endl;

                if(image%10000==0){correct=0;wrong=0;}
            }

            if(label==predictedLabel(outputs))  correct++;
            else                                wrong++;

            delete[]inputValues;
            delete[]errors;
            delete[]outputs;
        }
    }

    saveWeightsAndBiases(newWeightsDir);
}

int NeuroLabNet::predictedLabel(float*outputs){
    int label=0;
    float value=outputs[0];

    for(int i=1;i<6;i++){
        if(value<outputs[i]){
            value=outputs[i];
            label=i;
        }
    }

    return label;
}

void NeuroLabNet::calculateOutputErrors(int label, float* errors, float* outputs){
    float expectedOutputs[DENSE_NEURONS];
    for(int i=0;i<DENSE_NEURONS;i++)    expectedOutputs[i]=0;
    expectedOutputs[label]=1;

    for(int i=0;i<DENSE_NEURONS;i++){
        errors[i]=outputs[i]-expectedOutputs[i];
    }
}

void NeuroLabNet::loadImageToArray(float* inputValues, string path){
    Mat image;
    image = imread(path, 1);   // Read the file

    int i=0;
    for(int y = 0; y < image.rows; y++){
        for(int x = 0; x < image.cols; x++){
            Vec3b bgrPixel = image.at<Vec3b>(y, x);
            int col=bgrPixel[0];
            float value=(float)col/255.0;
            inputValues[i++]=value;
        }
    }
    for(int y = 0; y < image.rows; y++){
        for(int x = 0; x < image.cols; x++){
            Vec3b bgrPixel = image.at<Vec3b>(y, x);
            int col=bgrPixel[1];
            float value=(float)col/255.0;
            inputValues[i++]=value;
        }
    }
    for(int y = 0; y < image.rows; y++){
        for(int x = 0; x < image.cols; x++){
            Vec3b bgrPixel = image.at<Vec3b>(y, x);
            int col=bgrPixel[2];
            float value=(float)col/255.0;
            inputValues[i++]=value;
        }
    }
}

vector<TrainingItem> NeuroLabNet::shuffleDataset(vector<TrainingItem>vec){
    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(vec), std::end(vec), rng);

    return vec;
}

vector<TrainingItem> NeuroLabNet::getAllTrainingItems(string dataSetDir){
    vector<TrainingItem> trainingItems;

    DIR *dir = opendir(dataSetDir.c_str());

    struct dirent *entry = readdir(dir);
    int label=0;
    while (entry != NULL){
        if (entry->d_type == DT_DIR && entry->d_name[0]!='.'){
            string s=dataSetDir+"/"+entry->d_name+"/";

            DIR* dirp = opendir(s.c_str());
            struct dirent * dp;
            while ((dp = readdir(dirp)) != NULL) {
                if(dp->d_name[0]!='.'){
                    string f=s+dp->d_name;

                    TrainingItem trainItem(f, label);
                    trainingItems.push_back(trainItem);
                }
            }
            closedir(dirp);

            label++;
        }
        entry = readdir(dir);
    }
    closedir(dir);

    return trainingItems;
}

void NeuroLabNet::loadWeightsAndBiases(string weightsDir){
    int length;

    //load conv1 parameters
    length=INPUT_MAPS*CONV_1_OUTPUT_MAPS*CONV_1_KERNEL_SIZE*CONV_1_KERNEL_SIZE;
    float* conv1Weights=(float*)malloc(sizeof(float)*length);
    loadArray(conv1Weights, length, weightsDir+"/conv1_weights.txt");
    conv1->setWeights(env, conv1Weights, length);

    length=CONV_1_OUTPUT_MAPS;
    float* conv1Biases=(float*)malloc(sizeof(float)*length);
    loadArray(conv1Biases, length, weightsDir+"/conv1_biases.txt");
    conv1->setBiases(env, conv1Biases, length);

    //load conv2 parameters
    length=CONV_1_OUTPUT_MAPS*CONV_2_OUTPUT_MAPS*CONV_2_KERNEL_SIZE*CONV_2_KERNEL_SIZE;
    float* conv2Weights=(float*)malloc(sizeof(float)*length);
    loadArray(conv2Weights, length, weightsDir+"/conv2_weights.txt");
    conv2->setWeights(env, conv2Weights, length);

    length=CONV_2_OUTPUT_MAPS;
    float* conv2Biases=(float*)malloc(sizeof(float)*length);
    loadArray(conv2Biases, length, weightsDir+"/conv2_biases.txt");
    conv2->setBiases(env, conv2Biases, length);

    //load dense parameters
    length=CONV_2_OUTPUT_MAPS*MAX_2_OUTPUT_SIZE*MAX_2_OUTPUT_SIZE*DENSE_NEURONS;
    float* denseWeights=(float*)malloc(sizeof(float)*length);
    loadArray(denseWeights, length, weightsDir+"/dense_weights.txt");
    dense->setWeights(env, denseWeights, length);

    length=DENSE_NEURONS;
    float* denseBiases=(float*)malloc(sizeof(float)*length);
    loadArray(denseBiases, length, weightsDir+"/dense_biases.txt");
    dense->setBiases(env, denseBiases, length);
}

void NeuroLabNet::saveWeightsAndBiases(string newWeightsDir){
    int length;

    //save conv1 parameters
    length=INPUT_MAPS*CONV_1_OUTPUT_MAPS*CONV_1_KERNEL_SIZE*CONV_1_KERNEL_SIZE;
    float* conv1Weights=conv1->getWeights(env, length);
    saveArray(conv1Weights, length, newWeightsDir+"/conv1_weights.txt");

    length=CONV_1_OUTPUT_MAPS;
    float* conv1Biases=conv1->getBiases(env, length);
    saveArray(conv1Biases, length, newWeightsDir+"/conv1_biases.txt");

    //save conv2 parameters
    length=CONV_1_OUTPUT_MAPS*CONV_2_OUTPUT_MAPS*CONV_2_KERNEL_SIZE*CONV_2_KERNEL_SIZE;
    float* conv2Weights=conv2->getWeights(env, length);
    saveArray(conv2Weights, length, newWeightsDir+"/conv2_weights.txt");

    length=CONV_2_OUTPUT_MAPS;
    float* conv2Biases=conv2->getBiases(env, length);
    saveArray(conv2Biases, length, newWeightsDir+"/conv2_biases.txt");

    //save dense parameters
    length=CONV_2_OUTPUT_MAPS*MAX_2_OUTPUT_SIZE*MAX_2_OUTPUT_SIZE*DENSE_NEURONS;
    float* denseWeights=dense->getWeights(env, length);
    saveArray(denseWeights, length, newWeightsDir+"/dense_weights.txt");

    length=DENSE_NEURONS;
    float* denseBiases=dense->getBiases(env, length);
    saveArray(denseBiases, length, newWeightsDir+"/dense_biases.txt");

    delete conv1Weights;
    delete conv1Biases;
    delete conv2Weights;
    delete conv2Biases;
    delete denseWeights;
    delete denseBiases;
}

void NeuroLabNet::saveArray(float* array, int length, string dir){
    ofstream out(dir);

    for(int i=0;i<length;i++){
        out<<array[i]<<" ";
    }

    out.close();
}

void NeuroLabNet::loadArray(float* array, int length, string dir){
    ifstream in(dir);

    for(int i=0;i<length;i++){
        string value;
        in>>value;
        array[i]=stof(value);
    }

    in.close();
}

void NeuroLabNet::updateDataSet(vector<string> dataSet){
    this->dataSet = dataSet;
}

vector<pair<string,float>> NeuroLabNet::getLabelWithProb(float*outputs){
    vector<pair<string,float>>prop;

    prop.push_back(pair<string,float>("bianca",outputs[0]));
    prop.push_back(pair<string,float>("bonny",outputs[1]));
    prop.push_back(pair<string,float>("jens",outputs[2]));
    prop.push_back(pair<string,float>("mohamad",outputs[3]));
    prop.push_back(pair<string,float>("niklas",outputs[4]));
    prop.push_back(pair<string,float>("other",outputs[5]));

    return prop;
}
