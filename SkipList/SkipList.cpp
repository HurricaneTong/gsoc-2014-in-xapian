#include "SkipList.h"
#include "queue"

using std::queue;

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

int SkipList::encodeLength( int n )
{
	return 1;
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
	src.insert(src.begin()+pinfo1,-1);

	src.insert(src.begin()+pinfo2,0);
	src.insert(src.begin()+pinfo2,0);
	src.insert(src.begin()+pinfo2,-1);

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
		if ( src[i]==(unsigned)-1 && src[i+1]>=offset )
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