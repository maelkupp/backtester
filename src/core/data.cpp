#include "data.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <queue>


int DataHandler::max_instrument_id = 0;
void DataHandler::read_csv(std::string instrument_name){
    //this function reads the csv file and populates the vector of Bars in the DataHandler class
    std::string line;
    std::string info;

    std::string csv_path = this->base_dir + instrument_name;
    int instrument_id = this->max_instrument_id++; //get the max id and increment it
    std::queue<Bar> new_bars;
    std::ifstream csv_file(csv_path);
    getline(csv_file, line); //read the first line of the csv into the line, it is the garbage line
    while(getline(csv_file, line)){
        std::stringstream line_stream(line);
        std::vector<std::string> infos;
        while(std::getline(line_stream, info, ',')){
            infos.push_back(info);
        }
        
        new_bars.push({std::stoi(infos[0]), std::stoul(infos[7]), std::stod(infos[1]),
                        std::stod(infos[2]), std::stod(infos[3]), std::stod(infos[4]), std::stoul(infos[5])});
        
    }
    this->instrument_name_to_id[instrument_name] = instrument_id;
    this->instrument_id_to_name[instrument_id] = instrument_name;
    this->instrument_id_to_bars[instrument_id] = new_bars;
    std::cout << "Finished populating the Bars vector with the bars from the CSV file\n";

}

Bar DataHandler::get_next_market_event(int id){
    Bar bar = this->instrument_id_to_bars[id].front();
    this->instrument_id_to_bars[id].pop();
    return bar;
}

