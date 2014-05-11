#pragma once
#include "vector"
#include "map"

using std::vector;
using std::map;
typedef unsigned int docid;
typedef unsigned int doclength;

class SkipList
{
private:
	vector<unsigned> src;
	docid bias;
	void genDiffVector( const map<docid,doclength>& postlist );
	int encodeLength( int n );
	void addLevel ( int ps, int &pe, int& pinfo1_, int& pinfo2_, int curLevel );
public:
	void buildSkipList( int level );
	int getData( docid did, int* position = NULL ) const;
	SkipList( const map<docid,doclength>& postlist );
};