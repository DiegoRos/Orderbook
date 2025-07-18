#pragma once

#include <list>
#include <exception>
#include <format>
#include <memory>
#include <cmath>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"

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

    Order(OrderId orderId, Side side, Quantity quantity)
        : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
    { }

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

     void ToGoodTillCancel(Price price)
     {
        if (GetOrderType() != OrderType::Market)
            throw std::logic_error(std::format("Order ({}) cannot have its price modified as it is not a market order.\n", GetOrderId()));

        if (!std::isfinite(price))
            throw std::logic_error(std::format("Order ({}) must have a tradable price.\n", GetOrderId()));
     }

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

