#ifndef CERT_H
#define CERT_H

#include "rpc/server.h"
#include "dbwrapper.h"
#include "feedback.h"
class CWalletTx;
class CTransaction;
class CReserveKey;
class CCoinsViewCache;
class CCoins;
class CBlock;
class CAliasIndex;
bool CheckCertInputs(const CTransaction &tx, int op, int nOut, const std::vector<std::vector<unsigned char> > &vvchArgs, const CCoinsViewCache &inputs, bool fJustCheck, int nHeight, std::string &errorMessage, bool dontaddtodb=false);
bool DecodeCertTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch);
bool DecodeAndParseCertTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch);
bool DecodeCertScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch);
bool IsCertOp(int op);
int IndexOfCertOutput(const CTransaction& tx);
void CertTxToJSON(const int op, const std::vector<unsigned char> &vchData, const std::vector<unsigned char> &vchHash, UniValue &entry);
std::string certFromOp(int op);
bool RemoveCertScriptPrefix(const CScript& scriptIn, CScript& scriptOut);
class CCert {
public:
	std::vector<unsigned char> vchCert;
	CNameTXIDTuple aliasTuple;
	// to modify alias in certtransfer
	CNameTXIDTuple linkAliasTuple;
    uint256 txHash;
    uint64_t nHeight;
	// 1 can edit, 2 can edit/transfer
	unsigned char nAccessFlags;
	std::vector<unsigned char> vchTitle;
	std::vector<unsigned char> vchData;
	std::vector<unsigned char> vchPubData;
	std::vector<unsigned char> sCategory;
    CCert() {
        SetNull();
    }
    CCert(const CTransaction &tx) {
        SetNull();
        UnserializeFromTx(tx);
    }
	void ClearCert()
	{
		vchData.clear();
		vchPubData.clear();
		vchTitle.clear();
		sCategory.clear();
	}
	ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
	inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {		
		READWRITE(vchData);
		READWRITE(vchTitle);
		READWRITE(vchPubData);
		READWRITE(txHash);
		READWRITE(VARINT(nHeight));
		READWRITE(linkAliasTuple);
		READWRITE(nAccessFlags);
		READWRITE(vchCert);
		READWRITE(sCategory);
		READWRITE(aliasTuple);
	}
    friend bool operator==(const CCert &a, const CCert &b) {
        return (
        a.vchData == b.vchData
		&& a.vchTitle == b.vchTitle
		&& a.vchPubData == b.vchPubData
        && a.txHash == b.txHash
        && a.nHeight == b.nHeight
		&& a.aliasTuple == b.aliasTuple
		&& a.linkAliasTuple == b.linkAliasTuple
		&& a.nAccessFlags == b.nAccessFlags
		&& a.vchCert == b.vchCert
		&& a.sCategory == b.sCategory
        );
    }

    CCert operator=(const CCert &b) {
		vchTitle = b.vchTitle;
        vchData = b.vchData;
		vchPubData = b.vchPubData;
        txHash = b.txHash;
        nHeight = b.nHeight;
		aliasTuple = b.aliasTuple;
		linkAliasTuple = b.linkAliasTuple;
		nAccessFlags = b.nAccessFlags;
		vchCert = b.vchCert;
		sCategory = b.sCategory;
        return *this;
    }

    friend bool operator!=(const CCert &a, const CCert &b) {
        return !(a == b);
    }
    void SetNull() { sCategory.clear(); vchTitle.clear(); nAccessFlags = 2; linkAliasTuple.first.clear(); vchCert.clear(); nHeight = 0; txHash.SetNull(); aliasTuple.first.clear(); vchData.clear(); vchPubData.clear();}
    bool IsNull() const { return (sCategory.empty() && vchTitle.empty() && nAccessFlags == 2 && linkAliasTuple.first.empty() && vchCert.empty() && txHash.IsNull() &&  nHeight == 0 && vchData.empty() && vchPubData.empty() && aliasTuple.first.empty()); }
    bool UnserializeFromTx(const CTransaction &tx);
	bool UnserializeFromData(const std::vector<unsigned char> &vchData, const std::vector<unsigned char> &vchHash);
	void Serialize(std::vector<unsigned char>& vchData);
};


class CCertDB : public CDBWrapper {
public:
    CCertDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "certificates", nCacheSize, fMemory, fWipe) {}

    bool WriteCert(const CCert& cert) {
		WriteCertIndex(cert);
        return Write(make_pair(std::string("certi"), CNameTXIDTuple(cert.vchCert, cert.txHash)), cert) && WriteCertLastTXID(cert.vchCert, cert.txHash);
    }

    bool EraseCert(const CNameTXIDTuple& certTuple) {
		EraseCertLastTXID(certTuple.first);
		EraseCertFirstTXID(certTuple.first);
		EraseCertIndex(certTuple.first);
        return Erase(make_pair(std::string("certi"), certTuple));
    }

    bool ReadCert(const CNameTXIDTuple& certTuple, CCert& cert) {
        return Read(make_pair(std::string("certi"), certTuple), cert);
    }
	bool WriteCertLastTXID(const std::vector<unsigned char>& cert, const uint256& txid) {
		return Write(make_pair(std::string("certlt"), cert), txid);
	}
	bool ReadCertLastTXID(const std::vector<unsigned char>& cert, uint256& txid) {
		return Read(make_pair(std::string("certlt"), cert), txid);
	}
	bool EraseCertLastTXID(const std::vector<unsigned char>& cert) {
		return Erase(make_pair(std::string("certlt"), cert));
	}
	bool WriteCertFirstTXID(const std::vector<unsigned char>& cert, const uint256& txid) {
		return Write(make_pair(std::string("certft"), cert), txid);
	}
	bool ReadCertFirstTXID(const std::vector<unsigned char>& cert, uint256& txid) {
		return Read(make_pair(std::string("certft"), cert), txid);
	}
	bool EraseCertFirstTXID(const std::vector<unsigned char>& cert) {
		return Erase(make_pair(std::string("certft"), cert));
	}
	bool CleanupDatabase(int &servicesCleaned);
	void WriteCertIndex(const CCert& cert);
	void EraseCertIndex(const std::vector<unsigned char>& vchCert);

};
bool GetCert(const CNameTXIDTuple& certTuple);
bool GetCert(const std::vector<unsigned char> &vchCert,CCert& txPos);
bool BuildCertJson(const CCert& cert, const CAliasIndex& alias, UniValue& oName);
uint64_t GetCertExpiration(const CCert& cert);
#endif // CERT_H
