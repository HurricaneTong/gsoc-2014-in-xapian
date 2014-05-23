#include "DoclenChunk.h"
#include "pack.h"
#include "map"
#include "string"
#include "iostream"

using namespace std;

void inputMap( map<docid,doclen>& postlist )
{
	postlist[4]=5;
	postlist[6]=254;
	postlist[8]=1024;
	for ( int i=9 ; i<18 ; ++i )
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
	vector< pair<docid,doclen> > datas;
	DoclenChunkReader cr( chunk );
	map<docid,doclen>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		datas.push_back(*it);
	}
	for ( int i=0 ; i<100 ; ++i )
	{
		int p = rand()%datas.size();
		doclen l1 = datas[p].second;
		doclen l2 = cr.get_doclen( datas[p].first );
		if ( l1 != l2 )
		{
			cout << "@did " << datas[p].first << endl;
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
	string chunk;

	inputMap(postlist);
	DoclenChunkWriter dcw(chunk);
	dcw.merge_doclen_changes(postlist);
	test( postlist,chunk );

	inputChanges( changes );
	mergeChanges( postlist,changes );
	dcw.merge_doclen_changes(changes);
	test( postlist,chunk );

	return 0;
}