#include "DoclenChunk.h"
#include "pack.h"
#include "map"
#include "string"
#include "iostream"
#include "PostlistChunk.h"
#include "time.h"
#include "fstream"

using namespace std;

void inputMap( map<docid,doclen>& postlist )
{
	//postlist[4]=5;
	//postlist[6]=254;
	//postlist[8]=1024;
	//for ( int i=9 ; i<18 ; ++i )
	//{
	//	postlist[i]=2*i;
	//}
	//postlist[27]=32;
	//postlist[35]=68;
	//for ( int i=40 ; i<42 ; ++i )
	//{
	//	postlist[i]=i-39;
	//}
	//for ( int i=47; i<52 ; ++i )
	//{
	//	postlist[i]=256+3*i;
	//}
	ifstream in( "archives.doclens" );
	docid did = 0;
	doclen len = 0;
	while ( !in.eof() )
	{
		in >> did >> len;
		postlist[did] = len;

	}
	in.close();

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

void calFixedWidth( const map<docid, doclen>& postlist, const string& chunk, int n_test, const vector<int>& indices )
{
	vector< pair<docid,doclen> > datas;
	DoclenChunkReader cr( chunk );
	map<docid,doclen>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		datas.push_back(*it);
	}
	double t1 = clock();
	for ( int i=0 ; i<n_test ; ++i )
	{
		doclen l2 = cr.get_doclen( datas[indices[i]].first );
	}
	double t2=clock();
	cout << "Fixed Width ( search ): " << t2-t1 << endl;

}

void calVariableWidth( const map<docid, doclen>& postlist, const string& chunk, int n_test, const vector<int>& indices )
{
	vector< pair<docid,doclen> > datas;
	PostlistChunk pc( chunk );
	map<docid,doclen>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		datas.push_back(*it);
	}
	double t1 = clock();
	for ( int i=0 ; i<n_test ; ++i )
	{
		doclen l2 = pc.get_doc_length( datas[indices[i]].first );
	}
	double t2=clock();
	cout << "Variable Width ( search ): " << t2-t1 << endl;

}

void test( const map<docid,doclen>& postlist, const string& chunk, const string& chunk2 )
{
	vector< pair<docid,doclen> > datas;
	DoclenChunkReader cr( chunk );
	PostlistChunk pc(chunk2);
	map<docid,doclen>::const_iterator it = postlist.begin();
	for ( ; it!=postlist.end() ; ++it )
	{
		datas.push_back(*it);
	}

	int n_test = 10000;
	vector<int> indices;
	for ( int i=0 ; i<n_test ; ++i )
	{
		indices.push_back(rand()%datas.size());
	}
	calFixedWidth(postlist,chunk,n_test,indices);
	calVariableWidth(postlist,chunk2,n_test,indices);

	//for ( int i=0 ; i<(int)datas.size() ; ++i )
	//{
	//	doclen l1 = datas[i].second;
	//	doclen l2 = cr.get_doclen( datas[i].first );
	//	doclen l3 = pc.get_doc_length( datas[i].first );
	//	if ( l1 != l2 )
	//	{
	//		cout << "chunk:" << endl;
	//		cout << "@did " << datas[i].first << endl;
	//		cout << l1 << " : " << l2 << endl;
	//	}
	//	if ( l1 != l3 )
	//	{
	//		cout << "chunk2:" << endl; 
	//		cout << "@did " << datas[i].first << endl;
	//		cout << l1 << " : " << l3 << endl;
	//	}
	//}

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
	string chunk, chunk2;
	BTree b_tree;
	double t1=0, t2=0;

	inputMap(postlist);
	DoclenChunkWriter dcw(chunk,postlist,&b_tree,true);
	t1=clock();
	dcw.merge_doclen_changes();
	t2=clock();
	cout << "Fixed Width (merge changes): " << t2-t1 << endl;
	t1=clock();
	chunk2 = merge_doclen_changes(postlist,chunk2);
	t2=clock();
	cout << "Original (merge changes) : " << t2-t1 << endl;
	test( postlist,chunk, chunk2 );

	inputChanges(changes);
	DoclenChunkWriter dcw2(chunk,changes,&b_tree,true);
	dcw2.merge_doclen_changes();
	chunk2 = merge_doclen_changes(changes,chunk2);

	mergeChanges(postlist,changes);
	test( postlist, chunk, chunk2 );

	return 0;
}