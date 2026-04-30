#ifndef DATA_H
#define DATA_H

#include <string>
#include <vector>
#include <queue>

static const std::string BASE_DIR = "/home/mael/projects/backtester/src/output/";

struct Bar{
    unsigned long timestamp;
    double open;
    double high;
    double low;
    double close;
    unsigned long volume;
};

class DataHandler{
    std::string csv_path;
    std::string ticker;

    public:
        std::queue<Bar> bars; //need to decide if this is a vector or a queue
        DataHandler(std::string ticker): csv_path(BASE_DIR + ticker), ticker(ticker){};
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
};

#endif