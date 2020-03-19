#include "PretrainedNN.h"

PretrainedNN::PretrainedNN()
{

}

vector<Result> PretrainedNN::classify()
{
    cerr<<"PNN nn "<<currentNN<<endl;

    // thread per hardware. Then set images in an openVinoEnv on this hardware
    results.clear();
    vector<thread> threads;
    for(int i = 0; i < (int) envs.size(); i++){
        threads.push_back(thread(&PretrainedNN::threading, this, envs[i]));
    }

    for (auto& thread : threads) thread.join();

    return results;
}

void PretrainedNN::threading(OpenVinoEnv *env){
    vector<Result> intermidiate;
    intermidiate = env->classify();
    for(Result inter : intermidiate){
        results.push_back(inter);
    }
}


void PretrainedNN::setImagePaths(vector<string> imagePaths)
{
    allImages = imagePaths;
}

void PretrainedNN::setPlatforms(vector<pair<string, int> > platforms)
{
    //distribution = platforms;
    envs.clear();
    int count = 0;
    vector<string> imagesToDeploy;
    for(pair<string, int> dist : platforms){
        OpenVinoEnv *env;
        env = new OpenVinoEnv();
        env->setDistribution({dist});
        env->chooseNeuralNet(currentNN);
        int dif = dist.second + count;
        imagesToDeploy.clear();
        for(int i = count; i < dif && (int)allImages.size() >= dif; i++)
        {

            imagesToDeploy.push_back(allImages.at(i));

        }
        count += dist.second;

        env->setImageNames(imagesToDeploy);

        envs.push_back(env);
    }
}

void PretrainedNN::setNerualNet(string nn)
{
    currentNN = nn;
}


