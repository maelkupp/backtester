#ifndef EXECUTION_H
#define EXECUTION_H

#include "data.h"
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

    virtual ~Event() = default;
};

//forward declare these three classes so that the handler classes can be properly defined
class Order;
class Fill;
class Signal;

//this class will most likely have to get a lot more complicated as I continue developping
//need to add links to the portfolio, the signal etc to make the entire trade loop complete and make sense
class ExecutionHandler{
    std::queue<std::unique_ptr<Event>> event_queue; //the queue of events
    std::vector<Bar> past_bars; //growing vector of bars that have been seen

    public:
        void run(DataHandler& dh);
        void add_bar(Bar bar);
        std::unique_ptr<Event> pop_event();
        void add_event(std::unique_ptr<Event> event);
        void handle_market_event(Event& event);
        void handle_signal_event(Event& event);
        void handle_order_event(Event& event, DataHandler& dh);
        void handle_fill_event(Event& event);

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
    Order& order; //holds a reference to an order to be cheaper & faster
    public:
        OrderEvent(Order& order, unsigned long timestamp): order(order){
            this->timestamp = timestamp;
            this->type = EventType::ORDEREVENT;
        };

        Order& get_order(){
            return this->order;
        }


};

class FillEvent : public Event{
    Fill& fill;
    public:
        FillEvent(Fill& fill, unsigned long timestamp): fill(fill){
            this->timestamp = timestamp;
            this->type = EventType::FILLEVENT;
        };


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
    static int max_id; //will think about parallelism later, but this could be a static atomic<int> max_id
    int id;
    OrderState state;
    OrderType type;
    std::string ticker;
    double size; //negative represents a short positive represents long
    double price; //the price demanded from the order, not necessarily the one that will get filled by the market
    int time_in_force; //need to decide how I implement this, not too clear for me right now
    public:
        //I made the price optional as a market order does ask for a price wants to buy straight away
        Order(OrderType type, std::string ticker, double size, double price = 0.0, int time_in_force=-1): type(type), state(OrderState::CREATED), 
            ticker(ticker), price(price), size(size), time_in_force(time_in_force), id(max_id++){};

        //getters
        OrderState get_state(){return this->state;};

        std::string get_ticker(){return this->ticker;};

        int get_size(){return this->size;};

        double get_price(){return this->price;};

        OrderType get_order_type(){return this->type;};

        int get_id(){return this->id;};

        //setters
        void set_state(OrderState new_state){this->state = new_state;};


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