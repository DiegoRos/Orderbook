#pragma once

#include "TradeInfo.h"

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
