#pragma once
#include "vector"
#include "map"
#include "string"
#define SEPERATOR ((unsigned)-1)

using std::vector;
using std::map;
using std::string;
typedef unsigned int docid;
typedef unsigned int doclength;

int cal_level( unsigned size );

class SkipList
{
private:
	vector<unsigned> src;
	docid bias;
	void genDiffVector( const map<docid,doclength>& postlist );
	int encodeLength( unsigned n );
	void addLevel ( int ps, int &pe, int& pinfo1_, int& pinfo2_, int curLevel );
	int getData( docid did, int* position = NULL ) const;
public:
	void buildSkipList( int level );
	SkipList( const map<docid,doclength>& postlist );
	void encode( string& chunk ) const;
};

class SkipListReader
{
private:
	const char* pos;
	const char* end;
	string chunk;
	unsigned readCurrent();
public:
	bool getDoclen( docid did, doclength* len );
	SkipListReader( const string& chunk_ );
};

class SkipListWriter
{
private:
	const char* pos;
	const char* end;
	string& chunk;
public:
	SkipListWriter( string& chunk_ );
	bool merge_doclen_change( const map<docid,doclength>& changes );
};
