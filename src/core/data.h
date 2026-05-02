#ifndef DATA_H
#define DATA_H

#include <string>
#include <vector>
#include <queue>

extern inline const std::string BASE_DIR = "/home/mael/projects/backtester/src/output/";

struct Bar{
    int index;
    unsigned long timestamp; //the opening time in seconds
    double open;
    double high;
    double low;
    double close;
    unsigned long volume;
};

class DataHandler{
    std::string csv_path;
    std::string ticker;

    int bar_size; //how many seconds one bar is, for now I hardcode this to be the length of one day, later I should parametrize this variable

    public:
        std::queue<Bar> bars; //need to decide if this is a vector or a queue
        DataHandler(std::string ticker): csv_path(BASE_DIR + ticker), ticker(ticker), bar_size(86400){};
        void read_csv();

        Bar get_next_market_event();

        //getters
        std::string get_csv_path(){
            return this->csv_path;
        };

        std::queue<Bar> get_bars(){
            return this->bars;
        };

        std::string get_ticker(){
            return this->ticker;
        };

        int get_bar_size(){
            return this->bar_size;
        };
};

#endif