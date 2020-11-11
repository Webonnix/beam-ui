// Copyright 2018 The Beam Team
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

#include "settings_view.h"
#include "version.h"
#include <QtQuick>
#include <QApplication>
#include <QClipboard>
#include "model/app_model.h"
#include "model/helpers.h"
#include "model/swap_coin_client_model.h"
#include <thread>
#include "wallet/core/secstring.h"
#include "qml_globals.h"
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include "utility/string_helpers.h"
#include "mnemonic/mnemonic.h"
#include "viewmodel/ui_helpers.h"

using namespace beam;
using namespace ECC;
using namespace std;

namespace
{
    QString AddressToQstring(const io::Address& address) 
    {
        if (!address.empty())
        {
            return str2qstr(address.str());
        }
        return {};
    }

    QString formatAddress(const QString& address, const QString& port)
    {
        return QString("%1:%2").arg(address).arg(port);
    }

    QString formatPort(uint16_t port)
    {
        return port > 0 ? QString("%1").arg(port) : "";
    }

    struct UnpackedAddress 
    {
        QString address;
        QString port = 0;
    };

    UnpackedAddress parseAddress(const QString& address)
    {
        UnpackedAddress res;
        auto separator = address.indexOf(':');
        if (separator > -1)
        {
            res.address = address.left(separator);
            res.port = address.mid(separator + 1);
        }
        else
        {
            res.address = address;
        }
        return res;
    }

    const char ELECTRUM_PHRASES_SEPARATOR = ' ';
}


ElectrumPhraseItem::ElectrumPhraseItem(int index, const QString& phrase)
    : m_index(index)
    , m_phrase(phrase)
    , m_userInput(phrase)
{
}

bool ElectrumPhraseItem::isModified() const
{
    return m_userInput != m_phrase;
}

const QString& ElectrumPhraseItem::getValue() const
{
    return m_userInput;
}

void ElectrumPhraseItem::setValue(const QString& value)
{
    if (m_userInput != value)
    {
        m_userInput = value;
        emit valueChanged();
        emit isModifiedChanged();
        emit isAllowedChanged();
    }
}

const QString& ElectrumPhraseItem::getPhrase() const
{
    return m_phrase;
}

int ElectrumPhraseItem::getIndex() const
{
    return m_index;
}

bool ElectrumPhraseItem::isAllowed() const
{
    return bitcoin::isAllowedWord(m_userInput.toStdString());
}

void ElectrumPhraseItem::applyChanges()
{
    m_phrase = m_userInput;
}

void ElectrumPhraseItem::revertChanges()
{
    setValue(m_phrase);
}


SwapCoinSettingsItem::SwapCoinSettingsItem(wallet::AtomicSwapCoin swapCoin)
    : m_swapCoin(swapCoin)
    , m_coinClient(AppModel::getInstance().getSwapCoinClient(swapCoin))
{
    auto coinClient = m_coinClient.lock();
    connect(coinClient.get(), SIGNAL(statusChanged()), this, SLOT(onStatusChanged()));
    connect(coinClient.get(), SIGNAL(connectionErrorChanged()), this, SIGNAL(connectionErrorMsgChanged()));
    LoadSettings();
}

SwapCoinSettingsItem::~SwapCoinSettingsItem()
{
    qDeleteAll(m_seedPhraseItems);
}

QString SwapCoinSettingsItem::getFeeRateLabel() const
{
    return beamui::getFeeRateLabel(beamui::convertSwapCoinToCurrency(m_swapCoin));
}

QString SwapCoinSettingsItem::getTitle() const
{
    switch (m_settings->GetCurrentConnectionType())
    {
        case beam::bitcoin::ISettings::ConnectionType::None:
            return getGeneralTitle();
        case beam::bitcoin::ISettings::ConnectionType::Core:
            return getConnectedNodeTitle();
        case beam::bitcoin::ISettings::ConnectionType::Electrum:
            return getConnectedElectrumTitle();
        default:
        {
            assert(false && "unexpected connection type");
            return getGeneralTitle();
        }
    }
}

 QString SwapCoinSettingsItem::getCoinID() const
 {
     return beamui::getCurrencyUnitName(beamui::convertSwapCoinToCurrency(m_swapCoin));
 }

QString SwapCoinSettingsItem::getShowSeedDialogTitle() const
{
    switch (m_swapCoin)
    {
        case beam::wallet::AtomicSwapCoin::Bitcoin:
            //% "Bitcoin seed phrase"
            return qtTrId("bitcoin-show-seed-title");
        case beam::wallet::AtomicSwapCoin::Litecoin:
            //% "Litecoin seed phrase"
            return qtTrId("litecoin-show-seed-title");
        case beam::wallet::AtomicSwapCoin::Qtum:
            //% "Qtum seed phrase"
            return qtTrId("qtum-show-seed-title");
        case beam::wallet::AtomicSwapCoin::Dogecoin:
            //% "Dogecoin seed phrase"
            return qtTrId("dogecoin-show-seed-phrase");
        case beam::wallet::AtomicSwapCoin::Bitcoin_Cash:
            //% "Bitcoin Cash seed phrase"
            return qtTrId("bitcoin-cash-show-seed-phrase");
        case beam::wallet::AtomicSwapCoin::Dash:
            //% "Dash seed phrase"
            return qtTrId("dash-show-seed-phrase");
        default:
        {
            assert(false && "unexpected swap coin!");
            return QString();
        }
    }
}

QString SwapCoinSettingsItem::getShowAddressesDialogTitle() const
{
    switch (m_swapCoin)
    {
        case beam::wallet::AtomicSwapCoin::Bitcoin:
            //% "Bitcoin wallet addresses"
            return qtTrId("bitcoin-show-addresses-title");
        case beam::wallet::AtomicSwapCoin::Litecoin:
            //% "Litecoin wallet addresses"
            return qtTrId("litecoin-show-addresses-title");
        case beam::wallet::AtomicSwapCoin::Qtum:
            //% "Qtum wallet addresses"
            return qtTrId("qtum-show-addresses-title");
        case beam::wallet::AtomicSwapCoin::Dogecoin:
            //% "Dogecoin wallet addresses"
            return qtTrId("dogecoin-show-addresses-title");
        case beam::wallet::AtomicSwapCoin::Bitcoin_Cash:
            //% "Bitcoin Cash wallet addresses"
            return qtTrId("bitcoin-cash-show-addresses-title");
        case beam::wallet::AtomicSwapCoin::Dash:
            //% "Dash wallet addresses"
            return qtTrId("dash-show-addresses-title");
        default:
        {
            assert(false && "unexpected swap coin!");
            return QString();
        }
    }
}

QString SwapCoinSettingsItem::getGeneralTitle() const
{
    switch (m_swapCoin)
    {
        case wallet::AtomicSwapCoin::Bitcoin:
            //% "Bitcoin"
            return qtTrId("general-bitcoin");
        case wallet::AtomicSwapCoin::Litecoin:
            //% "Litecoin"
            return qtTrId("general-litecoin");
        case wallet::AtomicSwapCoin::Qtum:
            //% "QTUM"
            return qtTrId("general-qtum");
        case wallet::AtomicSwapCoin::Dogecoin:
            //% "Dogecoin"
            return qtTrId("general-dogecoin");
        case wallet::AtomicSwapCoin::Bitcoin_Cash:
            //% "Bitcoin Cash"
            return qtTrId("general-bitcoin-cash");
        case wallet::AtomicSwapCoin::Dash:
            //% "DASH"
            return qtTrId("general-dash");
        default:
        {
            assert(false && "unexpected swap coin!");
            return QString();
        }
    }
}

QString SwapCoinSettingsItem::getConnectedNodeTitle() const
{
    // TODO: check, is real need translations?
    switch (m_swapCoin)
    {
        case wallet::AtomicSwapCoin::Bitcoin:
            //% "Bitcoin node"
            return qtTrId("settings-swap-bitcoin-node");
        case wallet::AtomicSwapCoin::Litecoin:
            //% "Litecoin node"
            return qtTrId("settings-swap-litecoin-node");
        case wallet::AtomicSwapCoin::Qtum:
            //% "Qtum node"
            return qtTrId("settings-swap-qtum-node");
        case wallet::AtomicSwapCoin::Dogecoin:
            //% "Dogecoin node"
            return qtTrId("settings-swap-dogecoin-node");
        case wallet::AtomicSwapCoin::Bitcoin_Cash:
            //% "Bitcoin Cash node"
            return qtTrId("settings-swap-bitcoin-cash-node");
        case wallet::AtomicSwapCoin::Dash:
            //% "Dash node"
            return qtTrId("settings-swap-dash-node");
        default:
        {
            assert(false && "unexpected swap coin!");
            return QString();
        }
    }
}

QString SwapCoinSettingsItem::getConnectedElectrumTitle() const
{
    // TODO: check, is real need translations?
    switch (m_swapCoin)
    {
        case wallet::AtomicSwapCoin::Bitcoin:
            //% "Bitcoin electrum"
            return qtTrId("settings-swap-bitcoin-electrum");
        case wallet::AtomicSwapCoin::Litecoin:
            //% "Litecoin electrum"
            return qtTrId("settings-swap-litecoin-electrum");
        case wallet::AtomicSwapCoin::Qtum:
            //% "Qtum electrum"
            return qtTrId("settings-swap-qtum-electrum");
        case wallet::AtomicSwapCoin::Dogecoin:
            //% "Dogecoin electrum"
            return qtTrId("settings-swap-dogecoin-electrum");
        case wallet::AtomicSwapCoin::Bitcoin_Cash:
            //% "Bitcoin Cash electrum"
            return qtTrId("settings-swap-bitcoin-cash-electrum");
        case wallet::AtomicSwapCoin::Dash:
            //% "Dash electrum"
            return qtTrId("settings-swap-dash-electrum");
        default:
        {
            assert(false && "unexpected swap coin!");
            return QString();
        }
    }
}

bool SwapCoinSettingsItem::getFolded() const
{
    return m_isFolded;
}

void SwapCoinSettingsItem::setFolded(bool value)
{
    m_isFolded = value;
}

QString SwapCoinSettingsItem::getNodeUser() const
{
    return m_nodeUser;
}

void SwapCoinSettingsItem::setNodeUser(const QString& value)
{
    if (value != m_nodeUser)
    {
        m_nodeUser = value;
        emit nodeUserChanged();
    }
}

QString SwapCoinSettingsItem::getNodePass() const
{
    return m_nodePass;
}

void SwapCoinSettingsItem::setNodePass(const QString& value)
{
    if (value != m_nodePass)
    {
        m_nodePass = value;
        emit nodePassChanged();
    }
}

QString SwapCoinSettingsItem::getNodeAddress() const
{
    return m_nodeAddress;
}

void SwapCoinSettingsItem::setNodeAddress(const QString& value)
{
    const auto val = value == "0.0.0.0" ? "" : value;
    if (val != m_nodeAddress)
    {
        m_nodeAddress = val;
        emit nodeAddressChanged();
    }
}

QString SwapCoinSettingsItem::getNodePort() const
{
    return m_nodePort;
}

void SwapCoinSettingsItem::setNodePort(const QString& value)
{
    if (value != m_nodePort)
    {
        m_nodePort = value;
        emit nodePortChanged();
    }
}

QList<QObject*> SwapCoinSettingsItem::getElectrumSeedPhrases()
{
    return m_seedPhraseItems;
}

QChar SwapCoinSettingsItem::getPhrasesSeparatorElectrum() const
{
    return QChar(ELECTRUM_PHRASES_SEPARATOR);
}

bool SwapCoinSettingsItem::getIsCurrentSeedValid() const
{
    return m_isCurrentSeedValid;
}

bool SwapCoinSettingsItem::getIsCurrentSeedSegwit() const
{
    return m_isCurrentSeedSegwit;
}

QString SwapCoinSettingsItem::getNodeAddressElectrum() const
{
    return m_nodeAddressElectrum;
}

void SwapCoinSettingsItem::setNodeAddressElectrum(const QString& value)
{
    if (value != m_nodeAddressElectrum)
    {
        m_nodeAddressElectrum = value;
        emit nodeAddressElectrumChanged();
    }
}

QString SwapCoinSettingsItem::getNodePortElectrum() const
{
    return m_nodePortElectrum;
}

void SwapCoinSettingsItem::setNodePortElectrum(const QString& value)
{
    if (value != m_nodePortElectrum)
    {
        m_nodePortElectrum = value;
        emit nodePortElectrumChanged();
    }
}

bool SwapCoinSettingsItem::getSelectServerAutomatically() const
{
    return m_selectServerAutomatically;
}

void SwapCoinSettingsItem::setSelectServerAutomatically(bool value)
{
    if (value != m_selectServerAutomatically)
    {
        m_selectServerAutomatically = value;
        emit selectServerAutomaticallyChanged();
    }
}

bool SwapCoinSettingsItem::isSupportedElectrum() const
{
    return m_settings->IsSupportedElectrum();
}

QStringList SwapCoinSettingsItem::getAddressesElectrum() const
{
    auto electrumSettings = m_settings->GetElectrumConnectionOptions();

    if (electrumSettings.IsInitialized())
    {
        auto addresses = bitcoin::generateReceivingAddresses(electrumSettings.m_secretWords, 
            electrumSettings.m_receivingAddressAmount, m_settings->GetAddressVersion());

        QStringList result;
        result.reserve(static_cast<int>(addresses.size()));

        for (const auto& address : addresses)
        {
            result.push_back(QString::fromStdString(address));
        }
        return result;
    }
    return {};
}

void SwapCoinSettingsItem::onStatusChanged()
{
    emit connectionStatusChanged();

    if (m_selectServerAutomatically)
    {
        using beam::bitcoin::Client;

        switch (m_coinClient.lock()->getStatus())
        {
        case Client::Status::Connected:
        case Client::Status::Failed:
        case Client::Status::Unknown:
        {
            auto settings = m_coinClient.lock()->GetSettings();

            if (auto options = settings.GetElectrumConnectionOptions(); options.IsInitialized())
            {
                applyNodeAddressElectrum(str2qstr(options.m_address));
            }
            break;
        }
        default:
        {
            setNodeAddressElectrum("");
            setNodePortElectrum("");
        }
        }
    }
}

bool SwapCoinSettingsItem::getCanEdit() const
{
    return m_coinClient.lock()->canModifySettings();
}

bool SwapCoinSettingsItem::getIsConnected() const
{
    return m_connectionType != beam::bitcoin::ISettings::ConnectionType::None;
}

bool SwapCoinSettingsItem::getIsNodeConnection() const
{
    return m_connectionType == beam::bitcoin::ISettings::ConnectionType::Core;
}

bool SwapCoinSettingsItem::getIsElectrumConnection() const
{
    return m_connectionType == beam::bitcoin::ISettings::ConnectionType::Electrum;
}

QString SwapCoinSettingsItem::getConnectionStatus() const
{
    using beam::bitcoin::Client;

    switch (m_coinClient.lock()->getStatus())
    {
        case Client::Status::Uninitialized:
            return "uninitialized";
            
        case Client::Status::Initialized:
        case Client::Status::Connecting:
            return "disconnected";

        case Client::Status::Connected:
            return "connected";

        case Client::Status::Failed:
        case Client::Status::Unknown:
        default:
            return "error";
    }
}

QString SwapCoinSettingsItem::getConnectionErrorMsg() const
{
    using beam::bitcoin::IBridge;

    switch (m_coinClient.lock()->getConnectionError())
    {
        case IBridge::ErrorType::InvalidCredentials:
            //% "Cannot connect to node. Invalid credentials"
            return qtTrId("swap-invalid-credentials-error");

        case IBridge::ErrorType::IOError:
            //% "Cannot connect to node. Please check your network connection."
            return qtTrId("swap-connection-error");

        case IBridge::ErrorType::InvalidGenesisBlock:
            //% "Cannot connect to node. Invalid genesis block"
            return qtTrId("swap-invalid-genesis-block-error");

        default:
            return QString();
    }
}

void SwapCoinSettingsItem::applyNodeSettings()
{
    auto coinClient = m_coinClient.lock();
    bitcoin::BitcoinCoreSettings connectionSettings = coinClient->GetSettings().GetConnectionOptions();
    connectionSettings.m_pass = m_nodePass.toStdString();
    connectionSettings.m_userName = m_nodeUser.toStdString();

    if (!m_nodeAddress.isEmpty())
    {
        const std::string address = m_nodeAddress.toStdString();
        connectionSettings.m_address.resolve(address.c_str());
        connectionSettings.m_address.port(m_nodePort.toInt());
    }

    m_settings->SetConnectionOptions(connectionSettings);

    coinClient->SetSettings(*m_settings);
}

void SwapCoinSettingsItem::applyElectrumSettings()
{
    auto coinClient = m_coinClient.lock();
    bitcoin::ElectrumSettings electrumSettings = coinClient->GetSettings().GetElectrumConnectionOptions();
    
    if (!m_selectServerAutomatically && !m_nodeAddressElectrum.isEmpty())
    {
        electrumSettings.m_address = formatAddress(m_nodeAddressElectrum, m_nodePortElectrum).toStdString();
    }

    electrumSettings.m_automaticChooseAddress = m_selectServerAutomatically;
    electrumSettings.m_secretWords = GetSeedPhraseFromSeedItems();
    
    m_settings->SetElectrumConnectionOptions(electrumSettings);

    coinClient->SetSettings(*m_settings);
}

void SwapCoinSettingsItem::resetNodeSettings()
{
    SetDefaultNodeSettings();
    applyNodeSettings();
}

void SwapCoinSettingsItem::resetElectrumSettings()
{
    bool clearSeed = getCanEdit();
    SetDefaultElectrumSettings(clearSeed);
    applyElectrumSettings();
}

void SwapCoinSettingsItem::newElectrumSeed()
{
    auto secretWords = bitcoin::createElectrumMnemonic(getEntropy());    
    SetSeedElectrum(secretWords);
}

void SwapCoinSettingsItem::restoreSeedElectrum()
{
    SetSeedElectrum(m_settings->GetElectrumConnectionOptions().m_secretWords);
}

void SwapCoinSettingsItem::disconnect()
{
    auto connectionType = bitcoin::ISettings::ConnectionType::None;

    m_settings->ChangeConnectionType(connectionType);
    m_coinClient.lock()->SetSettings(*m_settings);
    setConnectionType(connectionType);
}

void SwapCoinSettingsItem::connectToNode()
{
    auto connectionType = bitcoin::ISettings::ConnectionType::Core;

    m_settings->ChangeConnectionType(connectionType);
    m_coinClient.lock()->SetSettings(*m_settings);
    setConnectionType(connectionType);
}

void SwapCoinSettingsItem::connectToElectrum()
{
    auto connectionType = bitcoin::ISettings::ConnectionType::Electrum;

    m_settings->ChangeConnectionType(connectionType);
    m_coinClient.lock()->SetSettings(*m_settings);
    setConnectionType(connectionType);
}

void SwapCoinSettingsItem::copySeedElectrum()
{
    auto seedElectrum = GetSeedPhraseFromSeedItems();
    auto seedString = vec2str(seedElectrum, ELECTRUM_PHRASES_SEPARATOR);
    QMLGlobals::copyToClipboard(QString::fromStdString(seedString));
}

void SwapCoinSettingsItem::validateCurrentElectrumSeedPhrase()
{
    std::vector<std::string> seedElectrum;
    seedElectrum.reserve(WORD_COUNT);

    // extract seed phrase from user input
    for (const auto phraseItem : m_seedPhraseItems)
    {
        auto word = static_cast<ElectrumPhraseItem*>(phraseItem)->getValue().toStdString();
        seedElectrum.push_back(word);
    }

    setIsCurrentSeedValid(bitcoin::validateElectrumMnemonic(seedElectrum));
    setIsCurrentSeedSegwit(bitcoin::validateElectrumMnemonic(seedElectrum, true));
}

void SwapCoinSettingsItem::LoadSettings()
{
    SetDefaultElectrumSettings();
    SetDefaultNodeSettings();

    m_settings = m_coinClient.lock()->GetSettings();

    setConnectionType(m_settings->GetCurrentConnectionType());

    if (auto options = m_settings->GetConnectionOptions(); options.IsInitialized())
    {
        setNodeUser(str2qstr(options.m_userName));
        setNodePass(str2qstr(options.m_pass));
        applyNodeAddress(AddressToQstring(options.m_address));
    }

    if (auto options = m_settings->GetElectrumConnectionOptions(); options.IsInitialized())
    {
        SetSeedElectrum(options.m_secretWords);
        setSelectServerAutomatically(options.m_automaticChooseAddress);

        if (m_settings->IsElectrumActivated() || !options.m_automaticChooseAddress)
        {
            applyNodeAddressElectrum(str2qstr(options.m_address));
        }
    }
}

void SwapCoinSettingsItem::SetSeedElectrum(const std::vector<std::string>& seedElectrum)
{
    if (!m_seedPhraseItems.empty())
    {
        qDeleteAll(m_seedPhraseItems);
        m_seedPhraseItems.clear();
    }

    m_seedPhraseItems.reserve(static_cast<int>(WORD_COUNT));

    if (seedElectrum.empty())
    {
        for (int index = 0; index < static_cast<int>(WORD_COUNT); ++index)
        {
            m_seedPhraseItems.push_back(new ElectrumPhraseItem(index, QString()));
        }
    }
    else
    {
        assert(seedElectrum.size() == WORD_COUNT);
        int index = 0;
        for (auto& word : seedElectrum)
        {
            m_seedPhraseItems.push_back(new ElectrumPhraseItem(index++, QString::fromStdString(word)));
        }
    }

    setIsCurrentSeedValid(bitcoin::validateElectrumMnemonic(seedElectrum));
    setIsCurrentSeedSegwit(bitcoin::validateElectrumMnemonic(seedElectrum, true));
    emit electrumSeedPhrasesChanged();
}

void SwapCoinSettingsItem::SetDefaultNodeSettings()
{
    setNodePort(0);
    setNodeAddress("");
    setNodePass("");
    setNodeUser("");
}

void SwapCoinSettingsItem::SetDefaultElectrumSettings(bool clearSeed)
{
    setNodeAddressElectrum("");
    setNodePortElectrum("");
    setSelectServerAutomatically(true);

    if (clearSeed)
    {
        SetSeedElectrum({});
    }
}

void SwapCoinSettingsItem::setConnectionType(beam::bitcoin::ISettings::ConnectionType type)
{
    if (type != m_connectionType)
    {
        m_connectionType = type;
        emit connectionTypeChanged();
    }
}

void SwapCoinSettingsItem::setIsCurrentSeedValid(bool value)
{
    if (m_isCurrentSeedValid != value)
    {
        m_isCurrentSeedValid = value;
        emit isCurrentSeedValidChanged();
    }
}

void SwapCoinSettingsItem::setIsCurrentSeedSegwit(bool value)
{
    if (m_isCurrentSeedSegwit != value)
    {
        m_isCurrentSeedSegwit = value;
        emit isCurrentSeedSegwitChanged();
    }
}

std::vector<std::string> SwapCoinSettingsItem::GetSeedPhraseFromSeedItems() const
{
    assert(static_cast<size_t>(m_seedPhraseItems.size()) == WORD_COUNT);

    std::vector<std::string> seedElectrum;

    for (const auto phraseItem : m_seedPhraseItems)
    {
        auto item = static_cast<ElectrumPhraseItem*>(phraseItem);
        auto word = item->getPhrase().toStdString();

        // TODO need to wath this code. fixed ui bug #58
        // secret word can not empty
        if (!word.empty())
            seedElectrum.push_back(word);
    }

    return seedElectrum;
}

void SwapCoinSettingsItem::applyNodeAddress(const QString& address)
{
    auto unpackedAddress = parseAddress(address);
    setNodeAddress(unpackedAddress.address);
    if (unpackedAddress.port > 0)
    {
        setNodePort(unpackedAddress.port);
    }
}

void SwapCoinSettingsItem::applyNodeAddressElectrum(const QString& address)
{
    auto unpackedAddress = parseAddress(address);
    setNodeAddressElectrum(unpackedAddress.address);
    if (unpackedAddress.port > 0)
    {
        setNodePortElectrum(unpackedAddress.port);
    }
}

SettingsViewModel::SettingsViewModel()
    : m_settings{AppModel::getInstance().getSettings()}
    , m_notificationsSettings(AppModel::getInstance().getSettings())
    , m_isValidNodeAddress{true}
    , m_isNeedToCheckAddress(false)
    , m_isNeedToApplyChanges(false)
    , m_supportedLanguages(WalletSettings::getSupportedLanguages())
    , m_supportedAmountUnits(WalletSettings::getSupportedRateUnits())
{
    undoChanges();

    m_lockTimeout = m_settings.getLockTimeout();
    m_isPasswordReqiredToSpendMoney = m_settings.isPasswordReqiredToSpendMoney();
    m_isAllowedBeamMWLinks = m_settings.isAllowedBeamMWLinks();
    m_currentLanguageIndex = m_supportedLanguages.indexOf(m_settings.getLanguageName());
    m_secondCurrency = m_settings.getSecondCurrency();

    connect(&AppModel::getInstance().getNode(), SIGNAL(startedNode()), SLOT(onNodeStarted()));
    connect(&AppModel::getInstance().getNode(), SIGNAL(stoppedNode()), SLOT(onNodeStopped()));
    connect(AppModel::getInstance().getWalletModel().get(), SIGNAL(addressChecked(const QString&, bool)), SLOT(onAddressChecked(const QString&, bool)));
    connect(AppModel::getInstance().getWalletModel().get(), SIGNAL(publicAddressChanged(const QString&)), SLOT(onPublicAddressChanged(const QString&)));
    connect(&m_settings, SIGNAL(beamMWLinksChanged()), SIGNAL(beamMWLinksPermissionChanged()));

    m_timerId = startTimer(CHECK_INTERVAL);
}

SettingsViewModel::~SettingsViewModel()
{
    qDeleteAll(m_swapSettings);
}

void SettingsViewModel::onNodeStarted()
{
    emit localNodeRunningChanged();
}

void SettingsViewModel::onNodeStopped()
{
    emit localNodeRunningChanged();
}

void SettingsViewModel::onAddressChecked(const QString& addr, bool isValid)
{
    if (m_nodeAddress == addr && m_isValidNodeAddress != isValid)
    {
        m_isValidNodeAddress = isValid;
        emit validNodeAddressChanged();

        if (m_isNeedToApplyChanges)
        {
            if (m_isValidNodeAddress)
                applyChanges();

            m_isNeedToApplyChanges = false;
        }
    }
}

void SettingsViewModel::onPublicAddressChanged(const QString& publicAddr)
{
    if (m_publicAddress != publicAddr)
    {
        m_publicAddress = publicAddr;
        emit publicAddressChanged();
    }
}

bool SettingsViewModel::isLocalNodeRunning() const
{
    return AppModel::getInstance().getNode().isNodeRunning();
}

bool SettingsViewModel::isValidNodeAddress() const
{
    return m_isValidNodeAddress;
}

QString SettingsViewModel::getNodeAddress() const
{
    return m_nodeAddress;
}

void SettingsViewModel::setNodeAddress(const QString& value)
{
    if (value != m_nodeAddress)
    {
        m_nodeAddress = value;

        if (!m_isNeedToCheckAddress)
        {
            m_isNeedToCheckAddress = true;
            m_timerId = startTimer(CHECK_INTERVAL);
        }

        emit nodeAddressChanged();
        emit nodeSettingsChanged();
    }
}

QString SettingsViewModel::getVersion() const
{
    return QString::fromStdString(PROJECT_VERSION);
}

bool SettingsViewModel::getLocalNodeRun() const
{
    return m_localNodeRun;
}

void SettingsViewModel::setLocalNodeRun(bool value)
{
    if (value != m_localNodeRun)
    {
        m_localNodeRun = value;

        if (!m_localNodeRun && !m_isNeedToCheckAddress)
        {
            m_isNeedToCheckAddress = true;
            m_timerId = startTimer(CHECK_INTERVAL);
        }

        emit localNodeRunChanged();
        emit nodeSettingsChanged();
    }
}

QString SettingsViewModel::getLocalNodePort() const
{
    return m_localNodePort;
}

void SettingsViewModel::setLocalNodePort(const QString& value)
{
    if (value != m_localNodePort)
    {
        m_localNodePort = value;
        emit localNodePortChanged();
        emit nodeSettingsChanged();
    }
}

QString SettingsViewModel::getRemoteNodePort() const
{
    return m_remoteNodePort;
}

void SettingsViewModel::setRemoteNodePort(const QString& value)
{
    if (value != m_remoteNodePort)
    {
        m_remoteNodePort = value;
        emit remoteNodePortChanged();
        emit nodeSettingsChanged();
    }
}

int SettingsViewModel::getLockTimeout() const
{
    return m_lockTimeout;
}

void SettingsViewModel::setLockTimeout(int value)
{
    if (value != m_lockTimeout)
    {
        m_lockTimeout = value;
        m_settings.setLockTimeout(m_lockTimeout);
        emit lockTimeoutChanged();
    }
}

bool SettingsViewModel::isPasswordReqiredToSpendMoney() const
{
    return m_isPasswordReqiredToSpendMoney;
}

void SettingsViewModel::setPasswordReqiredToSpendMoney(bool value)
{
    if (value != m_isPasswordReqiredToSpendMoney)
    {
        m_isPasswordReqiredToSpendMoney = value;
        m_settings.setPasswordReqiredToSpendMoney(m_isPasswordReqiredToSpendMoney);
        emit passwordReqiredToSpendMoneyChanged();
    }
}

bool SettingsViewModel::isAllowedBeamMWLinks()
{
    m_isAllowedBeamMWLinks = m_settings.isAllowedBeamMWLinks();
    return m_isAllowedBeamMWLinks;
}

void SettingsViewModel::allowBeamMWLinks(bool value)
{
    if (value != m_isAllowedBeamMWLinks)
    {
        m_settings.setAllowedBeamMWLinks(value);
    }
}

QStringList SettingsViewModel::getSupportedLanguages() const
{
    return m_supportedLanguages;
}

int SettingsViewModel::getCurrentLanguageIndex() const
{
    return m_currentLanguageIndex;
}

void SettingsViewModel::setCurrentLanguageIndex(int value)
{
    m_currentLanguageIndex = value;
    m_settings.setLocaleByLanguageName(
            m_supportedLanguages[m_currentLanguageIndex]);
    emit currentLanguageIndexChanged();
}

QString SettingsViewModel::getCurrentLanguage() const
{
    return m_supportedLanguages[m_currentLanguageIndex];
}

void SettingsViewModel::setCurrentLanguage(QString value)
{
    auto index = m_supportedLanguages.indexOf(value);
    if (index != -1 )
    {
        setCurrentLanguageIndex(index);
    }
}

QString SettingsViewModel::getSecondCurrency() const
{
    return m_secondCurrency;
}

void SettingsViewModel::setSecondCurrency(const QString& value)
{
    m_secondCurrency = value;
    m_settings.setSecondCurrency(value);
    emit secondCurrencyChanged();
}

const QString& SettingsViewModel::getPublicAddress() const
{
    if (m_publicAddress.isEmpty())
    {
        AppModel::getInstance().getWalletModel()->getAsync()->getPublicAddress();
    }
    return m_publicAddress;
}

uint SettingsViewModel::coreAmount() const
{
    return std::thread::hardware_concurrency();
}

bool SettingsViewModel::hasPeer(const QString& peer) const
{
    return m_localNodePeers.contains(peer, Qt::CaseInsensitive);
}

void SettingsViewModel::addLocalNodePeer(const QString& localNodePeer)
{
    m_localNodePeers.push_back(localNodePeer);
    emit localNodePeersChanged();
    emit nodeSettingsChanged();
}

void SettingsViewModel::deleteLocalNodePeer(int index)
{
    m_localNodePeers.removeAt(index);
    emit localNodePeersChanged();
    emit nodeSettingsChanged();
}

void SettingsViewModel::openUrl(const QString& url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void SettingsViewModel::refreshWallet()
{
    AppModel::getInstance().getWalletModel()->getAsync()->rescan();
}

void SettingsViewModel::openFolder(const QString& path)
{
    WalletSettings::openFolder(path);
}

bool SettingsViewModel::checkWalletPassword(const QString& oldPass) const
{
    SecString secretPass = oldPass.toStdString();
    return AppModel::getInstance().checkWalletPassword(secretPass);
}

QString SettingsViewModel::getOwnerKey(const QString& password) const
{
    SecString secretPass = password.toStdString();
    const auto& ownerKey = AppModel::getInstance().getWalletModel()->exportOwnerKey(secretPass);
    return QString::fromStdString(ownerKey);
}

bool SettingsViewModel::isNodeChanged() const
{
    return formatAddress(m_nodeAddress, m_remoteNodePort) != m_settings.getNodeAddress()
        || m_localNodeRun != m_settings.getRunLocalNode()
        || static_cast<uint>(m_localNodePort.toInt()) != m_settings.getLocalNodePort()
        || m_localNodePeers != m_settings.getLocalNodePeers();
}

void SettingsViewModel::applyChanges()
{
    if (!m_localNodeRun && m_isNeedToCheckAddress)
    {
        m_isNeedToApplyChanges = true;
        return;
    }

    m_settings.setNodeAddress(formatAddress(m_nodeAddress, m_remoteNodePort));
    m_settings.setRunLocalNode(m_localNodeRun);
    m_settings.setLocalNodePort(m_localNodePort.toInt());
    m_settings.setLocalNodePeers(m_localNodePeers);
    m_settings.applyChanges();
    emit nodeSettingsChanged();
}

QStringList SettingsViewModel::getLocalNodePeers() const
{
    return m_localNodePeers;
}

void SettingsViewModel::setLocalNodePeers(const QStringList& localNodePeers)
{
    m_localNodePeers = localNodePeers;
    emit localNodePeersChanged();
    emit nodeSettingsChanged();
}

QString SettingsViewModel::getWalletLocation() const
{
    return QString::fromStdString(m_settings.getAppDataPath());
}

void SettingsViewModel::undoChanges()
{
    auto unpackedAddress = parseAddress(m_settings.getNodeAddress());
    setNodeAddress(unpackedAddress.address);
    if (unpackedAddress.port > 0)
    {
        setRemoteNodePort(unpackedAddress.port);
    }

    setLocalNodeRun(m_settings.getRunLocalNode());
    setLocalNodePort(formatPort(m_settings.getLocalNodePort()));
    setLocalNodePeers(m_settings.getLocalNodePeers());
}

void SettingsViewModel::reportProblem()
{
    m_settings.reportProblem();
}

bool SettingsViewModel::exportData() const
{
    return AppModel::getInstance().exportData();
}

bool SettingsViewModel::importData() const
{
    return AppModel::getInstance().importData();
}

void SettingsViewModel::changeWalletPassword(const QString& pass)
{
    AppModel::getInstance().changeWalletPassword(pass.toStdString());
}

void SettingsViewModel::timerEvent(QTimerEvent *event)
{
    if (m_isNeedToCheckAddress && !m_localNodeRun)
    {
        m_isNeedToCheckAddress = false;
        AppModel::getInstance().getWalletModel()->getAsync()->checkAddress(m_nodeAddress.toStdString());
        killTimer(m_timerId);
    }
}

const QList<QObject*>& SettingsViewModel::getSwapCoinSettings()
{
    if (m_swapSettings.empty())
    {
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Bitcoin));
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Litecoin));
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Qtum));
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Bitcoin_Cash));
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Dogecoin));
        m_swapSettings.push_back(new SwapCoinSettingsItem(beam::wallet::AtomicSwapCoin::Dash));
    }
    return m_swapSettings;
}

QObject* SettingsViewModel::getNotificationsSettings()
{
    return &m_notificationsSettings;
}
