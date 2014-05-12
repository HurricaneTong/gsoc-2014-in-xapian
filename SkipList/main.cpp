#include "iostream"
#include "map"
#include "vector"
#include "math.h"
#include "queue"
#include "fstream"
#include "SkipList.h"
#include "string"

using namespace std;

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
	for ( int i = 1 ; i<1024 ; ++i )
	{
		postlist[i] = 2*i;
	}
}

void inputChanges( map<docid,doclength>& changes )
{
	changes[5] = 250;
	changes[6] = 25;
	changes[15] = 1025;
}

void mergeChanges( map<docid,doclength>& postlist, const map<docid,doclength>& changes )
{
	map<docid,doclength>::const_iterator it = changes.begin();
	for ( ; it!=changes.end() ; ++it )
	{
		postlist[it->first] = it->second;
	}
}

/*int deleteDid( docid did, docid bias, vector<unsigned>& src )
{
	int p = -1;
	if ( getData( did, bias, src, &p ) == -1)
	{
		return 0;
	}
	int next_p = p+2;
	while ( src[next_p] == (unsigned)-1 )
	{
		next_p += 3;
	}
	int tmp = src[next_p];
	src[next_p] += src[p];
	int longer = encodeLength(src[next_p])-encodeLength(tmp);
	for ( int i = next_p ; i>=0 ; --i )
	{
		if ( src[i] == (unsigned)-1 )
		{
			if ( i+3+src[i+1] >= next_p )
			{
				int tmp = src[i+1];
				src[i+1] += longer;
				longer += encodeLength(src[i+1])-encodeLength(tmp);
			}
		}
	}
	int shoter = encodeLength(src[p])+encodeLength(src[p+1]);
	for ( int i = p ; i>=0 ; --i )
	{
		if ( src[i] == (unsigned)-1 )
		{
			if ( i+3+src[i+1] > p )
			{
				int tmp = src[i+1];
				src[i+1] -= shoter;
				shoter += encodeLength(tmp)-encodeLength(src[i+1]);
			}
			else if ( i+3+src[i+1] == p )
			{

			}
		}
	}
	src.erase( src.begin()+p, src.begin()+p+2 );
	return 1;
}*/

void test( const map<docid,doclength>& postlist, const string& chunk )
{
	SkipListReader slr(chunk);
	map<docid,doclength>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		int v1 = it->second;
		doclength v2 = 0;
		slr.getDoclen( it->first, &v2 );
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
	string chunk;
	map<docid,doclength> postlist,changes;
	inputMap(postlist);
	inputChanges(changes);

	SkipList sl(postlist);
	sl.buildSkipList( cal_level(postlist.size()) );
	sl.encode(chunk);
	test(postlist,chunk);

	SkipListWriter slw(chunk);
	slw.merge_doclen_change(changes);

	mergeChanges(postlist,changes);
	test(postlist,chunk);

	
	//ofstream out("out.txt");
	//out<<src;
	//out.close();
	//cout << src << endl;
	return 0;
}