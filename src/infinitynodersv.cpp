// Copyright (c) 2018-2019 SIN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynodersv.h>
#include <infinitynodeman.h>
#include <util.h> //fMasterNode variable

CInfinitynodersv infnodersv;

const std::string CInfinitynodersv::SERIALIZATION_VERSION_STRING = "CInfinitynodeRSV-Version-1";

CInfinitynodersv::CInfinitynodersv()
: cs(),
  mapProposalVotes()
{}

void CInfinitynodersv::Clear()
{
    LOCK(cs);
    mapProposalVotes.clear();
}

std::vector<CVote>* CInfinitynodersv::Find(std::string proposal)
{
    LOCK(cs);
    auto it = mapProposalVotes.find(proposal);
    return it == mapProposalVotes.end() ? NULL : &(it->second);
}

bool CInfinitynodersv::Has(std::string proposal)
{
    LOCK(cs);
    return mapProposalVotes.find(proposal) != mapProposalVotes.end();
}

bool CInfinitynodersv::Add(CVote &vote)
{
    LOCK(cs);
    LogPrintf("CInfinitynodersv::new vote from %s %d\n", vote.getVoter().ToString(), vote.getHeight());
    auto it = mapProposalVotes.find(vote.getProposalId());
    if(it == mapProposalVotes.end()){
        LogPrintf("CInfinitynodersv::1st vote from %s\n", vote.getVoter().ToString());
        mapProposalVotes[vote.getProposalId()].push_back(vote);
    } else {
        LogPrintf("CInfinitynodersv::2nd vote from %s\n", vote.getVoter().ToString());
        int i=0;
        for (auto& v : it->second){
            //added
            if(v.getVoter() == vote.getVoter()){
                if(v.getHeight() >= vote.getHeight()){
                    LogPrintf("CInfinitynodersv::same voter from with low height %s\n", v.getVoter().ToString());
                    return false;
                }else{
                    mapProposalVotes[vote.getProposalId()].erase (mapProposalVotes[vote.getProposalId()].begin()+i);
                    mapProposalVotes[vote.getProposalId()].push_back(vote);
                    return true;
                }
            }
            i++;
        }
        //not found the same voter ==> add
        mapProposalVotes[vote.getProposalId()].push_back(vote);
    }
    return true;
}
/**
 * @param {String } proposal 8 digits number
 * @param {boolean} opinion
 * @param {interger} mode: 0: public, 1: node, 2: both
 */
int CInfinitynodersv::getResult(std::string proposal, bool opinion, int mode)
{
    LogPrintf("CInfinitynodersv::result --%s %d\n", proposal, mode);
    LOCK(cs);
    std::map<COutPoint, CInfinitynode> mapInfinitynodesCopy = infnodeman.GetFullInfinitynodeMap();
    int result = 0;
    auto it = mapProposalVotes.find(proposal);
    if(it == mapProposalVotes.end()){
        return 0;
    }else{
        for (auto& v : it->second){
            if(v.getOpinion() == opinion){
                int value = 0;
                if (mode == 0){value = 1;}
                if (mode == 1 || mode == 2){
                    if (mode == 1){value = 0;}
                    CTxDestination voter;
                    ExtractDestination(v.getVoter(), voter);
                    for (auto& infpair : mapInfinitynodesCopy) {
                        if (infpair.second.getCollateralAddress() == EncodeDestination(voter)) {
                            infinitynode_info_t infnode = infpair.second.GetInfo();
                            if(infnode.nSINType == 1){value=2;}
                            if(infnode.nSINType == 5){value=10;}
                            if(infnode.nSINType == 10){value=20;}
                        }
                    }
                }
                result += value;
            }
        }
        return result;
    }
}

bool CInfinitynodersv::rsvScan(int nBlockHeight)
{
    Clear();
    if (nBlockHeight <= INFINITYNODE_RSV_BEGIN) return false;
    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrint(BCLog::INFINITYNODE, "CInfinitynodeMan::buildInfinitynodeList -- can not read block hash\n");
        return false;
    }

    CBlockIndex* pindex;
    pindex = LookupBlockIndex(blockHash);
    CBlockIndex* prevBlockIndex = pindex;
    int nLastPaidScanDeepth = max(Params().GetConsensus().nLimitSINNODE_1, max(Params().GetConsensus().nLimitSINNODE_5, Params().GetConsensus().nLimitSINNODE_10));
    while (prevBlockIndex->nHeight >= INFINITYNODE_RSV_BEGIN)
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
                            //Amount for vote
                            if (out.nValue * 10 == Params().GetConsensus().nInfinityNodeVoteValue * COIN){
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
                                            LogPrintf("CInfinitynodersv::rsvScan -- PrevBurnFund tx is not in block.\n");
                                            return false;
                                        }

                                        CTxDestination addressBurnFund;
                                        if(!ExtractDestination(prevtx->vout[index].scriptPubKey, addressBurnFund)){
                                            LogPrintf("CInfinitynodersv::rsvScan -- False when extract payee from BurnFund tx.\n");
                                            return false;
                                        }
                                        CVote vote = CVote(proposalID, prevtx->vout[index].scriptPubKey, prevBlockIndex->nHeight, opinion);
                                        Add(vote);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else {
            LogPrint(BCLog::INFINITYNODE, "CInfinitynodersv::rsvScan -- can not read block from disk\n");
            return false;
        }
        // continue with previous block
        prevBlockIndex = prevBlockIndex->pprev;
    }
    return true;
}

std::string CInfinitynodersv::ToString() const
{
    std::ostringstream info;

    info << "InfinityNode: " << (int)mapProposalVotes.size();

    return info.str();
}

