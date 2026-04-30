#include "core/data.h"
#include "core/execution.h"
#include <iostream>




int main(int argc, char** argv){
    DataHandler dh("AAPL"); //for now hardcode a single stock to play around with and get the entire workflow working
    dh.read_csv();
    std::cout << "have " << dh.get_bars().size() << " bars \n";
    ExecutionHandler eh; //initialize the execution handler
    std::unique_ptr<MarketEvent> me = std::make_unique<MarketEvent>(dh.get_ticker(), dh.get_next_market_event());
    eh.add_event(std::move(me)); //add the first market event to the queue
    eh.run(dh);
}