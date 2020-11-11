// Copyright 2019 The Beam Team
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
#include "qml_globals.h"
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include "version.h"
#include "model/app_model.h"
#include "wallet/core/common.h"
#include "ui_helpers.h"
#include "wallet/client/extensions/offers_board/swap_offer_token.h"
#include "wallet/transactions/swaps/utils.h"
#include "utility/string_helpers.h"

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "3rdparty/libbitcoin/include/bitcoin/bitcoin/formats/base_10.hpp"

#include "fee_helpers.h"

using boost::multiprecision::cpp_dec_float_50;

namespace
{
    constexpr uint8_t kBTCDecimalPlaces = libbitcoin::btc_decimal_places;
    constexpr uint8_t kUSDDecimalPlaces = 2;

    template <char C>
    bool char_is(const char c)
    {
        return c == C;
    }

    QString roundWithPrecision(const QString& number, uint8_t precision)
    {
        //TODO rounding precision
        const char delimeter = '.';
        auto parts = string_helpers::split(number.toStdString(), delimeter);

        std::string result;
        std::ostringstream oss;
        if (parts.size() == 2)
        {    
            cpp_dec_float_50 afterPoint("0." + parts[1]);

            std::ostringstream afterPointOss;
            afterPointOss.precision(precision);
            afterPointOss << std::fixed << afterPoint;

            auto afterPointParts = string_helpers::split(afterPointOss.str(), delimeter);
            oss << parts[0] << delimeter << (afterPointParts.size() > 1 ? afterPointParts[1] : "0");
            result = oss.str();
            boost::algorithm::trim_right_if(result, char_is<'0'>);
            boost::algorithm::trim_right_if(result, char_is<'.'>);
        }
        else
        {
            oss << parts[0];
            result = oss.str();
        }

        return QString::fromStdString(result);
    }

    QString multiplyWithPrecision(const QString& first, const QString& second, uint8_t precision)
    {
        cpp_dec_float_50 dec_first(first.toStdString().c_str());
        cpp_dec_float_50 dec_second(second.toStdString().c_str());

        cpp_dec_float_50 product = dec_first * dec_second;

        std::ostringstream oss;
        oss.precision(std::numeric_limits<cpp_dec_float_50>::digits10);
        oss << std::fixed << product;

        QString result = QString::fromStdString(oss.str());
        return roundWithPrecision(result, precision);
    }

    beamui::Currencies convertUiCurrencyToCurrencies(WalletCurrency::Currency currency)
    {
        switch (currency)
        {
            case Currency::CurrBeam:
                return beamui::Currencies::Beam;

            case Currency::CurrBitcoin:
                return beamui::Currencies::Bitcoin;

            case Currency::CurrLitecoin:
                return beamui::Currencies::Litecoin;

            case Currency::CurrQtum:
                return beamui::Currencies::Qtum;

            case Currency::CurrBitcoinCash:
                return beamui::Currencies::BitcoinCash;

            case Currency::CurrDash:
                return beamui::Currencies::Dash;

            case Currency::CurrDogecoin:
                return beamui::Currencies::Dogecoin;

            default:
                return beamui::Currencies::Unknown;
        }
    }
}

QMLGlobals::QMLGlobals(QQmlEngine& engine)
    : _engine(engine)
{
}

void QMLGlobals::showMessage(const QString& message)
{
    QMessageBox::information(nullptr, "BeamWalletUI", message);
}

void QMLGlobals::copyToClipboard(const QString& text)
{
    QApplication::clipboard()->setText(text);
}


QString QMLGlobals::version()
{
    return QString::fromStdString(PROJECT_VERSION);
}

bool QMLGlobals::isTAValid(const QString& text)
{
    return QMLGlobals::isTransactionToken(text) || QMLGlobals::isAddress(text);
}

bool QMLGlobals::isAddress(const QString& text)
{
    return beam::wallet::CheckReceiverAddress(text.toStdString());
}

bool QMLGlobals::isTransactionToken(const QString& text)
{
    if (text.isEmpty()) return false;
    
    auto params = beam::wallet::ParseParameters(text.toStdString());
    return params && params->GetParameter<beam::wallet::TxType>(beam::wallet::TxParameterID::TransactionType);
}

bool QMLGlobals::isSwapToken(const QString& text)
{
    return beam::wallet::SwapOfferToken::isValid(text.toStdString());
}

QString QMLGlobals::getLocaleName()
{
    const auto& settings = AppModel::getInstance().getSettings();
    return settings.getLocale();
}

int QMLGlobals::maxCommentLength()
{
    return 1024;
}

bool QMLGlobals::needPasswordToSpend()
{
    return AppModel::getInstance().getSettings().isPasswordReqiredToSpendMoney();
}

bool QMLGlobals::isPasswordValid(const QString& value)
{
    beam::SecString secretPass = value.toStdString();
    return AppModel::getInstance().checkWalletPassword(secretPass);
}

QString QMLGlobals::calcTotalFee(Currency currency, unsigned int feeRate)
{
    return ::calcTotalFee(currency, feeRate);
}

QString QMLGlobals::calcFeeInSecondCurrency(int fee, const QString& exchangeRate, const QString& secondCurrencyUnitName)
{
    QString feeInOriginalCurrency = beamui::AmountToUIString(fee);
    return calcAmountInSecondCurrency(feeInOriginalCurrency, exchangeRate, secondCurrencyUnitName);
}

QString QMLGlobals::calcAmountInSecondCurrency(const QString& amount, const QString& exchangeRate, const QString& secondCurrLabel)
{
    if (exchangeRate.isEmpty() || exchangeRate == "0")
    {
        return "";
    }
    else
    {
#define MACRO(name, label, slabel, subunit, feeLabel, dec) \
        if (slabel == secondCurrLabel) \
        { \
            return multiplyWithPrecision(amount, exchangeRate, dec); \
        } 
        CURRENCY_MAP(MACRO)
#undef MACRO
    }
    return "";
}

QString QMLGlobals::roundUp(const QString& amount)
{
    const auto point = amount.indexOf('.');
    if (point == -1) return amount;

    const auto targetDecimals = amount.length() - point - 2;
    const auto scale = pow(cpp_dec_float_50(10), targetDecimals);

    const cpp_dec_float_50 original(amount.toStdString().c_str());
    const cpp_dec_float_50 rounded = ceil(original * scale)/scale;

    auto roundedStr = rounded.str(targetDecimals, std::ios_base::fixed);
    boost::algorithm::trim_right_if(roundedStr, char_is<'0'>);
    boost::algorithm::trim_right_if(roundedStr, char_is<'.'>);

    return QString(roundedStr.c_str());
}

bool QMLGlobals::canSwap()
{
    return haveSwapClient(Currency::CurrBitcoin) || haveSwapClient(Currency::CurrLitecoin) || haveSwapClient(Currency::CurrQtum)
        || haveSwapClient(Currency::CurrBitcoinCash) || haveSwapClient(Currency::CurrDash)
        || haveSwapClient(Currency::CurrDogecoin);
}

bool QMLGlobals::haveSwapClient(Currency currency)
{
    auto swapCoin = convertCurrencyToSwapCoin(currency);
    return AppModel::getInstance().getSwapCoinClient(swapCoin)->GetSettings().IsActivated();
}

QString QMLGlobals::rawTxParametrsToTokenStr(const QVariant& variantTxParams)
{
    if (!variantTxParams.isNull() && variantTxParams.isValid())
    {
        auto txParameters = variantTxParams.value<beam::wallet::TxParameters>();
        return QString::fromStdString(std::to_string(txParameters));
    }
    return "";
}

bool QMLGlobals::canReceive(Currency currency)
{
    if (Currency::CurrBeam == currency)
    {
        return true;
    }

    auto swapCoin = convertCurrencyToSwapCoin(currency);
    auto client = AppModel::getInstance().getSwapCoinClient(swapCoin);
    return client->GetSettings().IsActivated() && client->getStatus() == beam::bitcoin::Client::Status::Connected;
}

QString QMLGlobals::getBeamUnit() const
{
    return beamui::getCurrencyUnitName(beamui::Currencies::Beam);
}

QString QMLGlobals::getBeamFeeUnit() const
{
    return beamui::getFeeRateLabel(beamui::Currencies::Beam);
}

QString QMLGlobals::getCurrencyUnitName(Currency currency)
{
    beamui::Currencies currencyCommon = convertUiCurrencyToCurrencies(currency);
    return beamui::getCurrencyUnitName(currencyCommon);
}

QString QMLGlobals::getCurrencyName(Currency currency)
{
    switch(currency)
    {
    case Currency::CurrBeam:
    {
        //% "BEAM"
        return qtTrId("general-beam");
    }
    case Currency::CurrBitcoin:
    {
        //% "Bitcoin"
        return qtTrId("general-bitcoin");
    }
    case Currency::CurrLitecoin:
    {
        //% "Litecoin"
        return qtTrId("general-litecoin");
    }
    case Currency::CurrQtum:
    {
        //% "QTUM"
        return qtTrId("general-qtum");
    }
    case Currency::CurrDogecoin:
    {
        //% "Dogecoin"
        return qtTrId("general-dogecoin");
    }
    case Currency::CurrBitcoinCash:
    {
        //% "Bitcoin Cash"
        return qtTrId("general-bitcoin-cash");
    }
    case Currency::CurrDash:
    {
        //% "DASH"
        return qtTrId("general-dash");
    }
    default:
    {
        assert(false && "unexpected swap coin!");
        return QString();
    }
    }
}

QString QMLGlobals::getFeeRateLabel(Currency currency)
{
    beamui::Currencies currencyCommon = convertUiCurrencyToCurrencies(currency);
    return beamui::getFeeRateLabel(currencyCommon);
}

QString QMLGlobals::getCurrencySubunitFromLabel(const QString& currLabel)
{
    return beamui::getCurrencySubunitFromLabel(currLabel);
}

QString QMLGlobals::getMinimalFee(Currency currency, bool isShielded)
{
    return QString::fromStdString(std::to_string(minimalFee(currency, isShielded)));
}

QString QMLGlobals::getRecommendedFee(Currency currency)
{
    if (Currency::CurrBeam == currency)
    {
        // TODO roman.strilets need to investigate
        return QString::fromStdString(std::to_string(0));
    }

    auto swapCoin = convertCurrencyToSwapCoin(currency);
    return QString::fromStdString(std::to_string(AppModel::getInstance().getSwapCoinClient(swapCoin)->getEstimatedFeeRate()));
}

QString QMLGlobals::getDefaultFee(Currency currency)
{
    if (Currency::CurrBeam == currency)
    {
        return QString::fromStdString(std::to_string(minFeeBeam()));
    }

    auto swapCoin = convertCurrencyToSwapCoin(currency);
    return QString::fromStdString(std::to_string(AppModel::getInstance().getSwapCoinClient(swapCoin)->getEstimatedFeeRate()));
}

QString QMLGlobals::divideWithPrecision8(const QString& dividend, const QString& divider)
{
    cpp_dec_float_50 dec_dividend(dividend.toStdString().c_str());
    cpp_dec_float_50 dec_divider(divider.toStdString().c_str());

    cpp_dec_float_50 quotient = dec_dividend / dec_divider;

    std::ostringstream oss;
    oss.precision(std::numeric_limits<cpp_dec_float_50>::digits10);
    oss << std::fixed << quotient;

    QString result = QString::fromStdString(oss.str());
    return QMLGlobals::roundWithPrecision8(result);
}

QString QMLGlobals::multiplyWithPrecision8(const QString& first, const QString& second)
{
    return multiplyWithPrecision(first, second, kBTCDecimalPlaces);
}

QString QMLGlobals::roundWithPrecision8(const QString& number)
{
    return roundWithPrecision(number, kBTCDecimalPlaces);
}
