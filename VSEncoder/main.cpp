#define CRTDBG_MAP_ALLOC  
#include <stdlib.h>
#include "bitstream.h"
#include "vector"
#include "iostream"
#include "time.h"
#include "fstream"
#include "Interpolative.h"

using namespace std;

void testVSEncoding()
{
	int maxi = 0xffff;
	for ( int i = 1 ; i < maxi ; ++i )
	{
		string buf;
		VSEncoder vse( buf );
		vector< unsigned int > L,R;
		L.push_back(i);
		vse.encode(L);
		VSDecoder vsd( buf );
		//vsd.decode(R);
		if ( R[0] != L[0] )
		{
			cout << "error!" << endl;
		}
	}
}

void genRand()
{
	ofstream out( "/Users/HurricaneTong/code/Xapian_dev/VSEncoder/randNum" );
	srand(time(NULL));
	for ( int i = 1 ; i < 0xffffff ; i = i+rand()%50+50 )
	{
		out << i+rand()%50 << endl;
		if ( i%50 == 0 )
		{
			i *= 1.5;
		}
	}
	out << -1 << endl;
	out.close();
}

void readRand( vector<unsigned int>& src )
{
	ifstream in( "/Users/HurricaneTong/code/Xapian_dev/VSEncoder/randNum" );
	int tmp = 0;
	in >> tmp;
	while ( tmp != -1 )
	{
		src.push_back( tmp );
		in >> tmp;
	}
	in.close();
}

void readBigData( vector<unsigned int>& src )
{
	ifstream in( "/Users/HurricaneTong/code/Xapian_dev/VSEncoder/linux.postlist" );
	int tmp = 0;
	in >> tmp;
	while ( !in.eof() )
	{
		src.push_back( tmp );
		//if ( src.size() == 1000 )
		//{
		//	return;
		//}
		in >> tmp;
	}
	in.close();
}

double CalTimeVS( const vector<unsigned int>& src )
{

	double costT = 0, startT = 0, endT = 0;
	cout << "finish reading" << endl;
	string buf;
	VSEncoder vse(buf,16);
	startT = clock();
	vse.encode( src );
	endT = clock();
	cout << "VSEncoding: " << endT-startT << endl;
	cout << buf.size() << endl;
	cout << "finish encoding" << endl;
	VSDecoder vsd(buf);
	startT = clock();
	unsigned int last_entry = vsd.get_last_entry();
	unsigned tmp = vsd.get_first_entry();
	while ( tmp != last_entry )
	{
		tmp = vsd.get_next_entry();
	}
	endT = clock();
	costT = endT-startT;
	return costT;
}

double CalTimeInterpolative( const vector<unsigned int>& vec)
{
	string s;
	double costT = 0, startT = 0, endT = 0;
	//readRand( vec );
	cout << "finish reading" << endl;
	startT = clock();
	pack_uint(s, vec.back());
	if (vec.size() > 1) {
		BitWriter wr(s);
		wr.encode(vec[0], vec.back());
		wr.encode(vec.size() - 2, vec.back() - vec[0]);
		wr.encode_interpolative(vec, 0, vec.size() - 1);
		swap(s, wr.freeze());
	}
	endT = clock();
	cout << "Interpolative Encoding: " << endT-startT << endl;
	cout << s.size() << endl;
	cout << "finish encoding" << endl;
	BitReader rd;
	startT = clock();
	const char * pos = s.data();
	const char * end = pos + s.size();
	unsigned int pos_last;
	unpack_uint(&pos, end, &pos_last);
	// Skip the header we just read.
	rd.init(s, pos - s.data());
	unsigned int pos_first = rd.decode(pos_last);
	unsigned int pos_size = rd.decode(pos_last - pos_first) + 2;
	rd.decode_interpolative(0, pos_size - 1, pos_first, pos_last);
	unsigned int current_pos = pos_first;
	while ( current_pos != pos_last )
	{
		current_pos = rd.decode_interpolative_next();
	}
	endT = clock();
	costT = endT - startT;
	return costT;
}

int main()
{
	vector<unsigned int> src;
	//readBigData( src );
	readRand(src);
	cout << CalTimeVS( src ) << endl;
	cout << CalTimeInterpolative( src ) << endl;
	return 0;
}