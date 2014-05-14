#include "iostream"
#include "map"
#include "math.h"
#include "string"
#include "vector"
#define SEPERATOR ((unsigned)-1)

using namespace std;


typedef unsigned docid;
typedef unsigned doclen;

static const unsigned char flstab[256] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

// @return : the value returned is an integer which is no less than log2(@val) when @up is ture, 
//				or is no more than log2(@val) when @up is false
inline int log2(unsigned val, bool up )
{
	int result = 0;
	if (val >= 0x10000u) {
		val >>= 16;
		result = 16;
	}
	if (val >= 0x100u) {
		val >>= 8;
		result += 8;
	}
	result += flstab[val];
	if ( up )
	{
		if ( val == 1<<(result-1) )
		{
			result--;
		}
	}
	else
	{
		result--;
	}

	return result;
}

bool buildVector( const map<docid,doclen>& postlist, vector<unsigned>& src )
{
	map<docid,doclen>::const_iterator it = postlist.begin(), start_pos;
	while ( it!=postlist.end() )
	{
		bool is_contiguous = true;
		unsigned length_contiguous = 1;
		docid start_docid = 0, last_docid = 0, cur_docid = 0;
		unsigned max_bits = 2;
		last_docid = it->first;
		start_pos = it;
		unsigned min_len = it->second;
		it++;
		while ( it!=postlist.end() && is_contiguous )
		{
			cur_docid = it->first;
			if ( cur_docid != last_docid+1 )
			{
				is_contiguous = false;
				it--;
				break;
			}
			if ( it->second < min_len )
			{
				min_len = it->second;
			}
			length_contiguous++;
			last_docid = cur_docid;
			it++;
		}
		if ( length_contiguous > 1 )
		{
			if ( it!=postlist.end() )
			{
				it++;
			}
			src.push_back(SEPERATOR);
			src.push_back(length_contiguous);
			src.push_back(max_bits);
			src.push_back(start_pos->first);
			src.push_back(min_len);			
			while ( start_pos!=it )
			{
				src.push_back(start_pos->second-min_len);
				start_pos++;
			}
		}
		else
		{
			src.push_back(it->first);
			src.push_back(it->second);
		}
		if ( it==postlist.end() )
		{
			break;
		}
		it++;

	}
	return true;
}

void inputMap( map<docid,doclen>& postlist )
{
	postlist[4]=5;
	postlist[6]=254;
	for ( int i=9 ; i<13 ; ++i )
	{
		postlist[i]=2*i;
	}
	postlist[27]=32;
	postlist[35]=68;
	for ( int i=40; i<45 ; ++i )
	{
		postlist[i]=3*i;
	}
}

int main()
{
	map<docid,doclen> postlist;
	vector<unsigned> src;
	inputMap(postlist);
	buildVector(postlist,src);	
	return 0;
}