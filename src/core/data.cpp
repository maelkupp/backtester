#include "data.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

void DataHandler::read_csv(){
    //this function reads the csv file and populates the vector of Bars in the DataHandler class
    std::string line;
    std::string info;

    std::ifstream csv_file(this->get_csv_path());
    getline(csv_file, line); //read the first line of the csv into the line, it is the garbage line
    while(getline(csv_file, line)){
        std::stringstream line_stream(line);
        std::vector<std::string> infos;
        while(std::getline(line_stream, info, ',')){
            infos.push_back(info);
        }
        
        this->bars.push({std::stoi(infos[0]), std::stoul(infos[7]), std::stod(infos[1]), std::stod(infos[2]),
                              std::stod(infos[3]), std::stod(infos[4]), std::stoul(infos[5])});
        
    }

    std::cout << "Finished populating the Bars vector with the bars from the CSV file\n";

}

Bar DataHandler::get_next_market_event(){
    Bar bar = this->bars.front();
    this->bars.pop();
    return bar;
}

