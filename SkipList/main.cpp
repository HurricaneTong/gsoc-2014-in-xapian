#include "iostream"
#include "map"
#include "vector"
#include "math.h"
#include "queue"
#include "fstream"

using namespace std;
typedef unsigned int docid;
typedef unsigned int doclength;

void genDiffVector( const map<docid,doclength>& postlist, vector<unsigned>& src, docid& bias )
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

void inputMap( map<docid,doclength>& postlist )
{
	//postlist[4]=3;
	//postlist[5]=7;
	//postlist[9]=2;
	//postlist[15]=3;
	//postlist[16]=4;
	//postlist[18]=11;
	//postlist[27]=20;
	//postlist[37]=40;
	for ( int i = 1 ; i<128 ; ++i )
	{
		postlist[i] = 2*i;
	}
}

int encodeLength( int n )
{
	return 1;
}


void addLevel ( vector<unsigned>& src, int ps, int &pe, int& pinfo1_, int& pinfo2_, int curLevel )
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

void buildSkipList( vector<unsigned>& src, int level )
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
			addLevel(src,ps,pe,pinfo1,pinfo2,curLevel);
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

int getData( docid did, docid bias, const vector<unsigned>& src )
{
	int curPosition = 0;
	docid curDid = bias;
	while ( true )
	{
		if ( curDid > did )
		{
			return -1;
		}
		if ( curDid == did )
		{
			while ( src[curPosition] == (unsigned)-1 )
			{
				curPosition += 3;
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
				return -1;
			}
			if ( src[curPosition] != (unsigned)-1 )
			{
				curDid += src[curPosition];
			}
		}
	}
}

void test( const map<docid,doclength>& postlist, const vector<unsigned>& src, docid bias )
{
	map<docid,doclength>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		int v1 = it->second;
		int v2 = getData( it->first, bias, src );
		if ( v1 != v2 )
		{
			cout << it->first << endl;
			cout << "correct: " << v1 << endl;
			cout << "wrong: " << v2 << endl;
			return;
		}
	}
	cout << "OK!" << endl;
}

ostream& operator <<( ostream& out, const vector<unsigned>& src )
{
	for( int i = 0 ; i<(int)src.size() ; ++i )
	{
		cout << src[i] << ' ';
	}
	return out;
}

ofstream& operator << ( ofstream& out, const vector<unsigned>& src )
{
	for ( int i = 0 ; i<(int)src.size() ; ++i )
	{
		out << i << " : " << (int)src[i];
		if ( src[i] == 1 )
		{
			out << "!!!";
		}
		out << endl;
	}
	return out;
}
int main()
{
	//int s[]={0,3,1,7,4,2,6,3,1,4,2,11,9,20,10,40};
	//vector<unsigned> src(s,s+16);
	//int p1,p2;
	//addLevel(src,0,16,p1,p2);
	//int s[]={0,3,1,7,4,2,6,3,1,4,2,11,9,20,10,40,2,2};
	//vector<unsigned> src(s,s+18);
	//int p1,p2;
	//addLevel(src,0,18,p1,p2);
	map<docid,doclength> postlist;
	vector<unsigned> src;
	docid bias = 0;
	inputMap(postlist);
	genDiffVector(postlist,src,bias);
	buildSkipList(src,4);
	test(postlist,src,bias);
	ofstream out("out.txt");
	//out<<src;
	out.close();
	//cout << src << endl;
	return 0;
}