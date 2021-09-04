﻿// Copyright (c) 2017-2018 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_SERVICES_ASSET_H
#define SYSCOIN_SERVICES_ASSET_H
#include <amount.h>
class UniValue;
class CTransaction;
class TxValidationState;
class COutPoint;
class CAsset;

static const unsigned int MAX_GUID_LENGTH = 20;
static const unsigned int MAX_VALUE_LENGTH = 512;
static const unsigned int MAX_SYMBOL_SIZE = 12; // up to 9 characters
static const unsigned int MIN_SYMBOL_SIZE = 4;
static const unsigned int MAX_AUXFEES = 10;
// this should be set well after mempool expiry (DEFAULT_MEMPOOL_EXPIRY)
// so that miner will expire txs before they hit expiry errors if for some reason they aren't getting mined (geth issue etc)
static const int64_t MAINNET_MAX_MINT_AGE = 302400; // 0.5 week in seconds, should send to network in half a week or less
static const int64_t MAINNET_MAX_VALIDATE_AGE = 1512000; // 2.5 weeks in seconds
static const int64_t MAINNET_MIN_MINT_AGE = 3600; // 1 hr
static const uint32_t DOWNLOAD_ETHEREUM_TX_ROOTS = 60000; // roughly 1 week
static const uint32_t MAX_ETHEREUM_TX_ROOTS = DOWNLOAD_ETHEREUM_TX_ROOTS*4;
uint32_t GenerateSyscoinGuid(const COutPoint& outPoint);
std::string stringFromSyscoinTx(const int &nVersion);
std::string assetFromTx(const int &nVersion);
static CAsset emptyAsset;
bool GetAsset(const uint32_t &nBaseAsset,CAsset& txPos);
bool GetAssetPrecision(const uint32_t &nBaseAsset, uint8_t& nPrecision);
bool GetAssetNotaryKeyID(const uint32_t &nBaseAsset, std::vector<unsigned char>& keyID);
bool CheckTxInputsAssets(const CTransaction &tx, TxValidationState &state, const uint32_t &nBaseAsset, CAssetsMap mapAssetIn, const CAssetsMap &mapAssetOut);
UniValue AssetPublicDataToJson(const std::string &strPubData);
uint32_t GetBaseAssetID(const uint64_t &nAsset);
uint32_t GetNFTID(const uint64_t &nAsset);
uint64_t CreateAssetID(const uint32_t &NFTID, const uint32_t &nBaseAsset);
bool FillNotarySig(std::vector<CAssetOut> & voutAssets, const uint64_t& nBaseAsset, const std::vector<unsigned char> &vchSig);
#endif // SYSCOIN_SERVICES_ASSET_H
