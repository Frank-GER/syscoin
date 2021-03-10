// Copyright (c) 2019 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/quorums_chainlocks.h>
#include <llmq/quorums_utils.h>

#include <chain.h>
#include <masternode/masternodesync.h>
#include <net_processing.h>
#include <spork.h>
#include <txmempool.h>
#include <validation.h>

namespace llmq
{

static const std::string CLSIG_REQUESTID_PREFIX = "clsig";

// Used for iterating while signing/verifying CLSIGs by non-default quorums
struct RequestIdStep
{
    int nHeight{-1};
    int nStep{0};

    RequestIdStep(int _nHeight) : nHeight(_nHeight) {}

    SERIALIZE_METHODS(RequestIdStep, obj)
    {
        READWRITE(CLSIG_REQUESTID_PREFIX, obj.nHeight, obj.nStep);
    }
};

CChainLocksHandler* chainLocksHandler;

bool CChainLockSig::IsNull() const
{
    return nHeight == -1 && blockHash == uint256();
}

std::string CChainLockSig::ToString() const
{
    return strprintf("CChainLockSig(nHeight=%d, blockHash=%s, signers: hex=%s size=%d count=%d)",
                nHeight, blockHash.ToString(), CLLMQUtils::ToHexStr(signers), signers.size(),
                std::count(signers.begin(), signers.end(), true));
}

CChainLocksHandler::CChainLocksHandler(CConnman& _connman, PeerManager& _peerman):     
    connman(_connman),
    peerman(_peerman)
{
}

CChainLocksHandler::~CChainLocksHandler()
{
}

void CChainLocksHandler::Start()
{
    quorumSigningManager->RegisterRecoveredSigsListener(this);
}

void CChainLocksHandler::Stop()
{
    quorumSigningManager->UnregisterRecoveredSigsListener(this);
}

bool CChainLocksHandler::AlreadyHave(const uint256& hash)
{
    LOCK(cs);
    return seenChainLocks.count(hash) != 0;
}

bool CChainLocksHandler::GetChainLockByHash(const uint256& hash, llmq::CChainLockSig& ret)
{
    LOCK(cs);

    if (::SerializeHash(mostRecentChainLockShare) == hash) {
        ret = mostRecentChainLockShare;
        return true;
    }

    if (::SerializeHash(bestChainLockWithKnownBlock) == hash) {
        ret = bestChainLockWithKnownBlock;
        return true;
    }

    for (const auto& pair : bestChainLockShares) {
        for (const auto& pair2 : pair.second) {
            if (::SerializeHash(*pair2.second) == hash) {
                ret = *pair2.second;
                return true;
            }
        }
    }

    return false;
}


const CChainLockSig CChainLocksHandler::GetMostRecentChainLock()
{
    LOCK(cs);
    return mostRecentChainLockShare;
}
const CChainLockSig CChainLocksHandler::GetBestChainLock()
{
    LOCK(cs);
    return bestChainLockWithKnownBlock;
}
const std::map<CQuorumCPtr, CChainLockSigCPtr> CChainLocksHandler::GetBestChainLockShares()
{

    LOCK(cs);
    auto it = bestChainLockShares.find(bestChainLockWithKnownBlock.nHeight);
    if (it == bestChainLockShares.end()) {
        return {};
    }

    return it->second;
}

bool CChainLocksHandler::TryUpdateBestChainLock(const CBlockIndex* pindex)
{
    AssertLockHeld(cs);

    if (pindex == nullptr || pindex->nHeight <= bestChainLockWithKnownBlock.nHeight) {
        return false;
    }

    auto it1 = bestChainLockCandidates.find(pindex->nHeight);
    if (it1 != bestChainLockCandidates.end()) {
        bestChainLockWithKnownBlock = *it1->second;
        bestChainLockBlockIndex = pindex;
        LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- CLSIG from candidates (%s)\n", __func__, bestChainLockWithKnownBlock.ToString());
        return true;
    }

    auto it2 = bestChainLockShares.find(pindex->nHeight);
    if (it2 == bestChainLockShares.end()) {
        return false;
    }
    const auto& consensus = Params().GetConsensus();
    const auto& llmqParams = consensus.llmqs.at(consensus.llmqTypeChainLocks);
    const size_t& threshold = llmqParams.signingActiveQuorumCount / 2 + 1;

    std::vector<CBLSSignature> sigs;
    CChainLockSig clsigAgg;

    for (const auto& pair : it2->second) {
        if (pair.second->blockHash == pindex->GetBlockHash()) {
            assert(std::count(pair.second->signers.begin(), pair.second->signers.end(), true) <= 1);
            sigs.emplace_back(pair.second->sig);
            if (clsigAgg.IsNull()) {
                clsigAgg = *pair.second;
            } else {
                assert(clsigAgg.signers.size() == pair.second->signers.size());
                std::transform(clsigAgg.signers.begin(), clsigAgg.signers.end(), pair.second->signers.begin(), clsigAgg.signers.begin(), std::logical_or<bool>());
            }
            if (sigs.size() >= threshold) {
                // all sigs should be validated already
                clsigAgg.sig = CBLSSignature::AggregateInsecure(sigs);
                bestChainLockWithKnownBlock = clsigAgg;
                bestChainLockBlockIndex = pindex;
                bestChainLockCandidates[clsigAgg.nHeight] = std::make_shared<const CChainLockSig>(clsigAgg);
                LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- CLSIG aggregated (%s)\n", __func__, bestChainLockWithKnownBlock.ToString());
                return true;
            }
        }
    }
    return false;
}

bool CChainLocksHandler::VerifyChainLockShare(const CChainLockSig& clsig, const CBlockIndex* pindexScan, const uint256& idIn, std::pair<int, CQuorumCPtr>& ret)
{
    const auto& consensus = Params().GetConsensus();
    const auto& llmqType = consensus.llmqTypeChainLocks;
    const auto& llmqParams = consensus.llmqs.at(consensus.llmqTypeChainLocks);
    const auto& signingActiveQuorumCount = llmqParams.signingActiveQuorumCount;


    if (clsig.signers.size() != (size_t)signingActiveQuorumCount) {
        return false;
    }

    if (std::count(clsig.signers.begin(), clsig.signers.end(), true) > 1) {
        // too may signers
        return false;
    }
    bool fHaveSigner{std::count(clsig.signers.begin(), clsig.signers.end(), true) > 0};
    std::vector<CQuorumCPtr> quorums_scanned;
    llmq::quorumManager->ScanQuorums(llmqType, pindexScan, signingActiveQuorumCount, quorums_scanned);
    RequestIdStep requestIdStep{clsig.nHeight};

    for (const auto& quorum : quorums_scanned) {
        if (quorum == nullptr) {
            return false;
        }
        uint256 requestId = ::SerializeHash(requestIdStep);
        if ((!idIn.IsNull() && idIn != requestId)) {
            ++requestIdStep.nStep;
            continue;
        }
        if (fHaveSigner && !clsig.signers[requestIdStep.nStep]) {
            ++requestIdStep.nStep;
            continue;
        }
        uint256 signHash = CLLMQUtils::BuildSignHash(llmqType, quorum->qc.quorumHash, requestId, clsig.blockHash);
        LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- CLSIG (%s) requestId=%s, signHash=%s\n",
                __func__, clsig.ToString(), requestId.ToString(), signHash.ToString());

        if (clsig.sig.VerifyInsecure(quorum->qc.quorumPublicKey, signHash)) {
            if (!quorumSigningManager->HasRecoveredSigForId(llmqType, requestId)) {
                // We can reconstruct the CRecoveredSig from the clsig and pass it to the signing manager, which
                // avoids unnecessary double-verification of the signature. We can do this here because we just
                // verified the sig.
                std::shared_ptr<CRecoveredSig> rs = std::make_shared<CRecoveredSig>();
                rs->llmqType = llmqType;
                rs->quorumHash = quorum->qc.quorumHash;
                rs->id = requestId;
                rs->msgHash = clsig.blockHash;
                rs->sig.Set(clsig.sig);
                rs->UpdateHash();
                quorumSigningManager->PushReconstructedRecoveredSig(rs);
            }
            ret = std::make_pair(requestIdStep.nStep, quorum);
            return true;
        }
        if (!idIn.IsNull() || fHaveSigner) {
            return false;
        }
        ++requestIdStep.nStep;
    }
    return false;
}

bool CChainLocksHandler::VerifyAggregatedChainLock(const CChainLockSig& clsig, const CBlockIndex* pindexScan)
{
    const auto& consensus = Params().GetConsensus();
    const auto& llmqType = consensus.llmqTypeChainLocks;
    const auto& llmqParams = consensus.llmqs.at(consensus.llmqTypeChainLocks);
    const auto& signingActiveQuorumCount = llmqParams.signingActiveQuorumCount;

    std::vector<uint256> hashes;
    std::vector<CBLSPublicKey> quorumPublicKeys;

    if (clsig.signers.size() != (size_t)signingActiveQuorumCount) {
        return false;
    }

    if (std::count(clsig.signers.begin(), clsig.signers.end(), true) < (signingActiveQuorumCount / 2 + 1)) {
        // not enough signers
        return false;
    }
    std::vector<CQuorumCPtr> quorums_scanned;
    llmq::quorumManager->ScanQuorums(llmqType, pindexScan, signingActiveQuorumCount, quorums_scanned);
    RequestIdStep requestIdStep{clsig.nHeight};

    for (const auto& quorum : quorums_scanned) {
        if (quorum == nullptr) {
            return false;
        }
        if (!clsig.signers[requestIdStep.nStep]) {
            ++requestIdStep.nStep;
            continue;
        }
        quorumPublicKeys.emplace_back(quorum->qc.quorumPublicKey);
        uint256 requestId = ::SerializeHash(requestIdStep);
        uint256 signHash = CLLMQUtils::BuildSignHash(llmqType, quorum->qc.quorumHash, requestId, clsig.blockHash);
        hashes.emplace_back(signHash);
        LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- CLSIG (%s) requestId=%s, signHash=%s\n",
                __func__, clsig.ToString(), requestId.ToString(), signHash.ToString());

        ++requestIdStep.nStep;
    }
    return clsig.sig.VerifyInsecureAggregated(quorumPublicKeys, hashes);
}

void CChainLocksHandler::ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv)
{
    if (!AreChainLocksEnabled()) {
        return;
    }

    if (strCommand == NetMsgType::CLSIG) {
        CChainLockSig clsig;
        vRecv >> clsig;

        auto hash = ::SerializeHash(clsig);

        ProcessNewChainLock(pfrom->GetId(), clsig, hash);
    }
}

void CChainLocksHandler::ProcessNewChainLock(const NodeId from, llmq::CChainLockSig& clsig, const uint256&hash, const uint256& idIn )
{
    assert((from == -1) ^ idIn.IsNull());
    bool enforced = false;
    if (from != -1) {
        LOCK(cs_main);
        peerman.ReceivedResponse(from, hash);
    }
    bool bReturn = false;
    {
        LOCK(cs);
        if (!seenChainLocks.try_emplace(hash, GetTimeMillis()).second) {
            bReturn = true;
        }

        if (!bestChainLockWithKnownBlock.IsNull() && clsig.nHeight <= bestChainLockWithKnownBlock.nHeight) {
            // no need to process/relay older CLSIGs
            bReturn = true;
        }
    }
    // don't hold lock while relaying which locks nodes/mempool
    if (from != -1) {
        LOCK(cs_main);
        peerman.ForgetTxHash(from, hash);
    }
    if(bReturn) {
        return;
    }
    CInv clsigInv(MSG_CLSIG, hash);
    CBlockIndex* pindexSig{nullptr};
    CBlockIndex* pindexScan{nullptr};
    {
        LOCK(cs_main);
        if (clsig.nHeight > ::ChainActive().Height() + CSigningManager::SIGN_HEIGHT_OFFSET) {
            // too far into the future
            LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- future CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
            return;
        }
        pindexSig = pindexScan = g_chainman.m_blockman.LookupBlockIndex(clsig.blockHash);
        if (pindexScan == nullptr) {
            // we don't know the block/header for this CLSIG yet, try scanning quorums at chain tip
            pindexScan = ::ChainActive().Tip();
        }
        if (pindexSig != nullptr && pindexSig->nHeight != clsig.nHeight) {
            // Should not happen
            LogPrintf("CChainLocksHandler::%s -- height of CLSIG (%s) does not match the expected block's height (%d)\n",
                    __func__, clsig.ToString(), pindexSig->nHeight);
            if (from != -1) {
                peerman.Misbehaving(from, 10, "invalid CLSIG");
            }
            return;
        }
    }
    const auto& consensus = Params().GetConsensus();
    const auto& llmqType = consensus.llmqTypeChainLocks;
    const auto& llmqParams = consensus.llmqs.at(llmqType);
    const auto& signingActiveQuorumCount = llmqParams.signingActiveQuorumCount;
    if (clsig.signers.empty() || std::count(clsig.signers.begin(), clsig.signers.end(), true) <= 1) {
            // A part of a multi-quorum CLSIG signed by a single quorum
            std::pair<int, CQuorumCPtr> ret;
            clsig.signers.resize(signingActiveQuorumCount, false);
            if (!VerifyChainLockShare(clsig, pindexScan, idIn, ret)) {
                LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- invalid CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
                if (from != -1) {
                    LOCK(cs_main);
                    peerman.Misbehaving(from, 10, "invalid CLSIG");
                }
                return;
            }
            CInv clsigAggInv;
            {
                LOCK(cs);
                clsig.signers[ret.first] = true;
                if (std::count(clsig.signers.begin(), clsig.signers.end(), true) > 1) {
                    // this should never happen
                    LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- ERROR in VerifyChainLockShare, CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
                    return;
                }
                auto it = bestChainLockShares.find(clsig.nHeight);
                if (it == bestChainLockShares.end()) {
                    bestChainLockShares[clsig.nHeight].emplace(ret.second, std::make_shared<const CChainLockSig>(clsig));
                } else {
                    it->second.emplace(ret.second, std::make_shared<const CChainLockSig>(clsig));
                }
                mostRecentChainLockShare = clsig;
                if (TryUpdateBestChainLock(pindexSig)) {
                    clsigAggInv = CInv(MSG_CLSIG, ::SerializeHash(bestChainLockWithKnownBlock));
                }
            }
            // Note: do not hold cs while calling RelayInv
            AssertLockNotHeld(cs);
            if (clsigAggInv.type == MSG_CLSIG) {
                // We just created an aggregated CLSIG, relay it
                connman.RelayOtherInv(clsigAggInv);
            } else {
                // Relay partial CLSIGs to full nodes only, SPV wallets should wait for the aggregated CLSIG.
                connman.ForEachNode([&](CNode* pnode) {
                    bool fSPV{false};
                    {
                        LOCK(pnode->m_tx_relay->cs_filter);
                        fSPV = pnode->m_tx_relay->pfilter != nullptr;
                    }
                    if (!fSPV && !pnode->m_masternode_connection) {
                        pnode->PushOtherInventory(clsigInv);
                    }
                });
            }
        } else {
            // An aggregated CLSIG
            if (!VerifyAggregatedChainLock(clsig, pindexScan)) {
                LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- invalid CLSIG (%s), peer=%d\n", __func__, clsig.ToString(), from);
                if (from != -1) {
                    LOCK(cs_main);
                    peerman.Misbehaving(from, 10, "invalid CLSIG");
                }
                return;
            }
            {
                LOCK(cs);
                bestChainLockCandidates[clsig.nHeight] = std::make_shared<const CChainLockSig>(clsig);
                mostRecentChainLockShare = clsig;
                TryUpdateBestChainLock(pindexSig);
            }
            // Note: do not hold cs while calling RelayInv
            AssertLockNotHeld(cs);
            connman.RelayOtherInv(clsigInv);
        }
    
    if (pindexSig == nullptr) {
        // we don't know the block/header for this CLSIG yet, so bail out for now
        // when the block or the header later comes in, we will enforce the correct chain
        return;
    }
    if (bestChainLockBlockIndex == pindexSig) {
        CheckActiveState();
        const CBlockIndex* pindex;
        {       
            LOCK(cs);
            pindex = bestChainLockBlockIndex;
            enforced = isEnforced;
        }
        if(enforced) {
            AssertLockNotHeld(cs);
            ::ChainstateActive().EnforceBestChainLock(pindex);
        }
        LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- processed new CLSIG (%s), peer=%d\n",
              __func__, clsig.ToString(), from);
    }
}

void CChainLocksHandler::AcceptedBlockHeader(const CBlockIndex* pindexNew)
{
    LOCK(cs);

    auto it = bestChainLockCandidates.find(pindexNew->nHeight);
    if (it == bestChainLockCandidates.end()) {
        return;
    }

    LogPrintf("CChainLocksHandler::%s -- block header %s came in late, updating and enforcing\n", __func__, pindexNew->GetBlockHash().ToString());

    // when EnforceBestChainLock is called later, it might end up invalidating other chains but not activating the
    // CLSIG locked chain. This happens when only the header is known but the block is still missing yet. The usual
    // block processing logic will handle this when the block arrives
    TryUpdateBestChainLock(pindexNew);
}

void CChainLocksHandler::UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload)
{
    if(fInitialDownload)
        return;
    CheckActiveState();
    bool enforced;
    const CBlockIndex* pindex;
    {       
        LOCK(cs);
        pindex = bestChainLockBlockIndex;
        enforced = isEnforced;
    }
    
    if(enforced) {
        AssertLockNotHeld(cs);
        ::ChainstateActive().EnforceBestChainLock(pindex);
    }
    TrySignChainTip(pindexNew);
}

void CChainLocksHandler::CheckActiveState()
{
    bool sporkActive = AreChainLocksEnabled();
    {
        LOCK(cs);
        bool oldIsEnforced = isEnforced;
        isEnabled = sporkActive;
        isEnforced = isEnabled;
        if (!oldIsEnforced && isEnforced) {
            
            // ChainLocks got activated just recently, but it's possible that it was already running before, leaving
            // us with some stale values which we should not try to enforce anymore (there probably was a good reason
            // to disable spork19)
            mostRecentChainLockShare = bestChainLockWithKnownBlock = CChainLockSig();
            bestChainLockBlockIndex = nullptr;
        }
    }
}

void CChainLocksHandler::TrySignChainTip(const CBlockIndex* pindex)
{
    Cleanup();

    if (!fMasternodeMode) {
        return;
    }

    if (!masternodeSync.IsBlockchainSynced()) {
        return;
    }
    if (!pindex->pprev) {
        return;
    }
    const uint256 msgHash = pindex->GetBlockHash();
    const int32_t nHeight = pindex->nHeight;
    


    // DIP8 defines a process called "Signing attempts" which should run before the CLSIG is finalized
    // To simplify the initial implementation, we skip this process and directly try to create a CLSIG
    // This will fail when multiple blocks compete, but we accept this for the initial implementation.
    // Later, we'll add the multiple attempts process.

    {
        LOCK(cs);

        if (!isEnabled) {
            return;
        }

        if (nHeight == lastSignedHeight) {
            // already signed this one
            return;
        }

        if (bestChainLockWithKnownBlock.nHeight >= pindex->nHeight) {
            // already got the same CLSIG or a better one
            return;
        }

        if (InternalHasConflictingChainLock(nHeight, msgHash)) {
            // don't sign if another conflicting CLSIG is already present. EnforceBestChainLock will later enforce
            // the correct chain.
            return;
        }
    }

    LogPrint(BCLog::CHAINLOCKS, "CChainLocksHandler::%s -- trying to sign %s, height=%d\n", __func__, msgHash.ToString(), nHeight);
    const auto& consensus = Params().GetConsensus();
    const auto& llmqType = consensus.llmqTypeChainLocks;
    const auto& llmqParams = consensus.llmqs.at(consensus.llmqTypeChainLocks);
    const auto& signingActiveQuorumCount = llmqParams.signingActiveQuorumCount;
    std::vector<CQuorumCPtr> quorums_scanned;
    llmq::quorumManager->ScanQuorums(llmqType, pindex, signingActiveQuorumCount, quorums_scanned);
    RequestIdStep requestIdStep{pindex->nHeight};
    for (const auto& quorum : quorums_scanned) {
        uint256 requestId = ::SerializeHash(requestIdStep);
        if (quorum == nullptr) {
            return;
        }
        if (quorumSigningManager->AsyncSignIfMember(llmqType, requestId, pindex->GetBlockHash(), quorum->qc.quorumHash)) {
            LOCK(cs);
            lastSignedRequestIds.insert(requestId);
            lastSignedMsgHash = pindex->GetBlockHash();
        }
        ++requestIdStep.nStep;
    }
    {
        LOCK(cs);
        if (bestChainLockWithKnownBlock.nHeight >= pindex->nHeight) {
            // might have happened while we didn't hold cs
            return;
        }
        lastSignedHeight = pindex->nHeight;
    }
}


void CChainLocksHandler::HandleNewRecoveredSig(const llmq::CRecoveredSig& recoveredSig)
{
    CChainLockSig clsig;
    {
        LOCK(cs);

        if (!isEnabled) {
            return;
        }

        if (lastSignedRequestIds.count(recoveredSig.id) == 0 || recoveredSig.msgHash != lastSignedMsgHash) {
            // this is not what we signed, so lets not create a CLSIG for it
            return;
        }
        if (bestChainLockWithKnownBlock.nHeight >= lastSignedHeight) {
            // already got the same or a better CLSIG through the CLSIG message
            return;
        }

        clsig.nHeight = lastSignedHeight;
        clsig.blockHash = lastSignedMsgHash;
        clsig.sig = recoveredSig.sig.Get();
        lastSignedRequestIds.erase(recoveredSig.id);
    }
    ProcessNewChainLock(-1, clsig, ::SerializeHash(clsig), recoveredSig.id);
}

bool CChainLocksHandler::HasChainLock(int nHeight, const uint256& blockHash)
{
    LOCK(cs);
    return InternalHasChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!isEnforced) {
        return false;
    }

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash == bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    return pAncestor && pAncestor->GetBlockHash() == blockHash;
}

bool CChainLocksHandler::HasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    LOCK(cs);
    return InternalHasConflictingChainLock(nHeight, blockHash);
}

bool CChainLocksHandler::InternalHasConflictingChainLock(int nHeight, const uint256& blockHash)
{
    AssertLockHeld(cs);

    if (!isEnforced) {
        return false;
    }

    if (!bestChainLockBlockIndex) {
        return false;
    }

    if (nHeight > bestChainLockBlockIndex->nHeight) {
        return false;
    }

    if (nHeight == bestChainLockBlockIndex->nHeight) {
        return blockHash != bestChainLockBlockIndex->GetBlockHash();
    }

    auto pAncestor = bestChainLockBlockIndex->GetAncestor(nHeight);
    assert(pAncestor);
    return pAncestor->GetBlockHash() != blockHash;
}

void CChainLocksHandler::Cleanup()
{
    if (!masternodeSync.IsBlockchainSynced()) {
        return;
    }

    {
        LOCK(cs);
        if (GetTimeMillis() - lastCleanupTime < CLEANUP_INTERVAL) {
            return;
        }
    }

    LOCK(cs);

    for (auto it = seenChainLocks.begin(); it != seenChainLocks.end(); ) {
        if (GetTimeMillis() - it->second >= CLEANUP_SEEN_TIMEOUT) {
            it = seenChainLocks.erase(it);
        } else {
            ++it;
        }
    }

    if (bestChainLockBlockIndex != nullptr) {
        for (auto it = bestChainLockCandidates.begin(); it != bestChainLockCandidates.end(); ) {
            if (it->first == bestChainLockBlockIndex->nHeight) {
                it = bestChainLockCandidates.erase(++it, bestChainLockCandidates.end());
            } else {
                ++it;
            }
        }
        for (auto it = bestChainLockShares.begin(); it != bestChainLockShares.end(); ) {
            if (it->first == bestChainLockBlockIndex->nHeight) {
                it = bestChainLockShares.erase(++it, bestChainLockShares.end());
            } else {
                ++it;
            }
        }
    }
    lastCleanupTime = GetTimeMillis();
}

bool AreChainLocksEnabled()
{
    return sporkManager.IsSporkActive(SPORK_19_CHAINLOCKS_ENABLED);
}

} // namespace llmq
