// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_NODE_MEMPOOL_PERSIST_ARGS_H
#define SYSCOIN_NODE_MEMPOOL_PERSIST_ARGS_H

#include <util/fs.h>

class ArgsManager;

namespace node {

/**
 * Default for -persistmempool, indicating whether the node should attempt to
 * automatically load the mempool on start and save to disk on shutdown
 */
static constexpr bool DEFAULT_PERSIST_MEMPOOL{true};
static constexpr bool DEFAULT_SYNC_MEMPOOL{false};

bool ShouldPersistMempool(const ArgsManager& argsman);
bool ShouldSyncMempool(const ArgsManager& argsman);
fs::path MempoolPath(const ArgsManager& argsman);

} // namespace node

#endif // SYSCOIN_NODE_MEMPOOL_PERSIST_ARGS_H
