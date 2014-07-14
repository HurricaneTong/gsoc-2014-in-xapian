#include "SkipList.h"
#include "queue"
#include "math.h"

using std::queue;

template<class U>
inline void
	pack_uint(std::string & s, U value)
{
	// Check U is an unsigned type.
	//STATIC_ASSERT_UNSIGNED_TYPE(U);

	while (value >= 128) {
		s += static_cast<char>(static_cast<unsigned char>(value) | 0x80);
		value >>= 7;
	}
	s += static_cast<char>(value);
}

/** Decode an unsigned integer from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result (or NULL to just skip it).
 */
template<class U>
inline bool
unpack_uint(const char ** p, const char * end, U * result)
{
    // Check U is an unsigned type.
   // STATIC_ASSERT_UNSIGNED_TYPE(U);

    const char * ptr = *p;
   //Assert(ptr);
    const char * start = ptr;

    // Check the length of the encoded integer first.
    do {
	if (ptr == end) {
	    // Out of data.
	    *p = NULL;
	    return false;
	}
    } while (static_cast<unsigned char>(*ptr++) >= 128);

    *p = ptr;

    if (!result) return true;

    *result = U(*--ptr);
    if (ptr == start) {
	// Special case for small values.
	return true;
    }

    size_t maxbits = size_t(ptr - start) * 7;
    if (maxbits <= sizeof(U) * 8) {
	// No possibility of overflow.
	do {
	    unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
	    *result = (*result << 7) | U(chunk);
	} while (ptr != start);
	return true;
    }

    size_t minbits = maxbits - 6;
    if (minbits > sizeof(U) * 8) {
	// Overflow.
	return false;
    }

    while (--ptr != start) {
	unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
	*result = (*result << 7) | U(chunk);
    }

    U tmp = *result;
    *result <<= 7;
    if (*result < tmp) {
	// Overflow.
	return false;
    }
    *result |= U(static_cast<unsigned char>(*ptr) & 0x7f);
    return true;
}


inline bool
read_increase( const char** p, const char* end, docid* incre_did, doclength* len )
{
	unsigned tmp;
	if ( incre_did == NULL )
	{
		incre_did = &tmp;
	}
	if ( len == NULL )
	{
		len = &tmp;
	}
	unpack_uint( p, end, incre_did );
	unpack_uint( p, end, len );
	return true;
}

int SkipList::cal_level( unsigned size )
{
	return (int)(log10(size)/0.6);
}

void SkipList::genDiffVector( const map<docid,doclength>& postlist )
{
	map<docid,doclength>::const_iterator it = postlist.begin(), pre_it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		src.push_back(it->first-pre_it->first);
		src.push_back(it->second);
		pre_it = it;
	}
}

int SkipList::encodeLength( unsigned n )
{
	int len = 0;
	while ( n >= 128 )
	{
		len++;
		n >>= 7;
	}
	len++;
	return len;
}

void SkipList::addLevel ( int ps, int &pe, int& pinfo1_, int& pinfo2_, int curLevel )
{
	int size = pe-ps;
	int pinfo1 = ps;
	int pinfo2 = ps+3+size/2;

	if ( pinfo2%2 == pinfo1%2 )
	{
		pinfo2--;
	}

	int value1 = 0, value2 = 0;
	for ( int i = ps ; i<=pinfo2-5 ; i+=2 )
	{
		value1 += src[i];
	}
	for ( int i = pinfo2-3 ; i<= pe-2 ; i+=2 )
	{
		value2 += src[i];
	}

	src.insert(src.begin()+pinfo1,0);
	src.insert(src.begin()+pinfo1,0);
	src.insert(src.begin()+pinfo1,SEPERATOR);

	src.insert(src.begin()+pinfo2,0);
	src.insert(src.begin()+pinfo2,0);
	src.insert(src.begin()+pinfo2,SEPERATOR);

	pe += 6;
	src[pinfo2+2]=value2;
	int offset2 = 0;
	for ( int i = pinfo2+3 ; i<pe-2 ; ++i )
	{
		offset2 += encodeLength(src[i]);
	}
	src[pinfo2+1] = offset2;

	src[pinfo1+2]=value1;
	int offset1 = 0;
	for ( int i = pinfo1+3 ; i<pinfo2-2 ; ++i )
	{
		offset1 += encodeLength(src[i]);
	}
	src[pinfo1+1]=offset1;

	int longer = 0;
	for ( int i = 0 ; i<3 ; ++i )
	{
		longer += encodeLength(src[pinfo1+i]);
		longer += encodeLength(src[pinfo2+i]);
	}

	int updateCount = 0;
	unsigned offset = 0;
	for( int i = ps-3 ; i>=0 ; i-- )
	{
		offset += encodeLength(src[i]);
		if ( src[i]==SEPERATOR && src[i+1]>=offset )
		{
			if ( updateCount >= curLevel )
			{
				break;
			}
			int tmp = src[i+1];
			src[i+1] += longer;
			longer += encodeLength(src[i+1])-encodeLength(tmp);
			updateCount++;
		}
	}

	pinfo1_ = pinfo1;
	pinfo2_ = pinfo2;


}

void SkipList::buildSkipList( int level )
{
	if ( level == 0 )
	{
		return;
	}
	int curLevel = 0;
	queue<int> positions;
	positions.push(0);
	positions.push((int)src.size());
	while ( curLevel < level )
	{
		int n = (int)pow(2,curLevel);
		for ( int i = 0 ; i<n ; ++i )
		{
			int ps=0, pe=0, pinfo1=0, pinfo2=0;
			ps = positions.front();
			positions.pop();
			pe = positions.front();
			positions.pop();
			addLevel(ps,pe,pinfo1,pinfo2,curLevel);
			positions.push(pinfo2+3);
			positions.push(pe);
			positions.push(pinfo1+3);
			positions.push(pinfo2);

		}
		for ( int i = (int)positions.size()-1 ; i>=0 ; --i )
		{
			int tmp = positions.front();
			tmp += i/4*6;
			positions.pop();
			positions.push(tmp);
		}
		curLevel++;
	}

}


SkipList::SkipList( const map<docid,doclength>& postlist )
{
	genDiffVector(postlist);
    buildSkipList( cal_level((unsigned)postlist.size()) );
}

void SkipList::encode( string& chunk ) const
{
	for ( int i = 0 ; i<(int)src.size() ; ++i )
	{
		pack_uint( chunk,src[i] );
	}

}

SkipListReader::SkipListReader(const char* pos_, const char* end_, docid first_did_)
: ori_pos(pos_), pos(pos_), end(end_), first_did(first_did_) {
    if (pos != end){
        at_end = false;
        did = first_did;
        next();
    } else {
        at_end = true;
        did = -1;
        wdf = -1;
    }
}

bool SkipListReader::jump_to(docid desired_did) {
    if (did == desired_did) {
        return true;
    }
    if (did > desired_did) {
        pos = ori_pos;
        did = first_did;
        next();
    }
    while (pos != end) {
        if (did > desired_did) {
            return false;
        }
        if (did == desired_did) {
            return true;
        }
        termcount incre_did = 0;
        unpack_uint(&pos, end, &incre_did);
        if (incre_did == SEPERATOR) {
            unsigned p_offset = 0;
            unsigned d_offset = 0;
            read_increase(&pos, end, &p_offset, &d_offset);
            if (desired_did >= did+d_offset) {
                pos += p_offset;
                did += d_offset;
                read_increase(&pos, end, NULL, &wdf);
            } else {
                
            }
        } else {
            did += incre_did;
            unpack_uint(&pos, end, &wdf);
        }
    }
    return true;
    
    
    
}

bool SkipListReader::next() {
    if (at_end) {
        return false;
    }
    termcount incre_did = 0;
    unpack_uint(&pos, end, &incre_did);
    while (incre_did == SEPERATOR) {
        read_increase(&pos, end, NULL, NULL);
        if (pos == end) {
            at_end = true;
            return true;
        }
        unpack_uint(&pos, end, &incre_did);
    }
    did += incre_did;
    unpack_uint(&pos, end, &wdf);
    return true;
}

SkipListWriter::SkipListWriter(const char* start_, const char* end_ , docid first_did_, BTree* bt_)
: start(start_), end(end_), first_did(first_did_), bt(bt_)
{
    
}

bool SkipListWriter::merge_doclen_change(map<docid,termcount>::const_iterator start_, map<docid,termcount>::const_iterator end_)
{
	map<docid,doclength> origin_postlist;
	const char* pos = start;
	docid cur_did = 0, incre_did = 0;
	doclength len = 0;
    cur_did = first_did;
	while ( pos != end )
	{
		unpack_uint( &pos, end, &incre_did );
		while ( incre_did == SEPERATOR )
		{
			read_increase( &pos, end, NULL, NULL );
			unpack_uint( &pos, end, &incre_did );
		}
		unpack_uint( &pos, end, &len );
		cur_did += incre_did;
		origin_postlist[cur_did]=len;
	}
	map<docid,doclength>::const_iterator it = start_;
	for( ; it!=end_ ; ++it )
	{
		origin_postlist[it->first]=it->second;
	}
	SkipList sl( origin_postlist );
	string chunk;
	sl.encode( chunk );
    (*bt)[origin_postlist.begin()->first] = chunk;
	return true;
}