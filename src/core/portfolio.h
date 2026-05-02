#ifndef PORTFOLIO_H
#define PORTFOLIO_H

#include "data.h"
#include "execution.h"
#include <string>

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
        Position(int instrument_id): instrument_id(instrument_id), size(0.0), avg_entry_price(0.0), unrealized_PL(0.0), realized_PL(0.0) {};

        void update_unrealized_PL(Bar& bar); //update the unrealized PL of the position due to a new bar being seen
        void add_to_position(Fill& fill); //add to the position the order we filled, can be either a long or a short, updates realized_PL, quantity and avg_entry_price
        void update_avg_entry_price(double size, double filled_price);
        void close_position(double fill_price); //close the current position with a price fill_price

        //getters
        int get_instument_id(){return this->instrument_id;};
        double get_size(){return this->size;};
        double get_avg_entry_price(){return this->avg_entry_price;};

        //setters
        void set_size(double size){this->size = size;};
};

class PortfolioManager{
    double cash; //the total cash we have 

};


#endif