#ifndef DATA_H
#define DATA_H

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>


extern inline const std::string BASE_DIR = "/home/mael/projects/backtester/src/output/";

struct Bar{
    int index; //the # of the bar wrt to the other bars
    unsigned long timestamp; //the opening time in seconds
    double open;
    double high;
    double low;
    double close;
    unsigned long volume;
};

class DataHandler{
    std::string csv_path; //thinking of changing the format and having a single datahandler able to handle multiple differennt instruments, makes more sense
    static int max_instrument_id;
    std::unordered_map<int, std::string> instrument_id_to_name;
    std::unordered_map<std::string, int> instrument_name_to_id;
    std::unordered_map<int, std::queue<Bar>> instrument_id_to_bars;
    std::string base_dir;
    int bar_size; //how many seconds one bar is, for now I hardcode this to be the length of one day, later I should parametrize this variable

    public:
        std::queue<Bar> bars; //need to decide if this is a vector or a queue
        DataHandler(): base_dir(BASE_DIR), bar_size(86400){};
        void read_csv(std::string instrument_name);

        Bar get_next_market_event(int instrument_id);

        int* instrument_id(std::string name){
            auto it = this->instrument_name_to_id.find(name);
            return it != nullptr ? &it->second : nullptr;
        };

        std::string* instrument_name(int id){
            auto it = this->instrument_id_to_name.find(id);
            return it != nullptr ? &it->second : nullptr;
        };

        std::queue<Bar>* instrument_bars(int id){
            auto it = this->instrument_id_to_bars.find(id);
            return it != nullptr ? &it->second : nullptr;
        }

        //getters
        std::string get_csv_path(){
            return this->csv_path;
        };

        std::queue<Bar> get_bars(){
            return this->bars;
        };

        int get_max_instrument_id(){
            return this->max_instrument_id;
        };

        int get_bar_size(){
            return this->bar_size;
        };
};

#endif