#pragma once

#include "Usings.h"

// For an L3 orderbook Levels made up of price, quantity and time
    // Time will be represented as its position in the OrderPointers list
struct LevelInfo
{
    Price price_;
    Quantity quantity_;

};

// Vector version of LevelInfo struct 
using LevelInfos = std::vector<LevelInfo>;