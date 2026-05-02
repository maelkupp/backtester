#include "execution.h"
#include "portfolio.h"
#include <iostream>
#include <numeric>
#include <cmath>
#include <memory>
#include <algorithm>
#include <random>


int Order::max_id = 0;

void ExecutionHandler::add_event(std::unique_ptr<Event> event){
    std::cout << "adding event of type " << event->get_type() << "\n";
    this->event_queue.push(std::move(event));
};

std::unique_ptr<Event> ExecutionHandler::pop_event(){
    if(!this->event_queue.empty()){
        //if the queue is not empty return
        std::unique_ptr<Event> last_event = std::move(this->event_queue.front());
        this->event_queue.pop();
        return last_event;
    }

    return nullptr;

};

void ExecutionHandler::run(DataHandler& dh){
    while(! this->event_queue.empty()){
        //we continue looping until we have no more events to take into account

        //obtain the first event that we should consider of the front of the queue
        std::unique_ptr<Event> last_event = this->pop_event();
        switch(last_event->get_type()){
            case EventType::MARKETEVENT:
                this->handle_market_event(*last_event);
                break;
            case EventType::SIGNALEVENT:
                this->handle_signal_event(*last_event);
                break;
            case EventType::ORDEREVENT:
                this->handle_order_event(*last_event, dh);
                break;
            case EventType::FILLEVENT:
                this->handle_fill_event(*last_event);
                break;
        }
        this->post_loop_check(dh); // call a function after the loop ends to perform what is needed for the next day to work: add market event + potential order event
    }
};

void ExecutionHandler::post_loop_check(DataHandler& dh){
    //add the next market event, add this first
    if(!dh.bars.empty()){
        //if there are still bars to read
        std::unique_ptr<MarketEvent> me = std::make_unique<MarketEvent>(dh.get_ticker(), dh.get_next_market_event());
        this->add_event(std::move(me));
    }


    //add potential order event
    while(!this->orders_to_add_empty()){
        std::unique_ptr<Event> o_event = this->pop_o_event_to_add();
        if(o_event->get_type() != EventType::ORDEREVENT){
            continue; //pass this loop
        }
        this->add_event(std::move(o_event));
    }
};


void ExecutionHandler::handle_market_event(Event& event){
    //std::cout << "handling market event with timestamp " << event.get_timestamp() << "\n";
    EventType e_type = event.get_type();
    if(e_type != EventType::MARKETEVENT){
        throw std::invalid_argument("Asked the order event to handle another event than an market event");
        return;
    }

    MarketEvent& m_event = dynamic_cast<MarketEvent&>(event);
    this->past_bars.push_back(m_event.get_bar()); //add the bar stored in the market event into the vector of bars of the execution handler
    
};

void ExecutionHandler::handle_signal_event(Event& event){
    std::cout << "handling signal event\n";

};

void ExecutionHandler::create_fill_event_from_order_event(Order& order, double fill_price, unsigned long timestamp){
    std::cout << "creating fill event from order event\n";
    Fill fill = Fill(order.get_ticker(), order.get_size(), fill_price, order.get_id());
    std::unique_ptr<FillEvent> f_event = std::make_unique<FillEvent>(fill, timestamp); 
    this->add_event(std::move(f_event)); //add the FillEvent to the event queue
    order.set_state(OrderState::FILLED); //now set the order to filled
}
void ExecutionHandler::handle_order_event(Event& event, DataHandler& dh){
    //pass the datahandler as a reference to have access to the next bar we will see
    //this consumes an order event and creates a Fill event, simulating a Fill with slippage + Spread + Commission etc, uses the opening price of the next bar
    std::cout << "handling order event\n";
    EventType e_type = event.get_type();
    if(e_type != EventType::ORDEREVENT){
        throw std::invalid_argument("Asked the order event handler to handle another event than an order event");
        return;
    }

    if(this->past_bars.size() < 252){
        return;
    }

    OrderEvent& order_event = dynamic_cast<OrderEvent&>(event);
    Order order = order_event.get_order();
    Bar last_bar = this->past_bars[this->past_bars.size()-1]; //get the last bar we have seen

    //right now changing the state is pointless as order is a stack variable, will need to change this later on when the lifetime of the Order objects
    //becomes clearer, make sure the order is not a market order before cancelling it
    if(order.get_order_type() != OrderType::MARKET && order.get_expiry_bars() <= last_bar.index - order.get_index()){
        order.set_state(OrderState::CANCELLED);
        std::cout << "cancelled the order \n";
        return;
    }

    OrderType o_type = order.get_order_type();

    bool filled = false;

    //need to figure out how to obtain the next bar
    switch(o_type){
        case(OrderType::MARKET):{
            //for a market event we just get the next opening price (we look ahead) + some slippage in the direction that negatively impacts the PL
            //ask = mid + spread/2, fill_price = ask + slippage (a function of order size relative to volatility)
            std::cout << "handling market order\n";

            
            double adv = std::accumulate(this->past_bars.end() - 20, this->past_bars.end(), 0.0, 
                                        [](int accumulator, const Bar& bar){
                                            return accumulator + bar.volume;
                                        });

            adv /= static_cast<double>(this->past_bars.size());

            auto [vol_ratio, realized_vol] = compute_vol_ratio_n_realized_vol(this->past_bars);
            double base_spread  = volume_to_spread(last_bar.volume, last_bar.open); 

            double effective_spread = base_spread * vol_ratio;

            double k = 0.314;
            double slippage = k * realized_vol * sqrt(order.get_size() / adv);

            double mid = dh.bars.front().open; //get the next bar and set the mid price as the open of the next bar
            
            //now create a Fill object, for now assume that everything gits filled, there is no partial fill
            double fill_price = mid + copysign(1.0, order.get_size())*effective_spread/2 + copysign(1.0, order.get_size())*slippage; //copysign(1.0, x) behaves like sgn(x)
            this->create_fill_event_from_order_event(order, fill_price, dh.bars.front().timestamp);//it happens on the next day, so that is the timestapm we associate to the fill event
            filled = true;

            break;
        }
        case(OrderType::LIMIT):{
            //for a limit order we wait until the price exceeds the demand_price if it does we fill the order
            //gap fill is when the open price is below your ask price, you then fill at open price, this is favorable for the trader
            std::cout << "handling limit order \n";
            double open = dh.bars.front().open;
            double low = dh.bars.front().low;
            double high = dh.bars.front().high;
            double limit_price = order.get_price();

            double adv = std::accumulate(this->past_bars.end() - 20, this->past_bars.end(), 0.0, 
                                        [](int accumulator, const Bar& bar){
                                            return accumulator + bar.volume;
                                        });

            adv /= static_cast<double>(this->past_bars.size());

            double p = rand() / (RAND_MAX + 1.);
            double filled_prob;

            if(order.get_size() > 0){
                //buy order
                if(open < limit_price){
                    //gap fill we always fill our order
                    filled = true;

                }else if(low < limit_price && limit_price < open){
                    //market has crossed the order price so we can simulate that we would have bought
                    filled_prob = limit_fill_probability(limit_price, dh.bars.front(), adv, order.get_size());
                    filled = (filled_prob < p);
                } 
            }else{
                //sell order
                if(open > limit_price){
                    //gap fill alwyas fill
                    filled = true;

                }else if(high > limit_price && open < limit_price){
                    //market has crossed order price
                    filled_prob = limit_fill_probability(limit_price, dh.bars.front(), adv, order.get_size());
                    filled = (filled_prob < p);
                } 
            }

            if(filled){
                this->create_fill_event_from_order_event(order, limit_price, dh.bars.front().timestamp);
            }

            break;
        }
        case(OrderType::STOP):{
            //order is dormat until market trades through order price, then becomes market order need to implement the slippage that occurs
            std::cout << "handling stop order \n";
            double stop_price = order.get_price();
            double size = order.get_size();

            double open = dh.bars.front().open;
            double low = dh.bars.front().low;
            double high = dh.bars.front().high;

            bool stop_to_market_order = false; //boolean flag to check if our stop order got triggered into a market order at price S

            if(size > 0 && high > stop_price){
                //buy stop
                if(open > stop_price){
                    //stop slippage occurs so we fill at open
                    filled = true;
                }else{
                    //behaves like a regular buy market order with fill_price = S + spread/2 + slippage
                    stop_to_market_order = true;
                }

            }else if(size < 0 && low < stop_price){
                //sell stop
                if(open < stop_price){
                    filled = true;
                }else{
                    //behaves like a regular sell market order with fill_price = S - spread/2 - slippage
                    stop_to_market_order = true;
                }
            }

            if(stop_to_market_order){
                //use the same logic as in the market order case but instead of the mid_price being the open of the next bar, it is the stop_price
                double adv = std::accumulate(this->past_bars.end() - 20, this->past_bars.end(), 0.0, 
                                            [](int accumulator, const Bar& bar){
                                                return accumulator + bar.volume;
                                            });

                adv /= static_cast<double>(this->past_bars.size());

                auto [vol_ratio, realized_vol] = compute_vol_ratio_n_realized_vol(this->past_bars);
                double base_spread  = volume_to_spread(last_bar.volume, last_bar.open); 

                double effective_spread = base_spread * vol_ratio;

                double k = 0.5; //a higher slippage constant than in the regular market order case
                double slippage = k * realized_vol * sqrt(order.get_size() / adv);

                
                //now create a Fill object, for now assume that everything gits filled, there is no partial fill
                stop_price = stop_price + copysign(1.0, order.get_size())*effective_spread/2 + copysign(1.0, order.get_size())*slippage; //copysign(1.0, x) behaves like sgn(x)
            }
            

            if(filled){
                this->create_fill_event_from_order_event(order, stop_price, dh.bars.front().timestamp);
            }
        
        break;
        }
    }

    if(!filled){
        //an order was sent but we did not manage to full it so we add it to be handled on the next day again
        this->push_o_event_to_add(std::move(std::make_unique<OrderEvent>(order_event.get_order(), order_event.get_timestamp())));
        //cannot pass the order variable as it is a reference
    }
};

void ExecutionHandler::handle_fill_event(Event& event){
    std::cout << "handling fill event\n";


};



std::pair<double, double> compute_vol_ratio_n_realized_vol(const std::vector<Bar>& bars) {
    // the way this should be called is by called it with the size of the bar vector 
    // short window still 20, long window = min(252, available bars)
    if(bars.size() < 252){
        throw std::invalid_argument("Trying to compute the volatility ratio but do no have enough data points to compute the long term volatility");
        return {0.0, 0.0};
    }
    int long_window = 252;
    int short_window = 20;
    
    double short_vol = yang_zhang_rolling(bars, short_window);
    double long_vol  = yang_zhang_rolling(bars, long_window);
    return {short_vol / long_vol, short_vol};

};


double close_to_close_rolling(const std::vector<Bar>& bars, int window){
    //throws away all intrabar information and is sensitive to overnight gaps
    size_t n = bars.size();
    if(n+1 < static_cast<int>(window)){
        return 1.0;
    }

    double sum_squared {0.0};
    double sum {0.0};
    double t;
    for(size_t i=0; i<window; ++i){
        size_t index = n-i-1;
        t = log(bars[index].close/bars[index-1].close);
        sum += t;
        t *= t;
        sum_squared += t;
    }

    sum *= sum;
    sum /= (window*window);

    sum_squared /= window;
    double var = sum_squared - sum;

    return sqrt(var);
};

double high_to_low_rolling(const std::vector<Bar>& bars, int window){
    //underestimates volatility when there are trends and ignore overnight gaps
    size_t n = bars.size();
    if(n < static_cast<int>(window)){
        return 1.0;
    }

    double sum {0.0};
    Bar curr_bar;
    for(size_t i=0; i<window; ++i){
        curr_bar = bars[n-i-1];
        sum += log(curr_bar.high/curr_bar.low)*log(curr_bar.high/curr_bar.low);
    }

    sum *= 1/(4*log(2));
    return sum;
};

double garman_klass_rolling(const std::vector<Bar>& bars, int window){
    //takes into account full bars range and open to close drift, doesnt look at overnight gaps
    size_t n = bars.size();
    if(n < static_cast<int>(window)){
        //don't have enough values to compute the mean on the rolling window
        return 1.0; //don't know if this is the correct value to return
    }

    double sum {0.0};
    Bar curr_bar;
    for(size_t i=0; i<window; ++i){
        curr_bar = bars[n-i-1];
        sum += 0.5*log(curr_bar.high/curr_bar.low)*log(curr_bar.high/curr_bar.low) - (2*log(2)-1)*log(curr_bar.close/curr_bar.open)*log(curr_bar.close/curr_bar.open);
    }
    sum /= window;
    
    return sum;
}

double yang_zhang_rolling(const std::vector<Bar>& bars, int window){
    double k = 0.34;
    return close_to_close_rolling(bars, window) + k*high_to_low_rolling(bars, window) + (1-k)*garman_klass_rolling(bars, window);

};


//I should make this function take into account the duration of the bar too, otherwise it doesn't make sense
double volume_to_spread(double volume, double price){
    double notional = volume * price;  // dollar volume is more meaningful than share volume
    
    //values were determined by claude should look over them to see if I like them
    if (notional >= 1e9)       return 0.0001;  // 1 bp  - very liquid (SPY-like)
    else if (notional >= 1e8)  return 0.0002;  // 2 bps - large cap
    else if (notional >= 1e7)  return 0.0005;  // 5 bps - mid cap
    else if (notional >= 1e6)  return 0.0010;  // 10 bps - small cap
    else                      return 0.0050;  // 50 bps - illiquid

};


double limit_fill_probability(double limit_price, const Bar& bar, double adv, double order_size){
    double penetration;

    if(order_size > 0){
        //buy order, the penetrtion is higher the closer we are to the high
        penetration = (limit_price - bar.low)/(bar.high - bar.low);
    }else{
        //sell order, the penetration is higher the closer we are to the low
        penetration = (bar.high - limit_price)/(bar.high - bar.low);
    }

    double size_penalty = std::exp(-5.0 * (order_size/adv)); //-3 is an arbitrary parameter, the higher it is the less we punish relatively large orders
    return std::clamp(penetration*size_penalty, 0.0, 1.0); // returns min(0.0, penetration*size_penalty), max(0, penetration*size_penalty) ensures it lies between 0 and 1
    
};