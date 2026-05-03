#include "core/data.h"
#include "core/execution.h"
#include <iostream>




int main(int argc, char** argv){
    DataHandler dh; //for now hardcode a single stock to play around with and get the entire workflow working
    std::string inst_name = "AAPL";
    dh.read_csv(inst_name);
    std::cout << "have " << dh.get_bars().size() << " bars \n";
    ExecutionHandler eh; //initialize the execution handler
    int* instrument_id = dh.instrument_id(inst_name);
    if(instrument_id){
        std::unique_ptr<MarketEvent> me = std::make_unique<MarketEvent>(*instrument_id, dh.get_next_market_event(*instrument_id));
        eh.add_event(std::move(me)); //add the first market event to the queue
        eh.run(*instrument_id, dh);
    }

} 