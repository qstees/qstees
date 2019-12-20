// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIN_INFINITYNODEMAN_H
#define SIN_INFINITYNODEMAN_H

#include <infinitynode.h>



using namespace std;

class CInfinitynodeMan;
class CConnman;

extern CInfinitynodeMan infnodeman;

class CInfinitynodeMan
{
public:

private:
    static const std::string SERIALIZATION_VERSION_STRING;

    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    // Keep track of current block height
    int nCachedBlockHeight;

    //make sure that this value is sup than chain reorg limit. After this depth, situation of MAP is matured
    static const int INF_MATURED_LIMIT = 55;

    // map to hold all INFs
    std::map<COutPoint, CInfinitynode> mapInfinitynodes;
    std::map<COutPoint, CInfinitynode> mapInfinitynodesNonMatured;

    // map to hold all reward statement
    std::map<int, int> mapStatementBIG;
    std::map<int, int> mapStatementMID;
    std::map<int, int> mapStatementLIL;
    int nBIGLastStmHeight;
    int nMIDLastStmHeight;
    int nLILLastStmHeight;
    int nBIGLastStmSize;
    int nMIDLastStmSize;
    int nLILLastStmSize;

    // map to hold payee and lastPaid Height
    std::map<CScript, int> mapLastPaid;
    mutable CCriticalSection cs_LastPaid;


public:

    CInfinitynodeMan();

    int64_t nLastScanHeight;//last verification from blockchain

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING;
            READWRITE(strVersion);
        }

        READWRITE(mapInfinitynodes);
        READWRITE(mapLastPaid);
        READWRITE(nLastScanHeight);
        READWRITE(mapStatementBIG);
        READWRITE(mapStatementMID);
        READWRITE(mapStatementLIL);
        READWRITE(nBIGLastStmHeight);
        READWRITE(nMIDLastStmHeight);
        READWRITE(nLILLastStmHeight);
        READWRITE(nBIGLastStmSize);
        READWRITE(nMIDLastStmSize);
        READWRITE(nLILLastStmSize);

        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    std::string ToString() const;

    bool Add(CInfinitynode &mn);
    bool AddUpdateLastPaid(CScript scriptPubKey, int nHeightLastPaid);
    /// Find an entry
    CInfinitynode* Find(const COutPoint& outpoint);

    bool GetInfinitynodeInfo(std::string nodeowner, infinitynode_info_t& infInfoRet);
    bool GetInfinitynodeInfo(const COutPoint& outpoint, infinitynode_info_t& infInfoRet);

    /// Clear InfinityNode vector
    void Clear();
    /// Versions of Find that are safe to use from outside the class
    bool Get(const COutPoint& outpoint, CInfinitynode& infinitynodeRet);
    bool Has(const COutPoint& outpoint);
    bool HasPayee(CScript scriptPubKey);
    int Count();
    std::map<COutPoint, CInfinitynode> GetFullInfinitynodeMap() { LOCK(cs); return mapInfinitynodes; }
    std::map<COutPoint, CInfinitynode> GetFullInfinitynodeNonMaturedMap() { LOCK(cs); return mapInfinitynodesNonMatured; }
    std::map<int, int> getStatementMap(int nSinType){
        LOCK(cs);
        if(nSinType == 10) return mapStatementBIG;
        if(nSinType == 5) return mapStatementMID;
        if(nSinType == 1) return mapStatementLIL;
    }
    int getLastStatement(int nSinType){
        LOCK(cs);
        if(nSinType == 10) return nBIGLastStmHeight;
        if(nSinType == 5) return nMIDLastStmHeight;
        if(nSinType == 1) return nLILLastStmHeight;
    }
    int getLastStatementSize(int nSinType){
        LOCK(cs);
        if(nSinType == 10) return nBIGLastStmSize;
        if(nSinType == 5) return nMIDLastStmSize;
        if(nSinType == 1) return nLILLastStmSize;
    }

    std::map<CScript, int> GetFullLastPaidMap() { return mapLastPaid; }
    int64_t getLastScan(){return nLastScanHeight;}
    int64_t getLastScanWithLimit(){return nLastScanHeight + INF_MATURED_LIMIT;}

    bool buildInfinitynodeList(int nBlockHeight, int nLowHeight = 165000);
    bool buildListForBlock(int nBlockHeight);
    void updateLastPaid();
    void updateMetadata(std::string nodeowner, std::string nodeAddress, CService nodeService, int nHeightUpdate);
    bool updateInfinitynodeList(int fromHeight);//call in init.cppp
    bool initialInfinitynodeList(int fromHeight);//call in init.cpp

    bool deterministicRewardStatement(int nSinType);
    bool deterministicRewardAtHeight(int nBlockHeight, int nSinType, CInfinitynode& infinitynodeRet);
    std::map<int, CInfinitynode> calculInfinityNodeRank(int nBlockHeight, int nSinType, bool updateList=false);
    void calculAllInfinityNodesRankAtLastStm();
    std::pair<int, int> getLastStatementBySinType(int nSinType);
    std::string getLastStatementString() const;
    int getRoi(int nSinType, int totalNode);

    void CheckAndRemove(CConnman& connman);
    /// This is dummy overload to be used for dumping/loading mncache.dat
    void CheckAndRemove() {}
    void UpdatedBlockTip(const CBlockIndex *pindex);
};
#endif // SIN_INFINITYNODEMAN_H
