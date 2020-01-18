// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <chainparams.h>
#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>

const int nQsteesHeightFinalnet = 5;
const int nQsteesHeightTestnet  = 5;
const int nQsteesHeightMainnet  = 170000;

uint256 CBlockHeader::GetHash() const
{
    return HashX22I(BEGIN(nVersion), END(nNonce));
}

uint256 CBlockHeader::GetPoWHash(int nHeight) const
{
    bool fQsteesMode = false;

    if ((Params().NetworkIDString() == CBaseChainParams::MAIN && nHeight >= nQsteesHeightMainnet) ||
        (Params().NetworkIDString() == CBaseChainParams::TESTNET && nHeight >= nQsteesHeightTestnet) ||
        (Params().NetworkIDString() == CBaseChainParams::FINALNET && nHeight >= nQsteesHeightFinalnet))
        fQsteesMode = true;

    if (!fQsteesMode)
        return HashX22I(BEGIN(nVersion), END(nNonce));
    else
        return HashX25X(BEGIN(nVersion), END(nNonce));
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
