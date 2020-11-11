// Copyright 2020 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "currencies.h"

beam::wallet::AtomicSwapCoin convertCurrencyToSwapCoin(Currency currency)
{
    switch (currency)
    {
    case Currency::CurrBitcoin:
        return beam::wallet::AtomicSwapCoin::Bitcoin;
    case Currency::CurrLitecoin:
        return beam::wallet::AtomicSwapCoin::Litecoin;
    case Currency::CurrQtum:
        return beam::wallet::AtomicSwapCoin::Qtum;
    case Currency::CurrBitcoinCash:
        return beam::wallet::AtomicSwapCoin::Bitcoin_Cash;
    case Currency::CurrDash:
        return beam::wallet::AtomicSwapCoin::Dash;
    case Currency::CurrDogecoin:
        return beam::wallet::AtomicSwapCoin::Dogecoin;
    default:
        return beam::wallet::AtomicSwapCoin::Unknown;
    }
}

Currency convertSwapCoinToCurrency(beam::wallet::AtomicSwapCoin swapCoin)
{
    switch (swapCoin)
    {
    case beam::wallet::AtomicSwapCoin::Bitcoin:
        return Currency::CurrBitcoin;
    case beam::wallet::AtomicSwapCoin::Litecoin:
        return Currency::CurrLitecoin;
    case beam::wallet::AtomicSwapCoin::Qtum:
        return Currency::CurrQtum;
    case beam::wallet::AtomicSwapCoin::Bitcoin_Cash:
        return Currency::CurrBitcoinCash;
    case beam::wallet::AtomicSwapCoin::Dash:
        return Currency::CurrDash;
    case beam::wallet::AtomicSwapCoin::Dogecoin:
        return Currency::CurrDogecoin;
    default:
        return Currency::CurrEnd;
    }
}