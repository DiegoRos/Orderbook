#pragma once

/*
Available order types supported and descriptions:
    - Good Til Cancel: This is an order that remains active until the order is either filled or the investor cancels it.
    - Fill And Kill: This order attempts to fill as much, or all, of the order instantly. Whatever quantity is not filled instantly is canceled.
    - Fill Or Kill: This order must be excecuted in its entirety immeditely, or ti is canceled ientirely if it cannot be filled all at once.
    - Good For Day: This is an order that will remain active until the end of the trading day.
    - Market: This order will get the number of securities desired by the trader regardless of the price, will take all best available until quantity is filled.
*/


// Order type enum
enum class OrderType
{
    GoodTilCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market,

};