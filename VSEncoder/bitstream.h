
#ifndef XAPIAN_INCLUDED_BITSTREAM_H
#define XAPIAN_INCLUDED_BITSTREAM_H

#include <string>
#include <vector>
#include "math.h"

#include "pack.h"

inline int log2(unsigned val, bool up = true );

namespace Xapian {


//Encoder.h

class Encoder
{
protected:

	std::string& buf;
	unsigned char& acc;
	int& bits;

	inline bool check_acc();

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
    static const unsigned char mask_1s[8];
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
	const unsigned int num_of_bits;
	static const unsigned int mask_low_n_bits[9];
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
    string& chunk;
	string buf;
	int maxK;
	int n_info;
	unsigned int get_optimal_split( const std::vector<unsigned int>& L, std::vector<unsigned int>& S );
	unsigned int get_optimal_split2( const std::vector<unsigned int>& L, std::vector<unsigned int>& S );
	void encode( const std::vector<unsigned int>& L, int i, int j );

public:
	VSEncoder( std::string& chunk_, int maxK_ = 64 );
	void encode( const std::vector<unsigned int>& L );
};

//Decoder.h

/*class Decoder
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
};*/
    
class Decoder
{
protected:
    const char*& pos;
    const char* end;
    unsigned char& acc;
    int& acc_bits;
    int& p_bit;
    static unsigned int mask[8];
    static unsigned int get_bit_value(unsigned int n, int i);
public:
    virtual unsigned int decode() = 0;
    Decoder(const char*& pos_, const char* end_, unsigned char& acc_, int& acc_bits_, int& p_bit_)
		: pos(pos_), end(end_), acc( acc_ ), acc_bits(acc_bits_), p_bit(p_bit_)
    {
        
    }
    virtual ~Decoder()
    {
        
    }
};
    
    

class UnaryDecoder : public Decoder
{
public:
	unsigned int decode();
	UnaryDecoder(const char*& pos_, const char* end_, unsigned char& acc_, int& acc_bits_, int& p_bit_)
		: Decoder(pos_, end_, acc_, acc_bits_, p_bit_)
	{

	}
};


class GammaDecoder : public Decoder
{
private:
	UnaryDecoder* p_ud;
public:
	unsigned int decode();
	GammaDecoder(const char*& pos_, const char* end_, unsigned char& acc_, int& acc_bits_, int& p_bit_)
		: Decoder(pos_, end_, acc_, acc_bits_, p_bit_)
	{
		p_ud = new UnaryDecoder(pos_, end_, acc_, acc_bits_, p_bit_);
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
	int width;
	static const unsigned char mask_nbits[8][9];
public:
	unsigned int decode();
	OrdinaryDecoder(const char*& pos_, const char* end_, unsigned char& acc_, int& acc_bits_, int& p_bit_, int width_)
		: Decoder(pos_, end_, acc_, acc_bits_, p_bit_), width(width_)
	{

	}
	void setWidth( int width_ )
	{
		width = width_;
	}
};


class VSDecoder
{
private:
	std::string buf;
	unsigned char acc;
	int acc_bits;
	int p_bit;//the bit pointed by p_bit hasn't be handled.
    const char* pos;
    const char* end;
	unsigned int cur_num_width;
	unsigned int cur_remaining_nums;
	UnaryDecoder* p_ud;
	GammaDecoder* p_gd;
	OrdinaryDecoder* p_od;
	unsigned int bias;
	unsigned int next();
    unsigned int n_entry;
    unsigned int last_entry;
public:
	//void decode( std::vector< unsigned int >& R );
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