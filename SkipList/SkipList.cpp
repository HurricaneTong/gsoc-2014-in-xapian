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

int cal_level( unsigned size )
{
	return (int)(log10(size)/0.6);
}

void SkipList::genDiffVector( const map<docid,doclength>& postlist )
{
	map<docid,doclength>::const_iterator it = postlist.begin(), pre_it = postlist.begin();
	bias = (postlist.begin())->first;
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

int SkipList::getData( docid did, int* position ) const
{
	int curPosition = 0;
	docid curDid = bias;
	while ( true )
	{
		if ( curDid > did )
		{
			if ( position )
			{
				*position = curPosition;
			}
			return -1;
		}
		if ( curDid == did )
		{
			while ( src[curPosition] == (unsigned)-1 )
			{
				curPosition += 3;
			}
			if ( position )
			{
				*position = curPosition;
			}
			return src[curPosition+1];
		}
		if ( src[curPosition] == (unsigned)-1  )
		{
			int p_offset = src[curPosition+1];
			int d_offset = src[curPosition+2];
			curPosition += 3;
			if ( did >= curDid+d_offset )
			{
				curPosition += p_offset;
				curDid += d_offset;
			}
			else
			{
				if ( src[curPosition] != (unsigned)-1 )
				{
					curDid += src[curPosition];
				}
			}
		}
		else
		{
			curPosition += 2;
			if ( curPosition >= (int)src.size() )
			{
				if ( position )
				{
					*position = curPosition;
				}
				return -1;
			}
			if ( src[curPosition] != (unsigned)-1 )
			{
				curDid += src[curPosition];
			}
		}
	}
}

SkipList::SkipList( const map<docid,doclength>& postlist )
{
	genDiffVector(postlist);
}

void SkipList::encode( string& chunk ) const
{
	pack_uint( chunk,bias );
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
        first_wdf = wdf;
    } else {
        at_end = true;
    }
    did = first_did;
    wdf = 0;
    next();
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
    while (true) {
        if (did > desired_did) {
            return false;
        }
        if (did == desired_did) {
            unpack_uint(&pos, end, &wdf);
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
            } else {
                
            }
        }
    }
    return true;
    
    
    
}

bool SkipListReader::next() {
    return true;
}

unsigned SkipListReader::readCurrent()
{
	const char* p_ = pos;
	unsigned v = 0;
	unpack_uint( &p_, end, &v );
	return v;
}

bool SkipListReader::getDoclen( docid did, doclength* len )
{
	pos = ori_pos;
	int curPosition = 0;
	docid curDid;
	unpack_uint( &pos, end, &curDid );
	while ( true )
	{
		if ( curDid > did )
		{
			return false;
		}
		if ( curDid == did )
		{
			unsigned flag = 0;
			unpack_uint( &pos, end, &flag );
			while ( flag == SEPERATOR )
			{
				read_increase( &pos, end, NULL, NULL );
				flag=0;
				unpack_uint( &pos, end, &flag );
			}
			unpack_uint( &pos, end, len );
			return true;
		}
		unsigned incre_did=0;
		unpack_uint( &pos, end, &incre_did );
		if ( incre_did == SEPERATOR )
		{
			unsigned p_offset = 0;
			unsigned d_offset = 0;
			read_increase( &pos, end, &p_offset, &d_offset );
			if ( did >= curDid+d_offset )
			{
				pos += p_offset;
				curDid += d_offset;
			}
			else
			{
				if ( readCurrent() != SEPERATOR )
				{
					unsigned d_offset = 0;
					const char* p_ = pos;
					read_increase( &p_, end, &d_offset, NULL );
					curDid += d_offset;
				}
			}
		}
		else
		{
			unsigned cur_len = 0;
			unpack_uint( &pos, end, &cur_len );
			if ( pos == end )
			{
				return false;
			}
			if ( readCurrent() != SEPERATOR )
			{
				unsigned d_offset = 0;
				const char* p_ = pos;
				read_increase( &p_, end, &d_offset, NULL );
				curDid += d_offset;
			}
		}
	}
}

SkipListWriter::SkipListWriter( string& chunk_ )
	: chunk( chunk_ )
{
	pos = chunk.data();
	end = pos+chunk.size();
}

bool SkipListWriter::merge_doclen_change( const map<docid,doclength>& changes )
{
	map<docid,doclength> origin_postlist;
	pos = chunk.data();
	docid cur_did = 0, incre_did = 0;
	doclength len = 0;
	unpack_uint( &pos, end, &cur_did );
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
	map<docid,doclength>::const_iterator it = changes.begin(), p;
	for( ; it!=changes.end() ; ++it )
	{
		origin_postlist[it->first]=it->second;
	}
	SkipList sl( origin_postlist );
	sl.buildSkipList( cal_level(origin_postlist.size()) );
	chunk.clear();
	sl.encode( chunk );
	return true;
}