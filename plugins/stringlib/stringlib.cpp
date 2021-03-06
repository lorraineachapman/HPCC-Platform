/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "stringlib.hpp"
#include "wildmatch.tpp"

static const char * compatibleVersions[] = {
    "STRINGLIB 1.1.06 [fd997dc3feb4ca385d59a12b9dc4beab]", // windows version
    "STRINGLIB 1.1.06 [f8305e66ca26a1447dee66d4a36d88dc]", // linux version
    "STRINGLIB 1.1.07",
    "STRINGLIB 1.1.08",
    "STRINGLIB 1.1.09",
    "STRINGLIB 1.1.10",
    "STRINGLIB 1.1.11",
    "STRINGLIB 1.1.12",
    "STRINGLIB 1.1.13",
    NULL };

#define STRINGLIB_VERSION "STRINGLIB 1.1.14"

const char * EclDefinition =  
"export StringLib := SERVICE\n"
"  string StringFilterOut(const string src, const string _within) : c, pure,entrypoint='slStringFilterOut'; \n"
"  string StringFilter(const string src, const string _within) : c, pure,entrypoint='slStringFilter'; \n"
"  string StringSubstituteOut(const string src, const string _within, const string _newchar) : c, pure,entrypoint='slStringSubsOut'; \n"
"  string StringSubstitute(const string src, const string _within, const string _newchar) : c, pure,entrypoint='slStringSubs'; \n"
"  string StringRepad(const string src, unsigned4 size) : c, pure,entrypoint='slStringRepad'; \n"
"  string StringTranslate(const string src, const string _within, const string _mapping) : c, pure,entrypoint='slStringTranslate'; \n"
"  unsigned integer4 StringFind(const string src, const string tofind, unsigned4 instance ) : c, pure,entrypoint='slStringFind'; \n"
"  unsigned integer4 StringUnboundedUnsafeFind(const string src, const string tofind ) : c, pure,entrypoint='slStringFind2'; \n"
"  unsigned integer4 StringFindCount(const string src, const string tofind) : c, pure,entrypoint='slStringFindCount'; \n"
"  unsigned integer4 EbcdicStringFind(const ebcdic string src, const ebcdic string tofind , unsigned4 instance ) : c,pure,entrypoint='slStringFind'; \n"
"  unsigned integer4 EbcdicStringUnboundedUnsafeFind(const ebcdic string src, const ebcdic string tofind ) : c,pure,entrypoint='slStringFind2'; \n"
"  string StringExtract(const string src, unsigned4 instance) : c,pure,entrypoint='slStringExtract'; \n"
"  string8 GetDateYYYYMMDD() : c,once,entrypoint='slGetDateYYYYMMDD2';\n"
"  varstring GetBuildInfo() : c,once,entrypoint='slGetBuildInfo';\n"
"  string Data2String(const data src) : c,pure,entrypoint='slData2String';\n"
"  data String2Data(const string src) : c,pure,entrypoint='slString2Data';\n"
"  string StringToLowerCase(const string src) : c,pure,entrypoint='slStringToLowerCase';\n"
"  string StringToUpperCase(const string src) : c,pure,entrypoint='slStringToUpperCase';\n"
"  string StringToProperCase(const string src) : c,pure,entrypoint='slStringToProperCase';\n"
"  string StringToCapitalCase(const string src) : c,pure,entrypoint='slStringToCapitalCase';\n"
"  string StringToTitleCase(const string src) : c,pure,entrypoint='slStringToTitleCase';\n"
"  integer4 StringCompareIgnoreCase(const string src1, string src2) : c,pure,entrypoint='slStringCompareIgnoreCase';\n"
"  string StringReverse(const string src) : c,pure,entrypoint='slStringReverse';\n"
"  string StringFindReplace(const string src, const string stok, const string rtok) : c,pure,entrypoint='slStringFindReplace';\n"
"  string StringCleanSpaces(const string src) : c,pure,entrypoint='slStringCleanSpaces'; \n"
"  boolean StringWildMatch(const string src, const string _pattern, boolean _noCase) : c, pure,entrypoint='slStringWildMatch'; \n"
"  boolean StringWildExactMatch(const string src, const string _pattern, boolean _noCase) : c, pure,entrypoint='slStringWildExactMatch'; \n"
"  boolean StringContains(const string src, const string _pattern, boolean _noCase) : c, pure,entrypoint='slStringContains'; \n"
"  string StringExtractMultiple(const string src, unsigned8 mask) : c,pure,entrypoint='slStringExtractMultiple'; \n"
"  unsigned integer4 EditDistance(const string l, const string r) : c, pure,entrypoint='slEditDistance'; \n"
"  boolean EditDistanceWithinRadius(const string l, const string r, unsigned4 radius) : c,pure,entrypoint='slEditDistanceWithinRadius'; \n"
"  unsigned integer4 EditDistanceV2(const string l, const string r) : c, pure,entrypoint='slEditDistanceV2'; \n"
"  boolean EditDistanceWithinRadiusV2(const string l, const string r, unsigned4 radius) : c,pure,entrypoint='slEditDistanceWithinRadiusV2'; \n"
"  string StringGetNthWord(const string src, unsigned4 n) : c, pure,entrypoint='slStringGetNthWord'; \n"
"  unsigned4 StringWordCount(const string src) : c, pure,entrypoint='slStringWordCount'; \n"
"  unsigned4 CountWords(const string src, const string _separator, BOOLEAN allow_blanks) : c, pure,entrypoint='slCountWords'; \n"
"  SET OF STRING SplitWords(const string src, const string _separator, BOOLEAN allow_blanks) : c, pure,entrypoint='slSplitWords'; \n"
"  STRING CombineWords(set of string src, const string _separator) : c, pure,entrypoint='slCombineWords'; \n"
"  UNSIGNED4 StringToDate(const string src, const varstring format) : c, pure,entrypoint='slStringToDate'; \n"
"  UNSIGNED4 MatchDate(const string src, set of varstring formats) : c, pure,entrypoint='slMatchDate'; \n"
"  STRING FormatDate(UNSIGNED4 date, const varstring format) : c, pure,entrypoint='slFormatDate'; \n"
"END;";

STRINGLIB_API bool getECLPluginDefinition(ECLPluginDefinitionBlock *pb) 
{
    if (pb->size == sizeof(ECLPluginDefinitionBlockEx))
    {
        ECLPluginDefinitionBlockEx * pbx = (ECLPluginDefinitionBlockEx *) pb;
        pbx->compatibleVersions = compatibleVersions;
    }
    else if (pb->size != sizeof(ECLPluginDefinitionBlock))
        return false;
    pb->magicVersion = PLUGIN_VERSION;
    pb->version = STRINGLIB_VERSION;
    pb->moduleName = "lib_stringlib";
    pb->ECL = EclDefinition;
    pb->flags = PLUGIN_IMPLICIT_MODULE | PLUGIN_MULTIPLE_VERSIONS;
    pb->description = "StringLib string manipulation library";
    return true;
}

namespace nsStringlib {
    
IPluginContext * parentCtx = NULL;

enum { bitsInUnsigned = sizeof(unsigned) * 8 };

static const char hexchar[] = "0123456789ABCDEF";

static unsigned hex2digit(char c)
{
    switch (c)
    {
    default: case 0: return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    }
}

inline char char_toupper(char c) { return (char)toupper(c); }

inline void clip(unsigned &len, const char * s)
{
    while ( len > 0 && s[len-1]==' ' )
        len--;
}

inline unsigned min3(unsigned a, unsigned b, unsigned c)
{
    unsigned mi;

    mi = a;
    if (b < mi)
    {
        mi = b;
    }
    if (c < mi)
    {
        mi = c;
    }
    return mi;
}

class CEditDistance
{
private:
    unsigned char da[256][256];

public:
    unsigned editDistance(unsigned leftLen, const char * left, unsigned rightLen, const char * right)
    {
        unsigned i, j, cost;
        char l_i, r_j;

        clip(leftLen, left);
        clip(rightLen, right);

        if (leftLen > 255)
        {
            leftLen = 255;
        }
        if (rightLen > 255)
        {
            rightLen = 255;
        }

        if (leftLen == 0)
        {
            return rightLen;
        }
        if (rightLen == 0)
        {
            return leftLen;
        }

        for (i = 0; i <= leftLen; i++)
        {
            da[i][0] = i;
        }

        for (j = 0; j <= rightLen; j++)
        {
            da[0][j] = j;
        }

        for (i = 1; i <= leftLen; i++)
        {
            l_i = left[i - 1];

            for (j = 1; j <= rightLen; j++)
            {
                    r_j = right[j - 1];
                    cost = (l_i == r_j) ? 0 : 1;
                    da[i][j] = min3(da[i-1][j]+1, da[i][j-1]+1, da[i-1][j-1] + cost);
            }
        }

        return da[leftLen][rightLen];
    }

    unsigned editDistance(unsigned leftLen, const char * left, unsigned rightLen, const char * right, unsigned radius)
    {
        unsigned i, j, cost;
        char l_i, r_j;

        clip(leftLen, left);
        clip(rightLen, right);

        if (leftLen > 255)
        {
            leftLen = 255;
        }
        if (rightLen > 255)
        {
            rightLen = 255;
        }

        if (leftLen == 0)
        {
            return rightLen;
        }
        if (rightLen == 0)
        {
            return leftLen;
        }

        if (leftLen > rightLen)
        {
            const char *tstr = left;
            left = right;
            right = tstr;

            unsigned tlen = leftLen;
            leftLen = rightLen;
            rightLen = tlen;
        }

        for (i = 0; i <= leftLen; i++)
        {
            da[i][0] = i;
        }

        for (j = 0; j <= rightLen; j++)
        {
            da[0][j] = j;
        }

        for (i = 1; i <= leftLen; i++)
        {
            l_i = left[i - 1];

            for (j = 1; j <= rightLen; j++)
            {
                    r_j = right[j - 1];
                    cost = (l_i == r_j) ? 0 : 1;
                    da[i][j] = min3(da[i-1][j]+1, da[i][j-1]+1, da[i-1][j-1] + cost);
            }

            // bail out early if ed can't possibly be <= radius
            if ((da[i][rightLen] - (leftLen - i)) > radius)
                return da[i][rightLen];
        }

        return da[leftLen][rightLen];
    }
};

//--- Optimized versions of the edit distance functions above.
inline unsigned mask(unsigned x) { return x & 1; }

unsigned editDistanceV2(unsigned leftLen, const char * left, unsigned rightLen, const char * right)
{
    unsigned i, j;

    clip(leftLen, left);
    clip(rightLen, right);

    if (leftLen > 255)
        leftLen = 255;

    if (rightLen > 255)
        rightLen = 255;

    if (leftLen == 0)
        return rightLen;

    if (rightLen == 0)
        return leftLen;

    //Optimize the storage requirements by
    //i) Only storing two stripes
    //ii) Calculate, but don't store the row comparing against the null string
    unsigned char da[2][256];
    char r_0 = right[0];
    char l_0 = left[0];
    bool matched_l0 = false;
    for (j = 0; j < rightLen; j++)
    {
        if (right[j] == l_0) matched_l0 = true;
        da[0][j] = (matched_l0) ? j : j+1;
    }

    bool matched_r0 = (l_0 == r_0);
    for (i = 1; i < leftLen; i++)
    {
        char l_i = left[i];
        if (l_i == r_0)
            matched_r0 = true;

        byte da_i_0 = matched_r0 ? i : i+1;
        da[mask(i)][0] = da_i_0;
        byte da_i_prevj = da_i_0;
        for (j = 1; j < rightLen; j++)
        {
            char r_j = right[j];
            unsigned char next = (l_i == r_j) ? da[mask(i-1)][j-1] :
                        min3(da[mask(i-1)][j], da_i_prevj, da[mask(i-1)][j-1]) + 1;
            da[mask(i)][j] = next;
            da_i_prevj = next;
        }
    }

    return da[mask(leftLen-1)][rightLen-1];
}

unsigned editDistanceV2(unsigned leftLen, const char * left, unsigned rightLen, const char * right, unsigned radius)
{
    unsigned i, j;

    clip(leftLen, left);
    clip(rightLen, right);

    unsigned minED = (leftLen < rightLen)? rightLen - leftLen: leftLen - rightLen;
    if (minED > radius)
        return minED;

    if (leftLen > 255)
        leftLen = 255;

    if (rightLen > 255)
        rightLen = 255;

    //Checking for leading common substrings actually slows the function down.
    if (leftLen == 0)
        return rightLen;

    if (rightLen == 0)
        return leftLen;

    /*
    This function applies two optimizations over the function above.
    a) Adding a charcter (next row) can at most decrease the edit distance by 1, so short circuit when
       we there is no possiblity of getting within the distance.
    b) We only need to evaluate the martix da[i-radius..i+radius][j-radius..j+radius]
       not taking into account values outside that range [can use max value to prevent access]
    */

    //Optimize the storage requirements by
    //i) Only storing two stripes
    //ii) Calculate, but don't store the row comparing against the null string
    //NB: A byte array is ok because the +1 is added after the minimum, and that will always include 254 as an option.
    unsigned char da[2][255];
    char r_0 = right[0];
    char l_0 = left[0];
    bool matched_l0 = false;
    for (j = 0; j < rightLen; j++)
    {
        if (right[j] == l_0) matched_l0 = true;
        da[0][j] = (matched_l0) ? j : j+1;
    }

    bool matched_r0 = (l_0 == r_0);
    for (i = 1; i < leftLen; i++)
    {
        char l_i = left[i];
        if (l_i == r_0)
            matched_r0 = true;

        byte da_i_0 = matched_r0 ? i : i+1;
        da[mask(i)][0] = da_i_0;
        byte da_i_prevj = da_i_0;

        unsigned first = (i > radius) ? i-radius : 1;
        unsigned last = (i + radius > rightLen) ? rightLen : i + radius;
        for (j = 1; j < rightLen; j++)
        {
            char r_j = right[j];
            unsigned char next = (l_i == r_j) ? da[mask(i-1)][j-1] :
                        min3(da[mask(i-1)][j], da_i_prevj, da[mask(i-1)][j-1]) + 1;
            da[mask(i)][j] = next;
            da_i_prevj = next;
        }

        // bail out early if ed can't possibly be <= radius
        unsigned maxdelta = (leftLen - (i+1));
        if (da_i_prevj > radius + maxdelta)         // if da_i_prvj - maxdelta > radius can't ever get low enough
            return da_i_prevj;
    }

    return da[mask(leftLen-1)][rightLen-1];
}

//This could be further improved in the following ways:
// * Only use 2*radius bytes of temporary storage - I doubt it is worth it.
// * special case edit1 - you could use variables for the 6 interesting array elements, and get
//   rid of the array completely.  You could also unwind the first (and last iterations).
// * I suspect the early exit condition could be improved depending the lengths of the strings.
unsigned editDistanceV3(unsigned leftLen, const char * left, unsigned rightLen, const char * right, unsigned radius)
{
    if (radius >= 255)
        return 255;

    clip(leftLen, left);
    clip(rightLen, right);

    unsigned minED = (leftLen < rightLen)? rightLen - leftLen: leftLen - rightLen;
    if (minED > radius)
        return minED;

    if (leftLen > 255)
        leftLen = 255;

    if (rightLen > 255)
        rightLen = 255;

    //Checking for leading common substrings actually slows the function down.
    if (leftLen == 0)
        return rightLen;

    if (rightLen == 0)
        return leftLen;

    /*
    This function applies two optimizations over the function above.
    a) Adding a charcter (next row) can at most decrease the edit distance by 1, so short circuit when
       we there is no possiblity of getting within the distance.
    b) We only need to evaluate the martix da[i-radius..i+radius][j-radius..j+radius]
       not taking into account values outside that range [can use max value to prevent access]
    */

    //Optimize the storage requirements by
    //i) Only storing two stripes
    //ii) Calculate, but don't store the row comparing against the null string
    unsigned char da[2][256];
    char r_0 = right[0];
    char l_0 = left[0];
    bool matched_l0 = false;
    for (unsigned j = 0; j < rightLen; j++)
    {
        if (right[j] == l_0) matched_l0 = true;
        da[0][j] = (matched_l0) ? j : j+1;
    }

    bool matched_r0 = (l_0 == r_0);
    for (unsigned i = 1; i < leftLen; i++)
    {
        char l_i = left[i];
        if (l_i == r_0)
            matched_r0 = true;

        byte da_i_0 = matched_r0 ? i : i+1;
        da[mask(i)][0] = da_i_0;
        byte da_i_prevj = da_i_0;
        unsigned low = i-radius;
        unsigned high = i+radius;
        unsigned first = (i > radius) ? low : 1;
        unsigned last = (high >= rightLen) ? rightLen : high +1;

        for (unsigned j = first; j < last; j++)
        {
            char r_j = right[j];
            unsigned next = da[mask(i-1)][j-1];
            if (l_i != r_j)
            {
                if (j != low)
                {
                    if (next > da_i_prevj)
                        next = da_i_prevj;
                }
                if (j != high)
                {
                    byte da_previ_j = da[mask(i-1)][j];
                    if (next > da_previ_j)
                        next = da_previ_j;
                }
                next++;
            }
            da[mask(i)][j] = next;
            da_i_prevj = next;
        }

        // bail out early if ed can't possibly be <= radius
        // Only considering a strip down the middle of the matrix, so the maximum the score can ever be adjusted is 2xradius
        unsigned max_valid_score = 3*radius;

        // But maximum is also 1 for every difference in string length - comes in to play when close to the end.
        //In 32bit goes slower for radius=1 I suspect because running out of registers.  Retest in 64bit.
        if (radius > 1)
        {
            unsigned max_distance = radius + (leftLen - (i+1)) + (rightLen - last);
            if (max_valid_score > max_distance)
                max_valid_score = max_distance;
        }
        if (da_i_prevj > max_valid_score)
            return da_i_prevj;
    }

    return da[mask(leftLen-1)][rightLen-1];
}

} // namespace

//-------------------------------------------------------------------------------------------------------------------------------------------
// Exported functions are NOT in the namespace

using namespace nsStringlib; 

STRINGLIB_API void setPluginContext(IPluginContext * _ctx) { parentCtx = _ctx; }

STRINGLIB_API void STRINGLIB_CALL slStringFilterOut(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned hitLen, const char * hit)
{
    char *temp = (char *)CTXMALLOC(parentCtx, srcLen);
    unsigned tlen = 0;
    if (hitLen==1) 
    {
        char test = *hit;
        for ( unsigned i = 0; i < srcLen; i++ )
        {
            char c = src[i];
            if (c!=test)
                temp[tlen++] = c;
        }
    }
    else {
        unsigned filter[256/bitsInUnsigned];
        memset(filter,0,sizeof(filter));
        for (unsigned j = 0; j < hitLen; j++ )
        {
            unsigned c = (unsigned char)hit[j];
            filter[c/bitsInUnsigned] |= (1<<(c%bitsInUnsigned));
        }
        for ( unsigned i = 0; i < srcLen; i++ )
        {
            unsigned c = (unsigned char)src[i];
            if ((filter[c/bitsInUnsigned] & (1<<(c%bitsInUnsigned))) == 0)
                temp[tlen++] = (char)c;
        }
    }
    tgt = (char *)CTXREALLOC(parentCtx, temp, tlen);
    tgtLen = tlen;
}

STRINGLIB_API void STRINGLIB_CALL slStringFilter(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned hitLen, const char * hit)
{
    char *temp = (char *)CTXMALLOC(parentCtx, srcLen);
    unsigned tlen = 0;
    unsigned filter[256/bitsInUnsigned];
    memset(filter,0,sizeof(filter));
    for (unsigned j = 0; j < hitLen; j++ )
    {
        unsigned c = (unsigned char)hit[j];
        filter[c/bitsInUnsigned] |= (1<<(c%bitsInUnsigned));
    }
    for ( unsigned i = 0; i < srcLen; i++ )
    {
        unsigned c = (unsigned char)src[i];
        if ((filter[c/bitsInUnsigned] & (1<<(c%bitsInUnsigned))) != 0)
            temp[tlen++] = (char)c;
    }
    tgt = (char *)CTXREALLOC(parentCtx, temp, tlen);
    tgtLen = tlen;
}

STRINGLIB_API void STRINGLIB_CALL slStringSubsOut(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned hitLen, const char * hit, unsigned newCharLen, const char * newChar)
{
    bool filter[256];
    memset(filter,0,sizeof(filter));
    for (unsigned j = 0; j < hitLen; j++ )
    {
        unsigned char c = ((unsigned char *)hit)[j];
        filter[c] = true;
    }

    tgt = (char *)CTXMALLOC(parentCtx, srcLen);
    
    if (newCharLen > 0)
    {
        for ( unsigned i = 0; i < srcLen; i++ )
        {
            unsigned char c = ((unsigned char *)src)[i];
            if (!filter[c])
                tgt[i] = c;
            else
                tgt[i] = ((char *)newChar)[0];
        }
    }
    else
    {
        memcpy(tgt, src, srcLen);
    }

    tgtLen = srcLen;
}

STRINGLIB_API void STRINGLIB_CALL slStringSubs(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned hitLen, const char * hit, unsigned newCharLen, const char * newChar)
{
    bool filter[256];
    memset(filter,0,sizeof(filter));
    for (unsigned j = 0; j < hitLen; j++ )
    {
        unsigned char c = ((unsigned char *)hit)[j];
        filter[c] = true;
    }

    tgt = (char *)CTXMALLOC(parentCtx, srcLen);

    if (newCharLen > 0)
    {
        for ( unsigned i = 0; i < srcLen; i++ )
        {
            unsigned char c = ((unsigned char *)src)[i];
            if (filter[c])
                tgt[i] = c;
            else
                tgt[i] = ((char *)newChar)[0];
        }
    }
    else
    {
        memcpy(tgt, src, srcLen);
    }

    tgtLen = srcLen;
}

STRINGLIB_API void STRINGLIB_CALL slStringTranslate(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned hitLen, const char * hit, unsigned mappingLen, const char * mapping)
{
    char mapped[256];
    for (unsigned i=0; i < sizeof(mapped); i++)
        mapped[i] = i;
    if (hitLen == mappingLen)
    {
        for (unsigned j = 0; j < hitLen; j++ )
        {
            unsigned char c = ((unsigned char *)hit)[j];
            mapped[c] = mapping[j];
        }
    }

    char * ret = (char *)CTXMALLOC(parentCtx, srcLen);
    for ( unsigned i = 0; i < srcLen; i++ )
    {
        unsigned char c = ((unsigned char *)src)[i];
        ret[i] = mapped[c];
    }

    tgt = ret;
    tgtLen = srcLen;
}

STRINGLIB_API void STRINGLIB_CALL slStringRepad(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned tLen)
{
    char *base = (char *)src;
    while ( srcLen && *base == ' ' )
    {
        srcLen--;
        base++;
    }
    while ( srcLen && base[srcLen-1] == ' ' )
        srcLen--;
    if ( srcLen > tLen )
        srcLen = tLen;
    tgt = (char *)CTXMALLOC(parentCtx, tLen);
    tgtLen = tLen;
    memcpy(tgt,base,srcLen);
    memset(tgt+srcLen,' ',tLen-srcLen);
}

STRINGLIB_API unsigned STRINGLIB_CALL slStringFind(unsigned srcLen, const char * src, unsigned hitLen, const char * hit, unsigned instance)
{
    if ( srcLen < hitLen )
        return 0;   
    if (hitLen==1) {           // common case optimization
        const char *p=src;
        const char *e = p+srcLen;
        char c = *hit;
        while (p!=e)
            if ((*(p++)==c))
                if (!--instance)
                    return (unsigned)(p-src);
    }
    else 
    {
        unsigned steps = srcLen-hitLen+1;
        for ( unsigned i = 0; i < steps; i++ )
        {
            if ( !memcmp((char *)src+i,hit,hitLen) )
            {
                if ( !--instance )
                    return i+1;
                if (hitLen > 1)
                    i += (hitLen-1);
            }
        }
    }
    return 0;
}

STRINGLIB_API unsigned STRINGLIB_CALL slStringFindCount(unsigned srcLen, const char * src, unsigned hitLen, const char * hit)
{
    if ( srcLen < hitLen )
        return 0;   
    unsigned matches = 0;
    if (hitLen==1) {           // common case optimization
        const char *p=src;
        const char *e = p+srcLen;
        char c = *hit;
        while (p!=e)
            if ((*(p++)==c))
                matches++;
    }
    else 
    {
        unsigned steps = srcLen-hitLen+1;
        for ( unsigned i = 0; i < steps; i++ )
        {
            if ( !memcmp((char *)src+i,hit,hitLen) )
            {
                matches++;
                if (hitLen > 1)
                    i += (hitLen-1);
            }
        }
    }
    return matches;
}

STRINGLIB_API unsigned STRINGLIB_CALL slStringFind2(unsigned /*srcLen*/, const char * src, unsigned hitLen, const char * hit)
{
    if (hitLen==1) {           // common case optimization
        const char *p=src;
        char c = *hit;
        while (*(p++)!=c);
        return (unsigned)(p-src);
    }
    for ( unsigned i = 0; ; i++ )
        if ( !memcmp((char *)src+i,hit,hitLen) )
            return i+1;
    return 0;
}

STRINGLIB_API void STRINGLIB_CALL slStringExtract(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned instance)
{
    tgtLen = 0;
    tgt = NULL;
    char * finger = (char *)src;
    if ( !instance )
        return;
    while ( --instance )
    {
        while ( srcLen && *finger != ',' )
        {
            srcLen--;
            finger++;
        }
        if ( !srcLen )
            return;
        srcLen--; // Skip ,
        finger++;
    }
    unsigned len = 0;
    for ( ; len < srcLen; len++ )
        if ( finger[len] == ',' )
            break;
    tgt = (char *)CTXMALLOC(parentCtx, len);
    memcpy(tgt,finger,len);
    tgtLen = len;
}

STRINGLIB_API void STRINGLIB_CALL slStringExtractMultiple(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned __int64 mask)
{
    tgtLen = 0;
    tgt = NULL;
    char * finger = (char *)src;
    unsigned __int64 thisInstance = 1;
    while (mask)
    {
        while ( srcLen && *finger != ',' )
        {
            srcLen--;
            finger++;
        }
        if (mask & thisInstance)
        {
            mask &= ~thisInstance;
            unsigned matchLen = (unsigned)(finger - src);
            if (!tgt)
                tgt = (char *) CTXMALLOC(parentCtx, matchLen + srcLen);
            else
                tgt[tgtLen++] = ',';
            memcpy(tgt+tgtLen, src, finger - src);
            tgtLen += matchLen;
        }
        thisInstance <<= 1;
        if ( !srcLen )
            break;
        srcLen--; // Skip the ','
        finger++;
        src = finger;
    }
}


STRINGLIB_API char * STRINGLIB_CALL slGetDateYYYYMMDD(void)
{
    char * result = (char *)CTXMALLOC(parentCtx, 9);
    time_t ltime;
    time( &ltime );
    tm *today = localtime( &ltime );
    strftime(result, 9, "%Y%m%d", today);
    return result;
}

STRINGLIB_API void STRINGLIB_CALL slGetDateYYYYMMDD2(char * ret)
{
    char temp[9];
    time_t ltime;
    time( &ltime );
    tm *today = localtime( &ltime );
    strftime(temp, 9, "%Y%m%d", today);
    memcpy(ret, temp, 8);
}

STRINGLIB_API char * STRINGLIB_CALL slGetBuildInfo(void)
{ 
    return CTXSTRDUP(parentCtx, STRINGLIB_VERSION);
}

STRINGLIB_API void STRINGLIB_CALL slData2String(size32_t & __ret_len,char * & __ret_str,unsigned _len_y, const void * y)
{
    char *out = (char *)CTXMALLOC(parentCtx, _len_y * 2);
    char *res = out;
    
    unsigned char *yy = (unsigned char *) y;
    for (unsigned int i = 0; i < _len_y; i++)
    {
        *out++ = hexchar[yy[i] >> 4];
        *out++ = hexchar[yy[i] & 0x0f];
    }
    __ret_len = _len_y * 2;
    __ret_str = res;
}

STRINGLIB_API void STRINGLIB_CALL slString2Data(size32_t & __ret_len,void * & __ret_str,unsigned _len_src,const char * src)
{
    // trailing nibbles are ignored
    // embedded spaces are ignored
    // illegal hex values are treated as zero
    // we could do a stricter one if it was considered desirable.
    char *out = (char *)CTXMALLOC(parentCtx, _len_src / 2);
    char *target = out;
    for (;;)
    {
        while (_len_src > 1 && isspace(*src))
        {
            src++;
            _len_src--;
        }
        if (_len_src < 2)
            break;
        *target++ = (hex2digit(src[0]) << 4) | hex2digit(src[1]);
        _len_src -= 2;
        src += 2;
    }
    __ret_len = (size32_t)(target - out);
    __ret_str = out;
} 

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToLowerCase(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    char * res = (char *)CTXMALLOC(parentCtx, srcLen);
    
    for (unsigned int i=0;i<srcLen;i++)
        res[i] = tolower(src[i]);
    
    tgt = res;
    tgtLen = srcLen;
}


// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToUpperCase(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    char * res = (char *)CTXMALLOC(parentCtx, srcLen);
    
    for (unsigned int i=0;i<srcLen;i++)
        res[i] = toupper(src[i]);
    
    tgt = res;
    tgtLen = srcLen;
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToProperCase(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    tgt = (char *)CTXMALLOC(parentCtx, srcLen);
    char * res = tgt;
    
    bool seenSpace = true;
    for (unsigned int i=0;i<srcLen;i++)
    {
        char c = src[i];
        *tgt++ = seenSpace ? toupper(c) : c;
        seenSpace = (c==' ');
    }
    
    tgt = res;
    tgtLen = srcLen;
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToCapitalCase(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    char * const result = (char *)CTXMALLOC(parentCtx, srcLen);
    
    bool upperPending = true;
    for (unsigned int i=0;i<srcLen;i++)
    {
        byte c = src[i];
        result[i] = upperPending ? toupper(c) : c;
        upperPending = !isalnum(c);
    }

    tgt = result;
    tgtLen = srcLen;
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToTitleCase(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    char * const result = (char *)CTXMALLOC(parentCtx, srcLen);

    bool upperPending = true;
    for (unsigned int i=0;i<srcLen;i++)
    {
        byte c = src[i];
        result[i] = upperPending ? toupper(c) : tolower(c);
        upperPending = !isalnum(c);
    }

    tgt = result;
    tgtLen = srcLen;
}

// -----------------------------------------------------------------

STRINGLIB_API int STRINGLIB_CALL slStringCompareIgnoreCase (unsigned src1Len, const char * src1, unsigned src2Len, const char * src2)
{
    unsigned int i;
    for (i=0;i < src1Len && i < src2Len;i++)
    {
        byte lc = src1[i];
        byte rc = src2[i];
        if (lc != rc)
        {
            lc = tolower(lc);
            rc = tolower(rc);
            if (lc != rc)
                return lc > rc ? 1 : -1;
        }
    }
        
    while (i < src1Len)
    {
        if (src1[i++] != ' ')
            return 1;
    }
    while (i < src2Len)
    {
        if (src2[i++] != ' ')
            return -1;
    }
    return 0;
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringReverse (unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src)
{
    char * res = (char *)CTXMALLOC(parentCtx, srcLen);

    unsigned int n = srcLen - 1;
    for (unsigned int i=0;i<srcLen;i++)
        res[i] = src[n-i];
    
    tgt = res;
    tgtLen = srcLen;
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringFindReplace (unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned stokLen, const char * stok, unsigned rtokLen, const char * rtok)
{
    if ( srcLen < stokLen || stokLen == 0)
    {
        tgt = (char *) CTXMALLOC(parentCtx, srcLen);
        memcpy(tgt, src, srcLen);
        tgtLen = srcLen;
    }
    else
    {
        unsigned steps = srcLen-stokLen+1;
        unsigned tgtmax = rtokLen > stokLen ? srcLen + steps * (rtokLen - stokLen) : srcLen;
        // This is the upper limit on target size - not a problem if we allocate a bit too much
        char * res = (char *)CTXMALLOC(parentCtx, tgtmax);
        tgt = res;
        unsigned i;
        for ( i = 0; i < steps; )
        {
            if ( !memcmp(src+i,stok,stokLen) )
            {
                memcpy(res, rtok, rtokLen);
                res += rtokLen;
                i += stokLen;
            }
            else
                *res++ = src[i++];
        }
        while (i <srcLen)
            *res++ = src[i++];
        tgtLen = (size32_t)(res - tgt);
    }
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringCleanSpaces(size32_t & __ret_len,char * & __ret_str,unsigned _len_instr,const char * instr)
{
    // remove double spaces
    char *out = (char *) CTXMALLOC(parentCtx, _len_instr);
    char *origout = out;
    bool spacePending = false;
    bool atStart = true;
    for(unsigned idx = 0; idx < _len_instr; idx++)
    {
        char c = *instr++;
        switch (c)
        {
        case ' ':
        case '\t':
            spacePending = true;
            break;
        default:
            if (spacePending && !atStart)
                *out++ = ' ';
            spacePending = false;
            atStart = false;
            *out++ = c;
            break;
        }
    }
    __ret_str = origout;
    __ret_len = (size32_t)(out - origout);
}

STRINGLIB_API bool STRINGLIB_CALL slStringWildMatch(unsigned srcLen, const char * src, unsigned patLen, const char * pat, bool noCase)
{
    return wildTrimMatch<char, char_toupper, '?', '*', ' '>(src, srcLen, pat, patLen, noCase);
}


STRINGLIB_API bool STRINGLIB_CALL slStringWildExactMatch(unsigned srcLen, const char * src, unsigned patLen, const char * pat, bool noCase)
{
    return wildMatch<char, char_toupper, '?', '*'>(src, srcLen, pat, patLen, noCase);
}

STRINGLIB_API bool STRINGLIB_CALL slStringContains(unsigned srcLen, const char * src, unsigned patLen, const char * pat, bool noCase)
{
    unsigned char srcCount[256];
    memset(srcCount, 0, 256);
    while (srcLen && src[srcLen-1]==' ')
        srcLen--;
    while(srcLen-- > 0)
    {
        byte c = *src++;
        if (noCase)
            c = toupper(c);
        srcCount[c]++;
    }

    while (patLen && pat[patLen-1]==' ')
        patLen--;
    while(patLen-- > 0)
    {
        byte c = *pat++;

        if (noCase)
            c = toupper(c);
        if (srcCount[c] == 0)
            return false;
        else
            srcCount[c]--;
    }
    return true;
}

STRINGLIB_API unsigned STRINGLIB_CALL slEditDistance(unsigned leftLen, const char * left, unsigned rightLen, const char * right)
{
    CEditDistance * ed = new CEditDistance();
    unsigned rval = ed->editDistance(leftLen, left, rightLen, right);
    delete ed;
    return rval;
}


STRINGLIB_API bool STRINGLIB_CALL slEditDistanceWithinRadius(unsigned leftLen, const char * left, unsigned rightLen, const char * right, unsigned radius)
{
    unsigned minED = (leftLen < rightLen)? rightLen - leftLen: leftLen - rightLen;

    if (minED > radius)
    {
        return false;
    }
    else
    {
        CEditDistance *ed = new CEditDistance();
        unsigned rval = ed->editDistance(leftLen, left, rightLen, right, radius);
        delete ed;
        return (rval <= radius);
    }
}

STRINGLIB_API unsigned STRINGLIB_CALL slEditDistanceV2(unsigned leftLen, const char * left, unsigned rightLen, const char * right)
{
    return nsStringlib::editDistanceV2(leftLen, left, rightLen, right);
}


STRINGLIB_API bool STRINGLIB_CALL slEditDistanceWithinRadiusV2(unsigned leftLen, const char * left, unsigned rightLen, const char * right, unsigned radius)
{
    return nsStringlib::editDistanceV3(leftLen, left, rightLen, right, radius) <= radius;
}

STRINGLIB_API void STRINGLIB_CALL slStringGetNthWord(unsigned & tgtLen, char * & tgt, unsigned srcLen, const char * src, unsigned n)
{
    const char* start = 0;
    const char* end = 0;
    // skip any leading white space
    while (srcLen>0 && (unsigned char)*src<=0x20) {
        src++;
        srcLen--;
    }
    while (srcLen>0 && n>0) {
        start = src;
        n--;
        // go to the next white space
        while (srcLen>0 && (unsigned char)*src>0x20) {
            src++;
            srcLen--;
        }
        end = src;
        // skip white space again
        while (srcLen>0 && (unsigned char)*src<=0x20) {
            src++;
            srcLen--;
        }
    }
    if (!n && (end-start)) {
        tgt = (char *)CTXMALLOC(parentCtx, end-start);
        memcpy(tgt,start,end-start);
        tgtLen = end-start;
    } else {
        tgt = 0;
        tgtLen = 0;
    }

}

STRINGLIB_API unsigned STRINGLIB_CALL slStringWordCount(unsigned srcLen,const char * src)
{
    // skip any leading white space
    unsigned word_count = 0;
    while (srcLen>0 && (unsigned char)*src<=0x20) {
        src++;
        srcLen--;
    }

    while (srcLen>0) {
        word_count++;
        // go to the next white space
        while (srcLen>0 && (unsigned char)*src>0x20) {
            src++;
            srcLen--;
        }
        // skip white space again
        while (srcLen>0 && (unsigned char)*src<=0x20) {
            src++;
            srcLen--;
        }
    }
    return word_count;
}

//--------------------------------------------------------------------------------------------------------------------

STRINGLIB_API unsigned STRINGLIB_CALL slCountWords(size32_t lenSrc, const char * src, size32_t lenSeparator, const char * separator, bool allowBlankItems)
{
    if (lenSrc == 0)
        return 0;

    if ((lenSeparator == 0) || (lenSrc < lenSeparator))
        return 1;

    unsigned numWords=0;
    const char * end = src + lenSrc;
    const char * max = end - (lenSeparator - 1);
    const char * cur = src;
    const char * startWord = NULL;
    //MORE: optimize lenSeparator == 1!
    while (cur < max)
    {
        if (memcmp(cur, separator, lenSeparator) == 0)
        {
            if (startWord || allowBlankItems)
            {
                numWords++;
                startWord = NULL;
            }
            cur += lenSeparator;
        }
        else
        {
            if (!startWord)
                startWord = cur;
            cur++;
        }
    }
    if (startWord || (cur != end) || allowBlankItems)
        numWords++;
    return numWords;
}


static unsigned calcWordSetSize(size32_t lenSrc, const char * src, size32_t lenSeparator, const char * separator, bool allowBlankItems)
{
    if (lenSrc == 0)
        return 0;

    if ((lenSeparator == 0) || (lenSrc < lenSeparator))
        return sizeof(size32_t) + lenSrc;

    unsigned sizeWords=0;
    const char * end = src + lenSrc;
    const char * max = end - (lenSeparator - 1);
    const char * cur = src;
    const char * startWord = NULL;
    //MORE: optimize lenSeparator == 1!
    while (cur < max)
    {
        if (memcmp(cur, separator, lenSeparator) == 0)
        {
            if (startWord)
            {
                sizeWords += sizeof(size32_t) + (cur - startWord);
                startWord = NULL;
            }
            else if (allowBlankItems)
                sizeWords += sizeof(size32_t);

            cur += lenSeparator;
        }
        else
        {
            if (!startWord)
                startWord = cur;
            cur++;
        }
    }
    if (startWord || (cur != end) || allowBlankItems)
    {
        if (!startWord)
            startWord = cur;
        sizeWords += sizeof(size32_t) + (end - startWord);
    }
    return sizeWords;
}

STRINGLIB_API void STRINGLIB_CALL slSplitWords(bool & __isAllResult, size32_t & __lenResult, void * & __result, size32_t lenSrc, const char * src, size32_t lenSeparator, const char * separator, bool allowBlankItems)
{
    unsigned sizeRequired = calcWordSetSize(lenSrc, src, lenSeparator, separator, allowBlankItems);
    char * const result = static_cast<char *>(CTXMALLOC(parentCtx, sizeRequired));
    __isAllResult = false;
    __lenResult = sizeRequired;
    __result = result;

    if (lenSrc == 0)
        return;

    if ((lenSeparator == 0) || (lenSrc < lenSeparator))
    {
        *((size32_t *)result) = lenSrc;
        memcpy(result+sizeof(size32_t), src, lenSrc);
        return;
    }

    unsigned sizeWords=0;
    char * target = result;
    const char * end = src + lenSrc;
    const char * max = end - (lenSeparator - 1);
    const char * cur = src;
    const char * startWord = NULL;
    //MORE: optimize lenSeparator == 1!
    while (cur < max)
    {
        if (memcmp(cur, separator, lenSeparator) == 0)
        {
            if (startWord || allowBlankItems)
            {
                size32_t len = startWord ? (cur - startWord) : 0;
                memcpy(target, &len, sizeof(len));
                memcpy(target+sizeof(size32_t), startWord, len);
                target += sizeof(size32_t) + len;
                startWord = NULL;
            }

            cur += lenSeparator;
        }
        else
        {
            if (!startWord)
                startWord = cur;
            cur++;
        }
    }
    if (startWord || (cur != end) || allowBlankItems)
    {
        if (!startWord)
            startWord = cur;
        size32_t len = (end - startWord);
        memcpy(target, &len, sizeof(len));
        memcpy(target+sizeof(size32_t), startWord, len);
        target += sizeof(size32_t) + len;
    }
    assert(target == result + sizeRequired);
//        ctx->fail(1, "Size mismatch in StringLib.SplitWords");
}


static unsigned countWords(size32_t lenSrc, const char * src)
{
    unsigned count = 0;
    unsigned offset = 0;
    while (offset < lenSrc)
    {
        size32_t len;
        memcpy(&len, src+offset, sizeof(len));
        offset += sizeof(len) + len;
        count++;
    }
    return count;
}


STRINGLIB_API void STRINGLIB_CALL slCombineWords(size32_t & __lenResult, void * & __result, bool isAllSrc, size32_t lenSrc, const char * src, size32_t lenSeparator, const char * separator, bool allowBlankItems)
{
    if (lenSrc == 0)
    {
        __lenResult = 0;
        __result = NULL;
        return;
    }

    unsigned numWords = countWords(lenSrc, src);
    size32_t sizeRequired = lenSrc - numWords * sizeof(size32_t) + (numWords-1) * lenSeparator;
    char * const result = static_cast<char *>(CTXMALLOC(parentCtx, sizeRequired));
    __lenResult = sizeRequired;
    __result = result;

    char * target = result;
    unsigned offset = 0;
    while (offset < lenSrc)
    {
        if ((offset != 0) && lenSeparator)
        {
            memcpy(target, separator, lenSeparator);
            target += lenSeparator;
        }

        size32_t len;
        memcpy(&len, src+offset, sizeof(len));
        offset += sizeof(len);
        memcpy(target, src+offset, len);
        target += len;
        offset += len;
    }
    assert(target == result + sizeRequired);
}

//--------------------------------------------------------------------------------------------------------------------

inline bool readValue(unsigned & value, size32_t & _offset, size32_t lenStr, const char * str, unsigned max)
{
    unsigned total = 0;
    unsigned offset = _offset;
    if (lenStr - offset < max)
        max = lenStr - offset;
    unsigned i=0;
    for (; i < max; i++)
    {
        char next = str[offset+i];
        if (next >= '0' && next <= '9')
            total = total * 10 + (next - '0');
        else
            break;
    }
    if (i == 0)
        return false;
    value = total;
    _offset = offset+i;
    return true;
}

const char * const monthNames[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

inline bool matchString(unsigned & value, size32_t & strOffset, size32_t lenStr, const byte * str, unsigned num, const char * const * strings, unsigned minMatch)
{
    unsigned startOffset = strOffset;
    for (unsigned i =0; i < num; i++)
    {
        const char * cur = strings[i];
        unsigned offset = startOffset;
        while (offset < lenStr)
        {
            byte next = *cur++;
            if (!next || toupper(next) != toupper(str[offset]))
                break;
            offset++;
        }
        if (offset - startOffset >= minMatch)
        {
            value = i;
            strOffset = offset;
            return true;
        }
    }
    return false;
}

//This implements a subset of the specifiers allowed for strptime
//Another difference is it works on a string with a separate length
static const char * simple_strptime(size32_t lenStr, const char * str, const char * format, struct tm * tm)
{
    const char * curFormat = format;
    size32_t offset = 0;
    const byte * src = (const byte *)str;
    unsigned value;

    byte next;
    while ((next = *curFormat++) != '\0')
    {
        if (next == '%')
        {
            switch (*curFormat++)
            {
            case 't':
                while ((offset < lenStr) && isspace(src[offset]))
                    offset++;
                break;
            case 'Y':
                if (!readValue(value, offset, lenStr, str, 4))
                    return NULL;
                tm->tm_year = value-1900;
                break;
            case 'y':
                if (!readValue(value, offset, lenStr, str, 2))
                    return NULL;
                tm->tm_year = value > 68 ? value : value + 100;
                break;
            case 'm':
                if (!readValue(value, offset, lenStr, str, 2) || (value < 1) || (value > 12))
                    return NULL;
                tm->tm_mon = value-1;
                break;
            case 'd':
                if (!readValue(value, offset, lenStr, str, 2) || (value < 1) || (value > 31))
                    return NULL;
                tm->tm_mday = value;
                break;
            case 'b':
            case 'B':
            case 'h':
                if (!matchString(value, offset, lenStr, src, sizeof(monthNames)/sizeof(*monthNames), monthNames, 3))
                    return NULL;
                tm->tm_mon = value;
                break;
            case 'H':
                if (!readValue(value, offset, lenStr, str, 2)|| (value > 24))
                    return NULL;
                tm->tm_hour = value;
                break;
            case 'M':
                if (!readValue(value, offset, lenStr, str, 2)|| (value > 59))
                    return NULL;
                tm->tm_min = value;
                break;
            case 'S':
                if (!readValue(value, offset, lenStr, str, 2)|| (value > 59))
                    return NULL;
                tm->tm_sec = value;
                break;
            default:
                return NULL;
            }
        }
        else
        {
            if (isspace(next))
            {
                while ((offset < lenStr) && isspace(src[offset]))
                    offset++;
            }
            else
            {
                if ((offset >= lenStr) || (src[offset++] != next))
                    return NULL;
            }
        }
    }
    return str+offset;
}


inline unsigned makeDate(const tm & tm)
{
    return (tm.tm_year + 1900) * 10000 + (tm.tm_mon + 1) * 100 + tm.tm_mday;
}

inline void extractDate(tm & tm, unsigned date)
{
    tm.tm_year = (date / 10000) - 1900;
    tm.tm_mon = ((date / 100) % 100) - 1;
    tm.tm_mday = (date % 100);
}

STRINGLIB_API unsigned STRINGLIB_CALL slStringToDate(size32_t lenS, const char * s, const char * fmtin)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (simple_strptime(lenS, s, fmtin, &tm))
        return makeDate(tm);
    return 0;
}


STRINGLIB_API unsigned STRINGLIB_CALL slMatchDate(size32_t lenS, const char * s, bool isAllFormats, unsigned lenFormats, const void * _formats)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    const char * formats = (const char *)_formats;
    for (unsigned off=0; off < lenFormats; )
    {
        const char * curFormat = formats+off;
        if (simple_strptime(lenS, s, curFormat, &tm))
            return makeDate(tm);
        off += strlen(curFormat) + 1;
    }
    return 0;
}

STRINGLIB_API void STRINGLIB_CALL slFormatDate(size32_t & __lenResult, char * & __result, unsigned date, const char * format)
{
    size32_t len = 0;
    char * out = NULL;
    if (date)
    {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        extractDate(tm, date);
        char buf[255];
        strftime(buf, sizeof(buf), format, &tm);
        len = strlen(buf);
        out = static_cast<char *>(CTXMALLOC(parentCtx, len));
        memcpy(out, buf, len);
    }

    __lenResult = len;
    __result = out;
}


//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------
// Legacy functions that only work on fixed length strings
//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringExtract50(char *tgt, unsigned srcLen, const char * src, unsigned instance)
{
    unsigned lenret;
    char * resret;
    slStringExtract(lenret,resret,srcLen,src,instance);
    if (lenret >= 50)
        memcpy(tgt,resret,50);
    else
    {
        memcpy(tgt,resret,lenret);
        memset(tgt+lenret,' ',50-lenret);
    }
    CTXFREE(parentCtx, resret);
}

STRINGLIB_API void STRINGLIB_CALL slGetBuildInfo100(char *tgt)
{
    size32_t len = (size32_t) strlen(STRINGLIB_VERSION);
    if (len >= 100)
        len = 100;
    memcpy(tgt, STRINGLIB_VERSION, len);
    memset(tgt+len, ' ', 100-len);
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToLowerCase80(char *tgt, unsigned srcLen, const char * src)
{
    unsigned int i;
    for (i=0;i<srcLen && i < 80;i++)
        *tgt++ = tolower(src[i]);
    while (i < 80)
    {
        *tgt++=' ';
        i++;
    }
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringToUpperCase80(char *tgt, unsigned srcLen, const char * src)
{
    unsigned int i;
    for (i=0;i<srcLen && i < 80;i++)
        *tgt++ = toupper(src[i]);
    while (i < 80)
    {
        *tgt++=' ';
        i++;
    }
}

// -----------------------------------------------------------------

STRINGLIB_API void STRINGLIB_CALL slStringFindReplace80(char * tgt, unsigned srcLen, const char * src, unsigned stokLen, const char * stok, unsigned rtokLen, const char * rtok)
{
    if ( srcLen < stokLen )
    {
        if (srcLen > 80)
            srcLen = 80;
        memcpy(tgt, src, srcLen);
        if (srcLen < 80)
            memset(tgt+srcLen, ' ', 80 - srcLen);
    }
    else
    {
        unsigned steps = srcLen-stokLen+1;
        unsigned i;
        unsigned lim = 80;
        for ( i = 0; i < steps && lim > 0; )
        {
            if ( !memcmp(src+i,stok,stokLen) )
            {
                if (rtokLen > lim)
                    rtokLen = lim;
                memcpy(tgt, rtok, rtokLen);
                tgt += rtokLen;
                i += stokLen;
                lim -= rtokLen;
            }
            else
            {
                *tgt++ = src[i++];
                lim--;
            }
        }
        while (i < srcLen && lim > 0)
        {
            *tgt++ = src[i++];
            lim--;
        }
        if (lim)
            memset(tgt, ' ', lim);
    }
}

STRINGLIB_API void STRINGLIB_CALL slStringCleanSpaces25(char *__ret_str,unsigned _len_instr,const char * instr)
{
    // remove double spaces
    // Fixed width version for Hole
    unsigned outlen = _len_instr;
    if (outlen < 25)
        outlen = 25;
    char *out = (char *) alloca(outlen);
    char *origout = out;
    bool spacePending = false;
    bool atStart = true;
    for(unsigned idx = 0; idx < _len_instr; idx++)
    {
        char c = *instr++;
        switch (c)
        {
        case ' ':
        case '\t':
            spacePending = true;
            break;
        default:
            if (spacePending && !atStart)
                *out++ = ' ';
            spacePending = false;
            atStart = false;
            *out++ = c;
            break;
        }
    }
    unsigned len = (size32_t)(out-origout);
    if (len < 25)
        memset(out, ' ', 25 - len);
    memcpy(__ret_str, origout, 25);
}

STRINGLIB_API void STRINGLIB_CALL slStringCleanSpaces80(char *__ret_str,unsigned _len_instr,const char * instr)
{
    // remove double spaces
    // Another fixed width version for Hole
    unsigned outlen = _len_instr;
    if (outlen < 80)
        outlen = 80;
    char *out = (char *) alloca(outlen);
    char *origout = out;
    bool spacePending = false;
    bool atStart = true;
    for(unsigned idx = 0; idx < _len_instr; idx++)
    {
        char c = *instr++;
        switch (c)
        {
        case ' ':
        case '\t':
            spacePending = true;
            break;
        default:
            if (spacePending && !atStart)
                *out++ = ' ';
            spacePending = false;
            atStart = false;
            *out++ = c;
            break;
        }
    }
    unsigned len = (unsigned)(out-origout);
    if (len < 80)
        memset(out, ' ', 80 - len);
    memcpy(__ret_str, origout, 80);
}

