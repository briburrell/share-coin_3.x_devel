
/*
 * @copyright
 *
 *  Copyright 2016 Neo Natura
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

#ifndef __SERVER__MNEMONIC_H__
#define __SERVER__MNEMONIC_H__

#include <vector>
#ifdef _WIN32
#include <stdint.h>
#include <iostream>
#endif
#include <algorithm>
#include <sstream>


using namespace std;
typedef vector<string> string_list;


const string DecodeMnemonic(const string_list& words);

bool DecodeMnemonicSecret(int ifaceIndex, const string phrase, CCoinSecret& addr);


string_list EncodeMnemonic(string& seed);

const string EncodeMnemonicSecret(CCoinSecret& secret);



#endif /* ndef __SERVER__MNEMONIC_H__ */


