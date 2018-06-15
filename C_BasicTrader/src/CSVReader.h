#ifndef CSV_READER_H
#define CSV_READER_H

#include "helper/UsefullFunctions.h"
#include "helper/ConfigManager.h"

#include <vector>
#include <string>
#include <deque>
#include <stdlib.h>
#include <iostream>
#include "PriceFeedData.h"

using namespace std;
using namespace config;

class CSVReader
{    
public:
    static PriceFeedData* readExchangeFromFile(std::string filepath)
    {
        string csvFile = filepath;
        string line;
        string thisVal;
        string cvsSplitBy = ";";
        PriceFeedData* prices = new PriceFeedData();
        ifstream inputFile;
        functions::openInputFile(inputFile, configValues["exchangeInputDir"],  filepath);

        std::getline(inputFile, line);

        while (std::getline(inputFile, line, '\n'))
        {
            stringstream lineStream(line);
            vector<string> splitLine;
            while(getline(lineStream, thisVal,',')) 
            {
                splitLine.emplace_back(thisVal);
            }
            prices->addPrice(atof(splitLine[2].c_str())+0.0001, atof(splitLine[2].c_str())-0.0001, atof(splitLine[1].c_str())*1000); 
        }
        inputFile.close();
        return prices;
    }
};

#endif
