#include <iostream>

#include "Orderbook.h"


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

Trades Orderbook::AddOrder(OrderPointer order)
{
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
    return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
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
