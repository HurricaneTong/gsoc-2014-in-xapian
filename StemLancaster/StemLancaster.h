/** @file StemLancaster.h
 * @brief a class that implements the Lancaster (Paice/Husk) stemming algorithm
 *		  This class is implemented based on an ANSI C implementation by Andy Stark
 *		  and some bug is modified.
 *		  More details in http://www.comp.lancs.ac.uk/computing/research/stemming/index.htm
 */
 /* Copyright 2014 HurricaneTong
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include "iostream"
#include "string"
#include "list"
#include "xapian.h"
#define STEMLAN_NOT_APPLY 0			/* Rule did not apply */
#define STEMLAN_STOP 2				/* Stop here */
#define STEMLAN_CONTINUE 1			/* Continue with next rule */
#define STEMLAN_NOT_INTACT 3		/* Word no longer intact */

class StemLancasterRule 
{
public:
	std::string text;		/* To return stemmer output */
	std::string keystr;		/* Key string,ie,suffix to remove */
	std::string repstr;		/* string to replace deleted letters */
	bool intact;			/* Boolean-must word be intact? */
	bool cont;				/* Boolean-continue with another rule? */
	int ruleID;				/* the ID of the rule */
	bool prot;				/* Boolean-protect this ending? */
	int deltotal;			/* Delete how many letters? */
};

class StemLancaster : public Xapian::StemImplementation
{
private:

	/* the raw meterial of rules */
	/* Format is: keystr,repstr,flags where keystr and repstr are strings,and */
	/* flags is one of:"protect","intact","continue","protint","contint",or  */
	/* "stop" (without the inverted commas in the actual file).  */
	std::string raw_rules;

	/* a table of rules to be applied */
	std::list<StemLancasterRule> rule_table[26];

	/* return the index of rules to be applied to the desired word */
	int get_table_index( const std::string& word );

	/* apply the @ rule to the @ word */
	/* @word : _in_out, the original word should be passed onto this function by this parameter,
	*		   and when this function finishes, the word becomes the value stemming
	*		   from the original word.	
	*/
	int apply_rule( const StemLancasterRule& rule, std::string& word, bool is_intact );

	/* apply a set of rules to the word
	*  @used_rule : _out it's the rule which is applied to the word finally.
	*/
	int rule_walk( std::string& word, int is_intact, StemLancasterRule& used_rule );


	bool is_valid_str( const std::string& str );


	bool is_vowel( const char& c );


	bool is_consonant( const char& c );

	/* decide whether the format of the word is a acceptable format.*/
	bool is_acceptable( const std::string& word );

	/* read the raw meterials of rules */
	bool read_raw_rules();

	/* make a rule struct form a raw entry */
	StemLancasterRule* make_rule( const std::string& raw_rule, int ruleID );

	/* get the word stemming from @word */
	StemLancasterRule stem( const std::string& word );

public:

	std::string operator() ( const std::string& word );

	std::string get_description() const;

	~StemLancaster(){};
	StemLancaster();
};
