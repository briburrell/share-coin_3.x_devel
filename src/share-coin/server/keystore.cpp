
/*
 * @copyright
 *
 *  Copyright 2014 Neo Natura
 *
 *  This file is part of the Share Library.
 *  (https://github.com/neonatura/share)
 *        
 *  The Share Library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version. 
 *
 *  The Share Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Share Library.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  @endcopyright
 */  

#include "shcoind.h"
#include "main.h"
#include "keystore.h"
#include "script.h"

bool CKeyStore::GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const
{
  CKey key;

  if (!GetKey(address, key))
    return false;

  vchPubKeyOut = key.GetPubKey();
  return true;
}

bool CBasicKeyStore::AddKey(const HDPrivKey& key)
{
    bool fCompressed = false;
    CSecret secret = key.GetSecret(fCompressed);
    {
        LOCK(cs_KeyStore);
        mapKeys[key.GetPubKey().GetID()] = make_pair(secret, fCompressed);
    }
    return true;
}

bool CBasicKeyStore::AddKey(const CKey& key)
{
    bool fCompressed = false;
    CSecret secret = key.GetSecret(fCompressed);
    {
        LOCK(cs_KeyStore);
        mapKeys[key.GetPubKey().GetID()] = make_pair(secret, fCompressed);
    }
    return true;
}

bool CBasicKeyStore::AddCScript(const CScript& redeemScript)
{
    {
        LOCK(cs_KeyStore);
        mapScripts[redeemScript.GetID()] = redeemScript;
    }
    return true;
}

bool CBasicKeyStore::HaveCScript(const CScriptID& hash) const
{
    bool result;
    {
        LOCK(cs_KeyStore);
        result = (mapScripts.count(hash) > 0);
    }
    return result;
}


bool CBasicKeyStore::GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const
{
    {
        LOCK(cs_KeyStore);
        ScriptMap::const_iterator mi = mapScripts.find(hash);
        if (mi != mapScripts.end())
        {
            redeemScriptOut = (*mi).second;
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::SetCrypted()
{
    {
        LOCK(cs_KeyStore);
        if (fUseCrypto)
            return true;
        if (!mapKeys.empty())
            return false;
        fUseCrypto = true;
    }
    return true;
}

bool CCryptoKeyStore::Lock()
{
    if (!SetCrypted())
        return false;

    {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
    }

    //NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CSecret vchSecret;
            if(!DecryptSecret(vMasterKeyIn, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
                return false;
            if (vchSecret.size() != 32)
                return false;
            CKey key;
            key.SetPubKey(vchPubKey);
            key.SetSecret(vchSecret);
            if (key.GetPubKey() == vchPubKey)
                break;
            return false;
        }
        vMasterKey = vMasterKeyIn;
    }
    //NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::AddKey(const CKey& key)
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::AddKey(key);

        if (IsLocked())
            return false;

        std::vector<unsigned char> vchCryptedSecret;
        CPubKey vchPubKey = key.GetPubKey();
        bool fCompressed;
        if (!EncryptSecret(vMasterKey, key.GetSecret(fCompressed), vchPubKey.GetHash(), vchCryptedSecret))
            return false;

        if (!AddCryptedKey(key.GetPubKey(), vchCryptedSecret))
            return false;
    }
    return true;
}

bool CCryptoKeyStore::AddKey(const HDPrivKey& key)
{
  {
    LOCK(cs_KeyStore);
    if (!IsCrypted())
      return CBasicKeyStore::AddKey(key);

    if (IsLocked())
      return false;

    std::vector<unsigned char> vchCryptedSecret;
    CPubKey vchPubKey = key.GetPubKey();
    bool fCompressed;
    if (!EncryptSecret(vMasterKey, key.GetSecret(fCompressed), vchPubKey.GetHash(), vchCryptedSecret))
      return false;

    if (!AddCryptedKey(key.GetPubKey(), vchCryptedSecret))
      return false;
  }
  return true;
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID &address, CKey& keyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetKey(address, keyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CSecret vchSecret;
            if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
                return false;
            if (vchSecret.size() != 32)
                return false;
            keyOut.SetPubKey(vchPubKey);
            keyOut.SetSecret(vchSecret);
            return true;
        }
    }
    return false;
}
bool CCryptoKeyStore::GetKey(const CKeyID &address, HDPrivKey& keyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetKey(address, keyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CSecret vchSecret;
            if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
                return false;
            if (vchSecret.size() != 32)
                return false;
            keyOut.SetPubKey(vchPubKey);
            keyOut.SetSecret(vchSecret);
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CKeyStore::GetPubKey(address, vchPubKeyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            vchPubKeyOut = (*mi).second.first;
            return true;
        }
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!mapCryptedKeys.empty() || IsCrypted())
            return false;

        fUseCrypto = true;
        BOOST_FOREACH(KeyMap::value_type& mKey, mapKeys)
        {
            CKey key;
            if (!key.SetSecret(mKey.second.first, mKey.second.second))
                return false;
            const CPubKey vchPubKey = key.GetPubKey();
            std::vector<unsigned char> vchCryptedSecret;
            bool fCompressed;
            if (!EncryptSecret(vMasterKeyIn, key.GetSecret(fCompressed), vchPubKey.GetHash(), vchCryptedSecret))
                return false;
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        mapKeys.clear();
    }
    return true;
}
