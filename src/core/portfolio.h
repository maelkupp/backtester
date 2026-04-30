#ifndef PORTFOLIO_H
#define PORTFOLIO_H


//just defining this for now, still need to decide how I represent partially filled order and other details, for now will assume everything is fully filled
class Fill{
    static constexpr double commission_rate_per_share =  0.005; // 0.005 per share bought
    static constexpr double min_commission_rate = 1; //the commission can no go below 1 unit
    
    std::string ticker;
    double size; //could be positive or negative
    double filled_price;
    int order_id; //the id of the order we are filling


    public:
        Fill(std::string ticker, double size, double price, int order_id): ticker(ticker), size(size), filled_price(price), order_id(order_id){};

    //getters
    double get_comm_rate(){
        return this->commission_rate_per_share;
    };

    double get_min_comm(){
        return this->min_commission_rate;
    };
};



#endif