#include "iostream"
#include "map"
#include "vector"
#include "math.h"
#include "queue"
#include "fstream"
#include "SkipList.h"
#include "string"
#include "time.h"

using namespace std;

void inputMap( map<docid,doclength>& postlist )
{
//	postlist[4]=3;
//	postlist[5]=7;
//	postlist[9]=2;
//	postlist[15]=3;
//	postlist[16]=4;
//	postlist[18]=11;
//	postlist[27]=20;
//	postlist[37]=40;
	for ( int i = 1 ; i<2048 ; ++i )
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



void test( const map<docid,doclength>& postlist, const string& chunk )
{
    const char* pos = chunk.data();
    const char* end = pos+chunk.size();
    vector< pair<unsigned, unsigned> > data;
	SkipListReader slr(pos,end,postlist.begin()->first);
	map<docid,doclength>::const_iterator it = postlist.begin();
    for (; it!=postlist.end() ; ++it) {
        data.push_back(*it);
    }
    srand((int)time(0));
    for (int i=0 ; i<10000; ++i) {
        int k = rand()%data.size();
        unsigned v1, v2, v3;
        v1 = v2 = v3 = 0;
        if (k%2) {
            slr.jump_to(data[k].first);
            if (slr.next() && !slr.is_at_end()) {
                v1 = data[k+1].first;
                v2 = data[k+1].second;
                v3 = slr.get_wdf();
            }
        } else {
            v1 = data[k].first;
            v2 = data[k].second;
            if (slr.jump_to(v1)) {
                v3 = slr.get_wdf();
            } else {
                cout << "error: no such did" << endl;
            }
        }

        if (v2 != v3) {
            cout << v1 << endl;
            cout << "correct: " << v2 << endl;
            cout << "wrong: " << v3 << endl;
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
	sl.encode(chunk);
	test(postlist,chunk);

    const char* pos = chunk.data();
    const char* end = pos+chunk.size();
    BTree bt;
	SkipListWriter slw(pos,end,postlist.begin()->first,&bt);
	slw.merge_doclen_change(changes.begin(),changes.end());

	mergeChanges(postlist,changes);
	test(postlist,bt[postlist.begin()->first]);

	
	//ofstream out("out.txt");
	//out<<src;
	//out.close();
	//cout << src << endl;
	return 0;
}