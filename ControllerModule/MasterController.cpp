#include "MasterController.h"
#include <QApplication>
#include <QLabel>
#include <QDebug>
#include <DataModule/Result.h>
#include <Tools/openvino/include/inference_engine.hpp>

MasterController::MasterController() : viewObserver(this)
{
}
void MasterController::setPaths(vector<string> paths)
{
    this->nnObserver.setPathList(paths);
    //this->imagePaths = paths;
    // cout<< imagePaths->back()
    qDebug()<<"load in master"<<endl; //ok

}

void MasterController::classify()
{
    qDebug()<<"classify called in Master"<<endl; // debug: working

    vector<Result> results = nnObserver.classify();
    viewObserver.displayResults(results);
    //results = nnObserver.getResults(); crashes
    
}
void MasterController::getPrediction(const string net, const string mode, vector<string> hardware , int nrImages)
{
    //DataResults predictionResults;
    /*predictionResults = */predictionObserver.calculatePrediction(nrImages ,net, mode,hardware);
    vector<double> timeConsumption = predictionObserver.getTime();
    vector<double> powerConsumption = predictionObserver.getPower();
    double bandwidth = predictionObserver.getBandwidth();
    double flops = predictionObserver.getFlops();
    // analog for powerconsumption
    vector<pair<string, int>> distribution;
    nnObserver.setDistribution(distribution);
    viewObserver.displayPrediction(timeConsumption, powerConsumption, bandwidth, flops);
}

vector<string> MasterController::getAvailableHardware()
{
    InferenceEngine::Core core;
    for(auto &item: core.GetAvailableDevices())
    {
        cout<< item;
    }
    return core.GetAvailableDevices();

}

list<HardwareElement> MasterController::getSelectedHardware() const
{
    return selectedHardware;
}

void MasterController::setSelectedHardware(const list<HardwareElement> &value)
{
    selectedHardware = value;
}

void MasterController::setNNType(string nn)
{
    nnObserver.setCurrentNN(nn);
}
