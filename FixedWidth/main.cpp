#include "iostream"
#include "map"
#include "math.h"
#include "string"
#include "vector"
#include "fstream"
#include "FixedWidthChunk.h"
#define SEPERATOR ((unsigned)-1)

using namespace std;

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
	for ( int i=40 ; i<42 ; ++i )
	{
		postlist[i]=i-39;
	}
	for ( int i=47; i<52 ; ++i )
	{
		postlist[i]=256+3*i;
	}
}

void inputChanges( map<docid,doclen>& changes )
{
	changes[5] = 250;
	changes[6] = 25;
	changes[15] = 1025;
}

void mergeChanges( map<docid,doclen>& postlist, const map<docid,doclen>& changes )
{
	map<docid,doclen>::const_iterator it = changes.begin();
	for ( ; it!=changes.end() ; ++it )
	{
		postlist[it->first] = it->second;
	}
}

void test( const map<docid,doclen>& postlist, const string& chunk )
{
	FixedWidthChunkReader cr( chunk );
	map<docid,doclen>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		doclen l1 = it->second;
		doclen l2 = cr.getDoclen( it->first );
		if ( l1 != l2 )
		{
			cout << "wrong @did " << it->first << endl;
			cout << l1 << " : " << l2 << endl;
		}
	}
	cout << "OK!" << endl;
	return;
}

ostream& operator << ( ostream& out, const map<docid,doclen>& postlist )
{
	map<docid,doclen>::const_iterator it = postlist.begin();
	while ( it!=postlist.end() )
	{
		out << it->first << " : " << it->second << endl;
		it++;
	}
	return out;
}

int main()
{

	map<docid,doclen> postlist, changes;
	vector<unsigned> src;
	string chunk;
	unsigned r = 0;
	
	inputMap(postlist);
	FixedWidthChunk fwc( postlist );
	fwc.encode(chunk);
	test( postlist,chunk );

	inputChanges( changes );
	mergeChanges( postlist,changes );
	FixedWidthChunkWriter cw( chunk );
	cw.merge_doclen_change( changes );
	test( postlist, chunk );
	
	//ofstream out("data.txt");
	//out<<postlist;
	//out.close();

	//pack_uint_in_bytes( 25, 1, chunk );
	//pack_uint_in_bytes( 563, 2, chunk );
	//pack_uint_in_bytes( 1, 2, chunk );
	//const char* pos = chunk.data();
	//unpack_uint_in_bytes( &pos, 1, &r );
	//unpack_uint_in_bytes( &pos, 2, &r );
	//unpack_uint_in_bytes( &pos, 1, &r );

	
	return 0;
}