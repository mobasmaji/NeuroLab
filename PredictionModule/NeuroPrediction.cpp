#include "NeuroPrediction.h"
#include <iostream>
#include <math.h>
//#include "../PredictionModule/Hardware.h"

/**
 * @brief Construct a new Neuro Prediction:: Neuro Prediction object
 * 
 */
NeuroPrediction::NeuroPrediction()
{
}
/**
 * @brief this method makes the hardware distribution for our own nn. 
 * 
 * @param availableHardware a vector of strings that contain strings with the names of the hardware elements the user wants to use
 * @param numberOfImages  an integer with the number of images that the user wants to get classified
 * @return std::vector<Hardware>  a vector containing hardware elements.
 */
std::vector<Hardware> NeuroPrediction::distributeAndPredict(std::vector<std::string> availableHardware, int numberOfImages)
{
	//std::string cpu = "CPU";
	std::vector<double>polynomCPU{ -0.0000006,0.0237,1.1126 };
	std::vector<Hardware> list;
	double powerconsumption = 1;
	std::string cpu = "CPU";
	Hardware example{ cpu,numberOfImages,(double)9,polynomCPU,powerconsumption,5000000,14100 };
	for (std::string element : availableHardware) {
		if (element.compare(cpu) == 0) {
			example.name = cpu;
			example.numberOfAssignedImages = numberOfImages;
			example.polynome = polynomCPU;
			example.requiredTime = TimeValueOfX(polynomCPU, numberOfImages);
			example.powerconsumption = example.powerconsumption * example.requiredTime;
			example.bandwidth = 14100;
			list.push_back(example);
			return list;

		}
	}
	std::cerr << "The CPU is needed with this neural network";
	return list;

}

double NeuroPrediction::TimeValueOfX(std::vector<double>& polynome, double x)
{
	double value = 0;
	int i = 0;
	for (size_t sI = 0; sI < polynome.size(); sI++) {
		i = sI;
		int size = polynome.size();
		value = value + polynome.at(i) * pow(x, size - i - 1);
	}
	return value;
}

