
#include "LowestPowerConsumption.h"
std::vector<Hardware>LowestPowerConsumption::distributeAndPredict(std::vector<std::string>& hardware, int numberOfImages) {
	std::vector<Hardware> list;
	std::string examplestring = "example";
	double requiredTime = 0;
	std::vector<double> polynomCPU{ 2,3,4,5 };
	std::vector<double> polynomFPGA{ 2,3,4,5 };
	std::vector<double> polynomMovidius{ 2,3,4,5 };
	Hardware example{ examplestring,numberOfImages,requiredTime,polynomFPGA,requiredTime};
	std::string CPU = "CPU";
	std::string FPGA = "FPGA";
	std::string sticks[] = { "Movidius1","Movidius2","Movidius3","Movidius4" };
	double powerConsumptionFPGA = 0;
	double powerConsumptionCPU = 0;
	double powerConsumptionMovidius = 0;
	for (std::string movidius : sticks) {
		for (std::string elem : hardware) {
			if (elem.compare(movidius) == 0) {
				example.name = movidius;
				example.polynome = polynomMovidius;
				example.requiredTime = TimeValueOfX(example.polynome,example.numberOfAssignedImages);
				example.powerconsumption = example.requiredTime*powerConsumptionMovidius;
				list.push_back(example);
				return list;
			}

		}
	}
	for (std::string elem : hardware) {
		if (elem.compare(CPU)) == 0) {
			example.name = elem;
				example.polynome = polynomCPU;
				example.requiredTime = TimeValueOfX(example.polynome,example.numberOfAssignedImages);
				example.powerconsumption = example.requiredTime*powerConsumptionCPU;
				list.push_back(example);
				return list;
		}
	}
		for (std::string elem : hardware) {
		if (elem.compare(FPGA)) == 0) {
			example.name = elem;
				example.polynome = polynomFPGA;
				example.requiredTime = TimeValueOfX(example.polynome,example.numberOfAssignedImages);
				example.powerconsumption = example.requiredTime*powerConsumptionFPGA;
				list.push_back(example);
				return list;
		}
		return list;
	}
	
	//throw "CPU needed for this type of neural network. No Batch-processing is supported";


}

double LowestPowerConsumption::TimeValueOfX(std::vector<double>& polynome, double x)
{
	double value;
	for (int i = 0; i < polynome.size(); i++) {
		value = value + pow(polynome[polynome.size() - i], x);
	}
	return value;
}

