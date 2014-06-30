#define CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  
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
		vsd.decode(R);
		if ( R[0] != L[0] )
		{
			cout << "error!" << endl;
		}
	}
}

void genRand()
{
	ofstream out( "randNum.txt" );
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
	ifstream in( "randNum.txt" );
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
	ifstream in( "Z:\\linux.postlist" );
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

template<class U>
inline void
	pack_uint(std::string & s, U value)
{
	// Check U is an unsigned type.
	while (value >= 128) {
		s += static_cast<char>(static_cast<unsigned char>(value) | 0x80);
		value >>= 7;
	}
	s += static_cast<char>(value);
}

template<class U>
inline bool
	unpack_uint(const char ** p, const char * end, U * result)
{
	// Check U is an unsigned type.

	const char * ptr = *p;
	const char * start = ptr;

	// Check the length of the encoded integer first.
	do {
		if ((ptr == end)) {
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
	if ((minbits > sizeof(U) * 8)) {
		// Overflow.
		return false;
	}

	while (--ptr != start) {
		unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
		*result = (*result << 7) | U(chunk);
	}

	U tmp = *result;
	*result <<= 7;
	if ((*result < tmp)) {
		// Overflow.
		return false;
	}
	*result |= U(static_cast<unsigned char>(*ptr) & 0x7f);
	return true;
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
	readBigData( src );
	//readRand(src);
	cout << CalTimeVS( src ) << endl;
	cout << CalTimeInterpolative( src ) << endl;
	return 0;
}