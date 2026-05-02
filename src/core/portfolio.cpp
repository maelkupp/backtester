#include "portfolio.h"
#include "data.h"
#include "execution.h"
#include <iostream>
#include <cmath>

void Position::update_unrealized_PL(Bar& bar){
    //function responsible for updating the unrealized PL, decided to take it at the opening of the bar
    std::cout << "updating position with a new bar\n";
    this->unrealized_PL = (bar.close - this->avg_entry_price)*this->size; //the sign of this->size handles the case we are shorting etc
};


void Position::update_avg_entry_price(double size, double filled_price){
    std::cout << "updating avg entry price\n";
    this->avg_entry_price = (std::abs(this->size)*this->avg_entry_price + std::abs(size)*filled_price)/(std::abs(this->size) + std::abs(size));
};

void Position::close_position(double fill_price){
    //this method behaves like we are closing the position with fill_price
    double realized_PL;
    if(this->size > 0){
        //closing a long
        realized_PL = (fill_price - this->avg_entry_price)*std::abs(this->size);
    }else{
        //closing a short
        realized_PL = (this->avg_entry_price - fill_price)*std::abs(this->size);
    }
    this->realized_PL += realized_PL;
    this-> realized_PL -= std::max(Fill::min_commission_rate, std::abs(this->size)*Fill::commission_rate_per_share);
    //we are closing the position entirely so set both values to 0
    this->size = 0.0;
    this->avg_entry_price = 0.0;
}

//need to pay attention, I think I need to include the commissions from the fill
void Position::add_to_position(Fill& fill){
    //function that updates the position size, avg_entry_price and realised PL based on a fill that got executed
    std::cout << "add a filled order to the position \n";
    double fill_direction = copysign(1.0, fill.get_size());
    double portfolio_direction = copysign(1.0, this->size);

    double fill_size = fill.get_size();
    double fill_price = fill.get_filled_price();
    //apply the commissions related to the Fill order straight away, regardless of whether it is a buy or sell order
    if( fill_direction == portfolio_direction || portfolio_direction == 0.0){
        //we are either simply adding to a long, adding to a short or initiliazing or position (completeling our first fill with this instrument)
        //simply update the avg entry price and the overall size of the position
        this->update_avg_entry_price(fill_size, fill_price); 
        this->set_size(this->size + fill_size); //update the size with the rule: this->size += fill.size
    }else if(std::abs(fill_size) < std::abs(this->size)){
        //we are reducing our position, but not closing entirely and not reversing the position
        this->set_size(this->size + fill_size); //our position gets closer to 0 (either from positive or negative)
        double realized_PL;
        if(fill_size < 0){
            //closing a long
            realized_PL = (fill_price - this->avg_entry_price)*std::abs(fill_size);
        }else{
            //closing a short
            realized_PL = (this->avg_entry_price - fill_price)*std::abs(fill_size);
        }
        this->realized_PL += realized_PL;
    }else if(std::abs(std::abs(fill_size) - std::abs(this->size)) < 1e-5){
        this->close_position(fill_price);
    }else{
        //we are reversing the position, will split the operation and act like we are closing the first part and then only long/short what is left
        double size_difference = copysign(std::abs(fill_size) - std::abs(this->size), fill_size); //call this before closing the position so this->size != 0
        this->close_position(fill_price);
        // gives us number with magnitude |fill_size| - |this->sign| and sign sgn(fill_size), tells us how much we have left and in which direction
        this->size = size_difference;
        this->avg_entry_price = fill_price;
    }
};