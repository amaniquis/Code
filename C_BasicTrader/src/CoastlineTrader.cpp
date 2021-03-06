#include "CoastlineTrader.h"
#include <string>
#include <deque>
#include <iostream>
#include <cmath>
#include <stdlib.h>
#include <assert.h>
#include "helper/Macros.h"

typedef unsigned int uint;

CoastlineTrader::CoastlineTrader()
{
}

CoastlineTrader::CoastlineTrader(double dOriginal, double dUp, double dDown, double profitT, string FxRate, int lS) : deltaUp(dUp), deltaDown(dDown), deltaOriginal(dOriginal),
                                                                                                                      longShort(lS), profitTarget(profitT), cashLimit(profitT)
{
    tP = 0.0; /* -- Total position -- */

    pnl = tempPnl = pnlPerc = 0.0;
    shrinkFlong = shrinkFshort = 1.0;
    increaseLong = increaseShort = 0.0;

    fxRate = FxRate;
}

double CoastlineTrader::computePnl(PriceFeedData::Price price)
{
    double profitLoss = 0.0;
    double pricE = (tP > 0.0 ? price.bid : price.ask);
    for (uint i = 0; i < sizes.size(); i++)
    {
        profitLoss += sizes[i] * (pricE - prices[i]);
    }
    return profitLoss;
}

double CoastlineTrader::computePnlLastPrice()
{
    if (prices.size() == 0)
    {
        return 0.0;
    }

    double profitLoss = 0.0;
    for (uint i = 0; i < sizes.size(); i++)
    {
        profitLoss += sizes[i] * (lastPrice - prices[i]);
    }
    return profitLoss;
}

double CoastlineTrader::getPercPnl(PriceFeedData::Price price)
{
    double pricE = (tP > 0.0 ? price.bid : price.ask);
    double percentage = 0.0;

    for (uint i = 0; i < sizes.size(); i++)
    {
        double absProfitLoss = pricE - prices[i];
        percentage += (absProfitLoss / prices[i]) * sizes[i];
    }
    return percentage;
}

bool CoastlineTrader::tryToClose(PriceFeedData::Price price)
{
    if ((tempPnl + computePnl(price)) / cashLimit >= 1)
    {
        double pricE = (tP > 0.0 ? price.bid : price.ask);
        double addPnl = 0;
        uint len = prices.size();
        for (uint i = 0; i < len; i++)
        {
            addPnl = (pricE - prices.front()) * sizes.front();
            tempPnl += addPnl;
            tP -= sizes.front();
            sizes.erase(sizes.begin());
            prices.erase(prices.begin());
            if (i > 0)
                increaseLong += -1.0;
        }
        pnl += tempPnl;
        pnlPerc += (tempPnl) / cashLimit * profitTarget;
        tempPnl = 0;
        assert(sizes.empty());
        assert(prices.empty());
        tP = 0;
        return true;
    }
    return false;
}

bool CoastlineTrader::assignCashTarget()
{
    cashLimit = lastPrice * profitTarget;

    return true;
}

bool CoastlineTrader::runPriceAsymm(PriceFeedData::Price price, double oppositeInv)
{
    if (!initalized)
    {
        runner = Runner(deltaUp, deltaDown, price, fxRate, deltaUp, deltaDown);

        runnerG[0][0] = Runner(0.75 * deltaUp, 1.50 * deltaDown, price, fxRate, 0.75 * deltaUp, 0.75 * deltaUp);
        runnerG[0][1] = Runner(0.50 * deltaUp, 2.00 * deltaDown, price, fxRate, 0.50 * deltaUp, 0.50 * deltaUp);

        runnerG[1][0] = Runner(1.50 * deltaUp, 0.75 * deltaDown, price, fxRate, 0.75 * deltaDown, 0.75 * deltaDown);
        runnerG[1][1] = Runner(2.00 * deltaUp, 0.50 * deltaDown, price, fxRate, 0.50 * deltaDown, 0.50 * deltaDown);

        liquidity = LocalLiquidity(deltaOriginal, deltaUp, deltaDown, price, deltaOriginal * 2.525729, 50.0);
        initalized = true;
    }

    lastPrice = price.getMid();

    if (!liquidity.computation(price))
    {
        cout << "Didn't compute liquidity!" << endl;
    }

    int event = 0;
    double fraction = 1.0;
    double size = (liquidity.liq < 0.5 ? 0.5 : 1.0);
    size = (liquidity.liq < 0.1 ? 0.1 : size);

    if (longShort == 1)
    { // Long positions only
        event = runner.run(price);

        if (15.0 <= tP && tP < 30.0)
        {
            event = runnerG[0][0].run(price);
            runnerG[0][1].run(price);
            fraction = 0.5;
        }
        else if (tP >= 30.0)
        {
            event = runnerG[0][1].run(price);
            runnerG[0][0].run(price);
            fraction = 0.25;
        }
        else
        {
            runnerG[0][0].run(price);
            runnerG[0][1].run(price);
        }
    }
    else if (longShort == -1)
    { // Short positions only
        event = runner.run(price);
        if (-30.0 < tP && tP < -15.0)
        {
            event = runnerG[1][0].run(price);
            runnerG[1][1].run(price);
            fraction = 0.5;
        }
        else if (tP <= -30.0)
        {
            event = runnerG[1][1].run(price);
            runnerG[1][0].run(price);
            fraction = 0.25;
        }
        else
        {
            runnerG[1][0].run(price);
            runnerG[1][1].run(price);
        }
    }

    if (tryToClose(price))
    { /* -- Try to close position -- */
        IFDEBUG(cout << "longShort: " << longShort << "; tP: " << tP << "; pnl: " << pnl << "; pnlPerc: " << pnlPerc << "; tempPnl: " << tempPnl << "; unrealized: " << computePnlLastPrice() << "; cashLimit: " << cashLimit << "; price: " << lastPrice << "; runner.type: " << runner.type << std::endl);
        IFDEBUG(cout << "Close" << endl);
        return true;
    }

    if (longShort == 1)
    { // Long positions only
        if (event < 0)
        {
            if (tP == 0.0)
            { // Open long position
                int sign = -runner.type;
                if (std::abs(oppositeInv) > 15.0)
                {
                    size = 1.0;
                    if (std::abs(oppositeInv) > 30.0)
                    {
                        size = 1.0;
                    }
                }
                double sizeToAdd = sign * size;
                tP += sizeToAdd;
                sizes.push_back(sizeToAdd);

                prices.push_back(sign == 1 ? price.ask : price.bid);
                assignCashTarget();
                IFDEBUG(cout << "Open long" << endl);
            }
            else if (tP > 0.0)
            { // Increase long position (buy)
                int sign = -runner.type;
                double sizeToAdd = sign * size * fraction * shrinkFlong;
                if (sizeToAdd < 0.0)
                {
                    cout << "How did this happen! increase position but neg size: " << sizeToAdd << endl;
                    sizeToAdd = -sizeToAdd;
                }
                increaseLong += 1.0;
                tP += sizeToAdd;
                sizes.push_back(sizeToAdd);

                prices.push_back(sign == 1 ? price.ask : price.bid);
                IFDEBUG(cout << "Cascade" << endl);
            }
        }
        else if (event > 0 && tP > 0.0)
        { // Possibility to decrease long position only at intrinsic events
            double pricE = (tP > 0.0 ? price.bid : price.ask);
            uint len = prices.size();
            int removed = 0;
            for (uint i = 1; i < len; ++i)
            {
                int idx = i - removed;
                double tempP = (tP > 0.0 ? log(pricE / prices.at(idx)) : log(prices.at(idx) / pricE));
                if (tempP >= (tP > 0.0 ? deltaUp : deltaDown))
                {
                    double addPnl = (pricE - prices.at(idx)) * sizes.at(idx);
                    if (addPnl < 0.0)
                    {
                        IFDEBUG(cout << "Descascade with a loss: " << addPnl << endl);
                    }
                    tempPnl += addPnl;
                    tP -= sizes.at(idx);
                    sizes.erase(sizes.begin() + idx);
                    prices.erase(prices.begin() + idx);
                    removed++;
                    increaseLong += -1.0;
                    IFDEBUG(cout << "Decascade" << endl);
                }
            }
        }
    }
    else if (longShort == -1)
    { // Short positions only
        if (event > 0)
        {
            if (tP == 0.0)
            { // Open short position
                int sign = -runner.type;
                if (abs(oppositeInv) > 15.0)
                {
                    size = 1.0;
                    if (abs(oppositeInv) > 30.0)
                    {
                        size = 1.0;
                    }
                }
                double sizeToAdd = sign * size;
                if (sizeToAdd > 0.0)
                {
                    cout << "How did this happen! increase position but pos size: " << sizeToAdd << endl;
                    sizeToAdd = -sizeToAdd;
                }
                tP += sizeToAdd;
                sizes.push_back(sizeToAdd);

                prices.push_back(sign == 1 ? price.bid : price.ask);
                IFDEBUG(cout << "Open short" << endl);
                assignCashTarget();
            }
            else if (tP < 0.0)
            {
                int sign = -runner.type;
                double sizeToAdd = sign * size * fraction * shrinkFshort;
                if (sizeToAdd > 0.0)
                {
                    cout << "How did this happen! increase position but pos size: " << sizeToAdd << endl;
                    sizeToAdd = -sizeToAdd;
                }

                tP += sizeToAdd;
                sizes.push_back(sizeToAdd);
                increaseShort += 1.0;
                prices.push_back(sign == 1 ? price.bid : price.ask);
                IFDEBUG(cout << "Cascade" << endl);
            }
        }
        else if (event < 0.0 && tP < 0.0)
        {
            double pricE = (tP > 0.0 ? price.bid : price.ask);
            uint len = prices.size();
            int removed = 0;
            for (uint i = 1; i < len; ++i)
            {
                int idx = i - removed;
                double tempP = (tP > 0.0 ? log(pricE / prices.at(idx)) : log(prices.at(idx) / pricE));
                if (tempP >= (tP > 0.0 ? deltaUp : deltaDown))
                {
                    double addPnl = (pricE - prices.at(idx)) * sizes.at(idx);
                    if (addPnl < 0.0)
                    {
                        IFDEBUG(cout << "Descascade with a loss: " << addPnl << endl);
                    }
                    tempPnl += (pricE - prices.at(idx)) * sizes.at(idx);
                    tP -= sizes.at(idx);
                    sizes.erase(sizes.begin() + idx);
                    prices.erase(prices.begin() + idx);
                    removed++;
                    increaseShort += -1.0;
                    IFDEBUG(cout << "Decascade" << endl);
                }
            }
        }
    }
    else
    {
        cout << "Should never happen! " << longShort << endl;
    }
    //some prints
    IFDEBUG(cout << "longShort: " << longShort << "; tP: " << tP << "; pnl: " << pnl << "; pnlPerc: " << pnlPerc << "; tempPnl: " << tempPnl << "; unrealized: " << computePnlLastPrice() << "; cashLimit: " << cashLimit << "; price: " << lastPrice << "; runner.type: " << runner.type << std::endl);
    return true;
}