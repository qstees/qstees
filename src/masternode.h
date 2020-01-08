// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018 FXTC developers
// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FXTC_MASTERNODE_H
#define FXTC_MASTERNODE_H

#include <key.h>
#include <validation.h>
#include <spork.h>

class CMasternode;
class CMasternodeBroadcast;
class CConnman;

static const int MASTERNODE_CHECK_SECONDS               =   5;
static const int MASTERNODE_MIN_MNB_SECONDS             =   5 * 60;
static const int MASTERNODE_MIN_MNP_SECONDS             =  10 * 60;
static const int MASTERNODE_EXPIRATION_SECONDS          =  65 * 60;
static const int MASTERNODE_WATCHDOG_MAX_SECONDS        = 120 * 60;
static const int MASTERNODE_NEW_START_REQUIRED_SECONDS  = 180 * 60;

static const int MASTERNODE_POSE_BAN_MAX_SCORE          = 5;

//IN Vars
static const int IN_FULL_SCAN_SECONDS                   = 2 * 60;

//
// The Masternode Ping Class : Contains a different serialize method for sending pings from masternodes throughout the network
//

// sentinel version before sentinel ping implementation
#define DEFAULT_SENTINEL_VERSION 0x010001

class CMasternodePing
{
public:
    CTxIn vin{};
    uint256 blockHash{};
    int64_t sigTime{}; //mnb message times
    std::vector<unsigned char> vchSig{};
    bool fSentinelIsCurrent = false; // true if last sentinel ping was actual
    // MSB is always 0, other 3 bits corresponds to x.x.x version scheme
    uint32_t nSentinelVersion{DEFAULT_SENTINEL_VERSION};

    CMasternodePing() = default;

    CMasternodePing(const COutPoint& outpoint);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vin);
        READWRITE(blockHash);
        READWRITE(sigTime);
        READWRITE(vchSig);
        if(ser_action.ForRead() && (s.size() == 0))
        {
            fSentinelIsCurrent = false;
            nSentinelVersion = DEFAULT_SENTINEL_VERSION;
            return;
        }
        READWRITE(fSentinelIsCurrent);
        READWRITE(nSentinelVersion);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << sigTime;
        return ss.GetHash();
    }

    bool IsExpired() const { return GetAdjustedTime() - sigTime > MASTERNODE_NEW_START_REQUIRED_SECONDS; }

    bool Sign(const CKey& keyMasternode, const CPubKey& pubKeyMasternode);
    bool CheckSignature(CPubKey& pubKeyMasternode, int &nDos);
    bool SimpleCheck(int& nDos);
    bool CheckAndUpdate(CMasternode* pmn, bool fFromNewBroadcast, int& nDos, CConnman& connman);
    void Relay(CConnman& connman);
};

inline bool operator==(const CMasternodePing& a, const CMasternodePing& b)
{
    return a.vin == b.vin && a.blockHash == b.blockHash;
}
inline bool operator!=(const CMasternodePing& a, const CMasternodePing& b)
{
    return !(a == b);
}

struct masternode_info_t
{
    // Note: all these constructors can be removed once C++14 is enabled.
    // (in C++11 the member initializers wrongly disqualify this as an aggregate)
    masternode_info_t() = default;
    masternode_info_t(masternode_info_t const&) = default;

    masternode_info_t(int activeState, int protoVer, int64_t sTime) :
        nActiveState{activeState}, nProtocolVersion{protoVer}, sigTime{sTime} {}

    masternode_info_t(int activeState, int protoVer, int64_t sTime,
                      COutPoint const& outpoint, COutPoint const& outpointBurnFund, CService const& addr,
                      CPubKey const& pkCollAddr, CPubKey const& pkMN,
                      int64_t tWatchdogV = 0) :
        nActiveState{activeState}, nProtocolVersion{protoVer}, sigTime{sTime},
        vin{outpoint}, vinBurnFund{outpointBurnFund}, addr{addr},
        pubKeyCollateralAddress{pkCollAddr}, pubKeyMasternode{pkMN},
        nTimeLastWatchdogVote{tWatchdogV} {}

    int nActiveState = 0;
    int nProtocolVersion = 0;
    int64_t sigTime = 0; //mnb message time
    //infinitynode information
    int nSinType = -1;
    CAmount nBurnAmount = 0;
    CAmount nCollateralAmount = 0;
    int nExpireHeight = -1;
    std::string burnfundAddress = "";
    std::string nodeBurntoAddress = "";
    std::string collateralAddress = "";
    std::string burnTxStandard = "nonstandard";

    CTxIn vin{};
    CTxIn vinBurnFund{};
    CService addr{};
    CPubKey pubKeyCollateralAddress{};
    CPubKey pubKeyMasternode{};
    int64_t nTimeLastWatchdogVote = 0;

    int64_t nLastDsq = 0; //the dsq count from the last dsq broadcast of this node
    int64_t nTimeLastChecked = 0;
    int64_t nTimeLastFullINScan = 0;
    int64_t nTimeLastPaid = 0;
    int64_t nTimeLastPing = 0; //* not in CMN
    bool fInfoValid = false; //* not in CMN
};

//
// The Masternode Class. signature to prove
// it's the one who own that ip address and code for calculating the payment election.
//
class CMasternode : public masternode_info_t
{
private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

public:
    enum state {
        MASTERNODE_PRE_ENABLED,
        MASTERNODE_ENABLED,
        MASTERNODE_EXPIRED,
        MASTERNODE_OUTPOINT_SPENT,
        MASTERNODE_UPDATE_REQUIRED,
        MASTERNODE_WATCHDOG_EXPIRED,
        MASTERNODE_NEW_START_REQUIRED,
        MASTERNODE_POSE_BAN
    };

    enum CollateralStatus {
        COLLATERAL_OK,
        COLLATERAL_UTXO_NOT_FOUND,
        COLLATERAL_INVALID_AMOUNT
    };

    enum BurnFundStatus {
        BURNFUND_OK,
        BURNFUND_UTXO_NOT_FOUND,
        BURNFUND_ISMINE_NO,
        BURNFUND_INPUTADDRES_KO,
        BURNFUND_BURNADDRESS_KO,
        BURNFUND_EXPIRED,
        BURNFUND_INVALID_AMOUNT
    };

    enum SinType {
        SINNODE_1 = 1, SINNODE_5 = 5, SINNODE_10 = 10, SINNODE_UNKNOWN = 0
    };


    CMasternodePing lastPing{};
    std::vector<unsigned char> vchSig{};
    std::vector<SinType> vSinTypeCanVote = {SINNODE_5, SINNODE_10};

    uint256 nCollateralMinConfBlockHash{};
    int nBlockLastPaid{};
    int nPoSeBanScore{};
    int nPoSeBanHeight{};
    bool fAllowMixingTx{};
    bool fUnitTest = false;

    // KEEP TRACK OF GOVERNANCE ITEMS EACH MASTERNODE HAS VOTE UPON FOR RECALCULATION
    std::map<uint256, int> mapGovernanceObjectsVotedOn;

    CMasternode();
    CMasternode(const CMasternode& other);
    CMasternode(const CMasternodeBroadcast& mnb);
    CMasternode(CService addrNew, COutPoint outpointNew, COutPoint outpointBurnFundNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyMasternodeNew, int nProtocolVersionIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        LOCK(cs);
        READWRITE(vin);
        READWRITE(vinBurnFund);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyMasternode);
        READWRITE(lastPing);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nLastDsq);
        READWRITE(nTimeLastChecked);
        READWRITE(nTimeLastFullINScan);
        READWRITE(nTimeLastPaid);
        READWRITE(nTimeLastWatchdogVote);
        READWRITE(nActiveState);
        READWRITE(nCollateralMinConfBlockHash);
        READWRITE(nBlockLastPaid);
        READWRITE(nProtocolVersion);
        READWRITE(nPoSeBanScore);
        READWRITE(nPoSeBanHeight);
        READWRITE(fAllowMixingTx);
        READWRITE(fUnitTest);
        READWRITE(mapGovernanceObjectsVotedOn);
    }

    // CALCULATE A RANK AGAINST OF GIVEN BLOCK
    arith_uint256 CalculateScore(const uint256& blockHash);

    bool UpdateFromNewBroadcast(CMasternodeBroadcast& mnb, CConnman& connman);
    /*this function calcul all informations of infinityNode and update the variable which is init as "NULL" in masternode_info_t*/
    void updateInfinityNodeInfo(bool fAllowFull);

    CAmount CheckOutPointValue(const COutPoint& outpoint);
    
    static CollateralStatus CheckCollateral(const COutPoint& outpoint);
    static CollateralStatus CheckCollateral(const COutPoint& outpoint, int& nHeightRet);
    static BurnFundStatus CheckBurnFund(const COutPoint& outpoint, int nExpireHeight, CAmount nBurnAmount);
    static BurnFundStatus CheckBurnFund(const COutPoint& outpoint, int nExpireHeight, CAmount nBurnAmount, int& nHeightRet);
	bool CheckCollateralBurnFundRelation(const COutPoint& outpoint, const COutPoint& outpointBurnFund);
    bool CanVoteForReward(); //I can vote or not

    void Check(bool fForce = false);
    SinType GetSinType();
    SinType GetSinType(CAmount burnValue);
    int GetSinTypeInt();

    bool IsBroadcastedWithin(int nSeconds) { return GetAdjustedTime() - sigTime < nSeconds; }

    bool IsPingedWithin(int nSeconds, int64_t nTimeToCheckAt = -1)
    {
        if(lastPing == CMasternodePing()) return false;

        if(nTimeToCheckAt == -1) {
            nTimeToCheckAt = GetAdjustedTime();
        }
        return nTimeToCheckAt - lastPing.sigTime < nSeconds;
    }

    bool IsEnabled() { return nActiveState == MASTERNODE_ENABLED; }
    bool IsPreEnabled() { return nActiveState == MASTERNODE_PRE_ENABLED; }
    bool IsPoSeBanned() { return nActiveState == MASTERNODE_POSE_BAN; }
    // NOTE: this one relies on nPoSeBanScore, not on nActiveState as everything else here
    bool IsPoSeVerified() { return nPoSeBanScore <= -MASTERNODE_POSE_BAN_MAX_SCORE; }
    bool IsExpired() { return nActiveState == MASTERNODE_EXPIRED; }
    bool IsOutpointSpent() { return nActiveState == MASTERNODE_OUTPOINT_SPENT; }
    bool IsUpdateRequired() { return nActiveState == MASTERNODE_UPDATE_REQUIRED; }
    bool IsWatchdogExpired() { return nActiveState == MASTERNODE_WATCHDOG_EXPIRED; }
    bool IsNewStartRequired() { return nActiveState == MASTERNODE_NEW_START_REQUIRED; }

    static bool IsValidStateForAutoStart(int nActiveStateIn)
    {
        return  nActiveStateIn == MASTERNODE_ENABLED ||
                nActiveStateIn == MASTERNODE_PRE_ENABLED ||
                nActiveStateIn == MASTERNODE_EXPIRED ||
                nActiveStateIn == MASTERNODE_WATCHDOG_EXPIRED;
    }

    bool IsValidForPayment()
    {
        if(nActiveState == MASTERNODE_ENABLED) {
            return true;
        }
        if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
           (nActiveState == MASTERNODE_WATCHDOG_EXPIRED)) {
            return true;
        }

        return false;
    }

    /// Is the input associated with collateral public key? (and there is masternode collateral - checking if valid masternode)
    bool IsInputAssociatedWithPubkey();

    bool IsValidNetAddr();
    static bool IsValidNetAddr(CService addrIn);

    void IncreasePoSeBanScore() { if(nPoSeBanScore < MASTERNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore++; }
    void DecreasePoSeBanScore() { if(nPoSeBanScore > -MASTERNODE_POSE_BAN_MAX_SCORE) nPoSeBanScore--; }
    void PoSeBan() { nPoSeBanScore = MASTERNODE_POSE_BAN_MAX_SCORE; }

    masternode_info_t GetInfo();

    static std::string StateToString(int nStateIn);
    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetBurnFundTxInfo() const;

    int GetLastPaidTime() { return nTimeLastPaid; }
    int GetLastPaidBlock() { return nBlockLastPaid; }
    void UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack);

    void UpdateWatchdogVoteTime(uint64_t nVoteTime = 0);

    CMasternode& operator=(CMasternode const& from)
    {
        static_cast<masternode_info_t&>(*this)=from;
        lastPing = from.lastPing;
        vchSig = from.vchSig;
        nCollateralMinConfBlockHash = from.nCollateralMinConfBlockHash;
        nBlockLastPaid = from.nBlockLastPaid;
        nPoSeBanScore = from.nPoSeBanScore;
        nPoSeBanHeight = from.nPoSeBanHeight;
        fAllowMixingTx = from.fAllowMixingTx;
        fUnitTest = from.fUnitTest;
        mapGovernanceObjectsVotedOn = from.mapGovernanceObjectsVotedOn;
        return *this;
    }
};

inline bool operator==(const CMasternode& a, const CMasternode& b)
{
    return a.vin == b.vin;
}
inline bool operator!=(const CMasternode& a, const CMasternode& b)
{
    return !(a.vin == b.vin);
}


//
// The Masternode Broadcast Class : Contains a different serialize method for sending masternodes through the network
//

class CMasternodeBroadcast : public CMasternode
{
public:

    bool fRecovery;

    CMasternodeBroadcast() : CMasternode(), fRecovery(false) {}
    CMasternodeBroadcast(const CMasternode& mn) : CMasternode(mn), fRecovery(false) {}
    CMasternodeBroadcast(CService addrNew, COutPoint outpointNew, COutPoint outpointBurnFundNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyMasternodeNew, int nProtocolVersionIn) :
        CMasternode(addrNew, outpointNew, outpointBurnFundNew, pubKeyCollateralAddressNew, pubKeyMasternodeNew, nProtocolVersionIn), fRecovery(false) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vin);
        READWRITE(vinBurnFund);
        READWRITE(addr);
        READWRITE(pubKeyCollateralAddress);
        READWRITE(pubKeyMasternode);
        READWRITE(vchSig);
        READWRITE(sigTime);
        READWRITE(nProtocolVersion);
        READWRITE(lastPing);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin;
        ss << pubKeyCollateralAddress;
        ss << sigTime;
        return ss.GetHash();
    }

    /// Create Masternode broadcast, needs to be relayed manually after that
    static bool Create(const COutPoint& outpoint, const COutPoint& outpointBurnFund, const CService& service, const CKey& keyCollateralAddressNew, const CPubKey& pubKeyCollateralAddressNew, const CKey& keyMasternodeNew, const CPubKey& pubKeyMasternodeNew, std::string &strErrorRet, CMasternodeBroadcast &mnbRet);
    static bool Create(std::string strService, std::string strKey, std::string strTxHash, std::string strOutputIndex, std::string strTxHashBurnFund, std::string strOutputIndexBurnFund, std::string& strErrorRet, CMasternodeBroadcast &mnbRet, bool fOffline = false);

    bool SimpleCheck(int& nDos);
    bool Update(CMasternode* pmn, int& nDos, CConnman& connman);
    bool CheckOutpoint(int& nDos);
    bool Sign(const CKey& keyCollateralAddress);
    bool CheckSignature(int& nDos);
    void Relay(CConnman& connman);
};

class CMasternodeVerification
{
public:
    CTxIn vin1{};
    CTxIn vin2{};
    CService addr{};
    int nonce{};
    int nBlockHeight{};
    std::vector<unsigned char> vchSig1{};
    std::vector<unsigned char> vchSig2{};

    CMasternodeVerification() = default;

    CMasternodeVerification(CService addr, int nonce, int nBlockHeight) :
        addr(addr),
        nonce(nonce),
        nBlockHeight(nBlockHeight)
    {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vin1);
        READWRITE(vin2);
        READWRITE(addr);
        READWRITE(nonce);
        READWRITE(nBlockHeight);
        READWRITE(vchSig1);
        READWRITE(vchSig2);
    }

    uint256 GetHash() const
    {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << vin1;
        ss << vin2;
        ss << addr;
        ss << nonce;
        ss << nBlockHeight;
        return ss.GetHash();
    }

    void Relay() const
    {
        CInv inv(MSG_MASTERNODE_VERIFY, GetHash());
        g_connman->RelayInv(inv);
    }
};

#endif // FXTC_MASTERNODE_H
