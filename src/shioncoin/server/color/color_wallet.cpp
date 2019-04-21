
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
#include "net.h"
#include "init.h"
#include "strlcpy.h"
#include "ui_interface.h"
#include "chain.h"
#include "color_pool.h"
#include "color_block.h"

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
#include "color/color_block.h"
#include "color/color_wallet.h"
#include "color/color_txidx.h"

using namespace std;
using namespace boost;

COLORWallet *colorWallet;
CScript COLOR_COINBASE_FLAGS;


//extern void color_RelayTransaction(const CTransaction& tx, const uint256& hash);

static unsigned int color_nBytesPerSigOp = COLOR_DEFAULT_BYTES_PER_SIGOP;



bool color_LoadWallet(void)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
  std::ostringstream strErrors;

  const char* pszP2SH = "/P2SH/";
  COLOR_COINBASE_FLAGS << std::vector<unsigned char>(pszP2SH, pszP2SH+strlen(pszP2SH));

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
  colorWallet->LoadWallet(fFirstRun);

  if (fFirstRun)
  {
		string strAccount("");
		CPubKey newDefaultKey = GetAccountPubKey(colorWallet, strAccount, true);
		//CPubKey newDefaultKey = colorWallet->GenerateNewKey();
		colorWallet->SetDefaultKey(newDefaultKey);
		colorWallet->SetAddressBookName(colorWallet->vchDefaultKey.GetID(), "");
  }

  //RegisterWallet(colorWallet);

#if 0
  CBlockIndex *pindexRescan = GetBestBlockIndex(COLOR_COIN_IFACE);
  if (GetBoolArg("-rescan")) {
    pindexRescan = COLORBlock::pindexGenesisBlock;
	} else {
		LOCK(cs_wallet);

    CWalletDB walletdb("color_wallet.dat");
    CBlockLocator locator(GetCoinIndex(iface));
    if (walletdb.ReadBestBlock(locator))
      pindexRescan = locator.GetBlockIndex();
		walletdb.Close();
  }
  CBlockIndex *pindexBest = GetBestBlockIndex(COLOR_COIN_IFACE);
  if (pindexBest != pindexRescan && pindexBest && pindexRescan && pindexBest->nHeight > pindexRescan->nHeight)
  {
    int64 nStart;

    Debug("(color) LoadWallet: Rescanning last %i blocks (from block %i)...\n", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
    nStart = GetTimeMillis();
    colorWallet->ScanForWalletTransactions(pindexRescan, true);
//    printf(" rescan      %15"PRI64d"ms\n", GetTimeMillis() - nStart);
  }
#endif

  // Add wallet transactions that aren't already in a block to mapTransactions
  colorWallet->ReacceptWalletTransactions();

  return (true);
}

void COLORWallet::RelayWalletTransaction(CWalletTx& wtx)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);

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
      Debug("(color) RelayWalletTransaction: relayed tx '%s'\n", hash.GetHex().c_str());
    }
  }

}

void COLORWallet::ResendWalletTransactions()
{
#if 0
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
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
  if (COLORBlock::nTimeBestReceived < nLastTime)
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
      if (wtx.GetDepthInMainChain(COLOR_COIN_IFACE) != 0)
        continue;
total++;

      // Don't rebroadcast until it's had plenty of time that
      // it should have gotten in already by now.
      if (mapSorted.size() < 16 ||
          COLORBlock::nTimeBestReceived - (int64)wtx.nTimeReceived > 5 * 60)
        mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
    }
    BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
    {
      CWalletTx& wtx = *item.second;
//      wtx.RelayWalletTransaction(txdb);
      RelayWalletTransaction(wtx);
    }
  }
#endif
}

void COLORWallet::ReacceptWalletTransactions()
{
	{
		LOCK(cs_wallet);
		core_ReacceptWalletTransactions(this);
	}
}

int COLORWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
  if (pindexStart)
    InitServiceWalletEvent(this, pindexStart->nHeight);
  return (0);
}

int64 COLORWallet::GetTxFee(CTransaction tx)
{
  int64 nFees;
  int i;

  if (tx.IsCoinBase())
    return (0);
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
  CBlock *pblock = GetBlockByTx(iface, tx.GetHash());

  nFees = 0;
#ifdef USE_LEVELDB_COINDB
  bool fInvalid = false;
  map<uint256, CTxIndex> mapQueuedChanges;
  MapPrevTx inputs;
  COLORTxDB txdb;
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

bool COLORWallet::CommitTransaction(CWalletTx& wtxNew)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
  CTxMemPool *pool = GetTxMemPool(iface);

  /* perform final checks & submit to pool. */
  if (!pool->AddTx(wtxNew))
    return (false);

  {
    LOCK2(cs_main, cs_wallet);
    Debug("(color) CommitTransaction: \"%s\".", wtxNew.ToString(COLOR_COIN_IFACE).c_str());
    {
      // This is only to keep the database open to defeat the auto-flush for the
      // duration of this scope.  This is the only place where this optimization
      // maybe makes sense; please don't do it anywhere else.
//      CWalletDB* pwalletdb = new CWalletDB(strWalletFile,"r");

      // Add tx to wallet, because if it has change it's also ours,
      // otherwise just for transaction history.
      AddToWallet(wtxNew);

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

//			delete pwalletdb;
    }

#if 0
// Track how many getdata requests our transaction gets
mapRequestCount[wtxNew.GetHash()] = 0;
RelayWalletTransaction(wtxNew);
#endif

  }

  STAT_TX_SUBMITS(iface)++;

  return true;
}

bool COLORWallet::CreateAccountTransaction(string strFromAccount, const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxNew, string& strError, int64& nFeeRet)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);

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
        // if sub-cent change is required, the fee must be raised to at least COLOR_MIN_TX_FEE
        // or until nChange becomes zero
        // NOTE: this depends on the exact behaviour of GetMinFee
        if (nFeeRet < COLOR_MIN_TX_FEE && nChange > 0 && nChange < CENT)
        {
          int64 nMoveToFee = min(nChange, COLOR_MIN_TX_FEE - nFeeRet);
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
          CSignature sig(COLOR_COIN_IFACE, &wtxNew, nIn);
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
        int64 nMinFee = wtxNew.GetMinFee(COLOR_COIN_IFACE, 1, fAllowFree, GMF_SEND);
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

bool COLORWallet::CreateAccountTransaction(string strFromAccount, CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew, string& strError, int64& nFeeRet)
{
  vector< pair<CScript, int64> > vecSend;
  vecSend.push_back(make_pair(scriptPubKey, nValue));
  return CreateAccountTransaction(strFromAccount, vecSend, wtxNew, strError, nFeeRet);
}



void COLORWallet::AddSupportingTransactions(CWalletTx& wtx)
{
  wtx.AddSupportingTransactions();
}

#ifdef USE_LEVELDB_COINDB
bool COLORWallet::UnacceptWalletTransaction(const CTransaction& tx)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);

  if (!core_UnacceptWalletTransaction(iface, tx))
    return (false);

  {
    COLORTxDB txdb;

    BOOST_FOREACH(const CTxIn& in, tx.vin) {
      const uint256& prev_hash = in.prevout.hash; 
      int nTxOut = in.prevout.n;

      CTxIndex txindex;
      if (!txdb.ReadTxIndex(prev_hash, txindex))
        continue;

      if (nTxOut >= txindex.vSpent.size())
        continue; /* bad */

      /* set output as unspent */
      txindex.vSpent[nTxOut].SetNull();
      txdb.UpdateTxIndex(prev_hash, txindex);
    }

    /* remove pool tx from db */
    txdb.EraseTxIndex(tx);

    txdb.Close();
  }

  return (true);
}
#else
bool COLORWallet::UnacceptWalletTransaction(const CTransaction& tx)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
  return (core_UnacceptWalletTransaction(iface, tx));
}

#endif

int64 COLORWallet::GetBlockValue(int nHeight, int64 nFees, uint160 hColor)
{
  return (color_GetBlockValue(hColor, nHeight, nFees));
}

unsigned int COLORWallet::GetTransactionWeight(const CTransaction& tx)
{

  unsigned int nBytes;

  nBytes = 
    ::GetSerializeSize(tx, SER_NETWORK, COLOR_PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (COLOR_WITNESS_SCALE_FACTOR - 1) +
    ::GetSerializeSize(tx, SER_NETWORK, COLOR_PROTOCOL_VERSION);

  return (nBytes);
}

unsigned int COLORWallet::GetVirtualTransactionSize(int64 nWeight, int64 nSigOpCost)
{
  return (std::max(nWeight, nSigOpCost * color_nBytesPerSigOp) + COLOR_WITNESS_SCALE_FACTOR - 1) / COLOR_WITNESS_SCALE_FACTOR;
}
unsigned int COLORWallet::GetVirtualTransactionSize(const CTransaction& tx)
{
  unsigned int nWeight = GetTransactionWeight(tx);
  int nSigOpCost = 0;
  return (GetVirtualTransactionSize(nWeight, nSigOpCost));
}

/** Large (in bytes) low-priority (new, small-coin) transactions require fee. */
double COLORWallet::AllowFreeThreshold()
{
  static const double block_daily = 360;
  static const double block_bytes = 256;
  return ((double)COIN * block_daily / block_bytes);
}

int64 COLORWallet::GetFeeRate(uint160 hColor)
{
  CIface *iface = GetCoinByIndex(COLOR_COIN_IFACE);
	return (MIN_TX_FEE_RATE(iface));
}

int COLORWallet::GetCoinbaseMaturity(uint160 hColor)
{
	return (color_GetCoinbaseMaturity(hColor));
}
