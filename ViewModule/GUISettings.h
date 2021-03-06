    #ifndef GUISETTINGS_H
#define GUISETTINGS_H
#include <string>
#include <iostream>
#include <vector>
#include <ControllerModule/HardwareElement.h>
#include <algorithm>
using namespace std;

class GUISettings
{
public:
    GUISettings();

    vector<string> getSelectedHardware() const;
    void setSelectedHardware(vector<HardwareElement> selectedHardware);

    string getMode() const;
    void setMode(const string &value);

    string getNn() const;
    void setNn(const string &value);

    vector<string> getPaths() const;
    void setPaths(const vector<string> &value);
    void removePath(string value);

    void clearHardware();

    void unselectHardwareElement(string element);
    void selectHardwareElement(string element);

    string getDataSetDirectory() const;
    void setDataSetDirectory(string dir);

    string getWeightsDirectory() const;
    void setWeightsDirectory(string dir);



    string getNewWeightsDirectory() const;
    void setNewWeightsDirectory(string value);

    void clearPaths();
private:
    string nn;
    string mode;
    vector<string> selectedHardware;
    vector<string> paths;

    string dataSetDirectory;
    string weightsDirectory;
    string newWeightsDirectory;
};

#endif // GUISETTINGS_H
