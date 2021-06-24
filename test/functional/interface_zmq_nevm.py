#!/usr/bin/env python3
# Copyright (c) 2015-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the ZMQ notification interface."""

from test_framework.address import ADDRESS_BCRT1_UNSPENDABLE
from test_framework.test_framework import SyscoinTestFramework
from test_framework.messages import hash256, CNEVMBlock, CNEVMBlockConnect, uint256_from_str
from test_framework.util import (
    assert_equal,
)
from io import BytesIO
from time import sleep
from threading import Thread
import random
# these would be handlers for the 3 types of calls from Syscoin on Geth
def receive_thread_nevmblock(self, subscriber, publisher):
    while True:
        try:
            self.log.info('receive_thread_nevmblock waiting to receive...')
            subscriber.receive()
            hashStr = hash256(str(random.randint(-0x80000000, 0x7fffffff)).encode())
            hashTopic = uint256_from_str(hashStr)
            nevmBlock = CNEVMBlock(hashTopic, hashStr, hashStr, subscriber.topic)
            publisher.send([subscriber.topic, nevmBlock.serialize()])
        except zmq.ContextTerminated:
            sleep(1)
            break
        except zmq.ZMQError:
            self.log.warning('zmq error, socket closed unexpectedly.')
            sleep(1)
            break

def receive_thread_nevmblockconnect(self, idx, subscriber, publisher):
    while True:
        try:
            self.log.info('receive_thread_nevmblockconnect waiting to receive... idx {}'.format(idx))
            data = subscriber.receive()
            self.log.info('receive_thread_nevmblockconnect received data idx {}'.format(idx))
            evmBlockConnect = CNEVMBlockConnect()
            evmBlockConnect.deserialize(BytesIO(data))
            resBlock = publisher.addBlock(evmBlockConnect)
            res = b""
            if resBlock:
                res = b"connected"
            else:
                res = b"not connected"
            # only send response if Syscoin node is waiting for it
            if evmBlockConnect.waitforresponse == True:
                publisher.send([subscriber.topic, res])
        except zmq.ContextTerminated:
            sleep(1)
            break
        except zmq.ZMQError:
            self.log.warning('zmq error, socket closed unexpectedly.')
            sleep(1)
            break

def receive_thread_nevmblockdisconnect(self, idx, subscriber, publisher):
    while True:
        try:
            self.log.info('receive_thread_nevmblockdisconnect waiting to receive... idx {}'.format(idx))
            data = subscriber.receive()
            self.log.info('receive_thread_nevmblockdisconnect received data idx {}'.format(idx))
            evmBlockConnect = CNEVMBlockConnect()
            evmBlockConnect.deserialize(BytesIO(data))
            resBlock = publisher.deleteBlock(evmBlockConnect)
            res = b""
            if resBlock:
                res = b"disconnected"
            else:
                res = b"not disconnected"
            # only send response if Syscoin node is waiting for it (should always be true for disconnects)
            if evmBlockConnect.waitforresponse == True:
                publisher.send([subscriber.topic, res])
        except zmq.ContextTerminated:
            sleep(1)
            break
        except zmq.ZMQError:
            self.log.warning('zmq error, socket closed unexpectedly.')
            sleep(1)
            break

# Test may be skipped and not have zmq installed
try:
    import zmq
except ImportError:
    pass
# this simulates the Geth node subscriber, subscribing to Syscoin publisher
class ZMQSubscriber:
    def __init__(self, socket, topic):
        self.socket = socket
        self.topic = topic

        self.socket.setsockopt(zmq.SUBSCRIBE, self.topic)

    # Receive message from publisher and verify that topic and sequence match
    def _receive_from_publisher_and_check(self):
        topic, body, seq = self.socket.recv_multipart()
        # Topic should match the subscriber topic.
        assert_equal(topic, self.topic)
        return body

    def receive(self):
        return self._receive_from_publisher_and_check()

    def close(self):
        self.socket.close()

# this simulates the Geth node publisher, publishing back to Syscoin subscriber
class ZMQPublisher:
    def __init__(self, socket):
        self.socket = socket
        self.sysToNEVMBlockMapping = {}
        self.NEVMToSysBlockMapping = {}

    # Send message to subscriber
    def _send_to_publisher_and_check(self, msg_parts):
        self.socket.send_multipart(msg_parts)

    def send(self, msg_parts):
        return self._send_to_publisher_and_check(msg_parts)

    def close(self):
        self.socket.close()

    def addBlock(self, evmBlockConnect):
        # special case if miner is just testing validity of block, sys block hash is 0, we just want to provide message if evm block is valid without updating mappings
        if evmBlockConnect.sysblockhash == 0:
            return True
        # mappings should not already exist, if they do flag the block as invalid
        if self.sysToNEVMBlockMapping.get(evmBlockConnect.sysblockhash) or self.NEVMToSysBlockMapping.get(evmBlockConnect.blockhash):
            return False
        self.sysToNEVMBlockMapping[evmBlockConnect.sysblockhash] = evmBlockConnect
        self.NEVMToSysBlockMapping[evmBlockConnect.blockhash] = evmBlockConnect.sysblockhash
        return True

    def deleteBlock(self, evmBlockConnect):
        # mappings should already exist on disconnect, if they do not flag the disconnect as invalid
        if self.sysToNEVMBlockMapping.get(evmBlockConnect.sysblockhash) == False or self.NEVMToSysBlockMapping.get(evmBlockConnect.blockhash) == False:
            return False
        self.sysToNEVMBlockMapping.pop(evmBlockConnect.sysblockhash, None)
        self.NEVMToSysBlockMapping.pop(evmBlockConnect.blockhash, None)
        return True

    def getLastSYSBlock(self):
        return list(self.NEVMToSysBlockMapping.values())[-1]

    def getLastNEVMBlock(self):
        return self.sysToNEVMBlockMapping[self.getLastSYSBlock()]

class ZMQTest (SyscoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        if self.is_wallet_compiled():
            self.requires_wallet = True
        # This test isn't testing txn relay/timing, so set whitelist on the
        # peers for instant txn relay. This speeds up the test run time 2-3x.
        self.extra_args = [["-whitelist=noban@127.0.0.1"]] * self.num_nodes

    def skip_test_if_missing_module(self):
        self.skip_if_no_py3_zmq()
        self.skip_if_no_syscoind_zmq()

    def run_test(self):
        self.ctx = zmq.Context()
        self.ctxpub = zmq.Context()
        try:
            self.test_basic()
        finally:
            # Destroy the ZMQ context.
            self.log.debug("Destroying ZMQ context")
            self.ctx.destroy(linger=None)
            self.ctxpub.destroy(linger=None)
    # Restart node with the specified zmq notifications enabled, subscribe to
    # all of them and return the corresponding ZMQSubscriber objects.
    def setup_zmq_test(self, services, servicessub, idx, *, recv_timeout=60, sync_blocks=True):
        subscribers = []
        for topic, address in services:
            socket = self.ctx.socket(zmq.SUB)
            subscribers.append(ZMQSubscriber(socket, topic.encode()))
        self.extra_args[idx] = ["-zmqpub%s=%s" % (topic, address) for topic, address in services]
        # publisher on Syscoin can have option to also be a subscriber on another address, related each publisher to a subscriber (in our case we have 3 publisher events that also would subscribe to events on the same topic)
        self.extra_args[idx] += ["-zmqsubpub%s=%s" % (topic, address) for topic, address in servicessub]
        self.restart_node(idx, self.extra_args[idx])
        for i, sub in enumerate(subscribers):
            sub.socket.connect(services[i][1])

        # set subscriber's desired timeout for the test
        for sub in subscribers:
            sub.socket.set(zmq.RCVTIMEO, recv_timeout*1000)

        return subscribers

    # setup the publisher socket to publish events back to Syscoin from Geth simulated publisher
    def setup_zmq_test_pub(self, address):
        socket = self.ctxpub.socket(zmq.PUB)
        publisher = ZMQPublisher(socket)
        publisher.socket.bind(address)
        return publisher

    def test_basic(self):
        addresspub = 'tcp://127.0.0.1:29476'
        addresspub1 = 'tcp://127.0.0.1:29446'
        address = 'tcp://127.0.0.1:29445'
        address1 = 'tcp://127.0.0.1:29443'
        self.log.info("setup publishers...")
        publisher = self.setup_zmq_test_pub(addresspub)
        publisher1 = self.setup_zmq_test_pub(addresspub1)
        self.log.info("setup subscribers...")
        subs = self.setup_zmq_test([(topic, address) for topic in ["nevmconnect", "nevmdisconnect", "nevmblock"]], [(topic, addresspub) for topic in ["nevmconnect", "nevmdisconnect", "nevmblock"]], 0)
        subs1 = self.setup_zmq_test([(topic, address1) for topic in ["nevmconnect", "nevmdisconnect", "nevmblock"]], [(topic, addresspub1) for topic in ["nevmconnect", "nevmdisconnect", "nevmblock"]], 1)
        self.connect_nodes(0, 1)
        self.sync_blocks()

        nevmblockconnect = subs[0]
        nevmblockdisconnect = subs[1]
        nevmblock = subs[2]

        nevmblockconnect1 = subs1[0]
        nevmblockdisconnect1 = subs1[1]
        nevmblock1 = subs1[2]

        num_blocks = 10
        self.log.info("Generate %(n)d blocks (and %(n)d coinbase txes)" % {"n": num_blocks})
        # start the threads to handle pub/sub of SYS/GETH communications
        Thread(target=receive_thread_nevmblock, args=(self, nevmblock,publisher,)).start()
        Thread(target=receive_thread_nevmblockconnect, args=(self, 0, nevmblockconnect,publisher,)).start()
        Thread(target=receive_thread_nevmblockdisconnect, args=(self, 0, nevmblockdisconnect,publisher,)).start()

        Thread(target=receive_thread_nevmblock, args=(self, nevmblock1,publisher1,)).start()
        Thread(target=receive_thread_nevmblockconnect, args=(self, 1, nevmblockconnect1,publisher1,)).start()
        Thread(target=receive_thread_nevmblockdisconnect, args=(self, 1, nevmblockdisconnect1,publisher1,)).start()
        self.nodes[0].generatetoaddress(num_blocks, ADDRESS_BCRT1_UNSPENDABLE)
        self.sync_blocks()
        # test simple disconnect, save best block go back to 205 (first NEVM block) and then reconsider back to tip
        bestblockhash = self.nodes[0].getbestblockhash()
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        # save 205 since when invalidating 206, the best block should be 205
        prevblockhash = self.nodes[0].getblockhash(205)
        blockhash = self.nodes[0].getblockhash(206)
        self.nodes[0].invalidateblock(blockhash)
        self.nodes[1].invalidateblock(blockhash)
        self.sync_blocks()
        # ensure block 205 is the latest on publisher
        assert_equal(int(prevblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        # go back to 210 (tip)
        self.nodes[0].reconsiderblock(blockhash)
        self.nodes[1].reconsiderblock(blockhash)
        self.sync_blocks()
        # check that publisher is on the tip (210) again
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        # restart nodes and check for consistency
        self.log.info('restarting node 0')
        self.restart_node(0, self.extra_args[0])
        self.sync_blocks()
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        self.log.info('restarting node 1')
        self.restart_node(1, self.extra_args[1])
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        # reindex nodes and there should be 6 connect messages from blocks 205-210
        # but SYS node does not wait and validate a response, because publisher would have returned "not connected" yet its still OK because its set and forget on sync/reindex
        self.log.info('reindexing node 0')
        self.extra_args[0] += ["-reindex"]
        self.restart_node(0, self.extra_args[0])
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        self.log.info('reindexing node 1')
        self.extra_args[1] += ["-reindex"]
        self.restart_node(1, self.extra_args[1])
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(int(bestblockhash, 16), publisher.getLastSYSBlock())
        assert_equal(self.nodes[1].getbestblockhash(), bestblockhash)
        assert_equal(publisher1.getLastSYSBlock(), publisher.getLastSYSBlock())
        # reorg test
        self.disconnect_nodes(0, 1)
        self.log.info("Mine 4 blocks on Node 0")
        self.nodes[0].generatetoaddress(4, ADDRESS_BCRT1_UNSPENDABLE)
        # node1 should have 210 because its disconnected and node0 should have 4 more (214)
        assert_equal(self.nodes[1].getblockcount(), 210)
        assert_equal(self.nodes[0].getblockcount(), 214)
        besthash_n0 = self.nodes[0].getbestblockhash()

        self.log.info("Mine competing 6 blocks on Node 1")
        self.nodes[1].generatetoaddress(6, ADDRESS_BCRT1_UNSPENDABLE)
        assert_equal(self.nodes[1].getblockcount(), 216)

        self.log.info("Connect nodes to force a reorg")
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(self.nodes[0].getblockcount(), 216)
        badhash = self.nodes[1].getblockhash(212)

        self.log.info("Invalidate block 2 on node 0 and verify we reorg to node 0's original chain")
        self.nodes[0].invalidateblock(badhash)
        assert_equal(self.nodes[0].getblockcount(), 214)
        assert_equal(self.nodes[0].getbestblockhash(), besthash_n0)

        self.log.info('done')
if __name__ == '__main__':
    ZMQTest().main()