#ifndef GUIBUILDER_H
#define GUIBUILDER_H
#include <ViewModule/MainWindow.h>
#include <QApplication>

class GUIBuilder
{
public:
    GUIBuilder(QApplication& app);
    void drawGUI();
    void displayResults();
    void updatePredictions();
private:
    MainWindow mainWindow;
};

#endif // GUIBUILDER_H