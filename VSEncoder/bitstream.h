
#ifndef XAPIAN_INCLUDED_BITSTREAM_H
#define XAPIAN_INCLUDED_BITSTREAM_H

#include <string>
#include <vector>
#include "math.h"

inline int log2(unsigned val, bool up = true );

namespace Xapian {


//Encoder.h

class Encoder
{
protected:

	std::string& buf;
	unsigned char& acc;
	int& bits;

	bool check_acc();

public:
	Encoder( std::string& buf_, unsigned char& acc_, int& bits_ )
		: buf(buf_), acc(acc_), bits(bits_)
	{

	}
	virtual void encode( unsigned int n ) = 0;
	virtual ~Encoder()
	{

	}

};


class UnaryEncoder : public Encoder
{
public:
	UnaryEncoder( std::string& buf_, unsigned char& acc_, int& bits_ )
		: Encoder( buf_, acc_, bits_ )
	{

	}
	void encode( unsigned int n );

};

class GammaEncoder : public Encoder
{
public:
	GammaEncoder( std::string& buf_, unsigned char& acc_, int& bits_ )
		: Encoder( buf_, acc_, bits_ )
	{

	}
	void encode( unsigned int n );
};

class OrdinaryEncoder : public Encoder
{
private:
	unsigned int num_of_bits;
public:
	OrdinaryEncoder( std::string& buf_, unsigned char& acc_, int& bits_, int num_of_bits_ )
		: Encoder( buf_, acc_, bits_ ), num_of_bits( num_of_bits_ )
	{

	}
	void encode( unsigned int n );
};

class VSEncoder
{
private:
	int bits;
	unsigned char acc;
	std::string& buf;
	int maxK;
	int n_info;
	unsigned int get_optimal_split( const std::vector<unsigned int>& L, std::vector<unsigned int>& S );
	void encode( const std::vector<unsigned int>& L, int i, int j );

public:
	VSEncoder( std::string& buf_, int maxK_ = 64 );
	void encode( const std::vector<unsigned int>& L );
};

//Decoder.h

class Decoder
{
protected:
	std::string& buf;
	unsigned char& acc;
	int& bits;
	int &p_buf, &p_bit;
	static unsigned int mask[8];
	static unsigned int get_bit_value( unsigned int n, int i );
public:
	virtual unsigned int decode() = 0;
	Decoder( std::string& buf_, unsigned char& acc_, int& bits_, int& p_buf_, int& p_bit_ )
		: buf( buf_ ), acc( acc_ ), bits( bits_ ), p_buf( p_buf_ ), p_bit( p_bit_ )
	{

	}
	void assign( std::string& buf_, unsigned char& acc_, int& bits_, int& p_buf_, int& p_bit_ )
	{
		buf = buf_;
		acc = acc_;
		bits = bits_;
		p_buf = p_buf_;
		p_bit = p_bit_;
	}
	virtual ~Decoder()
	{

	}
};

class UnaryDecoder : public Decoder
{
public:
	unsigned int decode();
	UnaryDecoder( std::string& buf_, unsigned char& acc_, int& bits_, int& p_buf_, int& p_bit_ )
		: Decoder( buf_, acc_, bits_, p_buf_, p_bit_ )
	{

	}
};


class GammaDecoder : public Decoder
{
private:
	UnaryDecoder* p_ud;
public:
	unsigned int decode();
	GammaDecoder( std::string& buf_, unsigned char& acc_, int& bits_, int& p_buf_, int& p_bit_ )
		: Decoder( buf_, acc_, bits_, p_buf_, p_bit_ )
	{
		p_ud = new UnaryDecoder( buf, acc, bits, p_buf, p_bit );
	}
	~GammaDecoder()
	{
		if ( p_ud != NULL )
		{
			delete p_ud;
			p_ud = NULL;
		}
	}

};

class OrdinaryDecoder : public Decoder
{
private:
	int num_of_bits;
	static const unsigned char mask_nbits[8][9];
public:
	unsigned int decode();
	OrdinaryDecoder( std::string& buf_, unsigned char& acc_, int& bits_, int& p_buf_, int& p_bit_, int num_of_bits_ )
		: Decoder( buf_, acc_, bits_, p_buf_, p_bit_ ), num_of_bits( num_of_bits_ )
	{

	}
	void setWidth( int num_of_bits_ )
	{
		num_of_bits = num_of_bits_;
	}
};


class VSDecoder
{
private:
	std::string buf;
	unsigned char acc;
	int bits;
	int p_buf, p_bit;//the bit pointed by p_bit hasn't be handled. 
	int n_info;
	unsigned int cur_num_width;
	unsigned int cur_remaining_nums;
	UnaryDecoder* p_ud;
	GammaDecoder* p_gd;
	OrdinaryDecoder* p_od;
	unsigned int bias;
	unsigned int next();
public:
	void decode( std::vector< unsigned int >& R );
	VSDecoder( std::string& buf_ );
	unsigned int get_next_entry();
	unsigned int get_first_entry();
	unsigned int get_n_entry();
	unsigned int get_last_entry();
	~VSDecoder();
};

}

using Xapian::VSEncoder;
using Xapian::VSDecoder;

#endif // XAPIAN_INCLUDED_BITSTREAM_H