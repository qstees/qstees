// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynodeman.h>
#include <infinitynodersv.h>

#include <util.h> //fMasterNode variable
#include <chainparams.h>
#include <key_io.h>
#include <util.h>
#include <script/standard.h>
#include <flat-database.h>
#include <utilstrencodings.h>
#include <netbase.h>


CInfinitynodeMan infnodeman;

const std::string CInfinitynodeMan::SERIALIZATION_VERSION_STRING = "CInfinitynodeMan-Version-1";

struct CompareIntValue
{
    bool operator()(const std::pair<int, CInfinitynode*>& t1,
                    const std::pair<int, CInfinitynode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vinBurnFund < t2.second->vinBurnFund);
    }
};

struct CompareUnit256Value
{
    bool operator()(const std::pair<arith_uint256, CInfinitynode*>& t1,
                    const std::pair<arith_uint256, CInfinitynode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vinBurnFund < t2.second->vinBurnFund);
    }
};

CInfinitynodeMan::CInfinitynodeMan()
: cs(),
  mapInfinitynodes(),
  nLastScanHeight(0)
{}

void CInfinitynodeMan::Clear()
{
    LOCK(cs);
    mapInfinitynodes.clear();
    mapLastPaid.clear();
    nLastScanHeight = 0;
}

bool CInfinitynodeMan::Add(CInfinitynode &inf)
{
    LOCK(cs);
    if (Has(inf.vinBurnFund.prevout)) return false;
    mapInfinitynodes[inf.vinBurnFund.prevout] = inf;
    return true;
}

bool CInfinitynodeMan::AddUpdateLastPaid(CScript scriptPubKey, int nHeightLastPaid)
{
    LOCK(cs_LastPaid);
    auto it = mapLastPaid.find(scriptPubKey);
    if (it != mapLastPaid.end()) {
        if (mapLastPaid[scriptPubKey] < nHeightLastPaid) {
            mapLastPaid[scriptPubKey] = nHeightLastPaid;
        }
        return true;
    }
    mapLastPaid[scriptPubKey] = nHeightLastPaid;
    return true;
}

CInfinitynode* CInfinitynodeMan::Find(const COutPoint &outpoint)
{
    LOCK(cs);
    auto it = mapInfinitynodes.find(outpoint);
    return it == mapInfinitynodes.end() ? NULL : &(it->second);
}

bool CInfinitynodeMan::Get(const COutPoint& outpoint, CInfinitynode& infinitynodeRet)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    auto it = mapInfinitynodes.find(outpoint);
    if (it == mapInfinitynodes.end()) {
        return false;
    }

    infinitynodeRet = it->second;
    return true;
}

bool CInfinitynodeMan::Has(const COutPoint& outpoint)
{
    LOCK(cs);
    return mapInfinitynodes.find(outpoint) != mapInfinitynodes.end();
}

bool CInfinitynodeMan::HasPayee(CScript scriptPubKey)
{
    LOCK(cs_LastPaid);
    return mapLastPaid.find(scriptPubKey) != mapLastPaid.end();
}

int CInfinitynodeMan::Count()
{
    LOCK(cs);
    return mapInfinitynodes.size();
}

std::string CInfinitynodeMan::ToString() const
{
    std::ostringstream info;

    info << "InfinityNode: " << (int)mapInfinitynodes.size() <<
            ", nLastScanHeight: " << (int)nLastScanHeight;

    return info.str();
}

void CInfinitynodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    nCachedBlockHeight = pindex->nHeight;
    if(fMasterNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        /* SIN::TODO - update last paid for all infinitynode */
        //UpdateLastPaid(pindex);
    }
}

void CInfinitynodeMan::CheckAndRemove(CConnman& connman)
{
    /*this function is called in InfinityNode thread and after sync of node*/
    LOCK(cs);

    LogPrintf("CInfinitynodeMan::CheckAndRemove -- at Height: %d, last build height: %d nodes\n", nCachedBlockHeight, nLastScanHeight);
    //first scan -- normaly, list is built in init.cpp
    if (nLastScanHeight == 0 && nCachedBlockHeight > Params().GetConsensus().nInfinityNodeBeginHeight) {
        buildInfinitynodeList(nCachedBlockHeight, Params().GetConsensus().nInfinityNodeBeginHeight);
        return;
    }

    //2nd scan and loop
    if (nCachedBlockHeight > nLastScanHeight && nLastScanHeight > 0)
    {
        LogPrint(BCLog::INFINITYNODE, "CInfinitynodeMan::CheckAndRemove -- block height %d and lastScan %d\n", 
                   nCachedBlockHeight, nLastScanHeight);
        buildInfinitynodeList(nCachedBlockHeight, nLastScanHeight);
    }

    if (nBIGLastStmHeight + nBIGLastStmSize - nCachedBlockHeight < INF_MATURED_LIMIT){
        //calcul new Statement
        deterministicRewardStatement(10);
        //update rank for new Statement
        calculInfinityNodeRank(nBIGLastStmHeight, 10, true);
    }
    if (nMIDLastStmHeight + nMIDLastStmSize - nCachedBlockHeight < INF_MATURED_LIMIT){
        deterministicRewardStatement(5);
        calculInfinityNodeRank(nMIDLastStmHeight, 5, true);
    }
    if (nLILLastStmHeight + nLILLastStmSize - nCachedBlockHeight < INF_MATURED_LIMIT){
        deterministicRewardStatement(1);
        calculInfinityNodeRank(nLILLastStmHeight, 1, true);
    }

    return;
}

int CInfinitynodeMan::getRoi(int nSinType, int totalNode)
{
     LOCK(cs);
     int nBurnAmount = 0;
     if (nSinType == 10) nBurnAmount = Params().GetConsensus().nMasternodeBurnSINNODE_10;
     if (nSinType == 5) nBurnAmount = Params().GetConsensus().nMasternodeBurnSINNODE_5;
     if (nSinType == 1) nBurnAmount = Params().GetConsensus().nMasternodeBurnSINNODE_1;

     float nReward = GetMasternodePayment(nCachedBlockHeight, nSinType) / COIN;
     float roi = nBurnAmount / ((720 / (float)totalNode) * nReward) ;
     return (int) roi;
}

bool CInfinitynodeMan::initialInfinitynodeList(int nBlockHeight)
{
    LOCK(cs);
    if(nBlockHeight < Params().GetConsensus().nInfinityNodeBeginHeight) return false;
    LogPrintf("CInfinitynodeMan::initialInfinitynodeList -- initial at height: %d, last scan height: %d\n", nBlockHeight, nLastScanHeight);
    return buildInfinitynodeList(nBlockHeight, Params().GetConsensus().nInfinityNodeBeginHeight);
}

bool CInfinitynodeMan::updateInfinitynodeList(int nBlockHeight)
{
    LogPrintf("CInfinitynodeMan::updateInfinitynodeList -- begin at %d...\n", nBlockHeight);
    LOCK(cs);
    if (nLastScanHeight == 0) {
        LogPrintf("CInfinitynodeMan::updateInfinitynodeList -- update list for 1st scan at Height %d\n",nBlockHeight); 
        return buildInfinitynodeList(nBlockHeight, Params().GetConsensus().nInfinityNodeBeginHeight);
    }
    if(nBlockHeight < nLastScanHeight) return false;
    LogPrintf("CInfinitynodeMan::updateInfinitynodeList -- update at height: %d, last scan height: %d\n", nBlockHeight, nLastScanHeight);
    return buildInfinitynodeList(nBlockHeight, nLastScanHeight);
}

bool CInfinitynodeMan::buildInfinitynodeList(int nBlockHeight, int nLowHeight)
{
    assert(nBlockHeight >= nLowHeight);
    AssertLockHeld(cs);
    mapInfinitynodesNonMatured.clear();

    //first run, make sure that all variable is clear
    if (nLowHeight == Params().GetConsensus().nInfinityNodeBeginHeight){
        Clear();
        infnodersv.Clear();
    } else {
        nLowHeight = nLastScanHeight;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrint(BCLog::INFINITYNODE, "CInfinitynodeMan::buildInfinitynodeList -- can not read block hash\n");
        return false;
    }

    CBlockIndex* pindex;
    pindex = LookupBlockIndex(blockHash);
    CBlockIndex* prevBlockIndex = pindex;
    int nLastPaidScanDeepth = max(Params().GetConsensus().nLimitSINNODE_1, max(Params().GetConsensus().nLimitSINNODE_5, Params().GetConsensus().nLimitSINNODE_10));
    while (prevBlockIndex->nHeight >= nLowHeight)
    {
        CBlock blockReadFromDisk;
        if (ReadBlockFromDisk(blockReadFromDisk, prevBlockIndex, Params().GetConsensus()))
        {
            for (const CTransactionRef& tx : blockReadFromDisk.vtx) {
                //Not coinbase
                if (!tx->IsCoinBase()) {
                    for (unsigned int i = 0; i < tx->vout.size(); i++) {
                        const CTxOut& out = tx->vout[i];
                        std::vector<std::vector<unsigned char>> vSolutions;
                        txnouttype whichType;
                        const CScript& prevScript = out.scriptPubKey;
                        Solver(prevScript, whichType, vSolutions);
                        //Send to BurnAddress
                        if (whichType == TX_BURN_DATA && Params().GetConsensus().cBurnAddress == EncodeDestination(CKeyID(uint160(vSolutions[0]))))
                        {
                            //Amount for InfnityNode
                            if (
                            ((Params().GetConsensus().nMasternodeBurnSINNODE_1 - 1) * COIN < out.nValue && out.nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_1 * COIN) ||
                            ((Params().GetConsensus().nMasternodeBurnSINNODE_5 - 1) * COIN < out.nValue && out.nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_5 * COIN) ||
                            ((Params().GetConsensus().nMasternodeBurnSINNODE_10 - 1) * COIN < out.nValue && out.nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_10 * COIN)
                            ) {
                                COutPoint outpoint(tx->GetHash(), i);
                                CInfinitynode inf(PROTOCOL_VERSION, outpoint);
                                inf.setHeight(prevBlockIndex->nHeight);
                                inf.setBurnValue(out.nValue);
                                inf.setScriptPublicKey(prevScript);
                                if (vSolutions.size() == 2){
                                    std::string backupAddress(vSolutions[1].begin(), vSolutions[1].end());
                                    CTxDestination NodeAddress = DecodeDestination(backupAddress);
                                    if (IsValidDestination(NodeAddress)) {
                                        inf.setBackupAddress(backupAddress);
                                    }
                                }
                                //SINType
                                CAmount nBurnAmount = out.nValue / COIN + 1; //automaticaly round
                                inf.setSINType(nBurnAmount / 100000);
                                //Address payee: we known that there is only 1 input
                                const CTxIn& txin = tx->vin[0];
                                int index = txin.prevout.n;

                                CTransactionRef prevtx;
                                uint256 hashblock;
                                if(!GetTransaction(txin.prevout.hash, prevtx, Params().GetConsensus(), hashblock, false)) {
                                    LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- PrevBurnFund tx is not in block.\n");
                                    return false;
                                }

                                CTxDestination addressBurnFund;
                                if(!ExtractDestination(prevtx->vout[index].scriptPubKey, addressBurnFund)){
                                    LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- False when extract payee from BurnFund tx.\n");
                                    return false;
                                }
                                inf.setCollateralAddress(EncodeDestination(addressBurnFund));
                                //we have all infos. Then add in map
                                if(prevBlockIndex->nHeight < pindex->nHeight - INF_MATURED_LIMIT) {
                                    //matured
                                    Add(inf);
                                } else {
                                    //non matured
                                    mapInfinitynodesNonMatured[inf.vinBurnFund.prevout] = inf;
                                }
                            }
                            //Amount for vote
                            if (out.nValue == Params().GetConsensus().nInfinityNodeVoteValue * COIN){
                                if (vSolutions.size() == 2){
                                    std::string voteOpinion(vSolutions[1].begin(), vSolutions[1].end());
                                    if(voteOpinion.length() == 9){
                                        std::string proposalID = voteOpinion.substr(0, 8);
                                        bool opinion = false;
                                        if( voteOpinion.substr(8, 1) == "1" ){opinion = true;}
                                        //Address payee: we known that there is only 1 input
                                        const CTxIn& txin = tx->vin[0];
                                        int index = txin.prevout.n;

                                        CTransactionRef prevtx;
                                        uint256 hashblock;
                                        if(!GetTransaction(txin.prevout.hash, prevtx, Params().GetConsensus(), hashblock, false)) {
                                            LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- PrevBurnFund tx is not in block.\n");
                                            return false;
                                        }

                                        CTxDestination addressBurnFund;
                                        if(!ExtractDestination(prevtx->vout[index].scriptPubKey, addressBurnFund)){
                                            LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- False when extract payee from BurnFund tx.\n");
                                            return false;
                                        }
                                        CVote vote = CVote(proposalID, prevtx->vout[index].scriptPubKey, prevBlockIndex->nHeight, opinion);
                                        infnodersv.Add(vote);
                                    }
                                }
                            }
                        }
                        //Governance Vote Address
                        if (whichType == TX_BURN_DATA && Params().GetConsensus().cGovernanceAddress == EncodeDestination(CKeyID(uint160(vSolutions[0]))))
                        {
                            //Amount for vote
                            if (out.nValue == Params().GetConsensus().nInfinityNodeVoteValue * COIN){
                                if (vSolutions.size() == 2){
                                    std::string voteOpinion(vSolutions[1].begin(), vSolutions[1].end());
                                    if(voteOpinion.length() == 9){
                                        std::string proposalID = voteOpinion.substr(0, 8);
                                        bool opinion = false;
                                        if( voteOpinion.substr(8, 1) == "1" ){opinion = true;}
                                        //Address payee: we known that there is only 1 input
                                        const CTxIn& txin = tx->vin[0];
                                        int index = txin.prevout.n;

                                        CTransactionRef prevtx;
                                        uint256 hashblock;
                                        if(!GetTransaction(txin.prevout.hash, prevtx, Params().GetConsensus(), hashblock, false)) {
                                            LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- PrevBurnFund tx is not in block.\n");
                                            return false;
                                        }

                                        CTxDestination addressBurnFund;
                                        if(!ExtractDestination(prevtx->vout[index].scriptPubKey, addressBurnFund)){
                                            LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- False when extract payee from BurnFund tx.\n");
                                            return false;
                                        }
                                        CVote vote = CVote(proposalID, prevtx->vout[index].scriptPubKey, prevBlockIndex->nHeight, opinion);
                                        infnodersv.Add(vote);
                                    }
                                }
                            }
                        }
                        //Amount to update Metadata
                        if (whichType == TX_BURN_DATA && Params().GetConsensus().cMetadataAddress == EncodeDestination(CKeyID(uint160(vSolutions[0]))))
                        {
                            //Amount for UpdateMeta
                            if (( Params().GetConsensus().nMasternodeBurnSINNODE_1 - 1) * COIN < out.nValue 
                                && out.nValue <= Params().GetConsensus().nMasternodeBurnSINNODE_1 * COIN){
                                if (vSolutions.size() == 2){
                                    std::string metadata(vSolutions[1].begin(), vSolutions[1].end());
                                    string s;
                                    stringstream ss(metadata);
                                    int i=0;
                                    int check=0;
                                    while (getline(ss, s,' ')) {
                                        CTxDestination NodeAddress;
                                        CService service;
                                        //1st position: Node Address
                                        if (i==0) {
                                            NodeAddress = DecodeDestination(s);
                                            if (IsValidDestination(NodeAddress)) {check++;}
                                        }
                                        //2nd position: Node IP
                                        if (i==1 && Lookup(s.c_str(), service, 0, false)) {check++;}

                                        //Update node metadata if nHeight is bigger
                                        if (check == 2){
                                            //prevBlockIndex->nHeight
                                            const CTxIn& txin = tx->vin[0];
                                            int index = txin.prevout.n;

                                            CTransactionRef prevtx;
                                            uint256 hashblock;
                                            if(!GetTransaction(txin.prevout.hash, prevtx, Params().GetConsensus(), hashblock, false)) {
                                                LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- PrevBurnFund tx is not in block.\n");
                                                return false;
                                            }

                                            CTxDestination addressBurnFund;
                                            if(!ExtractDestination(prevtx->vout[index].scriptPubKey, addressBurnFund)){
                                                LogPrintf("CInfinitynodeMan::updateInfinityNodeInfo -- False when extract payee from BurnFund tx.\n");
                                                return false;
                                            }
                                            updateMetadata(EncodeDestination(addressBurnFund), EncodeDestination(NodeAddress), service, prevBlockIndex->nHeight);
                                        }
                                        i++;
                                    }
                                }
                            }
                        }
                    } //end loop for all output
                } else { //Coinbase tx => update mapLastPaid
                    if (prevBlockIndex->nHeight >= pindex->nHeight - nLastPaidScanDeepth){
                        //block payment value
                        CAmount nNodePaymentSINNODE_1 = GetMasternodePayment(prevBlockIndex->nHeight, 1);
                        CAmount nNodePaymentSINNODE_5 = GetMasternodePayment(prevBlockIndex->nHeight, 5);
                        CAmount nNodePaymentSINNODE_10 = GetMasternodePayment(prevBlockIndex->nHeight, 10);
                        //compare and update map
                        for (auto txout : blockReadFromDisk.vtx[0]->vout)
                        {
                            if (txout.nValue == nNodePaymentSINNODE_1 || txout.nValue == nNodePaymentSINNODE_5 ||
                                txout.nValue == nNodePaymentSINNODE_10)
                            {
                                AddUpdateLastPaid(txout.scriptPubKey, prevBlockIndex->nHeight);
                            }
                        }
                    }
                }
            }
        } else {
            LogPrint(BCLog::INFINITYNODE, "CInfinitynodeMan::buildInfinitynodeList -- can not read block from disk\n");
            return false;
        }
        // continue with previous block
        prevBlockIndex = prevBlockIndex->pprev;
    }

    nLastScanHeight = nBlockHeight - INF_MATURED_LIMIT;
    updateLastPaid();

    CFlatDB<CInfinitynodeMan> flatdb5("infinitynode.dat", "magicInfinityNodeCache");
    flatdb5.Dump(infnodeman);

    LogPrintf("CInfinitynodeMan::buildInfinitynodeList -- list infinity node was built from blockchain and has %d nodes\n", Count());
    return true;
}

bool CInfinitynodeMan::GetInfinitynodeInfo(std::string nodeowner, infinitynode_info_t& infInfoRet)
{
    LOCK(cs);
    for (auto& infpair : mapInfinitynodes) {
        if (infpair.second.collateralAddress == nodeowner) {
            infInfoRet = infpair.second.GetInfo();
            return true;
        }
    }
    return false;
}

bool CInfinitynodeMan::GetInfinitynodeInfo(const COutPoint& outpoint, infinitynode_info_t& infInfoRet)
{
    LOCK(cs);
    auto it = mapInfinitynodes.find(outpoint);
    if (it == mapInfinitynodes.end()) {
        return false;
    }
    infInfoRet = it->second.GetInfo();
    return true;
}

void CInfinitynodeMan::updateMetadata(std::string nodeowner, std::string nodeAddress, CService nodeService, int nHeightUpdate)
{
    AssertLockHeld(cs);

    for (auto& infpair : mapInfinitynodes) {
        if (infpair.second.collateralAddress == nodeowner) {
            if (infpair.second.getMetadataHeight() < nHeightUpdate){
                infpair.second.setNodeAddress(nodeAddress);
                infpair.second.setService(nodeService);
            }
        }
    }
}

void CInfinitynodeMan::updateLastPaid()
{
    AssertLockHeld(cs);

    if (mapInfinitynodes.empty())
        return;

    for (auto& infpair : mapInfinitynodes) {
        auto it = mapLastPaid.find(infpair.second.getScriptPublicKey());
        if (it != mapLastPaid.end()) {
            infpair.second.setLastRewardHeight(mapLastPaid[infpair.second.getScriptPublicKey()]);
        }
    }
}

bool CInfinitynodeMan::deterministicRewardStatement(int nSinType)
{
    int stm_height_temp = Params().GetConsensus().nInfinityNodeGenesisStatement;
    int stm_size_temp = 0;
    if (nSinType == 10) mapStatementBIG.clear();
    if (nSinType == 5) mapStatementMID.clear();
    if (nSinType == 1) mapStatementLIL.clear();

    LOCK(cs);
    while (stm_height_temp < nCachedBlockHeight)
    {
        std::map<COutPoint, CInfinitynode> mapInfinitynodesCopy;
        int totalSinType = 0;
        for (auto& infpair : mapInfinitynodes) {
            CInfinitynode inf = infpair.second;
            if (inf.getSINType() == nSinType && inf.getHeight() < stm_height_temp && stm_height_temp <= inf.getExpireHeight()){
                mapInfinitynodesCopy[inf.vinBurnFund.prevout] = inf;
                ++totalSinType;
            }
        }
        /*TODO: fix if totalSinType > limit node*/
        //update variable for each SinType
        if (nSinType == 10)
        {
            if(Params().GetConsensus().nLimitSINNODE_10 < totalSinType){totalSinType=Params().GetConsensus().nLimitSINNODE_10;}
            mapStatementBIG[stm_height_temp] = totalSinType;
            nBIGLastStmHeight = stm_height_temp;
            nBIGLastStmSize = totalSinType;
        }

        if (nSinType == 5)
        {
            if(Params().GetConsensus().nLimitSINNODE_5 < totalSinType){totalSinType=Params().GetConsensus().nLimitSINNODE_5;}
            mapStatementMID[stm_height_temp] = totalSinType;
            nMIDLastStmHeight = stm_height_temp;
            nMIDLastStmSize = totalSinType;
        }

        if (nSinType == 1)
        {
            if(Params().GetConsensus().nLimitSINNODE_1 < totalSinType){totalSinType=Params().GetConsensus().nLimitSINNODE_1;}
            mapStatementLIL[stm_height_temp] = totalSinType;
            nLILLastStmHeight = stm_height_temp;
            nLILLastStmSize = totalSinType;
        }

        //loop
        stm_height_temp = stm_height_temp + totalSinType;
        stm_size_temp = totalSinType;
    }
    return true;
}

std::pair<int, int> CInfinitynodeMan::getLastStatementBySinType(int nSinType)
{
    if (nSinType == 10) return std::make_pair(nBIGLastStmHeight, nBIGLastStmSize);
    else if (nSinType == 5) return std::make_pair(nMIDLastStmHeight, nMIDLastStmSize);
    else if (nSinType == 1) return std::make_pair(nLILLastStmHeight, nLILLastStmSize);
    else return std::make_pair(0, 0);
}

std::string CInfinitynodeMan::getLastStatementString() const
{
    std::ostringstream info;

    info << "BIG: [" << mapStatementBIG.size() << " / " << nBIGLastStmHeight << ":" << nBIGLastStmSize << "] - "
            "MID: [" << mapStatementMID.size() << " / " << nMIDLastStmHeight << ":" << nMIDLastStmSize << "] - "
            "LIL: [" << mapStatementLIL.size() << " / " << nLILLastStmHeight << ":" << nLILLastStmSize << "]";

    return info.str();
}

/**
* Rank = 0 when node is expired
* Rank > 0 node is not expired, order by nHeight and
*
* called in CheckAndRemove
*/
std::map<int, CInfinitynode> CInfinitynodeMan::calculInfinityNodeRank(int nBlockHeight, int nSinType, bool updateList)
{
    AssertLockHeld(cs);
    std::vector<std::pair<int, CInfinitynode*> > vecCInfinitynodeHeight;
    std::map<int, CInfinitynode> retMapInfinityNodeRank;

    for (auto& infpair : mapInfinitynodes) {
        CInfinitynode inf = infpair.second;
        //reinitial Rank to 0 all nodes of nSinType
        if (inf.getSINType() == nSinType) infpair.second.setRank(0);
        //put valid node in vector
        if (inf.getSINType() == nSinType && inf.getExpireHeight() >= nBlockHeight && inf.getHeight() < nBlockHeight)
        {
            vecCInfinitynodeHeight.push_back(std::make_pair(inf.getHeight(), &infpair.second));
        }
    }

    // Sort them low to high
    sort(vecCInfinitynodeHeight.begin(), vecCInfinitynodeHeight.end(), CompareIntValue());
    //update Rank at nBlockHeight
    int rank=1;
    for (std::pair<int, CInfinitynode*>& s : vecCInfinitynodeHeight){
        auto it = mapInfinitynodes.find(s.second->vinBurnFund.prevout);
        if(updateList) it->second.setRank(rank);
        retMapInfinityNodeRank[rank] = *s.second;
        ++rank;
    }

    return retMapInfinityNodeRank;
}

/*
* called in MN synced - just after download all last block
*/
void CInfinitynodeMan::calculAllInfinityNodesRankAtLastStm()
{
    LOCK(cs);
        calculInfinityNodeRank(nBIGLastStmHeight, 10, true);
        calculInfinityNodeRank(nMIDLastStmHeight, 5, true);
        calculInfinityNodeRank(nLILLastStmHeight, 1, true);
}

bool CInfinitynodeMan::deterministicRewardAtHeight(int nBlockHeight, int nSinType, CInfinitynode& infinitynodeRet)
{
    assert(nBlockHeight >= Params().GetConsensus().nInfinityNodeGenesisStatement);
    //step1: copy mapStatement for nSinType
    std::map<int, int> mapStatementSinType = getStatementMap(nSinType);

    LOCK(cs);
    //step2: find last Statement for nBlockHeight;
    int nDelta = 100000; //big enough > number of 
    int lastStatement = 0;
    for(auto& stm : mapStatementSinType)
    {
        if (nBlockHeight > stm.first && nDelta > (nBlockHeight -stm.first))
        {
            nDelta = nBlockHeight -stm.first;
            if(nDelta <= stm.second) lastStatement = stm.first;
        }
    }
    //return false if not found statement
    if (lastStatement == 0) return false;

    std::map<int, CInfinitynode> rankOfStatement = calculInfinityNodeRank(lastStatement, nSinType, false);
    infinitynodeRet = rankOfStatement[nBlockHeight - lastStatement];
    return true;
}
