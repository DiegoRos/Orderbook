#include <iostream>

#include "Orderbook.h"


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
