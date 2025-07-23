#pragma once

#include <map>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"

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

    struct LevelData
    {
        Quantity quantity_{};
        Quantity count_{};
    
        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };

    std::unordered_map<Price, LevelData> data_;
    
    // All orders
    std::unordered_map<OrderId, OrderEntry> orders_;

    // Order bids in descending order (largest first)
    std::map<Price, OrderPointers, std::greater<Price>> bids_;

    // Order asks in ascending order (smallest first)
    std::map<Price, OrderPointers, std::less<Price>> asks_;

    // Add mutex and threads for handling Good For Day orders
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{ false };
    
    void PruneGoodForDayOrders();

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();

public:
    Orderbook();
    // Create unique ownership of the orderbook so that it cannot be copied or moved.
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;
    ~Orderbook();
    
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify order);
    Trades MatchOrder(OrderModify order);
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;

};
