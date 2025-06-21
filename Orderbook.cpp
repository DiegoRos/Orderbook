#include <iostream>
#include <map>
#include <set>
#include <list>
#include <cmath>
#include <deque>
#include <queue>
#include <stack>
#include <limits>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>
#include <format>
#include <cstdint>

// Order type enum
enum class OrderType
{
    GoodTilCancel,
    FillAndKill

};

// Side of orderbook
enum class Side 
{
    Buy,
    Sell

};


// Aliasing variables to improve code readability
using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;


// For an L3 orderbook Levels made up of price, quantity and time
    // Time will be represented as its position in the OrderPointers list
struct LevelInfo
{
    Price price_;
    Quantity quantity_;

};

// Vector version of LevelInfo struct 
using LevelInfos = std::vector<LevelInfo>;

// Class representation of Order Book Level information
class OrderbookLevelInfos{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) 
        : bids_{ bids }, 
        asks_{ asks }
    { }

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;

};

// Order class to take in orders and do operations on them (fill, get quantity, etc.)
class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }, 
        orderId_{ orderId }, 
        side_{ side }, 
        price_{ price }, 
        initialQuantity_{ quantity }, 
        remainingQuantity_{ quantity }
    {}

    OrderType GetOrderType() const { return orderType_; }
    OrderId GetOrderId() const { return orderId_; } 
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaninig quantity.\n", GetOrderId()));

        remainingQuantity_ -= quantity;
    }

     bool IsFilled() const { return GetRemainingQuantity() == 0; }


private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

// Aliasing variables to improve code readability
// Orders will be stored in mulitple locations in the project so we will want Orders to be as shared pointers so they are not wrongly dereferenced.
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

// Creating abstractions for order modificaiotns.
class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_{ orderId },
        price_{ price },
        side_{ side },
        quantity_{ quantity }
    { }
    
    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const { return price_; }
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Price price_;
    Side side_;
    Quantity quantity_;

};

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : 
        bidTrade_{ bidTrade },
        askTrade_{ askTrade }    
    { }

    const TradeInfo& GetBidTarde() const { return bidTrade_; }
    const TradeInfo& GetAskTrde() const { return askTrade_; }
    
private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;

};

// Since one order can fulfill multiple orders we might want to have a vector of trades made
using Trades = std::vector<Trade>;


// Main Orderbook class
class Orderbook
{
private:
    // Implementing storage of bids and asks
    //  - We want to use a map that will have bids and asks stored in order
    //  - However, we also want to be able to access orders based on their ID. This is why we used an OrderPointer List above as we can store an iterator of each order
    //      to keep track of its location in the list so that we can access it as well.

    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };
    
    // All orders
    std::unordered_map<OrderId, OrderEntry> orders_;

    // Order bids in descending order (largest first)
    std::map<Price, OrderPointers, std::greater<Price>> bids_;

    // Order asks in ascending order (smallest first)
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    
    bool CanMatch(Side side, Price price) const
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

    Trades MatchOrders()
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

public:

    Trades AddOrder(OrderPointer order)
    {
        if (orders_.contains(order->GetOrderId()))
            return { };

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

    void CancelOrder(OrderId orderId)
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

    Trades MatchOrder(OrderModify order)
    {
        if (!orders_.contains(order.GetOrderId()))
            return { };

        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }

    OrderbookLevelInfos GetOrderInfos() const
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


};

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