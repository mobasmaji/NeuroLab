#include <iostream>
#include <string.h>
#include <utility> 
#include <list> 
#include "Layers/Layer.h"
//#include "Loss.h"

using namespace std;

class NeuralNet 
{
public:
    virtual void executeTransferLearning()=0;
    /*void updateDataSet(DataSet dataSet) {
		this->dataSet = dataSet;
	}
	list<pair<DataElement,string>> getResults() {
		return this->results;
    }*/

protected:
	unsigned int netID;
	double loss;
	double cumulativeLoss;
	double learningRate;
	unsigned int epochs;
	unsigned int batchSize;


};
