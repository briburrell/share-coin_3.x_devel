
/*
 * @copyright
 *
 *  Copyright 2014 Neo Natura
 *
 *  This file is part of ShionCoin.
 *  (https://github.com/neonatura/shioncoin)
 *        
 *  ShionCoin is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version. 
 *
 *  ShionCoin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ShionCoin.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  @endcopyright
 */

#include "shcoind.h"
#include "block.h"
#include "net.h"
#include "init.h"
#include "strlcpy.h"
#include "ui_interface.h"
#include "chain.h"
#include "shc_pool.h"
#include "shc_block.h"
#include "versionbits.h"
#include "algobits.h"

#ifdef WIN32
#include <string.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef fcntl
#undef fcntl
#endif

#include <boost/array.hpp>
#include <share.h>
#include "walletdb.h"
#include "txsignature.h"
#include "shc/shc_pool.h"
#include "shc/shc_block.h"
#include "shc/shc_wallet.h"
#include "shc/shc_txidx.h"

using namespace std;
using namespace boost;

SHCWallet *shcWallet;
CScript SHC_COINBASE_FLAGS;


//extern void shc_RelayTransaction(const CTransaction& tx, const uint256& hash);

static unsigned int shc_nBytesPerSigOp = SHC_DEFAULT_BYTES_PER_SIGOP;

extern VersionBitsCache *GetVersionBitsCache(CIface *iface);


bool shc_LoadWallet(void)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  std::ostringstream strErrors;

  const char* pszP2SH = "/P2SH/";
  SHC_COINBASE_FLAGS << std::vector<unsigned char>(pszP2SH, pszP2SH+strlen(pszP2SH));

#if 0
  if (!bitdb.Open(GetDataDir()))
  {
    fprintf(stderr, "error: unable to open data directory.\n");
    return (-1);
  }

  if (!LoadBlockIndex(iface)) {
    fprintf(stderr, "error: unable to open load block index.\n");
    return (-1);
  }
#endif

  bool fFirstRun = true;
  shcWallet->LoadWallet(fFirstRun);

  if (fFirstRun)
  {
		string strAccount("");
		CPubKey newDefaultKey = GetAccountPubKey(shcWallet, strAccount, true);
		//CPubKey newDefaultKey = shcWallet->GenerateNewKey();
		shcWallet->SetDefaultKey(newDefaultKey);
		shcWallet->SetAddressBookName(shcWallet->vchDefaultKey.GetID(), "");
  }

  //RegisterWallet(shcWallet);

#if 0
  CBlockIndex *pindexRescan = GetBestBlockIndex(SHC_COIN_IFACE);
  if (GetBoolArg("-rescan"))
    pindexRescan = SHCBlock::pindexGenesisBlock;
  else
  {
    LOCK(cs_wallet);

    CWalletDB walletdb("shc_wallet.dat");
    CBlockLocator locator(GetCoinIndex(iface));
    if (walletdb.ReadBestBlock(locator))
      pindexRescan = locator.GetBlockIndex();
		walletdb.Close();
  }
  CBlockIndex *pindexBest = GetBestBlockIndex(SHC_COIN_IFACE);
  if (pindexBest != pindexRescan && pindexBest && pindexRescan && pindexBest->nHeight > pindexRescan->nHeight)
  {
    int64 nStart;

    Debug("(shc) LoadWallet: Rescanning last %i blocks (from block %i)...\n", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
    nStart = GetTimeMillis();
    shcWallet->ScanForWalletTransactions(pindexRescan, true);
  }
#endif

  // Add wallet transactions that aren't already in a block to mapTransactions
  shcWallet->ReacceptWalletTransactions();

  return (true);
}

#if USE_LEVELDB_COINDB
void SHCWallet::RelayWalletTransaction(CWalletTx& wtx)
{
  SHCTxDB txdb;

  BOOST_FOREACH(const CMerkleTx& tx, wtx.vtxPrev)
  {
    if (!tx.IsCoinBase())
    {
      uint256 hash = tx.GetHash();
      if (!txdb.ContainsTx(hash))
        RelayMessage(CInv(ifaceIndex, MSG_TX, hash), (CTransaction)tx);
    }
  }

  if (!wtx.IsCoinBase())
  {
    uint256 hash = wtx.GetHash();
    if (!txdb.ContainsTx(hash))
    {
      RelayMessage(CInv(ifaceIndex, MSG_TX, hash), (CTransaction)wtx);
      Debug("(shc) RelayWalletTransaction: relayed tx '%s'\n", hash.GetHex().c_str());
    } else {
      Debug("(shc) RelayWalletTransaction: skipped already established tx '%s'\n", hash.GetHex().c_str());
    }
  }

  txdb.Close();

}
#else
void SHCWallet::RelayWalletTransaction(CWalletTx& wtx)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);

  BOOST_FOREACH(const CMerkleTx& tx, wtx.vtxPrev)
  {
    if (!tx.IsCoinBase() && !tx.vin.empty()) {
      uint256 hash = tx.GetHash();
      if (!VerifyTxHash(iface, hash))
        RelayMessage(CInv(ifaceIndex, MSG_TX, hash), (CTransaction)tx);
    }
  }

  if (!wtx.IsCoinBase()) {
    uint256 hash = wtx.GetHash();
    if (!VerifyTxHash(iface, hash)) {
      RelayMessage(CInv(ifaceIndex, MSG_TX, hash), (CTransaction)wtx);
      Debug("(shc) RelayWalletTransaction: relayed tx '%s'\n", hash.GetHex().c_str());
    }
  }

}
#endif

#if 0
void SHCWallet::RelayWalletTransaction(CWalletTx& wtx)
{

  BOOST_FOREACH(const CMerkleTx& tx, wtx.vtxPrev)
  {
    // Important: versions of bitcoin before 0.8.6 had a bug that inserted
    // empty transactions into the vtxPrev, which will cause the node to be
    // banned when retransmitted, hence the check for !tx.vin.empty()
    if (!tx.IsCoinBase() && !tx.vin.empty())
      if (tx.GetDepthInMainChain(SHC_COIN_IFACE) == 0)
        shc_RelayTransaction((CTransaction)tx, tx.GetHash());
  }

  if (!wtx.IsCoinBase())
  {
    if (wtx.GetDepthInMainChain(SHC_COIN_IFACE) == 0) {
      uint256 hash = wtx.GetHash();
      shc_RelayTransaction((CTransaction)wtx, hash);
      Debug("(shc) RelayWalletTransactions: wallet tx (%s) relayed.", hash.ToString().c_str());
    }
  }


}
#endif


void SHCWallet::ResendWalletTransactions()
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  CTxMemPool *pool = GetTxMemPool(iface);
  // Do this infrequently and randomly to avoid giving away
  // that these are our transactions.
  static int64 nNextTime;
  if (GetTime() < nNextTime)
    return;
//  bool fFirst = (nNextTime == 0);
  nNextTime = GetTime() + GetRand(30 * 60);
//  if (fFirst) return;

  // Only do it if there's been a new block since last time
  static int64 nLastTime;
  if (SHCBlock::nTimeBestReceived < nLastTime)
    return;
  nLastTime = GetTime();

  // Rebroadcast any of our txes that aren't in a block yet
  {
    LOCK(cs_wallet);
    // Sort them in chronological order
    multimap<unsigned int, CWalletTx*> mapSorted;
int total = 0;
    BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
    {

      CWalletTx& wtx = item.second;

      if (wtx.IsCoinBase())
        continue;
      if (wtx.vin.empty())
        continue;
      const uint256& tx_hash = item.first;
      if (!pool->exists(tx_hash))
        continue;
      if (wtx.GetDepthInMainChain(SHC_COIN_IFACE) != 0)
        continue;
total++;

      // Don't rebroadcast until it's had plenty of time that
      // it should have gotten in already by now.
      if (mapSorted.size() < 16 ||
          SHCBlock::nTimeBestReceived - (int64)wtx.nTimeReceived > 5 * 60)
        mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
    }
    BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
    {
      CWalletTx& wtx = *item.second;
//      wtx.RelayWalletTransaction(txdb);
      RelayWalletTransaction(wtx);
    }
  }
}

void SHCWallet::ReacceptWalletTransactions()
{
	{
		LOCK(cs_wallet);
		core_ReacceptWalletTransactions(this);
	}
}

#if 0
int SHCWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
  int ret = 0;

  CBlockIndex* pindex = pindexStart;
  {
    LOCK(cs_wallet);
    while (pindex)
    {
      SHCBlock block;
      block.ReadFromDisk(pindex, true);
      BOOST_FOREACH(CTransaction& tx, block.vtx)
      {
        if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
          ret++;
      }
      pindex = pindex->pnext;
    }
  }
  return ret;
}
#endif
int SHCWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
  if (pindexStart)
    InitServiceWalletEvent(this, pindexStart->nHeight);
  return (0);
}

int64 SHCWallet::GetTxFee(CTransaction tx)
{
  int64 nFees;
  int i;

  if (tx.IsCoinBase())
    return (0);
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  CBlock *pblock = GetBlockByTx(iface, tx.GetHash());

  nFees = 0;
#ifdef USE_LEVELDB_COINDB
  bool fInvalid = false;
  map<uint256, CTxIndex> mapQueuedChanges;
  MapPrevTx inputs;
  SHCTxDB txdb;
  if (tx.FetchInputs(txdb, mapQueuedChanges, pblock, false, inputs, fInvalid))
    nFees += tx.GetValueIn(inputs) - tx.GetValueOut();
  txdb.Close();
#else
  tx_cache inputs;
  if (FillInputs(tx, inputs)) {
    nFees += tx.GetValueIn(inputs) - tx.GetValueOut();
  }
#endif

  if (pblock) delete pblock;
  return (nFees);
}

bool SHCWallet::CommitTransaction(CWalletTx& wtxNew)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  CTxMemPool *pool = GetTxMemPool(iface);

  /* perform final checks & submit to pool. */
  if (!pool->AddTx(wtxNew))
    return (false);

  {
    LOCK2(cs_main, cs_wallet);
//    Debug("CommitTransaction:\n%s", wtxNew.ToString().c_str());
    {
      // This is only to keep the database open to defeat the auto-flush for the
      // duration of this scope.  This is the only place where this optimization
      // maybe makes sense; please don't do it anywhere else.
//      CWalletDB* pwalletdb = new CWalletDB(strWalletFile,"r");

      // Add tx to wallet, because if it has change it's also ours,
      // otherwise just for transaction history.
      AddToWallet(wtxNew);

#if 0
      // Mark old coins as spent
      set<CWalletTx*> setCoins;
      BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
      {
        CWalletTx &coin = mapWallet[txin.prevout.hash];
        coin.BindWallet(this);
        coin.MarkSpent(txin.prevout.n);
        coin.WriteToDisk();
        //NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
      }
#endif

//			delete pwalletdb;
    }

    // Track how many getdata requests our transaction gets
    mapRequestCount[wtxNew.GetHash()] = 0;

#if 0
    // Broadcast
    SHCTxDB txdb;
    bool ret = wtxNew.AcceptToMemoryPool(txdb);
    if (ret) {
//      wtxNew.RelayWalletTransaction(txdb);
      RelayWalletTransaction(wtxNew);
    }
    txdb.Close();
    if (!ret) {
      // This must not fail. The transaction has already been signed and recorded.
      printf("CommitTransaction() : Error: Transaction not valid");
      return false;
    }
#endif

    RelayWalletTransaction(wtxNew);
  }

  STAT_TX_SUBMITS(iface)++;

  return true;
}


//bool SHCWallet::CreateTransactionFromInput(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet)
#if 0
bool SHCWallet::CreateTransactionFromExt(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, int64& nFeeRet)
{
  return (core_CreateTransactionFromExt()); 
}
bool SHCWallet::CreateAccountTransaction(string strAccount, const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, int64& nFeeRet)
{

  return (core_CreateAccountTransaction(strAccount, vecSend, wtxNew, nFeeRet));
}
#endif

#if 0
bool SHCWallet::CreateTransaction(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  int64 nValue = 0;
  BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
  {
    if (nValue < 0)
      return false;
    nValue += s.second;
  }
  if (vecSend.empty() || nValue < 0)
    return false;

  wtxNew.BindWallet(this);

  {
    LOCK2(cs_main, cs_wallet);
    {
      nFeeRet = nTransactionFee;
      loop
      {
        wtxNew.vin.clear();
        wtxNew.vout.clear();
        wtxNew.fFromMe = true;

        int64 nTotalValue = nValue + nFeeRet;
        double dPriority = 0;
        // vouts to the payees
        BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
          wtxNew.vout.push_back(CTxOut(s.second, s.first));

        // Choose coins to use
        set<pair<const CWalletTx*,unsigned int> > setCoins;
        int64 nValueIn = 0;
        if (!SelectCoins(nTotalValue, setCoins, nValueIn))
          return false;
        BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
        {
          int64 nCredit = pcoin.first->vout[pcoin.second].nValue;
          dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain(ifaceIndex);
        }

        int64 nChange = nValueIn - nValue - nFeeRet;
        // if sub-cent change is required, the fee must be raised to at least SHC_MIN_TX_FEE
        // or until nChange becomes zero
        // NOTE: this depends on the exact behaviour of GetMinFee
        if (nFeeRet < SHC_MIN_TX_FEE && nChange > 0 && nChange < CENT)
        {
          int64 nMoveToFee = min(nChange, SHC_MIN_TX_FEE - nFeeRet);
          nChange -= nMoveToFee;
          nFeeRet += nMoveToFee;
        }

        if (nChange > 0) {
					CKeyID keyID;
					CCoinAddr addr = GetAccountAddress(this, strFromAccount, true);
					if (addr.GetKeyID(keyID)) {
						CScript scriptChange;
						scriptChange.SetDestination(keyID);

						// Insert change txn at random position:
						vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size());
						wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
					}
        }

        // Fill vin
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
          wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

        unsigned int nIn = 0;
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
          CSignature sig(SHC_COIN_IFACE, &wtxNew, nIn);
          if (!sig.SignSignature(*coin.first)) {
            return false;
          }

          nIn++;
        }
#if 0
        // Sign
        int nIn = 0;
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
          if (!SignSignature(*this, *coin.first, wtxNew, nIn++)) {
            return false;
          }
#endif

        /* Ensure transaction does not breach a defined size limitation. */
        unsigned int nWeight = GetTransactionWeight(wtxNew);
        if (nWeight >= MAX_TRANSACTION_WEIGHT(iface)) {
          return (error(SHERR_INVAL, "The transaction size is too large."));
        }

        unsigned int nBytes = GetVirtualTransactionSize(wtxNew);
        dPriority /= nBytes;

        // Check that enough fee is included
        int64 nPayFee = nTransactionFee * (1 + (int64)nBytes / 1000);
#if 0
        bool fAllowFree = AllowFree(dPriority);
        int64 nMinFee = wtxNew.GetMinFee(SHC_COIN_IFACE, 1, fAllowFree, GMF_SEND);
#endif
        int64 nMinFee = CalculateFee(wtxNew);

        if (nFeeRet < max(nPayFee, nMinFee))
        {
          nFeeRet = max(nPayFee, nMinFee);
          continue;
        }

        // Fill vtxPrev by copying from previous transactions vtxPrev
        wtxNew.AddSupportingTransactions();
        wtxNew.fTimeReceivedIsTxTime = true;

        break;
      }
    }
  }
  return true;
}
bool SHCWallet::CreateTransaction(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet)
{
    vector< pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));
    return CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet);
}
#endif

bool SHCWallet::CreateAccountTransaction(string strFromAccount, const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, string& strError, int64& nFeeRet)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);

  wtxNew.strFromAccount = strFromAccount;

  int64 nValue = 0;
  BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
  {
    if (nValue < 0) {
      strError = "invalid output coin value";
      return false;
    }
    nValue += s.second;
  }
  if (vecSend.empty() || nValue < 0) {
    strError = "incomplete output specified";
    return false;
  }

  wtxNew.BindWallet(this);

  {
    LOCK2(cs_main, cs_wallet);
    {
      nFeeRet = nTransactionFee;
      loop
      {
        wtxNew.vin.clear();
        wtxNew.vout.clear();
        wtxNew.fFromMe = true;

        int64 nTotalValue = nValue + nFeeRet;
        double dPriority = 0;
        // vouts to the payees
        BOOST_FOREACH (const PAIRTYPE(CScript, int64)& s, vecSend)
          wtxNew.vout.push_back(CTxOut(s.second, s.first));

        // Choose coins to use
        set<pair<const CWalletTx*,unsigned int> > setCoins;
        int64 nValueIn = 0;
        if (!SelectAccountCoins(strFromAccount, nTotalValue, setCoins, nValueIn)) {
          strError = "An error occurred obtaining sufficient coins in order perform the transaction. Check the transaction fee cost.";
          return false;
        }
        BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
        {
          int64 nCredit = pcoin.first->vout[pcoin.second].nValue;
          dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain(ifaceIndex);
        }

        int64 nChange = nValueIn - nValue - nFeeRet;
        // if sub-cent change is required, the fee must be raised to at least SHC_MIN_TX_FEE
        // or until nChange becomes zero
        // NOTE: this depends on the exact behaviour of GetMinFee
        if (nFeeRet < SHC_MIN_TX_FEE && nChange > 0 && nChange < CENT)
        {
          int64 nMoveToFee = min(nChange, SHC_MIN_TX_FEE - nFeeRet);
          nChange -= nMoveToFee;
          nFeeRet += nMoveToFee;
        }

        if (nChange > 0) {
					CKeyID keyID;
					CCoinAddr addr = GetAccountAddress(this, strFromAccount, true);
					if (addr.GetKeyID(keyID)) {
						CScript scriptChange;
						scriptChange.SetDestination(keyID);

						// Insert change txn at random position:
						vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size());
						wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
					}
        }

        // Fill vin
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
          wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

        unsigned int nIn = 0;
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
          CSignature sig(SHC_COIN_IFACE, &wtxNew, nIn);
          const CWalletTx *s_wtx = coin.first;
          if (!sig.SignSignature(*s_wtx)) {

#if 0
            /* failing signing against prevout. mark as spent to prohibit further attempts to use this output. */
            s_wtx->MarkSpent(nIn);
#endif

            strError = strprintf(_("An error occurred signing the transaction [input tx \"%s\", output #%d]."), s_wtx->GetHash().GetHex().c_str(), nIn);
            return false;
          }

          nIn++;
        }
#if 0
        // Sign
        int nIn = 0;
        BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins) {
          const CWalletTx *s_wtx = coin.first;
          if (!SignSignature(*this, *s_wtx, wtxNew, nIn)) {

#if 0
            /* failing signing against prevout. mark as spent to prohibit further attempts to use this output. */
            s_wtx->MarkSpent(nIn);
#endif

            strError = strprintf(_("An error occurred signing the transaction [input tx \"%s\", output #%d]."), s_wtx->GetHash().GetHex().c_str(), nIn);
            return false;
          }
          nIn++;
        }
#endif

        /* Ensure transaction does not breach a defined size limitation. */
        unsigned int nWeight = GetTransactionWeight(wtxNew);
        if (nWeight >= MAX_TRANSACTION_WEIGHT(iface)) {
          return (error(SHERR_INVAL, "The transaction size is too large."));
        }

        unsigned int nBytes = GetVirtualTransactionSize(wtxNew);
        dPriority /= nBytes;

        // Check that enough fee is included
        int64 nPayFee = nTransactionFee * (1 + (int64)nBytes / 1000);
#if 0
        bool fAllowFree = AllowFree(dPriority);
        int64 nMinFee = wtxNew.GetMinFee(SHC_COIN_IFACE, 1, fAllowFree, GMF_SEND);
#endif
        int64 nMinFee = CalculateFee(wtxNew); 

        if (nFeeRet < max(nPayFee, nMinFee))
        {
          nFeeRet = max(nPayFee, nMinFee);
          continue;
        }

        // Fill vtxPrev by copying from previous transactions vtxPrev
        wtxNew.AddSupportingTransactions();
        wtxNew.fTimeReceivedIsTxTime = true;

        break;
      }
    }
  }
  return true;
}

bool SHCWallet::CreateAccountTransaction(string strFromAccount, CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, string& strError, int64& nFeeRet)
{
  vector< pair<CScript, int64> > vecSend;
  vecSend.push_back(make_pair(scriptPubKey, nValue));
  return CreateAccountTransaction(strFromAccount, vecSend, wtxNew, strError, nFeeRet);
}



void SHCWallet::AddSupportingTransactions(CWalletTx& wtx)
{
  wtx.AddSupportingTransactions();
}

bool SHCWallet::UnacceptWalletTransaction(const CTransaction& tx)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
  return (core_UnacceptWalletTransaction(iface, tx));
}

int64 SHCWallet::GetBlockValue(int nHeight, int64 nFees, uint160 hColor)
{
  return (shc_GetBlockValue(nHeight, nFees));
}

unsigned int SHCWallet::GetTransactionWeight(const CTransaction& tx)
{

  unsigned int nBytes;

  nBytes = 
    ::GetSerializeSize(tx, SER_NETWORK, SHC_PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (SHC_WITNESS_SCALE_FACTOR - 1) +
    ::GetSerializeSize(tx, SER_NETWORK, SHC_PROTOCOL_VERSION);

  return (nBytes);
}

unsigned int SHCWallet::GetVirtualTransactionSize(int64 nWeight, int64 nSigOpCost)
{
  return (std::max(nWeight, nSigOpCost * shc_nBytesPerSigOp) + SHC_WITNESS_SCALE_FACTOR - 1) / SHC_WITNESS_SCALE_FACTOR;
}
unsigned int SHCWallet::GetVirtualTransactionSize(const CTransaction& tx)
{
  unsigned int nWeight = GetTransactionWeight(tx);
  int nSigOpCost = 0;
  return (GetVirtualTransactionSize(nWeight, nSigOpCost));
}

/** Large (in bytes) low-priority (new, small-coin) transactions require fee. */
double SHCWallet::AllowFreeThreshold()
{
  static const double block_daily = 360;
  static const double block_bytes = 256;
  return ((double)COIN * block_daily / block_bytes);
}

int64 SHCWallet::GetFeeRate(uint160 hColor)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
	return (MIN_TX_FEE_RATE(iface));
}

int SHCWallet::GetCoinbaseMaturity(uint160 hColor)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);
	return (iface ? iface->coinbase_maturity : 0);
}

bool SHCWallet::IsAlgoSupported(int alg, CBlockIndex *pindexPrev, uint160 hColor)
{
  CIface *iface = GetCoinByIndex(SHC_COIN_IFACE);

	if (alg == ALGO_SCRYPT)
		return (true);

	if (!pindexPrev)
		pindexPrev = GetBestBlockIndex(SHC_COIN_IFACE);

	return (VersionBitsState(pindexPrev, iface, DEPLOYMENT_ALGO) == THRESHOLD_ACTIVE);
}
