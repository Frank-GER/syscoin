#include "test_syscoin_services.h"
#include "utiltime.h"
#include "util.h"
#include "amount.h"
#include "rpc/server.h"
#include "feedback.h"
#include "cert.h"
#include "alias.h"
#include "wallet/crypter.h"
#include "random.h"
#include "base58.h"
#include "chainparams.h"
#include <memory>
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
static int node1LastBlock=0;
static int node2LastBlock=0;
static int node3LastBlock=0;
static int node4LastBlock=0;
static bool node1Online = false;
static bool node2Online = false;
static bool node3Online = false;

// SYSCOIN testing setup
void StartNodes()
{
	printf("Stopping any test nodes that are running...\n");
	StopNodes();
	node1LastBlock=0;
	node2LastBlock=0;
	node3LastBlock=0;
	node4LastBlock=0;
	if(boost::filesystem::exists(boost::filesystem::system_complete("node1/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node1//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node2/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node2//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node3/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node3//wallet.dat"));
	if(boost::filesystem::exists(boost::filesystem::system_complete("node4/wallet.dat")))
		boost::filesystem::remove(boost::filesystem::system_complete("node4//wallet.dat"));
	StopMainNetNodes();
	printf("Starting 4 nodes in a regtest setup...\n");
	StartNode("node1");
	StartNode("node2");
	StartNode("node3");
	StartNode("node4", true, "-txindex");
	StopNode("node4");
	StartNode("node4", true, "-txindex");
	SelectParams(CBaseChainParams::REGTEST);

}
void StartMainNetNodes()
{
	StopMainNetNodes();
	printf("Starting 1 node in mainnet setup...\n");
	StartNode("mainnet1", false);
}
void StopMainNetNodes()
{
	printf("Stopping mainnet1..\n");
	try{
		CallRPC("mainnet1", "stop");
	}
	catch(const runtime_error& error)
	{
	}	
	printf("Done!\n");
}
void StopNodes()
{
	StopNode("node1");
	StopNode("node2");
	StopNode("node3");
	StopNode("node4");
	printf("Done!\n");
}
void StartNode(const string &dataDir, bool regTest, const string& extraArgs)
{
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/wallet.dat")))
	{
		if (!boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest")))
			boost::filesystem::create_directory(boost::filesystem::system_complete(dataDir + "/regtest"));
		boost::filesystem::copy_file(boost::filesystem::system_complete(dataDir + "/wallet.dat"),boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat"),boost::filesystem::copy_option::overwrite_if_exists);
		boost::filesystem::remove(boost::filesystem::system_complete(dataDir + "/wallet.dat"));
	}
    boost::filesystem::path fpath = boost::filesystem::system_complete("../syscoind");
	string nodePath = fpath.string() + string(" -datadir=") + dataDir;
	if(regTest)
		nodePath += string(" -regtest -debug -addressindex");
	if(!extraArgs.empty())
		nodePath += string(" ") + extraArgs;
	if (!boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/db")))
		boost::filesystem::create_directory(boost::filesystem::system_complete(dataDir + "/db"));
	if (dataDir == "node1") {
		printf("Launching mongod on port 27017...\n");
		string path = string("mongod --port=27017 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
	else if (dataDir == "node2") {
		printf("Launching mongod on port 27018...\n");
		string path = string("mongod --port=27018 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
	else if (dataDir == "node3") {
		printf("Launching mongod on port 27019...\n");
		string path = string("mongod --port=27019 --quiet --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
		path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
		boost::thread t(runCommand, path);
	}
    boost::thread t(runCommand, nodePath);
	printf("Launching %s, waiting 1 second before trying to ping...\n", nodePath.c_str());
	MilliSleep(1000);
	UniValue r;
	while (1)
	{
		try{
			printf("Calling getinfo!\n");
			r = CallRPC(dataDir, "getinfo", regTest);
			if(dataDir == "node1")
			{
				if(node1LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node1LastBlock);
					MilliSleep(500);
					continue;
				}
				node1Online = true;
				node1LastBlock = 0;
			}
			else if(dataDir == "node2")
			{
				if(node2LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node2LastBlock);
					MilliSleep(500);
					continue;
				}
				node2Online = true;
				node2LastBlock = 0;
			}
			else if(dataDir == "node3")
			{
				if(node3LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node3LastBlock);
					MilliSleep(500);
					continue;
				}
				node3Online = true;
				node3LastBlock = 0;
			}
			else if(dataDir == "node4")
			{
				if(node4LastBlock > find_value(r.get_obj(), "blocks").get_int())
				{
					printf("Waiting for %s to catch up, current block number %d vs total blocks %d...\n", dataDir.c_str(), find_value(r.get_obj(), "blocks").get_int(), node4LastBlock);
					MilliSleep(500);
					continue;
				}
				node4LastBlock = 0;
			}
		}
		catch(const runtime_error& error)
		{
			printf("Waiting for %s to come online, trying again in 1 second...\n", dataDir.c_str());
			MilliSleep(1000);
			continue;
		}
		break;
	}
	printf("Done!\n");
}

void StopNode (const string &dataDir) {
	printf("Stopping %s..\n", dataDir.c_str());
	UniValue r;
	try{
		r = CallRPC(dataDir, "getinfo");
		if(r.isObject())
		{
			if(dataDir == "node1")
				node1LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node2")
				node2LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node3")
				node3LastBlock = find_value(r.get_obj(), "blocks").get_int();
			else if(dataDir == "node4")
				node4LastBlock = find_value(r.get_obj(), "blocks").get_int();
		}
	}
	catch(const runtime_error& error)
	{
	}
	try{
		CallRPC(dataDir, "stop");
	}
	catch(const runtime_error& error)
	{
	}
	while (1)
	{
		try {
			MilliSleep(1000);
			CallRPC(dataDir, "getinfo");
		}
		catch (const runtime_error& error)
		{
			break;
		}
	}
	try {
		MilliSleep(1000);
		if (dataDir == "node1" && node1Online) {
			printf("Stopping mongod on port 27017...\n");
			string path = string("mongod --port=27017 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		else if (dataDir == "node2" && node2Online) {
			printf("Stopping mongod on port 27018...\n");
			string path = string("mongod --port=27018 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		else if (dataDir == "node3" && node3Online) {
			printf("Stopping mongod on port 27019...\n");
			string path = string("mongod --port=27019 --quiet --shutdown --dbpath=") + boost::filesystem::system_complete(dataDir + string("/db")).string();
			path += string(" --logpath=") + boost::filesystem::system_complete(dataDir + string("/db/log")).string();
			boost::thread t(runCommand, path);
		}
		if (dataDir == "node1")
			node1Online = false;
		else if (dataDir == "node2")
			node2Online = false;
		else if (dataDir == "node3")
			node3Online = false;
	}
	catch (const runtime_error& error)
	{
		
	}
	MilliSleep(1000);
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat")))
		boost::filesystem::copy_file(boost::filesystem::system_complete(dataDir + "/regtest/wallet.dat"),boost::filesystem::system_complete(dataDir + "/wallet.dat"),boost::filesystem::copy_option::overwrite_if_exists);
	if(boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/regtest")))
		boost::filesystem::remove_all(boost::filesystem::system_complete(dataDir + "/regtest"));
	try {
		if (boost::filesystem::exists(boost::filesystem::system_complete(dataDir + "/db")))
			boost::filesystem::remove_all(boost::filesystem::system_complete(dataDir + "/db"));
	}
	catch (...) {
	}
}

UniValue CallRPC(const string &dataDir, const string& commandWithArgs, bool regTest, bool readJson)
{
	UniValue val;
	boost::filesystem::path fpath = boost::filesystem::system_complete("../syscoin-cli");
	string path = fpath.string() + string(" -datadir=") + dataDir;
	if(regTest)
		path += string(" -regtest ");
	else
		path += " ";
	path += commandWithArgs;
	string rawJson = CallExternal(path);
	if(readJson)
	{
		val.read(rawJson);
		if(val.isNull())
			throw runtime_error("Could not parse rpc results");
	}
	return val;
}
int fsize(FILE *fp){
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); //go back to where we were
    return sz;
}
void safe_fclose(FILE* file)
{
      if (file)
         BOOST_VERIFY(0 == fclose(file));
	if(boost::filesystem::exists("cmdoutput.log"))
		boost::filesystem::remove("cmdoutput.log");
}
int runSysCommand(const std::string& strCommand)
{
    int nErr = ::system(strCommand.c_str());
	return nErr;
}
std::string CallExternal(std::string &cmd)
{
	cmd += " > cmdoutput.log || true";
	if(runSysCommand(cmd))
		return string("ERROR");
    boost::shared_ptr<FILE> pipe(fopen("cmdoutput.log", "r"), safe_fclose);
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
	if(fsize(pipe.get()) > 0)
	{
		while (!feof(pipe.get())) {
			if (fgets(buffer, 128, pipe.get()) != NULL)
				result += buffer;
		}
	}
    return result;
}
void GenerateMainNetBlocks(int nBlocks, const string& node)
{
	int targetHeight, newHeight;
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	targetHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
	newHeight = 0;
	const string &sBlocks = strprintf("%d",nBlocks);

	while(newHeight < targetHeight)
	{
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks));
	  MilliSleep(1000);
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	  newHeight = find_value(r.get_obj(), "blocks").get_int();
	  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
	  CAmount balance = AmountFromValue(find_value(r.get_obj(), "balance"));
	  printf("Current block height %d, Target block height %d, balance %f\n", newHeight, targetHeight, ValueFromAmount(balance).get_real()); 
	}
	BOOST_CHECK(newHeight >= targetHeight);
}
// generate n Blocks, with up to 10 seconds relay time buffer for other nodes to get the blocks.
// may fail if your network is slow or you try to generate too many blocks such that can't relay within 10 seconds
void GenerateBlocks(int nBlocks, const string& node)
{
  int height, newHeight, timeoutCounter;
  UniValue r;
  string otherNode1, otherNode2;
  GetOtherNodes(node, otherNode1, otherNode2);
  try
  {
	r = CallRPC(node, "getinfo");
  }
  catch(const runtime_error &e)
  {
	return;
  }
  newHeight = find_value(r.get_obj(), "blocks").get_int() + nBlocks;
  const string &sBlocks = strprintf("%d",nBlocks);
  BOOST_CHECK_NO_THROW(r = CallRPC(node, "generate " + sBlocks));
  BOOST_CHECK_NO_THROW(r = CallRPC(node, "getinfo"));
  height = find_value(r.get_obj(), "blocks").get_int();
  BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
  while(!otherNode1.empty() && height < newHeight)
  {
	  MilliSleep(100);
	  try
	  {
		r = CallRPC(otherNode1, "getinfo");
	  }
	  catch(const runtime_error &e)
	  {
		r = NullUniValue;
	  }
	  if(!r.isObject())
	  {
		 height = newHeight;
		 break;
	  }
	  height = find_value(r.get_obj(), "blocks").get_int();
	  timeoutCounter++;
	  if (timeoutCounter > 300) {
		  printf("Error: Timeout on getinfo for %s, height %d vs newHeight %d!\n", otherNode1.c_str(), height, newHeight);
		  break;
	  }
  }
  if(!otherNode1.empty())
	BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
  while(!otherNode2.empty() &&height < newHeight)
  {
	  MilliSleep(100);
	  try
	  {
		r = CallRPC(otherNode2, "getinfo");
	  }
	  catch(const runtime_error &e)
	  {
		r = NullUniValue;
	  }
	  if(!r.isObject())
	  {
		 height = newHeight;
		 break;
	  }
	  height = find_value(r.get_obj(), "blocks").get_int();
	  timeoutCounter++;
	  if (timeoutCounter > 300) {
		printf("Error: Timeout on getinfo for %s, height %d vs newHeight %d!\n", otherNode2.c_str(), height, newHeight);
		break;
	  }
  }
  if(!otherNode2.empty())
	BOOST_CHECK(height >= newHeight);
  height = 0;
  timeoutCounter = 0;
}
void CreateSysRatesIfNotExist()
{
	string data = "\"{\\\"rates\\\":[{\\\"currency\\\":\\\"USD\\\",\\\"rate\\\":2690.1,\\\"precision\\\":2},{\\\"currency\\\":\\\"EUR\\\",\\\"rate\\\":2695.2,\\\"precision\\\":2},{\\\"currency\\\":\\\"GBP\\\",\\\"rate\\\":2697.3,\\\"precision\\\":2},{\\\"currency\\\":\\\"CAD\\\",\\\"rate\\\":2698.0,\\\"precision\\\":2},{\\\"currency\\\":\\\"BTC\\\",\\\"rate\\\":100000.0,\\\"fee\\\":75,\\\"escrowfee\\\":0.01,\\\"precision\\\":8},{\\\"currency\\\":\\\"ZEC\\\",\\\"rate\\\":1000000.0,\\\"fee\\\":50,\\\"escrowfee\\\":0.01,\\\"precision\\\":8},{\\\"currency\\\":\\\"SYS\\\",\\\"rate\\\":1.0,\\\"fee\\\":1000,\\\"escrowfee\\\":0.005,\\\"precision\\\":2}]}\"";
	// should get runtime error if doesnt exist
	try{
		UniValue r = CallRPC("node1", "aliasinfo sysrates.peg");
		if(r.isObject())
		{
			string hex_str = AliasUpdate("node1", "sysrates.peg", data, "priv");
			BOOST_CHECK(hex_str.empty());
		}
		else
			AliasNew("node1", "sysrates.peg", data);
	}
	catch(const runtime_error& err)
	{
		GenerateBlocks(200, "node1");	
		GenerateBlocks(200, "node2");	
		GenerateBlocks(200, "node3");
		try
		{
			AliasNew("node1", "sysrates.peg", data);
		}
		catch(const runtime_error &e)
		{
			throw runtime_error(e.what());
		}
	}
}
void ExpireAlias(const string& alias)
{
	UniValue r;
	int64_t expiryTime;
	try
	{
		r = CallRPC("node1", "aliasinfo " + alias);
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	if(r.isObject())
	{
		expiryTime = find_value(r.get_obj(), "expires_on").get_int64();
		string cmd = strprintf("setmocktime %lld", expiryTime);
		BOOST_CHECK_NO_THROW(CallRPC("node1", cmd, true, false));
	}
	try
	{
		r = CallRPC("node2", "getinfo");
		if(expiryTime > 0)
		{
			string cmd = strprintf("setmocktime %lld", expiryTime);
			BOOST_CHECK_NO_THROW(CallRPC("node2", cmd, true, false));
		}
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	try
	{
		r = CallRPC("node3", "getinfo");
		if(expiryTime > 0)
		{
			string cmd = strprintf("setmocktime %lld", expiryTime);
			BOOST_CHECK_NO_THROW(CallRPC("node3", cmd, true, false));
		}
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	GenerateBlocks(5);
	// ensure alias is expired
	try
	{
		r = CallRPC("node1", "aliasinfo " + alias);
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	if(r.isObject())
	{
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);
	}
	try
	{
		r = CallRPC("node2", "aliasinfo " + alias);
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	if(r.isObject())
	{
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);	
	}
	try
	{
		r = CallRPC("node3", "aliasinfo " + alias);
	}
	catch(const runtime_error &e)
	{
		r = NullUniValue;
	}
	if(r.isObject())
	{
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), true);
	}
}
void GetOtherNodes(const string& node, string& otherNode1, string& otherNode2)
{
	otherNode1 = "";
	otherNode2 = "";
	if(node == "node1")
	{
		if(node2Online)
			otherNode1 = "node2";
		if(node3Online)
			otherNode2 = "node3";
	}
	else if(node == "node2")
	{
		if(node1Online)
			otherNode1 = "node1";
		if(node3Online)
			otherNode2 = "node3";
	}
	else if(node == "node3")
	{
		if(node1Online)
			otherNode1 = "node1";
		if(node2Online)
			otherNode2 = "node2";
	}

}
string AliasNew(const string& node, const string& aliasname, const string& pubdata, string privdata, string witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);

	CKey privEncryptionKey;
	privEncryptionKey.MakeNewKey(true);
	CPubKey pubEncryptionKey = privEncryptionKey.GetPubKey();
	vector<unsigned char> vchPrivEncryptionKey(privEncryptionKey.begin(), privEncryptionKey.end());
	vector<unsigned char> vchPubEncryptionKey(pubEncryptionKey.begin(), pubEncryptionKey.end());
	
	string strCipherPrivateData = privdata;

	vector<unsigned char> vchPubKey;
	CKey privKey;
	vector<unsigned char> vchPasswordSalt;

	privKey.MakeNewKey(true);
	CPubKey pubKey = privKey.GetPubKey();
	vchPubKey = vector<unsigned char>(pubKey.begin(), pubKey.end());
	CSyscoinAddress aliasAddress(pubKey.GetID());
	vector<unsigned char> vchPrivKey(privKey.begin(), privKey.end());
	BOOST_CHECK(privKey.IsValid());
	BOOST_CHECK(privEncryptionKey.IsValid());
	BOOST_CHECK(pubKey.IsFullyValid());
	BOOST_CHECK_NO_THROW(CallRPC(node, "importprivkey " + CSyscoinSecret(privKey).ToString() + " \"\" false", true, false));
	BOOST_CHECK_NO_THROW(CallRPC(node, "importprivkey " + CSyscoinSecret(privEncryptionKey).ToString() + " \"\" false", true, false));

	string strPrivateHex = strCipherPrivateData;
	if(strCipherPrivateData.empty())
		strPrivateHex = "\"\"";
	string strEncryptionPrivateKeyHex = HexStr(vchPrivEncryptionKey);
	string expires = "\"\"";
	string aliases = "\"\"";
	string acceptTransfers = "\"\"";
	string expireTime = "\"\"";
	string salt = "\"\"";

	UniValue r;
	// registration
	BOOST_CHECK_NO_THROW(CallRPC(node, "aliasnew sysrates.peg " + aliasname + " " + pubdata + " " + strPrivateHex + " " + acceptTransfers +  " " + expireTime + " " + aliasAddress.ToString() + " " + salt + " " + strEncryptionPrivateKeyHex + " " + HexStr(vchPubEncryptionKey) + " " + witness));
	GenerateBlocks(5, node);
	// activation
	BOOST_CHECK_NO_THROW(CallRPC(node, "aliasnew sysrates.peg " + aliasname + " \"\" \"\""));
	GenerateBlocks(5, node);
	BOOST_CHECK_THROW(CallRPC(node, "sendtoaddress " + aliasname + " 10"), runtime_error);
	GenerateBlocks(5, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK(balanceAfter >= 10*COIN);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
	if(aliasname != "sysrates.peg")
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "privatevalue").get_str() , privdata == "\"\""? "": privdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "aliasinfo " + aliasname));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		if(aliasname != "sysrates.peg")
			BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "aliasinfo " + aliasname));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		if(aliasname != "sysrates.peg")
			BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str(), pubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	}
	return aliasAddress.ToString();
}
string AliasTransfer(const string& node, const string& aliasname, const string& tonode, const string& pubdata, const string& privdata,const string& witness)
{
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	string oldvalue = find_value(r.get_obj(), "publicvalue").get_str();
	string oldprivatevalue = find_value(r.get_obj(), "privatevalue").get_str();

	string encryptionkey = find_value(r.get_obj(), "encryption_publickey").get_str();
	string encryptionprivkey = find_value(r.get_obj(), "encryption_privatekey").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceBefore = AmountFromValue(find_value(r.get_obj(), "balance"));

	CKey privKey, encryptionPrivKey;
	privKey.MakeNewKey(true);
	CPubKey pubKey = privKey.GetPubKey();
	vector<unsigned char> vchPubKey(pubKey.begin(), pubKey.end());
	CSyscoinAddress aliasAddress(pubKey.GetID());
	vector<unsigned char> vchPrivKey(privKey.begin(), privKey.end());
	BOOST_CHECK(privKey.IsValid());
	BOOST_CHECK(pubKey.IsFullyValid());
	BOOST_CHECK_NO_THROW(CallRPC(tonode, "importprivkey " + CSyscoinSecret(privKey).ToString() + " \"\" false", true, false));	

	string strPrivateHex = privdata;
	if(privdata.empty())
		strPrivateHex = "\"\"";

	string acceptTransfers = "\"\"";
	string expires = "\"\"";
	string address = aliasAddress.ToString();
	string password = "\"\"";
	string passwordsalt = "\"\"";
	string encryptionpubkey = "\"\"";

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasupdate " + aliasname + " " + pubdata + " " + strPrivateHex + " " + address + " " + acceptTransfers + " " + expires + " " + passwordsalt + " " + encryptionpubkey + " " + encryptionpubkey + " " + witness));
	const UniValue& resArray = r.get_array();
	if(resArray.size() > 1)
	{
		const UniValue& complete_value = resArray[1];
		bool bComplete = false;
		if (complete_value.isStr())
			bComplete = complete_value.get_str() == "true";
		if(!bComplete)
		{
			string hex_str = resArray[0].get_str();	
			return hex_str;
		}
	}	
	GenerateBlocks(5, tonode);
	GenerateBlocks(5, node);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));
	BOOST_CHECK(balanceAfter >= (balanceBefore-COIN));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldvalue);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());

	// check xferred right person and data changed
	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "aliasbalance " + aliasname));
	balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "aliasinfo " + aliasname));
	BOOST_CHECK(balanceAfter >= (balanceBefore-COIN));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "privatevalue").get_str() , privdata != "\"\""? privdata: oldprivatevalue);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldvalue);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , aliasAddress.ToString());
	return "";
}
string AliasUpdate(const string& node, const string& aliasname, const string& pubdata, const string& privdata, string addressStr, string witness)
{
	string addressStr1 = addressStr;
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));

	string oldvalue = find_value(r.get_obj(), "publicvalue").get_str();
	string oldprivatevalue = find_value(r.get_obj(), "privatevalue").get_str();
	string oldAddressStr = find_value(r.get_obj(), "address").get_str();
	string encryptionkey = find_value(r.get_obj(), "encryption_publickey").get_str();
	string encryptionprivkey = find_value(r.get_obj(), "encryption_privatekey").get_str();
	
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceBefore = AmountFromValue(find_value(r.get_obj(), "balance"));

	string strCipherPrivateData = privdata;
	


	string strPasswordSalt = "\"\"";
	string strPrivateHex = strCipherPrivateData;
	if(strCipherPrivateData.empty())
		strPrivateHex = "\"\"";
	string acceptTransfers = "\"\"";
	string expires = "\"\"";
	string encryptionpubkey = "\"\"";
	// "aliasupdate <aliasname> [public value] [private value] [address] [accept_transfers=true] [expire_timestamp] [password_salt] [encryption_privatekey] [encryption_publickey] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasupdate " + aliasname + " " + pubdata + " " + strPrivateHex +  " " + addressStr + " " + acceptTransfers + " " + expires + " " + strPasswordSalt + " " + encryptionpubkey + " " + encryptionpubkey + " " + witness));
	const UniValue& resArray = r.get_array();
	if(resArray.size() > 1)
	{
		const UniValue& complete_value = resArray[1];
		bool bComplete = false;
		if (complete_value.isStr())
			bComplete = complete_value.get_str() == "true";
		if(!bComplete)
		{
			string hex_str = resArray[0].get_str();	
			return hex_str;
		}
	}
	GenerateBlocks(5, node);
	GenerateBlocks(5, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + aliasname));
	CAmount balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + aliasname));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , addressStr != "\"\""? addressStr: oldAddressStr);
	
	BOOST_CHECK(abs(balanceBefore-balanceAfter) < COIN);
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);

	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "privatevalue").get_str() , privdata != "\"\""? privdata: oldprivatevalue);
	if(aliasname != "sysrates.peg")
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldvalue);
	

	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_privatekey").get_str() , encryptionprivkey);


	
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "aliasbalance " + aliasname));
		balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "aliasinfo " + aliasname));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , addressStr != "\"\""? addressStr: oldAddressStr);
		
		BOOST_CHECK(abs(balanceBefore-balanceAfter) < COIN);	
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);

		
		if(aliasname != "sysrates.peg")
			BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldvalue);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);

	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "aliasbalance " + aliasname));
		balanceAfter = AmountFromValue(find_value(r.get_obj(), "balance"));
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "aliasinfo " + aliasname));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "address").get_str() , addressStr != "\"\""? addressStr: oldAddressStr);
		
		BOOST_CHECK(abs(balanceBefore-balanceAfter) < COIN);
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == aliasname);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "expired").get_bool(), false);

		if(aliasname != "sysrates.peg")
			BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldvalue);
		
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "encryption_publickey").get_str() , encryptionkey);
	}
	return "";

}
bool AliasFilter(const string& node, const string& alias)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + alias + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery alias " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}
bool OfferFilter(const string& node, const string& offer)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + offer + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery offer " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}
bool CertFilter(const string& node, const string& cert)
{
	UniValue r;
	int64_t currentTime = GetTime();
	string query = "\"{\\\"_id\\\":\\\"" + cert + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery cert " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}
bool EscrowFilter(const string& node, const string& escrow)
{
	UniValue r;
	string query = "\"{\\\"_id\\\":\\\"" + escrow + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery escrow " + query));
	const UniValue &arr = r.get_array();
	return !arr.empty();
}
const string CertNew(const string& node, const string& alias, const string& title, const string& privdata, const string& pubdata, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + alias));

	string strCipherPrivateData = "";
	if(privdata != "\"\"")
	{
		strCipherPrivateData = privdata;
	}
	if (strCipherPrivateData.empty())
		strCipherPrivateData = "\"\"";



	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certnew " + alias + " " + title + " " + pubdata + " " + strCipherPrivateData + " certificates " + witness));
	const UniValue &arr = r.get_array();
	string guid = arr[1].get_str();
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == alias);
	BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	if(privdata != "\"\"")
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "privatevalue").get_str() , privdata);
	BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	}
	return guid;
}
void CertUpdate(const string& node, const string& guid, const string& title, const string& privdata, const string& pubdata, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	string oldalias = find_value(r.get_obj(), "alias").get_str();
	string olddata = find_value(r.get_obj(), "privatevalue").get_str();
	string oldpubdata = find_value(r.get_obj(), "publicvalue").get_str();
	string oldtitle = find_value(r.get_obj(), "title").get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + oldalias));
	string strCipherPrivateData = "";
	// regenerate pub/priv encryption keypair on every update of pvt data
	if(privdata != "\"\"")
	{
		strCipherPrivateData = privdata;

	}

	if(strCipherPrivateData.empty())
		strCipherPrivateData = "\"\"";



	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certupdate " + guid + " " + title + " " + pubdata + " " + strCipherPrivateData + " certificates " + witness));
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "privatevalue").get_str() , privdata != "\"\""? privdata: olddata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldpubdata);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);

	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);

	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "certinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == oldalias);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "publicvalue").get_str() , pubdata != "\"\""? pubdata: oldpubdata);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);

	}
}
void CertTransfer(const string& node, const string &tonode, const string& guid, const string& toalias, const string& witness)
{
	UniValue r;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	string privdata = find_value(r.get_obj(), "privatevalue").get_str();
	string pubdata = find_value(r.get_obj(), "publicvalue").get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + toalias));

	string strCipherPrivateData = "";
	if(privdata != "\"\"")
	{
		strCipherPrivateData = privdata;
		
	}

	if(strCipherPrivateData.empty())
		strCipherPrivateData = "\"\"";


	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certtransfer " + guid + " " + toalias + " " + pubdata + " " + strCipherPrivateData + " " + witness));
	GenerateBlocks(5, node);
	GenerateBlocks(5, tonode);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "certinfo " + guid));
	BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);

	BOOST_CHECK_NO_THROW(r = CallRPC(tonode, "certinfo " + guid));
	BOOST_CHECK(find_value(r.get_obj(), "alias").get_str() == toalias);
	BOOST_CHECK(find_value(r.get_obj(), "privatevalue").get_str() == privdata);
	BOOST_CHECK(find_value(r.get_obj(), "publicvalue").get_str() == pubdata);

}
const string OfferLink(const string& node, const string& alias, const string& guid, const string& commissionStr, const string& newdetails, const string &witness)
{
	UniValue r;
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + guid));
	const string &olddetails = find_value(r.get_obj(), "description").get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerlink " + alias + " " + guid + " " + commissionStr + " " + newdetails + " " + witness));
	const UniValue &arr = r.get_array();
	string linkedguid = arr[1].get_str();
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + linkedguid));
	if(!newdetails.empty())
		BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == newdetails);
	else
		BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == olddetails);
	int commission = atoi(commissionStr.c_str());
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == linkedguid);
	BOOST_CHECK(find_value(r.get_obj(), "offerlink_guid").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "commission").get_int() == commission);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "offerinfo " + linkedguid));
		if(!newdetails.empty())
			BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == newdetails);
		else
			BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == olddetails);
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == linkedguid);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "offerinfo " + linkedguid));
		if(!newdetails.empty())
			BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == newdetails);
		else
			BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == olddetails);
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == linkedguid);
	}
	return linkedguid;
}
const string OfferNew(const string& node, const string& aliasname, const string& category, const string& title, const string& qtyStr, const string& price, const string& description, const string& currency, const string& certguid, const string& paymentoptions, const string& witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	CreateSysRatesIfNotExist();
	string coinoffer = "\"\"";
	string pvt = "\"\"";
	string units = "\"\"";
	int qty = atoi(qtyStr.c_str());
	UniValue r;
	//						"offernew <alias> <category> <title> <quantity> <price> <description> <currency> [cert. guid] [payment options=SYS] [private=false] [units] [coinoffer=false] [witness]"
	string offercreatestr = "offernew " + aliasname + " " + category + " " + title + " " + qtyStr + " " + price + " " + description + " " + currency  + " " + certguid + " " + paymentoptions + " " + pvt + " " + units + " " + coinoffer + " " + witness;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, offercreatestr));
	const UniValue &arr = r.get_array();
	string guid = arr[1].get_str();
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + guid));

	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	if(certguid != "\"\"")
		BOOST_CHECK(find_value(r.get_obj(), "cert").get_str() == certguid);

	BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
	BOOST_CHECK(find_value(r.get_obj(), "currency").get_str() == currency);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price);
	BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
	BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == description);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "offerinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		if(certguid != "\"\"")
			BOOST_CHECK(find_value(r.get_obj(), "cert").get_str() == certguid);

		BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
		BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == description);
		BOOST_CHECK(find_value(r.get_obj(), "currency").get_str() == currency);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "offerinfo " + guid));
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		if(certguid != "\"\"")
			BOOST_CHECK(find_value(r.get_obj(), "cert").get_str() == certguid);
		BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
		BOOST_CHECK(find_value(r.get_obj(), "title").get_str() == title);
		BOOST_CHECK(find_value(r.get_obj(), "description").get_str() == description);
		BOOST_CHECK(find_value(r.get_obj(), "currency").get_str() == currency);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price);
	}
	return guid;
}

void OfferUpdate(const string& node, const string& aliasname, const string& offerguid, const string& category, const string& title, const string& qtyStr, const string& price, const string& description, const string& currency, const string &isprivateStr, const string& certguid, const string& commissionStr, const string& paymentoptions, const string& witness) {
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	
	CreateSysRatesIfNotExist();
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offerguid));
	int oldqty = find_value(r.get_obj(), "quantity").get_int();
	int qty = atoi(qtyStr.c_str());
	string oldprice = find_value(r.get_obj(), "price").get_str();
	string oldcurrency = find_value(r.get_obj(), "currency").get_str();
	bool oldprivate = find_value(r.get_obj(), "private").get_bool();
	bool isprivate = isprivateStr == "true";
	string oldcert = find_value(r.get_obj(), "cert").get_str();
	int oldcommission = find_value(r.get_obj(), "commission").get_int();
	int commission = atoi(commissionStr.c_str());
	string oldpaymentoptions = find_value(r.get_obj(), "paymentoptions_display").get_str();
	string olddescription = find_value(r.get_obj(), "description").get_str();
	string oldtitle = find_value(r.get_obj(), "title").get_str();
	string oldcategory = find_value(r.get_obj(), "category").get_str();
	//						"offerupdate <alias> <guid> [category] [title] [quantity] [price] [description] [currency] [private=false] [cert. guid] [commission] [paymentOptions] [witness]"
	string offerupdatestr = "offerupdate " + aliasname + " " + offerguid + " " + category + " " + title + " " + qtyStr + " " + price + " " + description + " " + currency + " " + isprivateStr + " " + certguid + " " +  commissionStr + " " + paymentoptions + " " + witness;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, offerupdatestr));
	GenerateBlocks(10, node);


		
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offerguid));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "_id").get_str() , offerguid);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "cert").get_str() , certguid != "\"\""? certguid: oldcert);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "quantity").get_int() , qtyStr != "\"\""? qty: oldqty);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "currency").get_str() , currency != "\"\""? currency: oldcurrency);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price != "\"\""? price: oldprice);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "commission").get_int() , commissionStr != "\"\""? commission: oldcommission);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "paymentoptions_display").get_str() , paymentoptions != "\"\""? paymentoptions: oldpaymentoptions);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "private").get_bool() , isprivateStr != "\"\""? isprivate: oldprivate);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "description").get_str(), description != "\"\""? description: olddescription);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "category").get_str(), category != "\"\""? category: oldcategory);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "offerinfo " + offerguid));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "_id").get_str() , offerguid);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "cert").get_str() , certguid != "\"\""? certguid: oldcert);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "quantity").get_int() , qtyStr != "\"\""? qty: oldqty);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "currency").get_str() , currency != "\"\""? currency: oldcurrency);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price != "\"\""? price: oldprice);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "commission").get_int() , commissionStr != "\"\""? commission: oldcommission);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "paymentoptions_display").get_str() , paymentoptions != "\"\""? paymentoptions: oldpaymentoptions);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "private").get_bool() , isprivateStr != "\"\""? isprivate: oldprivate);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "description").get_str(), description != "\"\""? description: olddescription);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "category").get_str(), category != "\"\""? category: oldcategory);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "offerinfo " + offerguid));
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "_id").get_str() , offerguid);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "cert").get_str() , certguid != "\"\""? certguid: oldcert);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "quantity").get_int() , qtyStr != "\"\""? qty: oldqty);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "currency").get_str() , currency != "\"\""? currency: oldcurrency);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "price").get_str(), price != "\"\""? price: oldprice);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "commission").get_int() , commissionStr != "\"\""? commission: oldcommission);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "paymentoptions_display").get_str() , paymentoptions != "\"\""? paymentoptions: oldpaymentoptions);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "private").get_bool() , isprivateStr != "\"\""? isprivate: oldprivate);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "description").get_str(), description != "\"\""? description: olddescription);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "title").get_str(), title != "\"\""? title: oldtitle);
		BOOST_CHECK_EQUAL(find_value(r.get_obj(), "category").get_str(), category != "\"\""? category: oldcategory);
	}
}

void EscrowFeedback(const string& node, const string& role, const string& escrowguid, const string& feedback, const string& rating,  char user, const string& witness) {

	UniValue r, ret;
	string escrowfeedbackstr = "escrowfeedback " + escrowguid + " " + role + " " + feedback + " " + rating  + witness;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, escrowfeedbackstr));
	const UniValue &arr = r.get_array();
	string escrowTxid = arr[0].get_str();

	GenerateBlocks(10, node);
	
	r = FindFeedback(node, escrowTxid);
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == escrowguid + user);
	BOOST_CHECK(find_value(r.get_obj(), "escrow").get_str() == escrowguid);
	BOOST_CHECK(find_value(r.get_obj(), "txid").get_str() == escrowTxid);
	BOOST_CHECK(find_value(r.get_obj(), "rating").get_int() == atoi(rating.c_str()));
	BOOST_CHECK(find_value(r.get_obj(), "feedback").get_str() == rating);
	BOOST_CHECK(find_value(r.get_obj(), "feedbackto").get_int() == user);
}
const string OfferAccept(const string& ownernode, const string& buyernode, const string& aliasname, const string& offerguid, const string& qty, const string& witness) {
	CreateSysRatesIfNotExist();
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(ownernode, "offerinfo " + offerguid));
	string escrowguid = EscrowNew(buyernode, ownernode, aliasname, offerguid, qty, "sysrates.peg", find_value(r.get_obj(), "alias").get_str());
	EscrowRelease("node2", "buyer", escrowguid);
	EscrowClaimRelease("node1", escrowguid);
	return escrowguid;
}
const string EscrowNew(const string& node, const string& sellernode, const string& buyeralias, const string& offerguid, const string& qtyStr, const string& arbiteralias, const string& selleralias, const string &discountexpected, const string &witness)
{
	string otherNode1, otherNode2;
	GetOtherNodes(node, otherNode1, otherNode2);
	UniValue r;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasinfo " + selleralias));
	string encryptionkey = find_value(r.get_obj(), "encryption_publickey").get_str();
	int qty = atoi(qtyStr.c_str());
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offerguid));
	int nQtyBefore = find_value(r.get_obj(), "quantity").get_int();
	string exttxid = "\"\"";
	string merchantaliaspegtxid = "\"\"";
	string paymentoptions = "\"\"";
	string redeemscript = "\"\"";
	//										escrownew <alias> <offer> <quantity> <arbiter alias> [extTx] [merchantAliasPegTx] [payment option] [redeemScript] [witness]
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrownew " + buyeralias + " " + offerguid + " " + qtyStr + " " + arbiteralias + " " + exttxid + " " + merchantaliaspegtxid + " " + paymentoptions + " " + redeemscript + " " + witness));
	const UniValue &arr = r.get_array();
	string guid = arr[1].get_str();
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offerguid));
	CAmount offerprice = AmountFromValue(find_value(r.get_obj(), "price"));
	int nQtyAfter = find_value(r.get_obj(), "quantity").get_int();
	BOOST_CHECK_EQUAL(nQtyAfter, nQtyBefore-qty);
	CAmount nTotal = offerprice*qty;
	if(discountexpected != "\"\"")
		nTotal = nTotal*(float)((100-atoi(discountexpected.c_str()))/100.0f);
	CAmount arbiterFee = AmountFromValue(find_value(r.get_obj(), "arbiterfee"));
	CAmount networkFee = AmountFromValue(find_value(r.get_obj(), "networkfee"));
	CAmount nodeTotal = AmountFromValue(find_value(r.get_obj(), "total")) - arbiterFee - networkFee;
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowinfo " + guid));
	BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
	BOOST_CHECK(find_value(r.get_obj(), "offer").get_str() == offerguid);
	BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
	BOOST_CHECK(nodeTotal, nTotal);
	BOOST_CHECK(find_value(r.get_obj(), "arbiter").get_str() == arbiteralias);
	BOOST_CHECK(find_value(r.get_obj(), "seller").get_str() == selleralias);
	if(!otherNode1.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode1, "escrowinfo " + guid));
		arbiterFee = AmountFromValue(find_value(r.get_obj(), "arbiterfee"));
		networkFee = AmountFromValue(find_value(r.get_obj(), "networkfee"));
		nodeTotal = AmountFromValue(find_value(r.get_obj(), "total")) - arbiterFee - networkFee;
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "offer").get_str() == offerguid);
		BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
		BOOST_CHECK_EQUAL(nodeTotal, nTotal);
		BOOST_CHECK(find_value(r.get_obj(), "arbiter").get_str() == arbiteralias);
		BOOST_CHECK(find_value(r.get_obj(), "seller").get_str() == selleralias);
	}
	if(!otherNode2.empty())
	{
		BOOST_CHECK_NO_THROW(r = CallRPC(otherNode2, "escrowinfo " + guid));
		arbiterFee = AmountFromValue(find_value(r.get_obj(), "arbiterfee"));
		networkFee = AmountFromValue(find_value(r.get_obj(), "networkfee"));
		nodeTotal = AmountFromValue(find_value(r.get_obj(), "total")) - arbiterFee - networkFee;
		BOOST_CHECK(find_value(r.get_obj(), "_id").get_str() == guid);
		BOOST_CHECK(find_value(r.get_obj(), "offer").get_str() == offerguid);
		BOOST_CHECK(find_value(r.get_obj(), "quantity").get_int() == qty);
		BOOST_CHECK_EQUAL(nodeTotal, nTotal);
		BOOST_CHECK(find_value(r.get_obj(), "arbiter").get_str() == arbiteralias);
		BOOST_CHECK(find_value(r.get_obj(), "seller").get_str() == selleralias);
	}
	

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowacknowledge " + guid));
	const UniValue& resArray = r.get_array();
	if(resArray.size() > 1)
	{
		const UniValue& complete_value = resArray[1];
		bool bComplete = false;
		if (complete_value.isStr())
			bComplete = complete_value.get_str() == "true";
		BOOST_CHECK(!bComplete);
	}
	BOOST_CHECK_NO_THROW(r = CallRPC(sellernode, "escrowacknowledge " + guid));
	GenerateBlocks(10, sellernode);
	BOOST_CHECK_THROW(r = CallRPC(sellernode, "escrowacknowledge " + guid), runtime_error);
	BOOST_CHECK_NO_THROW(r = CallRPC(sellernode, "offerinfo " + offerguid));
	nQtyAfter = find_value(r.get_obj(), "quantity").get_int();
	BOOST_CHECK_EQUAL(nQtyAfter, nQtyBefore-qty);
	return guid;
}
void EscrowRelease(const string& node, const string& role, const string& guid ,const string& witness)
{
	UniValue r;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowinfo " + guid));
	string offer = find_value(r.get_obj(), "offer").get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferBefore = find_value(r.get_obj(), "quantity").get_int();
	string rawtx = "\"\"";
	BOOST_CHECK_NO_THROW(CallRPC(node, "escrowrelease " + guid + " " + role + " " + rawtx + " " + witness));
	GenerateBlocks(10, node);
	GenerateBlocks(10, node);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferAfter = find_value(r.get_obj(), "quantity").get_int();
	BOOST_CHECK_EQUAL(nQtyOfferAfter, nQtyOfferBefore);

}
void EscrowRefund(const string& node, const string& role, const string& guid, const string &witness)
{
	UniValue r;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowinfo " + guid));
	string offer = find_value(r.get_obj(), "offer").get_str();
	int nQty = find_value(r.get_obj(), "quantity").get_int();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferBefore = find_value(r.get_obj(), "quantity").get_int();
	string rawtx = "\"\"";
	BOOST_CHECK_NO_THROW(CallRPC(node, "escrowrefund " + guid + " " + role + " " + rawtx + " " + witness));
	GenerateBlocks(10, node);
	GenerateBlocks(10, node);

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferAfter = find_value(r.get_obj(), "quantity").get_int();
	// refund adds qty
	BOOST_CHECK_EQUAL(nQtyOfferAfter, nQtyOfferBefore+nQty);
}
void EscrowClaimRefund(const string& node, const string& guid, const string& witness)
{

	UniValue r, a;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowinfo " + guid));
	string buyeralias = find_value(r.get_obj(), "buyer").get_str();
	CAmount nEscrowFee = find_value(r.get_obj(), "networkfee").get_int64() + find_value(r.get_obj(), "arbiterfee").get_int64();
	CAmount nBuyerTotal = find_value(r.get_obj(), "systotal").get_int64();
	string escrowaddress = find_value(r.get_obj(), "escrowaddress").get_str();
	BOOST_CHECK(!buyeralias.empty());
	string offer = find_value(r.get_obj(), "offer").get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	string rootselleralias = find_value(r.get_obj(), "offerlink_seller").get_str();
	int nQtyOfferBefore = find_value(r.get_obj(), "quantity").get_int();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getaddressutxos '{\\\"addresses\": [\\\"" + escrowaddress + "\\\"]}'"));
	UniValue addressUTXOsArray = r.get_array();
	// "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0,\\\"satoshis\\\":10000}]\"
	string inputStr = "\"[";
	for (unsigned int i = 0; i < addressUTXOsArray.size(); i++)
	{
		const UniValue& utxoObj = addressUTXOsArray[i].get_obj();
		const string& txidStr = find_value(utxoObj.get_obj(), "txid").get_str();
		const int& nOut = find_value(utxoObj.get_obj(), "outputIndex").get_int();
		CAmount satoshis = AmountFromValue(find_value(utxoObj.get_obj(), "satoshis"));
		inputStr += "{\\\"txid\\\":\\\"" + txidStr + "\\\",\\\"vout\\\":" + boost::lexical_cast<string>(nOut) + ",\\\"satoshis\\\":" + boost::lexical_cast<string>(satoshis) + "}";
	}
	inputStr += "]\"";

	// get balances before
	BOOST_CHECK_NO_THROW(a = CallRPC(node, "aliasbalance " + buyeralias));
	CAmount balanceBuyerBefore = AmountFromValue(find_value(a.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowclaimrefund " + guid  + " " + inputStr + " " + witness));
	UniValue resArray = r.get_array();
	string strRawTx = resArray[0].get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowcompleterefund " + guid + " " + strRawTx + " " + witness));
	GenerateBlocks(10, node);
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferAfter = find_value(r.get_obj(), "quantity").get_int();
	// claim doesn't touch qty
	BOOST_CHECK_EQUAL(nQtyOfferAfter, nQtyOfferBefore);

	// get balances after
	BOOST_CHECK_NO_THROW(a = CallRPC(node, "aliasbalance " + buyeralias));
	CAmount balanceBuyerAfter = AmountFromValue(find_value(a.get_obj(), "balance"));
	BOOST_CHECK(balanceBuyerBefore != balanceBuyerAfter);
	balanceBuyerBefore += nBuyerTotal;
	if(rootselleralias.empty())
	{
		if(abs(balanceBuyerAfter - balanceBuyerBefore) > 0.1*COIN)
			balanceBuyerBefore += nEscrowFee;	
		BOOST_CHECK(abs(balanceBuyerAfter - balanceBuyerBefore) <= 0.1*COIN);
	}

}
void OfferAddWhitelist(const string& node,const string& offerguid, const string& aliasname, const string& discount, const string& witness)
{
	bool found = false;
	UniValue r;
	BOOST_CHECK_NO_THROW(CallRPC(node, "offeraddwhitelist " + offerguid + " " + aliasname + " " + discount + " " + witness));
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerwhitelist " + offerguid));
	const UniValue &arrayValue = r.get_array();
	for(int i=0;i<arrayValue.size();i++)
	{
		const string &aliasguid = find_value(arrayValue[i].get_obj(), "alias").get_str();
		
		if(aliasguid == aliasname)
		{
			found = true;
			BOOST_CHECK_EQUAL(find_value(arrayValue[i].get_obj(), "offer_discount_percentage").get_int(), atoi(discount));
		}

	}
	BOOST_CHECK(found);
}
void OfferRemoveWhitelist(const string& node, const string& offer, const string& aliasname, const string& witness)
{
	UniValue r;
	BOOST_CHECK_NO_THROW(CallRPC(node, "offerremovewhitelist " + offer + " " + aliasname + " " + witness));
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerwhitelist " + offer));
	const UniValue &arrayValue = r.get_array();
	for(int i=0;i<arrayValue.size();i++)
	{
		const string &aliasguid = find_value(arrayValue[i].get_obj(), "alias").get_str();
		BOOST_CHECK(aliasguid != aliasname);
	}
}
void OfferClearWhitelist(const string& node, const string& offer, const string &witness)
{
	UniValue r;
	BOOST_CHECK_NO_THROW(CallRPC(node, "offerclearwhitelist " + offer + " " + witness));
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerwhitelist " + offer));
	const UniValue &arrayValue = r.get_array();
	BOOST_CHECK(arrayValue.empty());

}
const UniValue FindFeedback(const string& node, const string& txid)
{
	UniValue r, ret;
	string query = "\"{\\\"_id\\\":\\\"" + txid + "\\\"}\"";
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "syscoinquery feedback " + query));
	BOOST_CHECK(r.type() == UniValue::VARR);
	const UniValue &arrayValue = r.get_array();
	BOOST_CHECK(ret.read(arrayValue[0].get_str()));
	BOOST_CHECK(!ret.isNull());
	return ret;
}
void EscrowClaimRelease(const string& node, const string& guid, const string &witness)
{
	UniValue r;

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowinfo " + guid));
	string selleralias = find_value(r.get_obj(), "seller").get_str();
	CAmount nEscrowFee = find_value(r.get_obj(), "networkfee").get_int64() + find_value(r.get_obj(), "arbiterfee").get_int64();
	CAmount nSellerTotal = find_value(r.get_obj(), "systotal").get_int64();
	string escrowaddress = find_value(r.get_obj(), "escrowaddress").get_str();
	BOOST_CHECK(!selleralias.empty());
	string offer = find_value(r.get_obj(), "offer").get_str();

	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	string rootselleralias = find_value(r.get_obj(), "offerlink_seller").get_str();
	int nQtyOfferBefore = find_value(r.get_obj(), "quantity").get_int();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "getaddressutxos '{\\\"addresses\": [\\\"" + escrowaddress + "\\\"]}'"));
	UniValue addressUTXOsArray = r.get_array();
	// "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0,\\\"satoshis\\\":10000}]\"
	string inputStr = "\"[";
	for (unsigned int i = 0; i < addressUTXOsArray.size(); i++)
	{
		const UniValue& utxoObj = addressUTXOsArray[i].get_obj();
		const string& txidStr = find_value(utxoObj.get_obj(), "txid").get_str();
		const int& nOut = find_value(utxoObj.get_obj(), "outputIndex").get_int();
		CAmount satoshis = AmountFromValue(find_value(utxoObj.get_obj(), "satoshis"));
		inputStr += "{\\\"txid\\\":\\\"" + txidStr + "\\\",\\\"vout\\\":" + boost::lexical_cast<string>(nOut) + ",\\\"satoshis\\\":" + boost::lexical_cast<string>(satoshis) + "}";
	}
	inputStr += "]\"";
	// get balances before
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + selleralias));
	CAmount balanceSellerBefore = AmountFromValue(find_value(r.get_obj(), "balance"));
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowclaimrelease " + guid + " " + inputStr + " " + witness));
	UniValue resArray = r.get_array();
	string strRawTx = resArray[0].get_str();
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "escrowcompleterelease " + guid + " " + strRawTx + " " + witness));
	GenerateBlocks(10, node);
	GenerateBlocks(10, node);
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "offerinfo " + offer));
	int nQtyOfferAfter = find_value(r.get_obj(), "quantity").get_int();
	// release doesnt touch qty
	BOOST_CHECK_EQUAL(nQtyOfferBefore, nQtyOfferAfter);

	// get balances after
	BOOST_CHECK_NO_THROW(r = CallRPC(node, "aliasbalance " + selleralias));
	CAmount balanceSellerAfter = AmountFromValue(find_value(r.get_obj(), "balance"));

	balanceSellerBefore += nSellerTotal;
	// check balance after and before within 0.1 COIN (because of escrow output sent to the seller which adds to seller balance)
	if(rootselleralias.empty())
	{
		if(abs(balanceSellerAfter - balanceSellerBefore) > 0.1*COIN)
			balanceSellerBefore += nEscrowFee;	
		BOOST_CHECK(abs(balanceSellerAfter - balanceSellerBefore) <= 0.1*COIN);
	}

}
BasicSyscoinTestingSetup::BasicSyscoinTestingSetup()
{
}
BasicSyscoinTestingSetup::~BasicSyscoinTestingSetup()
{
}
SyscoinTestingSetup::SyscoinTestingSetup()
{
	StartNodes();
}
SyscoinTestingSetup::~SyscoinTestingSetup()
{
	StopNodes();
}
SyscoinMainNetSetup::SyscoinMainNetSetup()
{
	StartMainNetNodes();
}
SyscoinMainNetSetup::~SyscoinMainNetSetup()
{
	StopMainNetNodes();
}
