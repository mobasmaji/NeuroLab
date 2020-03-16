#include "NeuroLabNet.h"
#include <QDebug>


using namespace cv;
// the division (imageSize-filterSize)/stride      must be an integer
// output is    (imageSize-filterSize)/stride + 1
#define BATCH_SIZE 1
#define CHANNELS 3

#define CONV_1_INPUT 82
#define CONV_1_KERNEL 7
#define CONV_1_STRIDE 3
#define CONV_1_OUTPUT 26
#define FILTERS_1 15

#define CONV_2_INPUT 13
#define CONV_2_KERNEL 4
#define CONV_2_STRIDE 1
#define CONV_2_OUTPUT 10
#define FILTERS_2 40

#define MAX_1_INPUT 26
#define MAX_1_WINDOW 2
#define MAX_1_STRIDE 2
#define MAX_1_OUTPUT 13

#define MAX_2_INPUT 10
#define MAX_2_WINDOW 2
#define MAX_2_STRIDE 2
#define MAX_2_OUTPUT 5

#define DENSE_INPUT 5

#define SOFTMAX_INPUT 5

#define LOSS_INPUT 5



NeuroLabNet::NeuroLabNet(){
    OpenCLEnvironmentCreator* clCreator = new OpenCLEnvironmentCreator();
    OpenCLEnvironment* env = clCreator->createOpenCLEnvironment(HardwareType::CPU);
    clEnv = env;
    OpenCLLayerCreator* layerCreator = new OpenCLLayerCreator();

    conv1 = layerCreator->createConvLayer(clEnv,BATCH_SIZE,CHANNELS,CONV_1_INPUT,CONV_1_INPUT,
                                          FILTERS_1,CONV_1_OUTPUT,CONV_1_OUTPUT,CONV_1_KERNEL,
                                          CONV_1_KERNEL,CONV_1_STRIDE,CONV_1_STRIDE);
    relu1 = layerCreator->createReluLayer(clEnv,BATCH_SIZE,FILTERS_1,CONV_1_OUTPUT,CONV_1_OUTPUT);
    max1 = layerCreator->createMaxPoolLayer(clEnv,BATCH_SIZE,FILTERS_1,MAX_1_INPUT,MAX_1_INPUT,MAX_1_OUTPUT,
                                            MAX_1_OUTPUT,MAX_1_WINDOW,MAX_1_WINDOW,MAX_1_STRIDE,MAX_1_STRIDE);
    conv2 = layerCreator->createConvLayer(clEnv,BATCH_SIZE,CHANNELS,CONV_2_INPUT,CONV_2_INPUT,
                                          FILTERS_2,CONV_2_OUTPUT,CONV_2_OUTPUT,CONV_2_KERNEL,
                                          CONV_2_KERNEL,CONV_2_STRIDE,CONV_2_STRIDE);
    relu2 = layerCreator->createReluLayer(clEnv,BATCH_SIZE,FILTERS_2,CONV_2_OUTPUT,CONV_2_OUTPUT);

    max2 = layerCreator->createMaxPoolLayer(clEnv,BATCH_SIZE,FILTERS_2,MAX_2_INPUT,MAX_2_INPUT,MAX_2_OUTPUT,
                                            MAX_2_OUTPUT,MAX_2_WINDOW,MAX_2_WINDOW,MAX_2_STRIDE,MAX_2_STRIDE);

    dense = layerCreator->createDenseLayer(clEnv,BATCH_SIZE,FILTERS_2,DENSE_INPUT,DENSE_INPUT,SOFTMAX_INPUT);

    soft = layerCreator->createSoftmaxLayer(clEnv,BATCH_SIZE,SOFTMAX_INPUT,1,1);


    // connect Layers
    relu1->setPreviousOpenCLLayer(conv1);
    max1->setPreviousOpenCLLayer(relu1);
    conv2->setPreviousOpenCLLayer(max1);
    relu2->setPreviousOpenCLLayer(conv2);
    max2->setPreviousOpenCLLayer(relu2);
    dense->setPreviousOpenCLLayer(max2);
    soft->setPreviousOpenCLLayer(dense);
}


void NeuroLabNet::init() {
}

vector<Result> NeuroLabNet::classify() {
    vector<Result> results;
    size_t LastBatchSize;
    if(this->dataSet.size()%BATCH_SIZE!=0)
    {
        LastBatchSize = this->dataSet.size() % BATCH_SIZE;
    }

    // for each image, read, resize and turn into a 3D array of pixels
    float pixels[BATCH_SIZE*CONV_1_INPUT*CONV_1_INPUT*CHANNELS];

    for(auto &item:dataSet)
    {


        cv::Mat img = cv::imread(item);
        if(img.empty())
        {
            cerr<<"Image is invalid";
            continue;
        }
        if(img.channels()!=3)
        {
            cerr<<"Image doesn't have enough channels";
        }
        cv::Mat resizedImg;
        cv::Size size(CONV_1_INPUT,CONV_1_INPUT);
        // resize image
        cv::resize(img,resizedImg,size,cv::INTER_LINEAR);
        // a 3D array of type float to save pixel values
        //float pixels[105][105][3]={};


        for(unsigned int i =0;i<CONV_1_INPUT;i++)
        {
            for(unsigned int j =0;j<CONV_1_INPUT;j++)
            {
                for(unsigned int h =0;h<CHANNELS;h++)
                {
                    // the function at returns an array with the length 3, whereby each field has the red, blue or green value
                    // umgekehrt?
                    pixels[(i * CONV_1_INPUT + j) * CHANNELS + h] = resizedImg.at<cv::Vec3b>(i,j).val[h]/255.0;
                }
            }
        }
        this->conv1->setInputs(clEnv,pixels,CONV_1_INPUT*CONV_1_INPUT*CHANNELS);
        // start forward passing
        this->conv1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
        this->relu1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
        this->max1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
        this->conv2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
        this->relu2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
        this->max2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
        this->dense->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
        this->soft->computeForward(clEnv,BATCH_SIZE,SOFTMAX_INPUT);
        //////////////////
        float* prob = this->soft->getOutputs(clEnv,BATCH_SIZE,5,1,1,nullptr);
        Result result;
        result.setPath(item);
        result.setLabelsAndProb(getLabelWithProb(prob));
        results.push_back(result);
    }
    return results;
}

void NeuroLabNet::train(string weightsDir, string dataSetDir) {
    // parsing weights for conv 1
    float weightsConv1[CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1];
    float weightsConv2[CONV_2_KERNEL * CONV_2_KERNEL * FILTERS_2];
    float weightsDense[DENSE_INPUT*DENSE_INPUT* FILTERS_2];
    qDebug() << "Parsing weights" << endl;
    {
        ifstream file(weightsDir+"/weights_conv1");
        if(file.is_open())
        {
            for(int i = 0; i < CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1; ++i)
            {
                file >> weightsConv1[i];
            }
        }
        // parsing weights for conv 2
        ifstream file1(weightsDir+"/weights_conv2");
        if(file1.is_open())
        {
            for(int i = 0; i < CONV_2_KERNEL * CONV_2_KERNEL * FILTERS_2; ++i)
            {
                file1 >> weightsConv2[i];
            }
        }
        // parsing weights for dense
        ifstream file2(weightsDir+"/weights_dense");
        if(file2.is_open())
        {
            for(int i = 0; i < DENSE_INPUT * DENSE_INPUT * FILTERS_2; ++i)
            {
                file2 >> weightsDense[i];
            }
        }
    }
    qDebug() << "Setting weights" << endl;
    conv1->setWeights(clEnv,weightsConv1,CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1);
    conv2->setWeights(clEnv,weightsConv1,CONV_2_KERNEL * CONV_2_KERNEL * FILTERS_2);
    dense->setWeights(clEnv,weightsConv1,DENSE_INPUT * DENSE_INPUT * FILTERS_2);
    qDebug() << "Reading folders and images" << endl;
    // read image list from directory
    QDir dir(QString::fromStdString(dataSetDir));
    QStringList files = dir.entryList(QStringList(),QDir::Dirs);
    string labels[files.size()-2];

    int i = 0;
    for(auto &item : files) {
        if(item.startsWith(".")){
            continue;
        }
        labels[i] = item.toStdString();
        i++;
    }


    // for each folder
    unsigned int labelCount = 0;
    for(auto & folder : files)
    {
        if(folder.startsWith(".")){
            continue;
        }
        labelCount++;
        qDebug()<<"#### Training with label "<< folder.toUpper() << endl;
        QDir dirOfImages(QString::fromStdString(dataSetDir)+"/"+folder);
        QStringList images = dirOfImages.entryList((QStringList()<<"*.jpg"<<"*.JPG",QDir::Files));
        unsigned int count = 0;
        for(auto & image:images)
        {
            if(image.startsWith(".")){
                continue;
            }
            count++;
            qDebug()<<"Training with image Nr. "<< count << endl;
            qDebug()<< dirOfImages.absolutePath() << image << endl;
            // get pixels from path
            // CRASHING HERE ON THE SECOND IMAGE
            cv::Mat img = cv::imread(dirOfImages.absolutePath().toStdString() +"/"+ image.toStdString());
            if(img.empty())
            {
                cerr<<"Image is invalid"<<endl;
                continue;
            }
            if(img.channels()!=3)
            {
                cerr<<"Image doesn't have enough channels"<<endl;
                continue;
            }
            cv::Mat resizedImg;
            cv::Size size(CONV_1_INPUT,CONV_1_INPUT);
            // resize image
            cv::resize(img,resizedImg,size,cv::INTER_LINEAR);

            float pixels[CONV_1_INPUT*CONV_1_INPUT*CHANNELS];

            for(unsigned int i =0;i<CONV_1_INPUT;i++)
            {
                for(unsigned int j =0;j<CONV_1_INPUT;j++)
                {
                    for(unsigned int h =0;h<CHANNELS;h++)
                    {
                        // the function at returns an array with the length 3, whereby each field has the red, blue or green value
                        //pixels[i][j][h] = resizedImg.at<cv::Vec3b>(i,j).val[h];
                        pixels[(i * CONV_1_INPUT + j) * CHANNELS + h] = resizedImg.at<cv::Vec3b>(i,j).val[h]/255.0;
                    }
                }
            }
            this->conv1->setInputs(clEnv,pixels,CONV_1_INPUT*CONV_1_INPUT*CHANNELS);

            // start forward passing
            this->conv1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
            this->relu1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
            this->max1->computeForward(clEnv,BATCH_SIZE,FILTERS_1);
            this->conv2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
            this->relu2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
            this->max2->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
            this->dense->computeForward(clEnv,BATCH_SIZE,FILTERS_2);
            this->soft->computeForward(clEnv,BATCH_SIZE,SOFTMAX_INPUT);

            // compute error with Loss (using labels as well)
            //float* outputs = this->soft->getOutputs(clEnv,BATCH_SIZE,SOFTMAX_INPUT,1,1,nullptr);
            float targetDistribution[] = {0,0,0,0,0};
            if(folder.toStdString() == "bianca"){
                targetDistribution[0] = 1;

            }else if (folder.toStdString() == "bonny"){

                targetDistribution[1] = 1;

            }else if(folder.toStdString() == "jens"){
                targetDistribution[2] = 1;

            }else if (folder.toStdString() == "mohamad"){
                targetDistribution[3] = 1;

            }else if(folder.toStdString() == "niklas"){
                targetDistribution[4] = 1;

            }
            float * actualDistribution = soft->getOutputs(clEnv,BATCH_SIZE,1,5,1,nullptr);
            float lossOutput[5] ={0};
            //float * lossOutput=lossFunction->getOutputError(outputs,folder.toStdString(),dataSetDir);
            for(unsigned int i=0;i<5;i++)
            {
                loss += targetDistribution[i]*std::log(actualDistribution[i]);
            }
            // save loss in case needed
            this->loss = -loss;
            for (size_t j=0; j<5;j++)
            {
                // derivative
                lossOutput[j] =-(targetDistribution[j]/actualDistribution[j]);
            }
            //set upstream gradient
            soft->setOutputErrors(clEnv,lossOutput,5);
            // back propagate
            conv1->computeErrorComp(clEnv,BATCH_SIZE);
            relu1->computeErrorComp(clEnv,BATCH_SIZE);
            max1->computeErrorComp(clEnv,BATCH_SIZE);
            conv2->computeErrorComp(clEnv,BATCH_SIZE);
            relu2->computeErrorComp(clEnv,BATCH_SIZE);
            max2->computeErrorComp(clEnv,BATCH_SIZE);
            dense->computeErrorComp(clEnv,BATCH_SIZE);
            soft->computeErrorComp(clEnv,BATCH_SIZE);
        }
    }
    qDebug()<<"Saving weights and biases"<< endl;
    float* calculatedWeightsConv1 = conv1->getWeights(clEnv,CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1);
    float* calculatedWeightsConv2 = conv2->getWeights(clEnv,CONV_2_KERNEL * CONV_2_KERNEL * FILTERS_2);
    float* calculatedWeightsDense = dense->getWeights(clEnv,DENSE_INPUT * DENSE_INPUT* FILTERS_2);

    QFile file(QString::fromStdString(weightsDir)+"/new/weights_conv1");
    QTextStream stream(&file);
    if(file.open(QIODevice::WriteOnly |QIODevice::Text))
    {
        for(int i = 0; i < CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1; ++i)
        {
            stream << calculatedWeightsConv1[i] << " ";
        }
    }
    // parsing weights for conv 2
    QFile file1(QString::fromStdString(weightsDir)+"/new/weights_conv2");
    QTextStream stream1(&file1);
    if(file.open(QIODevice::WriteOnly |QIODevice::Text))
    {
        for(int i = 0; i < CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1; ++i)
        {
            stream1 << calculatedWeightsConv2[i] << " ";
        }
    }
    // parsing weights for dense
    QFile file2(QString::fromStdString(weightsDir)+"/new/weights_dense");
    QTextStream stream2(&file2);
    if(file.open(QIODevice::WriteOnly |QIODevice::Text))
    {
        for(int i = 0; i < CONV_1_KERNEL * CONV_1_KERNEL * FILTERS_1; ++i)
        {
            stream2 << calculatedWeightsDense[i] << " ";
        }
    }

}

void NeuroLabNet::executeTransferLearning() {
    // TODO

}

void NeuroLabNet::updateDataSet(vector<string> dataSet)
{
    this->dataSet = dataSet;
}


vector<pair<string,float>> NeuroLabNet::getLabelWithProb(float prob[])
{
    // need to parse labels with prob. and sort it and get the top 5
    vector<pair<string,float>> vec;
    QDir dir(QString::fromStdString("/home/mo/dataset"));
    QStringList files = dir.entryList(QStringList(),QDir::Dirs);

    int i =0;
    for(auto &item : files) {
        if(item.startsWith(".")){
            continue;
        }
        pair<string,float> pair;
        pair.first = item.toStdString();
        pair.second = prob[i];
        vec.push_back(pair);
        i++;
    }


    //sort(vec.begin(),vec.end(),sortBySec);
    return vec ;

}

