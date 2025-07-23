#include <iostream>
#include <chrono>
#include <ctime> 

#include "Orderbook.h"


// PRIVATE METHODS

// This function will opearate in a separate thread to prune Good For Day orders at the end of the trading day.
void Orderbook::PruneGoodForDayOrders()
{
    using namespace std::chrono;
    const auto end = hours(16);

    while (true)
    {
        const auto now = system_clock::now();
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;
        localtime_r(&now_c, &now_parts);

        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1; // Move to next day if we are past the end of the trading day.

        // Set the time to the end of the trading day (4pm).
        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;

        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(1); // Add 1 millisecond to ensure we don't miss the next prune time.

        {
            std::unique_lock ordersLock{ ordersMutex_ };

            // If orderbook is shut down or orderbook is shut down while waiting, exit the loop and return.
            if(shutdown_.load(std::memory_order_acquire) || 
                shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
                
                return; // Exit if shutdown is requested or if we are notified to stop.
        }

        OrderIds orderIds;

        // Collect all Good for day orders in orderIds vetor to cancel them.
        {
            std::scoped_lock ordersLock{ ordersMutex_ };
            for (const auto& [_, entry] : orders_)
            {
                const auto& [order, __] = entry;
                
                if (order->GetOrderType() == OrderType::GoodForDay)
                    continue;
                
                orderIds.push_back(order->GetOrderId());

            }

        }
        
        CancelOrders(orderIds);
    }

}

/*
NOTE: We split the CancelOrders and CancelOrder functions so that we only take the mutex lock once when cancelling orders.
If we were to use the original CancelOrder function then the mutex would be "taken" every time the function is called, which would lead to a performance hit.
*/

void Orderbook::CancelOrders(OrderIds orderIds)
{
    std::scoped_lock ordersLock { ordersMutex_ };
    for (const auto& orderId : orderIds)
        CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId)
{
    if (!orders_.contains(orderId))
        return;

    const auto& [order, iterator] = orders_.at(orderId);
    orders_.erase(orderId);

    if (order->GetSide() == Side::Buy)
    {
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(iterator);

        if (orders.empty())
            bids_.erase(price);
    }
    else
    {
        auto price = order->GetPrice();
        auto& orders = asks_.at(price);
        orders.erase(iterator);

        if (orders.empty())
            asks_.erase(price);
    }

    OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
    UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
     auto & data = data_[price];

     data.count_ += action == LevelData::Action::Add ? 1 : (action == LevelData::Action::Remove ? -1 : 0);
     if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
        data.quantity_ -= quantity;
    else
        data.quantity_ += quantity;
    
    if (data.count_ == 0)
        data_.erase(price);

}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;
    
    std::optional<Price> threshold;
    if (side == Side::Buy)
    {
        const auto& [askPrice, _] = *asks_.begin();
        threshold = askPrice;
    }
    else
    {
        const auto& [bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
    }

    for (const auto& [levelPrice, levelData] : data_)
    {
        if (threshold.has_value() && 
            ((side == Side::Buy && threshold.value() > levelPrice) || 
             (side == Side::Sell && threshold.value() < levelPrice)))
            continue;
        
        if ((side == Side::Buy && levelPrice > price) || 
            (side == Side::Sell && levelPrice < price))
            continue;
        
        if (quantity <= levelData.quantity_)
            return true;
        
        quantity -= levelData.quantity_;
    }

    return false;
}


bool Orderbook::CanMatch(Side side, Price price) const
{
    // Buy side check
    if (side == Side::Buy)
    {
        if (asks_.empty())
            return false;
        
        // Get best ask from our ordered map.
        const auto& [bestAsk, _] = *asks_.begin();
        // If our bid price is higher than the best ask then we can fill.
        return price >= bestAsk;
    }
    // Sell side check
    else
    {
        if (bids_.empty())
            return false;
        
        // Get best bid from our ordered map.
        const auto& [bestBid, _] = *bids_.begin();
        // if our is at least as big as our ask then we fill the order.
        return price <= bestBid;
    }
}

Trades Orderbook::MatchOrders()
{
    Trades trades;
    // Assuming all orders can fill the orderbook we can create a maximum size for the vector 
    trades.reserve(orders_.size());

    while (true)
    {
        if (bids_.empty() || asks_.empty())
            break;

        auto& [bidPrice, bids] = *bids_.begin();
        auto& [askPrice, asks] = *asks_.begin();

        // If our highest bidPrice is not as big as our lowest ask price no orders can be filled. Break.
        if (bidPrice < askPrice)
            break;

        while(bids.size() && asks.size())
        {
            auto& bid = bids.front();
            auto& ask = asks.front();
            
            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetFilledQuantity());
            bid->Fill(quantity);
            ask->Fill(quantity);
            
            if (bid->IsFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }

            if (ask->IsFilled())
            {
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }

            if (bids.empty())
                bids_.erase(bidPrice);

            if (asks.empty())
                asks_.erase(askPrice);

            trades.push_back(Trade{ 
                TradeInfo { bid->GetOrderId(), bid->GetPrice(), quantity},
                TradeInfo { ask->GetOrderId(), ask->GetPrice(), quantity}
            });

            OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
            OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());

        }
        
        if (bids.empty())
        {
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
        }
    
        if (asks.empty())
        {
            asks_.erase(askPrice);
            data_.erase(askPrice);
        }
	}

    if (!bids_.empty())
    {
        auto& [_, bids] = *bids_.begin();
        auto& order = bids.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrder(order->GetOrderId());
    }

    if (!asks_.empty())
    {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrder(order->GetOrderId());
    }

    return trades;
}


// PUBLIC METHODS
Orderbook::Orderbook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } } { }

Orderbook::~Orderbook()
{
    shutdown_.store(true, std::memory_order_release);
    shutdownConditionVariable_.notify_one();
    ordersPruneThread_.join();
}


Trades Orderbook::AddOrder(OrderPointer order)
{
    std::scoped_lock ordersLock{ ordersMutex_ };

    if (orders_.contains(order->GetOrderId()))
        return { };

    if (order->GetOrderType() == OrderType::Market)
    {
        // We will essentially create a limit order for the market order, with the worst price as the upper or lower bound (depending on the side).
        // This way the market order will buy/sell from the best available orders until it is either filled or there are no more orders to match against.
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto& [worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto& [worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        }
        else
            return { }; // No orders to match against.
    }

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return { };

    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity()))
        return { };

    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    }
    else
    {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    }

    orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });

    OnOrderAdded(order);

    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
{
    std::scoped_lock ordersLock { ordersMutex_ };
    CancelOrderInternal(orderId);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
    OrderType orderType;
    {
        std::scoped_lock ordersLock{ ordersMutex_ };

        if (!orders_.contains(order.GetOrderId()))
            return { };

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        orderType = existingOrder->GetOrderType();
    }

    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(orderType));

}


Trades Orderbook::MatchOrder(OrderModify order)
{
    if (!orders_.contains(order.GetOrderId()))
        return { };

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

std::size_t Orderbook::Size() const { return orders_.size(); }

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
    {
        // Recursively get number of orders
        return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](std::size_t runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); }) };
    };

    for (const auto& [price, orders] :bids_)
        bidInfos.push_back(CreateLevelInfos(price, orders));

    for (const auto& [price, orders] : asks_)
        askInfos.push_back(CreateLevelInfos(price, orders));

    return OrderbookLevelInfos{ bidInfos, askInfos };
}

int main()
{
    Orderbook orderbook;
    const OrderId orderId = 1;
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTilCancel, orderId, Side::Buy, 100, 10));
    std::cout << orderbook.Size() << std::endl; // 1

    orderbook.CancelOrder(orderId);
    std::cout << orderbook.Size() << std::endl; // 0

    return 0;

}
