// Copyright (c) 2018-2019 QSTEES developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <infinitynode.h>
#include <key_io.h>
#include <netbase.h>
#include <messagesigner.h>
#include <script/standard.h>
#include <timedata.h>

#include <shutdown.h>

#include <util.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <boost/lexical_cast.hpp>

CInfinitynode::CInfinitynode() :
    infinitynode_info_t{PROTOCOL_VERSION, GetAdjustedTime()}
{}

CInfinitynode::CInfinitynode(const CInfinitynode& other) :
    infinitynode_info_t{other}
{}

CInfinitynode::CInfinitynode(int nProtocolVersionIn, COutPoint outpointBurnFund) :
    infinitynode_info_t{nProtocolVersionIn, GetAdjustedTime(), outpointBurnFund}
{}

infinitynode_info_t CInfinitynode::GetInfo()
{
    infinitynode_info_t info{*this};
    return info;
}