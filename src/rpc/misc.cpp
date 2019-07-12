// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <clientversion.h>
#include <core_io.h>
#include <crypto/ripemd160.h>
#include <key_io.h>
#include <validation.h>
#include <net.h>
#include <netbase.h>
#include <outputtype.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <timedata.h>
#include <txmempool.h>
#include <util.h>
#include <utilstrencodings.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#endif
#include <warnings.h>

// Dash
#include <masternode-sync.h>
#include <spork.h>
//

#include <stdint.h>

#include <univalue.h>

// Dash
UniValue mnsync(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "mnsync [status|next|reset]\n"
            "Returns the sync status, updates to the next step or resets it entirely.\n"
        );

    std::string strMode = request.params[0].get_str();

    if(strMode == "status") {
        UniValue objStatus(UniValue::VOBJ);
        objStatus.push_back(Pair("AssetID", masternodeSync.GetAssetID()));
        objStatus.push_back(Pair("AssetName", masternodeSync.GetAssetName()));
        objStatus.push_back(Pair("AssetStartTime", masternodeSync.GetAssetStartTime()));
        objStatus.push_back(Pair("Attempt", masternodeSync.GetAttempt()));
        objStatus.push_back(Pair("IsBlockchainSynced", masternodeSync.IsBlockchainSynced()));
        objStatus.push_back(Pair("IsMasternodeListSynced", masternodeSync.IsMasternodeListSynced()));
        objStatus.push_back(Pair("IsWinnersListSynced", masternodeSync.IsWinnersListSynced()));
        objStatus.push_back(Pair("IsSynced", masternodeSync.IsSynced()));
        objStatus.push_back(Pair("IsFailed", masternodeSync.IsFailed()));
        return objStatus;
    }

    if(strMode == "next")
    {
        masternodeSync.SwitchToNextAsset(*g_connman);
        return "sync updated to " + masternodeSync.GetAssetName();
    }

    if(strMode == "reset")
    {
        masternodeSync.Reset();
        masternodeSync.SwitchToNextAsset(*g_connman);
        return "success";
    }
    return "failure";
}

/*
    Used for updating/reading spork settings on the network
*/
UniValue spork(const JSONRPCRequest& request)
{
    if(request.params.size() == 1 && request.params[0].get_str() == "show"){
        UniValue ret(UniValue::VOBJ);
        for(int nSporkID = SPORK_START; nSporkID <= SPORK_END; nSporkID++){
            if(sporkManager.GetSporkNameByID(nSporkID) != "Unknown")
                ret.push_back(Pair(sporkManager.GetSporkNameByID(nSporkID), sporkManager.GetSporkValue(nSporkID)));
        }
        return ret;
    } else if(request.params.size() == 1 && request.params[0].get_str() == "active"){
        UniValue ret(UniValue::VOBJ);
        for(int nSporkID = SPORK_START; nSporkID <= SPORK_END; nSporkID++){
            if(sporkManager.GetSporkNameByID(nSporkID) != "Unknown")
                ret.push_back(Pair(sporkManager.GetSporkNameByID(nSporkID), sporkManager.IsSporkActive(nSporkID)));
        }
        return ret;
    }
#ifdef ENABLE_WALLET
    else if (request.params.size() == 2){
        RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VNUM});

        int nSporkID = sporkManager.GetSporkIDByName(request.params[0].get_str());
        if(nSporkID == -1){
            return "Invalid spork name";
        }

        if (!g_connman)
            throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

        // SPORK VALUE
        int64_t nValue = request.params[1].get_int64();

        //broadcast new spork
        if(sporkManager.UpdateSpork(nSporkID, nValue, *g_connman)){
            sporkManager.ExecuteSpork(nSporkID, nValue);
            return "success";
        } else {
            return "failure";
        }

    }

    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
    throw std::runtime_error(
        "spork <name> [<value>]\n"
        "<name> is the corresponding spork name, or 'show' to show all current spork settings, active to show which sporks are active\n"
        "<value> is a epoch datetime to enable or disable spork\n"
        + HelpRequiringPassphrase(pwallet));
#else // ENABLE_WALLET
    throw std::runtime_error(
        "spork <name>\n"
        "<name> is the corresponding spork name, or 'show' to show all current spork settings, active to show which sporks are active\n");
#endif // ENABLE_WALLET

}
//

static UniValue validateaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "validateaddress \"address\"\n"
            "\nReturn information about the given sin address.\n"
            "DEPRECATION WARNING: Parts of this command have been deprecated and moved to getaddressinfo. Clients must\n"
            "transition to using getaddressinfo to access this information before upgrading to v0.18. The following deprecated\n"
            "fields have moved to getaddressinfo and will only be shown here with -deprecatedrpc=validateaddress: ismine, iswatchonly,\n"
            "script, hex, pubkeys, sigsrequired, pubkey, addresses, embedded, iscompressed, account, timestamp, hdkeypath, kdmasterkeyid.\n"
            "\nArguments:\n"
            "1. \"address\"                    (string, required) The sin address to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,       (boolean) If the address is valid or not. If not, this is the only property returned.\n"
            "  \"address\" : \"address\",        (string) The sin address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"isscript\" : true|false,      (boolean) If the key is a script\n"
            "  \"iswitness\" : true|false,     (boolean) If the address is a witness address\n"
            "  \"witness_version\" : version   (numeric, optional) The version number of the witness program\n"
            "  \"witness_program\" : \"hex\"     (string, optional) The hex value of the witness program\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("validateaddress", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("isvalid", isValid);
    if (isValid)
    {

#ifdef ENABLE_WALLET
        if (HasWallets() && IsDeprecatedRPCEnabled("validateaddress")) {
            ret.pushKVs(getaddressinfo(request));
        }
#endif
        if (ret["address"].isNull()) {
            std::string currentAddress = EncodeDestination(dest);
            ret.pushKV("address", currentAddress);

            CScript scriptPubKey = GetScriptForDestination(dest);
            ret.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));

            UniValue detail = DescribeAddress(dest);
            ret.pushKVs(detail);
        }
    }
    return ret;
}

static UniValue createmultisig(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
    {
        std::string msg = "createmultisig nrequired [\"key\",...] ( \"address_type\" )\n"
            "\nCreates a multi-signature address with n signature of m keys required.\n"
            "It returns a json object with the address and redeemScript.\n"
            "\nArguments:\n"
            "1. nrequired                    (numeric, required) The number of required signatures out of the n keys.\n"
            "2. \"keys\"                       (string, required) A json array of hex-encoded public keys\n"
            "     [\n"
            "       \"key\"                    (string) The hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"
            "3. \"address_type\"               (string, optional) The address type to use. Options are \"legacy\", \"p2sh-segwit\", and \"bech32\". Default is legacy.\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",  (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"       (string) The string value of the hex-encoded redemption script.\n"
            "}\n"

            "\nExamples:\n"
            "\nCreate a multisig address from 2 public keys\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"")
        ;
        throw std::runtime_error(msg);
    }

    int required = request.params[0].get_int();

    // Get the public keys
    const UniValue& keys = request.params[1].get_array();
    std::vector<CPubKey> pubkeys;
    for (unsigned int i = 0; i < keys.size(); ++i) {
        if (IsHex(keys[i].get_str()) && (keys[i].get_str().length() == 66 || keys[i].get_str().length() == 130)) {
            pubkeys.push_back(HexToPubKey(keys[i].get_str()));
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Invalid public key: %s\nNote that from v0.16, createmultisig no longer accepts addresses."
            " Users must use addmultisigaddress to create multisig addresses with addresses known to the wallet.", keys[i].get_str()));
        }
    }

    // Get the output type
    OutputType output_type = OutputType::LEGACY;
    if (!request.params[2].isNull()) {
        if (!ParseOutputType(request.params[2].get_str(), output_type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Unknown address type '%s'", request.params[2].get_str()));
        }
    }

    // Construct using pay-to-script-hash:
    const CScript inner = CreateMultisigRedeemscript(required, pubkeys);
    CBasicKeyStore keystore;
    const CTxDestination dest = AddAndGetDestinationForScript(keystore, inner, output_type);

    UniValue result(UniValue::VOBJ);
    result.pushKV("address", EncodeDestination(dest));
    result.pushKV("redeemScript", HexStr(inner.begin(), inner.end()));

    return result;
}

static UniValue verifymessage(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "verifymessage \"address\" \"signature\" \"message\"\n"
            "\nVerify a signed message\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The sin address to use for the signature.\n"
            "2. \"signature\"       (string, required) The signature provided by the signer in base 64 encoding (see signmessage).\n"
            "3. \"message\"         (string, required) The message that was signed.\n"
            "\nResult:\n"
            "true|false   (boolean) If the signature is verified or not.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    std::string strAddress  = request.params[0].get_str();
    std::string strSign     = request.params[1].get_str();
    std::string strMessage  = request.params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

static UniValue signmessagewithprivkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "signmessagewithprivkey \"privkey\" \"message\"\n"
            "\nSign a message with the private key of an address\n"
            "\nArguments:\n"
            "1. \"privkey\"         (string, required) The private key to sign the message with.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nCreate the signature\n"
            + HelpExampleCli("signmessagewithprivkey", "\"privkey\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        );

    std::string strPrivkey = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CKey key = DecodeSecret(strPrivkey);
    if (!key.IsValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

static UniValue setmocktime(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "setmocktime timestamp\n"
            "\nSet the local time to given timestamp (-regtest only)\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix seconds-since-epoch timestamp\n"
            "   Pass 0 to go back to using the system time."
        );

    if (!Params().MineBlocksOnDemand())
        throw std::runtime_error("setmocktime for regression testing (-regtest mode) only");

    // For now, don't change mocktime if we're in the middle of validation, as
    // this could have an effect on mempool time-based eviction, as well as
    // IsCurrentForFeeEstimation() and IsInitialBlockDownload().
    // TODO: figure out the right way to synchronize around mocktime, and
    // ensure all call sites of GetTime() are accessing this safely.
    LOCK(cs_main);

    RPCTypeCheck(request.params, {UniValue::VNUM});
    SetMockTime(request.params[0].get_int64());

    return NullUniValue;
}

static UniValue RPCLockedMemoryInfo()
{
    LockedPool::Stats stats = LockedPoolManager::Instance().stats();
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("used", uint64_t(stats.used));
    obj.pushKV("free", uint64_t(stats.free));
    obj.pushKV("total", uint64_t(stats.total));
    obj.pushKV("locked", uint64_t(stats.locked));
    obj.pushKV("chunks_used", uint64_t(stats.chunks_used));
    obj.pushKV("chunks_free", uint64_t(stats.chunks_free));
    return obj;
}

static UniValue getmemoryinfo(const JSONRPCRequest& request)
{
    /* Please, avoid using the word "pool" here in the RPC interface or help,
     * as users will undoubtedly confuse it with the other "memory pool"
     */
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getmemoryinfo\n"
            "Returns an object containing information about memory usage.\n"
            "\nResult:\n"
            "{\n"
            "  \"locked\": {               (json object) Information about locked memory manager\n"
            "    \"used\": xxxxx,          (numeric) Number of bytes used\n"
            "    \"free\": xxxxx,          (numeric) Number of bytes available in current arenas\n"
            "    \"total\": xxxxxxx,       (numeric) Total number of bytes managed\n"
            "    \"locked\": xxxxxx,       (numeric) Amount of bytes that succeeded locking. If this number is smaller than total, locking pages failed at some point and key data could be swapped to disk.\n"
            "    \"chunks_used\": xxxxx,   (numeric) Number allocated chunks\n"
            "    \"chunks_free\": xxxxx,   (numeric) Number unused chunks\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmemoryinfo", "")
            + HelpExampleRpc("getmemoryinfo", "")
        );

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("locked", RPCLockedMemoryInfo()));
    return obj;
}

static void EnableOrDisableLogCategories(UniValue cats, bool enable) {
    cats = cats.get_array();
    for (unsigned int i = 0; i < cats.size(); ++i) {
        std::string cat = cats[i].get_str();

        bool success;
        if (enable) {
            success = g_logger->EnableCategory(cat);
        } else {
            success = g_logger->DisableCategory(cat);
        }

        if (!success) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown logging category " + cat);
        }
    }
}

UniValue echo(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
                            "echo|echojson \"message\" ...\n"
                            "\nSimply echo back the input arguments. This command is for testing.\n"
                            "\nThe difference between echo and echojson is that echojson has argument conversion enabled in the client-side table in"
                            "bitcoin-cli and the GUI. There is no server-side difference."
                            );
    
    return request.params;
}

bool getAddressFromIndex(const int &type, const uint160 &hash, std::string &address)
{
    if (type == 2) {
        address = EncodeDestination(CScriptID(hash));
    } else if (type == 1) {
        address = EncodeDestination(CKeyID(hash));
    } else {
        return false;
    }
    return true;
}

bool getAddressesFromParams(const UniValue& params, std::vector<std::pair<uint160, int> > &addresses)
{
    if (params[0].isStr()) {
        uint160 hashBytes;
        int type = 0;
        if (!GetIndexKey(params[0].get_str(), hashBytes, type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("Invalid address %d",type));
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    } else if (params[0].isObject()) {
        
        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }
        
        std::vector<UniValue> values = addressValues.getValues();
        
        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {
            uint160 hashBytes;
            int type = 0;
            if (!GetIndexKey(it->get_str(), hashBytes, type)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }
            addresses.push_back(std::make_pair(hashBytes, type));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    
    return true;
}

bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}

bool timestampSort(std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> a,
                   std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> b) {
    return a.second.time < b.second.time;
}
UniValue getaddressmempool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                            "getaddressmempool\n"
                            "\nReturns all mempool deltas for an address (requires addressindex to be enabled).\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"addresses\"\n"
                            "    [\n"
                            "      \"address\"  (string) The base58check encoded address\n"
                            "      ,...\n"
                            "    ]\n"
                            "}\n"
                            "\nResult:\n"
                            "[\n"
                            "  {\n"
                            "    \"address\"  (string) The base58check encoded address\n"
                            "    \"txid\"  (string) The related txid\n"
                            "    \"index\"  (number) The related input or output index\n"
                            "    \"satoshis\"  (number) The difference of satoshis\n"
                            "    \"timestamp\"  (number) The time the transaction entered the mempool (seconds)\n"
                            "    \"prevtxid\"  (string) The previous txid (if spending)\n"
                            "    \"prevout\"  (string) The previous transaction output index (if spending)\n"
                            "  }\n"
                            "]\n"
                            "\nExamples:\n"
                            + HelpExampleCli("getaddressmempool", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
                            + HelpExampleRpc("getaddressmempool", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
                            );
    
    std::vector<std::pair<uint160, int> > addresses;
    
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    
    std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> > indexes;
    
    if (!mempool.getAddressIndex(addresses, indexes)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
    }
    
    std::sort(indexes.begin(), indexes.end(), timestampSort);
    
    UniValue result(UniValue::VARR);
    
    for (std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> >::iterator it = indexes.begin();
         it != indexes.end(); it++) {
        
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.addressBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        
        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("address", address));
        delta.push_back(Pair("txid", it->first.txhash.GetHex()));
        delta.push_back(Pair("index", (int)it->first.index));
        delta.push_back(Pair("satoshis", it->second.amount));
        delta.push_back(Pair("timestamp", it->second.time));
        if (it->second.amount < 0) {
            delta.push_back(Pair("prevtxid", it->second.prevhash.GetHex()));
            delta.push_back(Pair("prevout", (int)it->second.prevout));
        }
        result.push_back(delta);
    }
    
    return result;
}

UniValue getaddressutxos(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                            "getaddressutxos\n"
                            "\nReturns all unspent outputs for an address (requires addressindex to be enabled).\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"addresses\"\n"
                            "    [\n"
                            "      \"address\"  (string) The base58check encoded address\n"
                            "      ,...\n"
                            "    ],\n"
                            "  \"chainInfo\"  (boolean) Include chain info with results\n"
                            "}\n"
                            "\nResult\n"
                            "[\n"
                            "  {\n"
                            "    \"address\"  (string) The address base58check encoded\n"
                            "    \"txid\"  (string) The output txid\n"
                            "    \"height\"  (number) The block height\n"
                            "    \"outputIndex\"  (number) The output index\n"
                            "    \"script\"  (strin) The script hex encoded\n"
                            "    \"satoshis\"  (number) The number of satoshis of the output\n"
                            "  }\n"
                            "]\n"
                            "\nExamples:\n"
                            + HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
                            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
                            );
    
    bool includeChainInfo = false;
    if (request.params[0].isObject()) {
        UniValue chainInfo = find_value(request.params[0].get_obj(), "chainInfo");
        if (chainInfo.isBool()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }
    
    std::vector<std::pair<uint160, int> > addresses;
    
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }
    
    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);
    
    UniValue utxos(UniValue::VARR);
    
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        
        output.push_back(Pair("address", address));
        output.push_back(Pair("txid", it->first.txhash.GetHex()));
        output.push_back(Pair("outputIndex", (int)it->first.index));
        output.push_back(Pair("script", HexStr(it->second.script.begin(), it->second.script.end())));
        output.push_back(Pair("satoshis", it->second.satoshis));
        output.push_back(Pair("height", it->second.blockHeight));
        utxos.push_back(output);
    }
    
    if (includeChainInfo) {
        UniValue result(UniValue::VOBJ);
        result.push_back(Pair("utxos", utxos));
        
        LOCK(cs_main);
        result.push_back(Pair("hash", chainActive.Tip()->GetBlockHash().GetHex()));
        result.push_back(Pair("height", (int)chainActive.Height()));
        return result;
    } else {
        return utxos;
    }
}

UniValue getaddressdeltas(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1 || !request.params[0].isObject())   {
        throw std::runtime_error(
                            "getaddressdeltas\n"
                            "\nReturns all changes for an address (requires addressindex to be enabled).\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"addresses\"\n"
                            "    [\n"
                            "      \"address\"  (string) The base58check encoded address\n"
                            "      ,...\n"
                            "    ]\n"
                            "  \"start\" (number) The start block height\n"
                            "  \"end\" (number) The end block height\n"
                            "  \"chainInfo\" (boolean) Include chain info in results, only applies if start and end specified\n"
                            "}\n"
                            "\nResult:\n"
                            "[\n"
                            "  {\n"
                            "    \"satoshis\"  (number) The difference of satoshis\n"
                            "    \"txid\"  (string) The related txid\n"
                            "    \"index\"  (number) The related input or output index\n"
                            "    \"height\"  (number) The block height\n"
                            "    \"address\"  (string) The base58check encoded address\n"
                            "  }\n"
                            "]\n"
            "\nExamples:\n"
                            + HelpExampleCli("getaddressdeltas", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
                            + HelpExampleRpc("getaddressdeltas", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
        );
    }

    UniValue startValue = find_value(request.params[0].get_obj(), "start");
    UniValue endValue = find_value(request.params[0].get_obj(), "end");

    UniValue chainInfo = find_value(request.params[0].get_obj(), "chainInfo");
    bool includeChainInfo = false;
    if (chainInfo.isBool()) {
        includeChainInfo = chainInfo.get_bool();
    }
    int start = 0;
    int end = 0;

    if (startValue.isNum() && endValue.isNum()) {
        start = startValue.get_int();
        end = endValue.get_int();
        if (start <= 0 || end <= 0) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start and end is expected to be greater than zero");
        }
        if (end < start) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "End value is expected to be greater than start");
        }
    }

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    // bitcore merge (MIPPL), Unused
    //uint32_t updated_log_categories = g_logger->GetCategoryMask();
    //uint32_t changed_log_categories = original_log_categories ^ updated_log_categories;

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }

    UniValue deltas(UniValue::VARR);

    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        
        UniValue delta(UniValue::VOBJ);
        delta.push_back(Pair("satoshis", it->second));
        delta.push_back(Pair("txid", it->first.txhash.GetHex()));
        delta.push_back(Pair("index", (int)it->first.index));
        delta.push_back(Pair("blockindex", (int)it->first.txindex));
        delta.push_back(Pair("height", it->first.blockHeight));
        delta.push_back(Pair("address", address));
        deltas.push_back(delta);
    }

    UniValue result(UniValue::VOBJ);
    if (includeChainInfo && start > 0 && end > 0) {
        LOCK(cs_main);
        
        if (start > chainActive.Height() || end > chainActive.Height()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Start or end is outside chain range");
        }
        
        CBlockIndex* startIndex = chainActive[start];
        CBlockIndex* endIndex = chainActive[end];
        
        UniValue startInfo(UniValue::VOBJ);
        UniValue endInfo(UniValue::VOBJ);
        
        startInfo.push_back(Pair("hash", startIndex->GetBlockHash().GetHex()));
        startInfo.push_back(Pair("height", start));
        
        endInfo.push_back(Pair("hash", endIndex->GetBlockHash().GetHex()));
        endInfo.push_back(Pair("height", end));
        
        result.push_back(Pair("deltas", deltas));
        result.push_back(Pair("start", startInfo));
        result.push_back(Pair("end", endInfo));
        
        return result;
    } else {
        return deltas;
    }
}

UniValue getaddressbalance(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                            "getaddressbalance\n"
                            "\nReturns the balance for an address(es) (requires addressindex to be enabled).\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"addresses\"\n"
                            "    [\n"
                            "      \"address\"  (string) The base58check encoded address\n"
                            "      ,...\n"
                            "    ]\n"
                            "}\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"balance\"  (string) The current balance in satoshis\n"
                            "  \"received\"  (string) The total number of satoshis received (including change)\n"
                            "}\n"
                            "\nExamples:\n"
                            + HelpExampleCli("getaddressbalance", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
                            + HelpExampleRpc("getaddressbalance", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
                            );
    
    std::vector<std::pair<uint160, int> > addresses;
    
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }
    
    CAmount balance = 0;
    CAmount received = 0;
    
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        if (it->second > 0) {
            received += it->second;
        }
        balance += it->second;
    }
    
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("balance", balance));
    result.push_back(Pair("received", received));
    
    return result;
}

UniValue getaddresstxids(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
                            "getaddresstxids\n"
                            "\nReturns the txids for an address(es) (requires addressindex to be enabled).\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"addresses\"\n"
                            "    [\n"
                            "      \"address\"  (string) The base58check encoded address\n"
                            "      ,...\n"
                            "    ]\n"
                            "  \"start\" (number) The start block height\n"
                            "  \"end\" (number) The end block height\n"
                            "}\n"
                            "\nResult:\n"
                            "[\n"
                            "  \"transactionid\"  (string) The transaction id\n"
                            "  ,...\n"
                            "]\n"
                            "\nExamples:\n"
                            + HelpExampleCli("getaddresstxids", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
                            + HelpExampleRpc("getaddresstxids", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
        );

    std::vector<std::pair<uint160, int> > addresses;
    
    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    
    int start = 0;
    int end = 0;
    if (request.params[0].isObject()) {
        UniValue startValue = find_value(request.params[0].get_obj(), "start");
        UniValue endValue = find_value(request.params[0].get_obj(), "end");
        if (startValue.isNum() && endValue.isNum()) {
            start = startValue.get_int();
            end = endValue.get_int();
        }
    }
    
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {
        if (start > 0 && end > 0) {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex, start, end)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        } else {
            if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
        }
    }
    
    std::set<std::pair<int, std::string> > txids;
    UniValue result(UniValue::VARR);
    
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++) {
        int height = it->first.blockHeight;
        std::string txid = it->first.txhash.GetHex();
        
        if (addresses.size() > 1) {
            txids.insert(std::make_pair(height, txid));
        } else {
            if (txids.insert(std::make_pair(height, txid)).second) {
                result.push_back(txid);
            }
        }
    }
    
    if (addresses.size() > 1) {
        for (std::set<std::pair<int, std::string> >::const_iterator it=txids.begin(); it!=txids.end(); it++) {
            result.push_back(it->second);
        }
    }
    
    return result;
    
}

UniValue getspentinfo(const JSONRPCRequest& request)
{
    
    if (request.fHelp || request.params.size() != 1 || !request.params[0].isObject())
        throw std::runtime_error(
                            "getspentinfo\n"
                            "\nReturns the txid and index where an output is spent.\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"txid\" (string) The hex string of the txid\n"
                            "  \"index\" (number) The start block height\n"
                            "}\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"txid\"  (string) The transaction id\n"
                            "  \"index\"  (number) The spending input index\n"
                            "  ,...\n"
                            "}\n"
                            "\nExamples:\n"
                            + HelpExampleCli("getspentinfo", "'{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}'")
                            + HelpExampleRpc("getspentinfo", "{\"txid\": \"0437cd7f8525ceed2324359c2d0ba26006d92d856a9c20fa0241106ee5a597c9\", \"index\": 0}")
                            );
    
    UniValue txidValue = find_value(request.params[0].get_obj(), "txid");
    UniValue indexValue = find_value(request.params[0].get_obj(), "index");
    
    if (!txidValue.isStr() || !indexValue.isNum()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid txid or index");
    }
    
    uint256 txid = ParseHashV(txidValue, "txid");
    int outputIndex = indexValue.get_int();
    
    CSpentIndexKey key(txid, outputIndex);
    CSpentIndexValue value;
    
    if (!GetSpentIndex(key, value)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unable to get spent info");
    }
    
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("txid", value.txid.GetHex()));
    obj.push_back(Pair("index", (int)value.inputIndex));
    obj.push_back(Pair("height", value.blockHeight));
    
    return obj;
}

static UniValue getinfo_deprecated(const JSONRPCRequest& request)
{
    throw JSONRPCError(RPC_METHOD_NOT_FOUND,
        "getinfo\n"
        "\nThis call was removed in version 0.16.0. Use the appropriate fields from:\n"
        "- getblockchaininfo: blocks, difficulty, chain\n"
        "- getnetworkinfo: version, protocolversion, timeoffset, connections, proxy, relayfee, warnings\n"
        "- getwalletinfo: balance, keypoololdest, keypoolsize, paytxfee, unlocked_until, walletversion\n"
        "\nsin-cli has the option -getinfo to collect and format these in the old format."
    );
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getmemoryinfo",          &getmemoryinfo,          {} },
    { "util",               "validateaddress",        &validateaddress,        {"address"} }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         {"nrequired","keys","address_type"} },
    { "util",               "verifymessage",          &verifymessage,          {"address","signature","message"} },
    { "util",               "signmessagewithprivkey", &signmessagewithprivkey, {"privkey","message"} },

    /* Address index */
    { "addressindex",       "getaddressmempool",      &getaddressmempool,      {}},
    { "addressindex",       "getaddressutxos",        &getaddressutxos,        {} },
    { "addressindex",       "getaddressdeltas",       &getaddressdeltas,       {} },
    { "addressindex",       "getaddresstxids",        &getaddresstxids,        {} },
    { "addressindex",       "getaddressbalance",      &getaddressbalance,      {} },
    
    /* Blockchain */
    { "blockchain",         "getspentinfo",           &getspentinfo,           {} },
    
    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            {"timestamp"}},
    { "hidden",             "echo",                   &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
    { "hidden",             "echojson",               &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"}},
    { "hidden",             "getinfo",                &getinfo_deprecated,     {}},

    // Dash
    { "dash",               "mnsync",                 &mnsync,                 {"status-next-reset"}  },
    { "dash",               "spork",                  &spork,                  {"name", "value"}  },
    //
};

void RegisterMiscRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
