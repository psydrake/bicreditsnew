// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcredit Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/standard.h"

#include "pubkey.h"
#include "script/script.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

CScriptID::CScriptID(const CScript& in) : uint160(in.size() ? Hash160(in.begin(), in.end()) : 0) {}

const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_ESCROW_FEE: return "escrow-fee";
    case TX_ESCROW_SENDER: return "escrow-sender";
    case TX_ESCROW: return "escrow";
    case TX_PUBKEYHASH_NONCED: return "pubkeyhash-nonced";    
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_NULL_DATA: return "nulldata";
    case TX_DELAYEDPUBKEY:      return "delayedpubkey";
    case TX_DELAYEDPUBKEYHASH:  return "delayedpubkeyhash";
    case TX_DELAYEDSCRIPTHASH:  return "delayedscripthash";
    case TX_DELAYEDMULTISIG:    return "delayedmultisig";    
    }
    return NULL;
}

/**
 * Return public keys or hashes from scriptPubKey, for 'standard' transaction types.
 */
bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, vector<vector<unsigned char> >& vSolutionsRet)
{
    // Templates
    static multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // P2SH puts the redemption conditions in the hands of the receiver
        mTemplates.insert(make_pair(TX_SCRIPTHASH, CScript() << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));
        mTemplates.insert(make_pair(TX_DELAYEDSCRIPTHASH, CScript() << OP_SCRIPTNUMBER << OP_CHECKLOCKTIMEVERIFY << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));
 
        // Bitcredit address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
        mTemplates.insert(make_pair(TX_DELAYEDPUBKEYHASH, CScript() << OP_SCRIPTNUMBER << OP_CHECKLOCKTIMEVERIFY << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // Old Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));
        mTemplates.insert(make_pair(TX_DELAYEDPUBKEY, CScript() << OP_SCRIPTNUMBER << OP_CHECKLOCKTIMEVERIFY << OP_PUBKEY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));
        mTemplates.insert(make_pair(TX_DELAYEDMULTISIG, CScript() << OP_SCRIPTNUMBER << OP_CHECKLOCKTIMEVERIFY << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));
         
        //Escrow transactions
        mTemplates.insert(make_pair(TX_ESCROW, CScript() << OP_IF << OP_PUBKEYHASH << OP_DUP << OP_PUBKEY << OP_PUBKEY << OP_CHECKDATASIG << OP_VERIFY << OP_SWAP << OP_HASH160 << OP_EQUAL << OP_VERIFY << OP_PUBKEYHASH << OP_TOALTSTACK << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG << OP_ELSE << OP_NUMERIC << OP_CHECKEXPIRY << OP_ENDIF));

        mTemplates.insert(make_pair(TX_ESCROW_SENDER, CScript() << OP_IF << OP_IF << OP_PUBKEYHASH << OP_DUP << OP_PUBKEY << OP_PUBKEY << OP_CHECKDATASIG << OP_VERIFY << OP_SWAP << OP_HASH160 << OP_EQUAL << OP_VERIFY << OP_CHECKTRANSFERNONCE << OP_ELSE << OP_PUBKEYHASH << OP_TOALTSTACK << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG << OP_ENDIF << OP_ELSE << OP_NUMERIC << OP_CHECKEXPIRY << OP_ENDIF));

        mTemplates.insert(make_pair(TX_ESCROW_FEE, CScript() << OP_IF << OP_PUBKEYHASH << OP_TOALTSTACK << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG << OP_ELSE << OP_NUMERIC << OP_CHECKEXPIRY << OP_ENDIF));
        
      // transfer nonce, Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(make_pair(TX_PUBKEYHASH_NONCED, CScript() << OP_NONCE << OP_TOALTSTACK << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
  

        // Empty, provably prunable, data-carrying output
        if (GetBoolArg("-datacarrier", true))
            mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN << OP_SMALLDATA));
        mTemplates.insert(make_pair(TX_NULL_DATA, CScript() << OP_RETURN));
    }

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    // Scan templates
    const CScript& script1 = scriptPubKey;
    BOOST_FOREACH(const PAIRTYPE(txnouttype, CScript)& tplate, mTemplates)
    {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        vector<unsigned char> vch1, vch2;

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG)
                {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                while (vch1.size() >= 33 && vch1.size() <= 65)
                {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 65)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_NONCE)
            {
                if (vch1.size() > 9)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_NUMERIC)
            {
                if (vch1.size() > 4)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                    break;
            }
            else if (opcode2 == OP_SCRIPTNUMBER)
            {   
                // Pushing 0 to 5 bytes on, to be interpreted as an unsigned integer
                if (opcode1 >= 0x00 && opcode1 <= 0x05)
                {
                    vSolutionsRet.push_back(vch1);
                }
                else
                    break;
            }
            else if (opcode2 == OP_SMALLDATA)
            {
                // small pushdata, <= nMaxDatacarrierBytes
                if (vch1.size() > nMaxDatacarrierBytes)
                    break;
            }
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions)
{
    switch (t)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        return -1;
    case TX_PUBKEY:
        return 1;
    case TX_ESCROW_SENDER:
        return 5;
    case TX_ESCROW:
        return 4;
    case TX_ESCROW_FEE:
        return 3;    
    case TX_PUBKEYHASH:
    case TX_PUBKEYHASH_NONCED:
        return 2;
    case TX_MULTISIG:
        if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
            return -1;
        return vSolutions[0][0] + 1;
    case TX_SCRIPTHASH:
        return 1; // doesn't include args needed by the script
    case TX_DELAYEDPUBKEY:
        return DELAYED_DELTA + 1;
    case TX_DELAYEDPUBKEYHASH:        
        return DELAYED_DELTA + 2;
    case TX_DELAYEDSCRIPTHASH:        
        return DELAYED_DELTA + 1;
    case TX_DELAYEDMULTISIG :        
        if (vSolutions.size() < 1 || vSolutions[0].size() < 1)
            return -1;
        return DELAYED_DELTA + 1 +vSolutions[0][0] + 1;
    }
    return -1;
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_MULTISIG)
    {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    }

    return whichType != TX_NONSTANDARD;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
    vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY || whichType == TX_DELAYEDPUBKEY)
    {
        CPubKey pubKey(vSolutions[whichType == TX_PUBKEY ? 0 : 1]);
        if (!pubKey.IsValid())
            return false;

        addressRet = pubKey.GetID();
        return true;
    }
    else if (whichType == TX_PUBKEYHASH || whichType == TX_DELAYEDPUBKEYHASH)
    {
        addressRet = CKeyID(uint160(vSolutions[whichType == TX_PUBKEYHASH ? 0 : 1]));
        return true;
    }
    else if (whichType == TX_SCRIPTHASH)
    {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    }
    else if (whichType == TX_ESCROW_FEE)
    {
        addressRet = CKeyID(uint160(vSolutions[1]));
        return true;
    }
    else if (whichType == TX_ESCROW_SENDER)
    {
        addressRet = CKeyID(uint160(vSolutions[4]));
        return true;
    }
    else if (whichType == TX_ESCROW)
    {
        addressRet = CKeyID(uint160(vSolutions[4]));
        return true;
    }
    else if (whichType == TX_PUBKEYHASH_NONCED)
    {
        addressRet = CKeyID(uint160(vSolutions[1]));
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_NULL_DATA){
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG || typeRet == TX_DELAYEDMULTISIG)
    {
        nRequiredRet = vSolutions.front()[typeRet == TX_MULTISIG ? 0 : 1];
        for (unsigned int i = (typeRet == TX_MULTISIG ? 1 : 2); i < vSolutions.size()-1; i++)
        {
            CPubKey pubKey(vSolutions[i]);
            if (!pubKey.IsValid())
                continue;

            CTxDestination address = pubKey.GetID();
            addressRet.push_back(address);
        }

        if (addressRet.empty())
            return false;
    }
    else
    {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address))
           return false;
        addressRet.push_back(address);
    }

    return true;
}

namespace
{
class CScriptVisitor : public boost::static_visitor<bool>
{
private:
    CScript *script;
public:
    CScriptVisitor(CScript *scriptin) { script = scriptin; }

    bool operator()(const CNoDestination &dest) const {
        script->clear();
        return false;
    }

    bool operator()(const CKeyID &keyID) const {
        script->clear();
        *script << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CScriptID &scriptID) const {
        script->clear();
        *script << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
        return true;
    }
};
}

CScript GetScriptForDestination(const CTxDestination& dest)
{
    CScript script;

    boost::apply_visitor(CScriptVisitor(&script), dest);
    return script;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    BOOST_FOREACH(const CPubKey& key, keys)
        script << ToByteVector(key);
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}
