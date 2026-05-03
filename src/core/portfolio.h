#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include "data.h"
#include "execution.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class Fill;


//tracks a single open position
class Position{
    int instrument_id;
    double size; //can be positive or negative
    double avg_entry_price;
    double unrealized_PL;
    double realized_PL;

    public:
        //when constructing a position we do not have
        Position(): instrument_id(0), size(0.0), avg_entry_price(0.0), unrealized_PL(0.0), realized_PL(0.0) {}; //default constructor
        Position(int instrument_id): instrument_id(instrument_id), size(0.0), avg_entry_price(0.0), unrealized_PL(0.0), realized_PL(0.0) {};

        void update_unrealized_PL(Bar& bar); //update the unrealized PL of the position due to a new bar being seen
        void add_to_position(Fill& fill); //add to the position the order we filled, can be either a long or a short, updates realized_PL, quantity and avg_entry_price
        void update_avg_entry_price(double size, double filled_price);
        void close_position(double fill_price); //close the current position with a price fill_price

        //getters
        int get_instument_id(){return this->instrument_id;};
        double get_size(){return this->size;};
        double get_avg_entry_price(){return this->avg_entry_price;};
        double get_realized_PL(){return this->realized_PL;};

        //setters
        void set_size(double size){this->size = size;};
};

//class that does the accounting of the trader
class PortfolioManager{
    double cash; //the total cash we have
    std::vector<double> equity_ts; //the equity time series, so we can see the evolution of our strategies and get the equity curve
    double curr_equity; //portfolio value = cash + \sum_{i} quantity_i x market_price_i (or equal to cash + unrealized_PL) the cash already holds the realized_PL
    //depending on how i implement the instrument ID i might change this into a vector for efficiency

    std::unordered_map<int, Position> positions; //a map from instrument id to the Position with that id, only includes 'open' positions
    //if a position hase size < epsilon, we remove it from the map
    std::unordered_set<int> instrument_ids; //set of all the instrument ids that are in our positions map


    public:
        PortfolioManager(double cash, std::unordered_set<int> instrument_ids): cash(cash), curr_equity(cash), instrument_ids(instrument_ids) {};

        //update the cash & equity and lets the Position class handle itself
        void close_position(int instrument_id); //removes a position from the positions map, should only do this if the position has been closed
        void update_instrument_position_from_fill(int instrument_id, Fill& fill);
        void update_positions_from_bars(std::vector<std::pair<int, Bar>>& new_bars);

        void insert_id(int id){instrument_ids.insert(id);};

        bool equity_value_check(std::vector<std::pair<int, Bar>>& present_bars);

        //getters
        double get_cash(){return this->cash;};
        double get_equity(){return this-> curr_equity;};
        std::unordered_set<int> get_instrument_ids(){return this->instrument_ids;};

        Position* get_Position(int instrument_id){
            auto it = this->positions.find(instrument_id);
            return it != nullptr ? &it->second : nullptr;
        }
};


#endif