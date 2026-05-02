#ifndef EXECUTION_H
#define EXECUTION_H

#include "data.h"
#include "portfolio.h"
#include <queue>
#include <string>
#include <memory>

enum EventType{
    MARKETEVENT,
    SIGNALEVENT,
    ORDEREVENT,
    FILLEVENT
};

class Event{
    protected:
        unsigned long timestamp;
        EventType type;
    public:

    EventType get_type(){
        return this->type;
    }

    unsigned long get_timestamp(){
        return this->timestamp;
    }

    virtual ~Event() = default; //have a virtual method function to make this a polymorphic class
};

//foward declarations
class Order;

//this class will most likely have to get a lot more complicated as I continue developping
//need to add links to the portfolio, the signal etc to make the entire trade loop complete and make sense
class ExecutionHandler{
    std::queue<std::unique_ptr<Event>> event_queue; //the queue of events
    std::vector<Bar> past_bars; //growing vector of bars that have been seen

    //a vector of pointers to order events that were not filled on the previous day so we need to add it to the event_queue after having added the next bar
    std::queue<std::unique_ptr<Event>> orders_to_add;

    public:
        void run(DataHandler& dh);
        void post_loop_check(DataHandler& dh);
        void add_bar(Bar bar);
        std::unique_ptr<Event> pop_event();
        void add_event(std::unique_ptr<Event> event);
        void handle_market_event(Event& event);
        void handle_signal_event(Event& event);
        void handle_order_event(Event& event, DataHandler& dh);
        void handle_fill_event(Event& event);

        void create_fill_event_from_order_event(Order& order, double fill_price, unsigned long timestamp);

        //getter for past_bars
        std::vector<Bar> get_past_bars(){return this->past_bars;};


        //getters and setters for order_to_add
        std::unique_ptr<Event> pop_o_event_to_add(){
            std::unique_ptr<Event> o_event = std::move(this->orders_to_add.front());
            this->orders_to_add.pop(); //remove the first element
            return o_event;
        };
            
        void push_o_event_to_add(std::unique_ptr<Event> order_to_add){
            if(order_to_add->get_type() == EventType::ORDEREVENT){
                this->orders_to_add.push(std::move(order_to_add));
            }
        };

        bool orders_to_add_empty(){
            return this->orders_to_add.empty();
        }

};


//don't know how many of these I will be using
enum OrderState{
    CREATED,
    SUBMITTED,
    PARTIALFILL,
    FILLED,
    CANCELLED,
    REJECTED
};

enum OrderType{
    MARKET,
    LIMIT,
    STOP
};


class Order{
    static int max_id;
    int id;
    unsigned long timestamp;
    int index; //corresponds to the index of the bar during which this order was placed
    OrderState state;
    OrderType type;
    std::string ticker;
    double size; //negative represents a short positive represents long
    double price; //the price demanded from the order, not necessarily the one that will get filled by the market
    int expiry_bars; //the number of bars this order remains valid before being cancelled
    public:
        //I made the price optional as a market order does ask for a price wants to buy straight away
        Order(OrderType type, std::string ticker, double size, unsigned long timestamp, int index, double price = 0.0, int expiry_bars=1): type(type), state(OrderState::CREATED), 
            ticker(ticker), price(price), size(size), expiry_bars(expiry_bars), id(max_id++), timestamp(timestamp), index(index){};

        //getters
        OrderState get_state(){return this->state;};
        
        unsigned long get_timestamp(){return this->timestamp;};

        std::string get_ticker(){return this->ticker;};

        double get_size(){return this->size;};

        double get_price(){return this->price;};

        OrderType get_order_type(){return this->type;};

        int get_id(){return this->id;};

        int get_expiry_bars(){return this->expiry_bars;};

        int get_index(){return this->index;};

        //setters
        void set_state(OrderState new_state){this->state = new_state;};


};


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

        double get_commission(){
            //returns the commission associated to a fill, it must be initialized
            return std::max(this->min_commission_rate, std::abs(this->size) * this->commission_rate_per_share);
        }

        //getters
        double get_filled_price(){return this->filled_price;};
        double get_size(){return this->size;};
        double get_comm_rate(){
            return this->commission_rate_per_share;
        };

        double get_min_comm(){
            return this->min_commission_rate;
        };
};

class MarketEvent : public Event{
    Bar bar;
    std::string ticker;
    public:
        MarketEvent(std::string ticker, Bar bar){
            this->timestamp = bar.timestamp; //this information is kind of redundant but we should keep it for the sake of cohesion with other sibling classes
            this->type = EventType::MARKETEVENT;
            this->bar = bar;
            this->ticker = ticker;
        };

        Bar get_bar(){
            return this->bar;
        };

};

class SignalEvent : public Event{
    public:
        SignalEvent(unsigned long timestamp){
            this->timestamp = timestamp;
            this->type = EventType::SIGNALEVENT;
        };


};

class OrderEvent : public Event{
    Order order; //holds a reference to an order to be cheaper & faster
    public:
        OrderEvent(Order order, unsigned long timestamp): order(order){
            this->timestamp = timestamp;
            this->type = EventType::ORDEREVENT;
        };

        Order get_order(){
            return this->order;
        }


};

class FillEvent : public Event{
    Fill fill;
    public:
        FillEvent(Fill fill, unsigned long timestamp): fill(fill){
            this->timestamp = timestamp;
            this->type = EventType::FILLEVENT;
        };


};



//the following functions are used by the Execution handler when determining the slippage and spread of market orders
std::pair<double, double> compute_vol_ratio_n_realized_vol(const std::vector<Bar>& bars); // a function to compute the volatility ratio short_term/long_term
double yang_zhang_rolling(const std::vector<Bar>& bars, int window = 20); //method to estimate the volatility which also handles overnight gaps
double garman_klass_rolling(const std::vector<Bar>& bars, int window);
double corwin_schultz_rolling(const std::vector<Bar>& bars); //method to estimate the spread, or we use a fixed spread depending on what the instrument is 
double volume_to_spread(double volume, double price); //based on the dollar volume of the bar we compute the spread

//functions used to calculate the probability of a limit order going through, models how far our limit order lies in the limit order book
//based on our penetration in the bar and the volume traded
double limit_fill_probability(double limit_price, const Bar& bar, double adv, double order_size);
#endif