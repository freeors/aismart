/* $Id: string_utils.cpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Copyright (C) 2005 - 2013 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various string-routines.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "util.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"

#include <algorithm>
#include <iomanip>

#include <openssl/aes.h>
#include <openssl/sha.h>

#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>

const std::string null_str = "";

#ifndef _WIN32
void CoCreateGuid(GUID* pguid)
{
	VALIDATE(false, null_str); // now CoCreateGuid only call when windows.
	memset(pguid, 0, sizeof(GUID));
}
#endif

namespace utils {

const std::string ellipsis = "...";

const std::string unicode_minus = "-";
const std::string unicode_en_dash = "-";
const std::string unicode_em_dash = "-";
const std::string unicode_figure_dash = "-";
const std::string unicode_multiplication_sign = "-";
const std::string unicode_bullet = "-";

bool to_bool(const std::string& str, bool def)
{
	if (str.empty()) {
		return def;
	}

	//  priority of use 'no'.
	if (str == "no" || str == "false") {
		return false;
	}
	return true;
}

int to_int(const std::string& str)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	int ret = 0;
	if (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X')) {
		// hex
		for (int at = 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			at = 1;
			flag = -1;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

size_t to_size_t(const std::string& str)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	size_t ret = 0;
	if (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X')) {
		// hex
		for (int at = 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			return 0;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

bool isnewline(const char c)
{
	return c == '\r' || c == '\n';
}

// Make sure that we can use Mac, DOS, or Unix style text files on any system
// and they will work, by making sure the definition of whitespace is consistent
bool portable_isspace(const char c)
{
	// returns true only on ASCII spaces
	if (static_cast<unsigned char>(c) >= 128)
		return false;
	return isnewline(c) || isspace(c);
}

// Make sure we regard '\r' and '\n' as a space, since Mac, Unix, and DOS
// all consider these differently.
bool notspace(const char c)
{
	return !portable_isspace(c);
}

std::string& strip(std::string &str)
{
	// If all the string contains is whitespace,
	// then the whitespace may have meaning, so don't strip it
	std::string::iterator it = std::find_if(str.begin(), str.end(), notspace);
	if (it == str.end())
		return str;

	str.erase(str.begin(), it);
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::string &strip_end(std::string &str)
{
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::vector< std::string > split(std::string const &val, const char c, const int flags)
{
	std::vector< std::string > res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
			
	while (i2 != val.end()) {
		if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip_end(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

std::vector< std::string > square_parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;
	std::vector<std::string::const_iterator> square_left;
	std::vector<std::string::const_iterator> square_right;
	std::vector< std::string > square_expansion;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	std::string::const_iterator j1;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	j1=i1;

	if (i1 == val.end()) return res;
	
	if (!separator) {
		// "Separator must be specified for square bracket split funtion.\n";
		return res;
	}

	if(left.size()!=right.size()){
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (true) {
		if(i2 == val.end() || (!in_parenthesis && *i2 == separator)) {
			//push back square contents
			size_t size_square_exp = 0;
			for (size_t i=0; i < square_left.size(); i++) {
				std::string tmp_val(square_left[i]+1,square_right[i]);
				std::vector< std::string > tmp = split(tmp_val);
				std::vector<std::string>::const_iterator itor = tmp.begin();
				for(; itor != tmp.end(); ++itor) {
					size_t found_tilde = (*itor).find_first_of('~');
					if (found_tilde == std::string::npos) {
						size_t found_asterisk = (*itor).find_first_of('*');
						if (found_asterisk == std::string::npos) {
							std::string tmp = (*itor);
							square_expansion.push_back(strip(tmp));
						}
						else { //'*' multiple expansion
							std::string s_begin = (*itor).substr(0,found_asterisk);
							s_begin = strip(s_begin);
							std::string s_end = (*itor).substr(found_asterisk+1);
							s_end = strip(s_end);
							for (int ast=atoi(s_end.c_str()); ast>0; --ast)
								square_expansion.push_back(s_begin);
						}
					}
					else { //expand number range
						std::string s_begin = (*itor).substr(0,found_tilde);
						s_begin = strip(s_begin);
						int begin = atoi(s_begin.c_str());
						size_t padding = 0;
						while (padding<s_begin.size() && s_begin[padding]=='0') {
							padding++;
						}
						std::string s_end = (*itor).substr(found_tilde+1);
						s_end = strip(s_end);
						int end = atoi(s_end.c_str());
						if (padding==0) {
							while (padding<s_end.size() && s_end[padding]=='0') {
								padding++;
							}
						}
						int increment = (end >= begin ? 1 : -1);
						end+=increment; //include end in expansion
						for (int k=begin; k!=end; k+=increment) {
							std::string pb = boost::lexical_cast<std::string>(k);
							for (size_t p=pb.size(); p<=padding; p++)
								pb = std::string("0") + pb;
							square_expansion.push_back(pb);
						}
					}
				}
				if (i*square_expansion.size() != (i+1)*size_square_exp ) {
					std::string tmp(i1, i2);
					// "Square bracket lengths do not match up: "+tmp+"\n";
					return res;
				}
				size_square_exp = square_expansion.size();
			}
			
			//combine square contents and rest of string for comma zone block
			size_t j = 0;
			size_t j_max = 0;
			if (square_left.size() != 0)
				j_max = square_expansion.size() / square_left.size();
			do {
				j1 = i1;
				std::string new_val;
				for (size_t i=0; i < square_left.size(); i++) {
					std::string tmp_val(j1, square_left[i]);
					new_val.append(tmp_val);
					size_t k = j+i*j_max;
					if (k < square_expansion.size())
						new_val.append(square_expansion[k]);
					j1 = square_right[i]+1;
				}
				std::string tmp_val(j1, i2);
				new_val.append(tmp_val);
				if (flags & STRIP_SPACES)
					strip_end(new_val);
				if (!(flags & REMOVE_EMPTY) || !new_val.empty())
					res.push_back(new_val);
				j++;
			} while (j<j_max);
			
			if (i2 == val.end()) //escape loop
				break;
			++i2;
			if (flags & STRIP_SPACES) { //strip leading spaces
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}
			i1=i2;
			square_left.clear();
			square_right.clear();
			square_expansion.clear();
			continue;
		}
		if(!part.empty() && *i2 == part.back()) {
			part.pop_back();
			if (*i2 == ']') square_right.push_back(i2);
			if (part.empty())
				in_parenthesis = false;
			++i2;
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++) {
			if (*i2 == lp[i]){
				if (*i2 == '[')
					square_left.push_back(i2);
				++i2;
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

std::vector< std::string > parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	
	if (left.size()!=right.size()) {
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (i2 != val.end()) {
		if(!in_parenthesis && separator && *i2 == separator){
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}
			i1=i2;
			continue;
		}
		if(!part.empty() && *i2 == part.back()){
			part.pop_back();
			if(!separator && part.empty()){
				std::string new_val(i1, i2);
				if (flags & STRIP_SPACES)
					strip(new_val);
				res.push_back(new_val);
				++i2;
				i1=i2;
			}else{
				if (part.empty())
					in_parenthesis = false;
				++i2;
			}
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++){
			if (*i2 == lp[i]){
				if (!separator && part.empty()){
					std::string new_val(i1, i2);
					if (flags & STRIP_SPACES)
						strip(new_val);
					res.push_back(new_val);
					++i2;
					i1=i2;
				}else{
					++i2;
				}
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

// Modify a number by string representing integer difference, or optionally %
int apply_modifier( const int number, const std::string &amount, const int minimum ) {
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(number * value);
	}
	value += number;
	if (( minimum > 0 ) && ( value < minimum ))
	    value = minimum;
	return value;
}

int apply_modifier( const int number, const std::string &amount, const int minimum, const int maximum)
{
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(maximum * value);
	}
	value += number;
	if (value < minimum)
	    value = minimum;
	if (value > maximum)
	    value = maximum;
	return value;
}

std::string escape(const std::string &str, const char *special_chars)
{
	std::string::size_type pos = str.find_first_of(special_chars);
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.insert(pos, 1, '\\');
		pos = res.find_first_of(special_chars, pos + 2);
	} while (pos != std::string::npos);
	return res;
}

std::string unescape(const std::string &str)
{
	std::string::size_type pos = str.find('\\');
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.erase(pos, 1);
		pos = res.find('\\', pos + 1);
	} while (pos != std::string::npos);
	return str;
}

bool string_bool(const std::string& str, bool def) {
	if (str.empty()) return def;

	// yes/no is the standard, test it first
	if (str == "yes") return true;
	if (str == "no"|| str == "false" || str == "off" || str == "0" || str == "0.0")
		return false;

	// all other non-empty string are considered as true
	return true;
}

std::string signed_value(int val)
{
	std::ostringstream oss;
	oss << (val >= 0 ? "+" : unicode_minus) << abs(val);
	return oss.str();
}

std::string half_signed_value(int val)
{
	std::ostringstream oss;
	if (val < 0)
		oss << unicode_minus;
	oss << abs(val);
	return oss.str();
}

static void si_string_impl_stream_write(std::stringstream &ss, double input) {
#ifdef _MSC_VER
	// Visual C++ makes 'precision' set the number of decimal places.
	// Other platforms make it set the number of significant figures
	ss.precision(1);
	ss << std::fixed
	   << input;
#else
	// Workaround to display 1023 KiB instead of 1.02e3 KiB
	if (input >= 1000)
		ss.precision(4);
	else
		ss.precision(3);
	ss << input;
#endif
}

std::string si_string(double input, bool base2, std::string unit) {
	const double multiplier = base2 ? 1024 : 1000;

	typedef boost::array<std::string, 9> strings9;

	strings9 prefixes;
	strings9::const_iterator prefix;
	if (input < 1.0) {
		strings9 tmp = { {
			"",
			_("prefix_milli^m"),
			_("prefix_micro^u"),
			_("prefix_nano^n"),
			_("prefix_pico^p"),
			_("prefix_femto^f"),
			_("prefix_atto^a"),
			_("prefix_zepto^z"),
			_("prefix_yocto^y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input < 1.0  && *prefix != prefixes.back()) {
			input *= multiplier;
			++prefix;
		}
	} else {
		strings9 tmp = { {
			"",
			(base2 ?
				_("prefix_kibi^K") :
				_("prefix_kilo^k")
			),
			_("prefix_mega^M"),
			_("prefix_giga^G"),
			_("prefix_tera^T"),
			_("prefix_peta^P"),
			_("prefix_exa^E"),
			_("prefix_zetta^Z"),
			_("prefix_yotta^Y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input > multiplier && *prefix != prefixes.back()) {
			input /= multiplier;
			++prefix;
		}
	}

	std::stringstream ss;
	si_string_impl_stream_write(ss, input);
	ss << ' '
	   << *prefix
	   << (base2 && (*prefix != "") ? _("infix_binary^i") : "")
	   << unit;
	return ss.str();
}

bool word_completion(std::string& text, std::vector<std::string>& wordlist) {
	std::vector<std::string> matches;
	const size_t last_space = text.rfind(" ");
	// If last character is a space return.
	if (last_space == text.size() -1) {
		wordlist = matches;
		return false;
	}

	bool text_start;
	std::string semiword;
	if (last_space == std::string::npos) {
		text_start = true;
		semiword = text;
	} else {
		text_start = false;
		semiword.assign(text, last_space + 1, text.size());
	}

	std::string best_match = semiword;
	for (std::vector<std::string>::const_iterator word = wordlist.begin();
			word != wordlist.end(); ++word)
	{
		if (word->size() < semiword.size()
		|| !std::equal(semiword.begin(), semiword.end(), word->begin(),
				chars_equal_insensitive))
		{
			continue;
		}
		if (matches.empty()) {
			best_match = *word;
		} else {
			int j = 0;
			while (toupper(best_match[j]) == toupper((*word)[j])) j++;
			if (best_match.begin() + j < best_match.end()) {
				best_match.erase(best_match.begin() + j, best_match.end());
			}
		}
		matches.push_back(*word);
	}
	if(!matches.empty()) {
		text.replace(last_space + 1, best_match.size(), best_match);
	}
	wordlist = matches;
	return text_start;
}

static bool is_word_boundary(char c) {
	return (c == ' ' || c == ',' || c == ':' || c == '\'' || c == '"' || c == '-');
}

bool word_match(const std::string& message, const std::string& word) {
	size_t first = message.find(word);
	if (first == std::string::npos) return false;
	if (first == 0 || is_word_boundary(message[first - 1])) {
		size_t next = first + word.size();
		if (next == message.size() || is_word_boundary(message[next])) {
			return true;
		}
	}
	return false;
}

bool wildcard_string_match(const std::string& str, const std::string& match) {
	const bool wild_matching = (!match.empty() && match[0] == '*');
	const std::string::size_type solid_begin = match.find_first_not_of('*');
	const bool have_solids = (solid_begin != std::string::npos);
	// Check the simple case first
	if(str.empty() || !have_solids) {
		return wild_matching || str == match;
	}
	const std::string::size_type solid_end = match.find_first_of('*', solid_begin);
	const std::string::size_type solid_len = (solid_end == std::string::npos)
		? match.length() - solid_begin : solid_end - solid_begin;
	std::string::size_type current = 0;
	bool matches;
	do {
		matches = true;
		// Now try to place the str into the solid space
		const std::string::size_type test_len = str.length() - current;
		for(std::string::size_type i=0; i < solid_len && matches; ++i) {
			char solid_c = match[solid_begin + i];
			if(i > test_len || !(solid_c == '?' || solid_c == str[current+i])) {
				matches = false;
			}
		}
		if(matches) {
			// The solid space matched, now consume it and attempt to find more
			const std::string consumed_match = (solid_begin+solid_len < match.length())
				? match.substr(solid_end) : "";
			const std::string consumed_str = (solid_len < test_len)
				? str.substr(current+solid_len) : "";
			matches = wildcard_string_match(consumed_str, consumed_match);
		}
	} while(wild_matching && !matches && ++current < str.length());
	return matches;
}

std::vector< std::string > quoted_split(std::string const &val, char c, int flags, char quote)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while (i2 != val.end()) {
		if (*i2 == quote) {
			// Ignore quoted character
			++i2;
			if (i2 != val.end()) ++i2;
		} else if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while(i2 != val.end() && *i2 == ' ')
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

std::pair< int, int > parse_range(std::string const &str)
{
	const std::string::const_iterator dash = std::find(str.begin(), str.end(), '-');
	const std::string a(str.begin(), dash);
	const std::string b = dash != str.end() ? std::string(dash + 1, str.end()) : a;
	std::pair<int,int> res(atoi(a.c_str()), atoi(b.c_str()));
	if (res.second < res.first)
		res.second = res.first;

	return res;
}

std::vector< std::pair< int, int > > parse_ranges(std::string const &str)
{
	std::vector< std::pair< int, int > > to_return;
	std::vector<std::string> strs = utils::split(str);
	std::vector<std::string>::const_iterator i, i_end=strs.end();
	for(i = strs.begin(); i != i_end; ++i) {
		to_return.push_back(parse_range(*i));
	}
	return to_return;
}

static int byte_size_from_utf8_first(unsigned char ch)
{
	int count;

	if ((ch & 0x80) == 0)
		count = 1;
	else if ((ch & 0xE0) == 0xC0)
		count = 2;
	else if ((ch & 0xF0) == 0xE0)
		count = 3;
	else if ((ch & 0xF8) == 0xF0)
		count = 4;
	else if ((ch & 0xFC) == 0xF8)
		count = 5;
	else if ((ch & 0xFE) == 0xFC)
		count = 6;
	else
		throw invalid_utf8_exception(); // Stop on invalid characters

	return count;
}

utf8_iterator::utf8_iterator(const std::string& str) :
	current_char(0),
	string_end(str.end()),
	current_substr(std::make_pair(str.begin(), str.begin()))
{
	update();
}

utf8_iterator::utf8_iterator(std::string::const_iterator const &beg,
		std::string::const_iterator const &end) :
	current_char(0),
	string_end(end),
	current_substr(std::make_pair(beg, beg))
{
	update();
}

utf8_iterator utf8_iterator::begin(std::string const &str)
{
	return utf8_iterator(str.begin(), str.end());
}

utf8_iterator utf8_iterator::end(const std::string& str)
{
	return utf8_iterator(str.end(), str.end());
}

bool utf8_iterator::operator==(const utf8_iterator& a) const
{
	return current_substr.first == a.current_substr.first;
}

utf8_iterator& utf8_iterator::operator++()
{
	current_substr.first = current_substr.second;
	update();
	return *this;
}

wchar_t utf8_iterator::operator*() const
{
	return current_char;
}

bool utf8_iterator::next_is_end()
{
	if(current_substr.second == string_end)
		return true;
	return false;
}

const std::pair<std::string::const_iterator, std::string::const_iterator>& utf8_iterator::substr() const
{
	return current_substr;
}

void utf8_iterator::update()
{
	// Do not try to update the current unicode char at end-of-string.
	if(current_substr.first == string_end)
		return;

	size_t size = byte_size_from_utf8_first(*current_substr.first);
	current_substr.second = current_substr.first + size;

	current_char = static_cast<unsigned char>(*current_substr.first);
	// Convert the first character
	if(size != 1) {
		current_char &= 0xFF >> (size + 1);
	}

	// Convert the continuation bytes
	for(std::string::const_iterator c = current_substr.first+1;
			c != current_substr.second; ++c) {
		// If the string ends occurs within an UTF8-sequence, this is bad.
		if (c == string_end)
			throw invalid_utf8_exception();

		if ((*c & 0xC0) != 0x80)
			throw invalid_utf8_exception();

		current_char = (current_char << 6) | (static_cast<unsigned char>(*c) & 0x3F);
	}
}


std::string wstring_to_string(const wide_string &src)
{
	std::string ret;

	try {
		for(wide_string::const_iterator i = src.begin(); i != src.end(); ++i) {
			unsigned int count;
			wchar_t ch = *i;

			// Determine the bytes required
			count = 1;
			if(ch >= 0x80)
				count++;

			Uint32 bitmask = 0x800;
			for(unsigned int j = 0; j < 5; ++j) {
				if(static_cast<Uint32>(ch) >= bitmask) {
					count++;
				}

				bitmask <<= 5;
			}

			if(count > 6) {
				throw invalid_utf8_exception();
			}

			if(count == 1) {
				ret.push_back(static_cast<char>(ch));
			} else {
				for(int j = static_cast<int>(count) - 1; j >= 0; --j) {
					unsigned char c = (ch >> (6 * j)) & 0x3f;
					c |= 0x80;
					if(j == static_cast<int>(count) - 1) {
						c |= 0xff << (8 - count);
					}
					ret.push_back(c);
				}
			}

		}

		return ret;
	}
	catch (invalid_utf8_exception&) {
		// "Invalid wide character string\n";
		return ret;
	}
}

std::string wchar_to_string(const wchar_t c)
{
	wide_string s;
	s.push_back(c);
	return wstring_to_string(s);
}

wide_string string_to_wstring(const std::string &src)
{
	wide_string res;

	try {
		utf8_iterator i1(src);
		const utf8_iterator i2(utf8_iterator::end(src));

		// Equivalent to res.insert(res.end(),i1,i2) which doesn't work on VC++6.
		while(i1 != i2) {
			res.push_back(*i1);
			++i1;
		}
	}
	catch (invalid_utf8_exception&) {
		// "Invalid UTF-8 string: \"" << src << "\"\n";
		return res;
	}

	return res;
}

std::string utf8str_insert(const std::string& src, const int from, const std::string& insert)
{
	if (insert.empty()) {
		return src;
	}
	VALIDATE(from >= 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	int at = 0;
	for (; it != end; ++ it, at ++) {
		if (at == from) {
			ret.append(insert);
		}
		ret.append(it.substr().first, it.substr().second);
	}
	if (at == from) {
		ret.append(insert);
	}
	return ret;
}

std::string utf8str_erase(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			continue;
		}
		ret.append(it.substr().first, it.substr().second);
	}
	return ret;
}

std::string utf8str_substr(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			ret.append(it.substr().first, it.substr().second);
			continue;
		}
	}
	return ret;
}

size_t utf8str_len(const std::string& utf8str)
{
	size_t size = 0;
	utils::utf8_iterator itor(utf8str);
	utils::utf8_iterator end = utils::utf8_iterator::end(utf8str);
	for (; itor != end; ++ itor) {
		size ++;
	}
	return size;
}

bool is_utf8str(const std::string& utf8str)
{
	try {
		utf8_iterator ch(utf8str);
		for (utf8_iterator end = utf8_iterator::end(utf8str); ch != end; ++ ch) {
		}
	}
	catch (utils::invalid_utf8_exception&) {
		return false;
	}
	return true;
}

void truncate_as_wstring(std::string& str, const size_t size)
{
	wide_string utf8_str = utils::string_to_wstring(str);
	if(utf8_str.size() > size) {
		utf8_str.resize(size);
		str = utils::wstring_to_string(utf8_str);
	}
}

void ellipsis_truncate(std::string& str, const size_t size)
{
	const size_t prev_size = str.length();

	truncate_as_wstring(str, size);

	if(str.length() != prev_size) {
		str += ellipsis;
	}
}

bool utf8str_compare(const std::string& str1, const std::string& str2)
{
	utils::utf8_iterator itor1(str1);
	utils::utf8_iterator itor1_end = utils::utf8_iterator::end(str1);
	utils::utf8_iterator itor2(str2);
	utils::utf8_iterator itor2_end = utils::utf8_iterator::end(str2);

	for (; itor1 != itor1_end; ++ itor1, ++ itor2) {
		if (itor2 == itor2_end) {
			return false;
		}
		wchar_t key1 = *itor1;
		wchar_t key2 = *itor2;
		if (key1 < key2) {
			return true;
		} else if (key1 > key2) {
			return false;
		}
	}
	return true;
}

uint32_t check_color(const std::string& color_str)
{
	std::vector<std::string> fields = utils::split(color_str);
	if (fields.size() != 4) {
		return 0;
	}

	int val;
	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		
		val = utils::to_int(fields[i]);
		if (val < 0 || val > 255) {
			return 0;
		}
		result |= utils::to_int(fields[i]);
	}
	if (!(result & 0xff000000)) {
		// avoid to confuse with special color.
		return 0;
	}
	return result;
}

std::set<int> to_set_int(const std::string& value)
{
	std::vector<std::string> vstr = split(value);
	std::set<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.insert(lexical_cast_default<int>(*it));
	}
	return ret;
}

std::vector<int> to_vector_int(const std::string& value)
{
	std::vector<std::string> vstr = split(value);
	std::vector<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.push_back(lexical_cast_default<int>(*it));
	}
	return ret;
}

std::string lowercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void lowercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			str[i] = c_str[i] + diff;
		}
	}
}

std::string uppercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void uppercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			str[i] = c_str[i] + diff;
		}
	}
}


// condition id(only ASCII)/variable(only ASCII)/username
static bool is_id_char(char c)
{
	return ((c == '_') || (c == '-') || (c == ' '));
}

static bool is_variable_char(char c) 
{
	return ((c == '_') || (c == '-') || (c == '.'));
}

typedef bool (*is_xxx_char)(char c);

std::string errstr;
bool isvalid_id_base(const std::string& id, bool first_must_alpha, is_xxx_char fn, int min, int max)
{
	utils::string_map symbols;
	int s = (int)id.size();
	if (!s) {
		if (min > 0) {
			errstr = _("Can not empty!");
 			return false;
		}
		return true;
	}
	if (s < min) {
		symbols["min"] = str_cast(min);
		errstr = vgettext2("At least $min characters!", symbols);
		return false;
	}
	if (s > max) {
		symbols["max"] = str_cast(max);
		errstr = vgettext2("Can not be larger than $max characters!", symbols);
		return false;
	}
	char c = id.at(0);
	if (c == ' ') {
		errstr = _("First character can not empty!");
		return false;
	}
	if (first_must_alpha && !isalpha(c)) {
		errstr = _("First character must be alpha!");
		return false;
	}
	if (id == "null") {
		symbols["str"] = id;
		errstr = vgettext2("$str is reserved string!", symbols);
		return false;
	}

	const size_t alnum = std::count_if(id.begin(), id.end(), isalnum);
	const size_t valid_char = std::count_if(id.begin(), id.end(), fn);
	if ((alnum + valid_char != s) || valid_char == id.size()) {
		errstr = _("Contains invalid characters!");
		return false;
	}
	return true;
}

bool isvalid_id(const std::string& id, bool first_must_alpha, int min, int max)
{
	return isvalid_id_base(id, first_must_alpha, is_id_char, min, max);
}

bool isvalid_variable(const std::string& id, int min, int max)
{
	return isvalid_id_base(id, true, is_variable_char, min, max);
}

bool isvalid_nick(const std::string& nick)
{ 
	const int max_nick_size = 31;
	return isvalid_id_base(nick, false, is_id_char, 1, max_nick_size);
}

static bool is_username_char(char c) {
	return ((c == '_') || (c == '-'));
}

static bool is_wildcard_char(char c) {
    return ((c == '?') || (c == '*'));
}

bool isvalid_username(const std::string& username) 
{
	size_t alnum = 0, valid_char = 0, chars = 0;
	try {
		utils::utf8_iterator itor(username);
		for (; itor != utils::utf8_iterator::end(username); ++ itor) {
			wchar_t w = *itor;
			
			if ((w & 0xff00) || isalnum(w)) {
				alnum ++;
			} else if (is_username_char(w)) {
				valid_char ++;
			}
			chars ++;
		}
		if ((alnum + valid_char != chars) || valid_char == chars || !chars) {
			errstr = _("Contains invalid characters!");
			return false;
		}
	} catch (utils::invalid_utf8_exception&) {
		errstr = _("Invalid UTF-8 string!");
		return false;
	}
	return true;
}

bool isvalid_wildcard(const std::string& username) 
{
    const size_t alnum = std::count_if(username.begin(), username.end(), isalnum);
	const size_t valid_char =
			std::count_if(username.begin(), username.end(), is_username_char);
    const size_t wild_char =
            std::count_if(username.begin(), username.end(), is_wildcard_char);
	if ((alnum + valid_char + wild_char != username.size())
			|| valid_char == username.size() || username.empty() )
	{
		return false;
	}
	return true;
}

bool isinteger(const std::string& str)
{
	const int len = (int)str.size();
	if (!len) {
		return false;
	}

	const char* c_str = str.c_str();
	char ch = c_str[0];
	int at = 0;
	if (ch == '-' || ch == '+') {
		if (len == 1) {
			return false;
		}
		at = 1;
	}
	for (; at < len; at ++) {
		if (c_str[at] < '0' || c_str[at] > '9') {
			return false;
		}
	}
	return true;
}

// if cstr is NULL, 'return cstr' directly will result 'access violation'.
std::string cstr_2_str(const char* cstr)
{
	return (cstr)? (cstr): null_str;
}

std::string generate_app_prefix_id(const std::string& app, const std::string& id)
{
	if (!app.empty()) {
		return app + "__" + id;
	}
	return id;
}

std::pair<std::string, std::string> split_app_prefix_id(const std::string& id2)
{
	size_t pos = id2.find("__");
	if (pos == std::string::npos) {
		return std::make_pair(null_str, id2);
	}
	return std::make_pair(id2.substr(0, pos), id2.substr(pos + 2));
}

bool bom_magic_started(const uint8_t* data, int size)
{
	return size >= BOM_LENGTH && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf;
}

// find the pos that non-blank character at it.
const char* skip_blank_characters(const char* start)
{
	while (start[0] == '\r' || start[0] == '\n' || start[0] == '\t' || start[0] == ' ') {
		start ++;
	}
	return start;
}

// find the pos that blank character(\r\n\t ) at it.
const char* until_blank_characters(const char* start)
{
	while (start[0] != '\r' && start[0] != '\n' && start[0] != '\t' && start[0] != ' ') {
		start ++;
	}
	return start;
}

// find the pos that c-style word terminate character(\r\n\t ;) at it.
const char* until_c_style_characters(const char* start)
{
	while (start[0] != '\r' && start[0] != '\n' && start[0] != '\t' && start[0] != ' ' && start[0] != ';') {
		start ++;
	}
	return start;
}

// 1. single block. cannot only attribute.
// 2. may exist space char before [name].
// 3. may exist space char after [/name].
bool is_single_cfg(const std::string& str, std::string* element_name)
{
	const int len = str.size();
	const char* cstr = str.c_str();
	int ch;

	if (len < 7) { // [1][1/]
		return false;
	}
	int pos = 0;
	int name_start = -1;
	for (pos = 0; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		if (name_start == -1) {
			if (ch != '[') {
				// non-space char must be begin with [.
				return false;
			}
			name_start = pos;
		} else if (ch == ']') {
			break;
		}
	}
	if (pos == len) {
		return false;
	}
	std::string key(cstr + name_start, pos + 1 - name_start); // key = [name].
	key.insert(1, "/");
	const char* ptr = strstr(cstr, key.c_str());
	if (ptr == NULL) {
		// cannot find [/name]
		return false;
	}
	for (pos = ptr + key.size() - cstr; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		// all characters after [name] must space char.
		return false;
	}
	if (element_name) {
		*element_name = key.substr(2, key.size() - 3);
	}
	return true;
}

void verify_splited_intergate(const std::string& text, const std::map<int, tcfg_string_pair>& segments)
{
	if (text.empty()) {
		VALIDATE(segments.empty(), null_str);
		return;
	}
	VALIDATE(!segments.empty(), null_str);

	const char* c_str = text.c_str();
	const int size = text.size();
	int next_item_start = 0;
	for (std::map<int, tcfg_string_pair>::const_iterator it = segments.begin(); it !=segments.end(); ++ it) {
		const int item_start = it->first;
		const tcfg_string_pair& segment = it->second;
		VALIDATE(item_start >= 0 && item_start < size, null_str);
		if (next_item_start != -1) {
			VALIDATE(item_start == next_item_start, null_str);
		}

		if (segment.iscfg) {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + (text_size + 2) * 2 < size, null_str);
			std::stringstream s;
			s << "<" << segment.text << ">";
			VALIDATE(!SDL_memcmp(c_str + item_start, s.str().c_str(), s.str().size()), null_str); 
			next_item_start = -1;
		} else {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + text_size <= size, null_str);
			VALIDATE(!SDL_memcmp(c_str + item_start, segment.text.c_str(), text_size), null_str);
			next_item_start = item_start + text_size;
		}
	}
}

static config convert_to_wml(const char* contents, const int size)
{
	std::stringstream ss;
	tpoint quotes_pos(-1, -1);
	bool in_quotes = false;
	bool last_char_escape = false;
	const char escape_char = '\\';

	VALIDATE(contents, null_str);

	if (!size || !notspace(contents[0]) || !notspace(contents[size - 1])) {
		return null_cfg;
	}

	std::vector<std::pair<std::string, tpoint> > attributes;
	// Find the different attributes.
	// No checks are made for the equal sign or something like that.
	// Attributes are just separated by spaces or newlines.
	// Attributes that contain spaces must be in single quotes.
	for (int pos = 0; pos < size; ++ pos) {
		const char c = contents[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		} else {
			if (c == '"' && !last_char_escape) {
				if (in_quotes) {
					quotes_pos.y = ss.str().size();
				} else {
					quotes_pos.x = ss.str().size();
				}
				ss << '"';
				in_quotes = !in_quotes;
			} else if ((c == ' ' || c == '\n') && !last_char_escape && !in_quotes) {
				// Space or newline, end of attribute.
				attributes.push_back(std::make_pair(ss.str(), quotes_pos));
				quotes_pos.x = quotes_pos.y = -1;
				ss.str("");
			} else {
				ss << c;
			}
			last_char_escape = false;
		}
	}
	if (in_quotes) {
		// Unterminated single quote after: 'ss.str()'
		return null_cfg;
	}
	if (ss.str() != "") {
		attributes.push_back(std::make_pair(ss.str(), quotes_pos));
	}

	config ret;
	for (std::vector<std::pair<std::string, tpoint> >::const_iterator it = attributes.begin(); it != attributes.end(); ++ it) {
		const std::string& text = it->first;
		const tpoint quotes_pos = it->second;
		const char* c_str = text.c_str();
		const int size = text.size();
		if (size <= 2) {
			return null_cfg;
		}
		size_t equal_pos = text.find('=');
		if (equal_pos == std::string::npos) {
			return null_cfg;
		}
		if (quotes_pos.x != -1 && quotes_pos.y <= (int)equal_pos) {
			return null_cfg;
		}
		std::string key = text.substr(0, equal_pos);
		strip(key);

		if (key.empty() || !notspace(key[0])) {
			// If all the string contains is whitespace,
			// then the whitespace may have meaning, strip will don't strip it
			return null_cfg;
		}
		
		std::string value;
		if (quotes_pos.x != -1) {
			value.assign(text, quotes_pos.x + 1, quotes_pos.y - quotes_pos.x - 1);
		} else {
			value = text.substr(equal_pos + 1);
			strip(value);
		}
		ret[key] = value;
	}

	return ret;
}

void split_integrate_src(const std::string& text, const std::set<std::string>& support_markups, std::map<int, tcfg_string_pair>& ret)
{
	ret.clear();

	int item_start = 0;
	int maybe_cfg_item_start = 0;
	const char* c_str = text.c_str();
	const int size = text.size();
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (int pos = 0; pos < size; ++ pos) {
		const char c = c_str[pos];
		if (state == OTHER) {
			if (c == '<') {
				maybe_cfg_item_start = pos;
				state = ELEMENT_NAME;
			}

		} else if (state == ELEMENT_NAME) {
			if (c == '/') {
				// Erroneous / in element name.
				// maybe_cfg_item_start = -1;
				state = OTHER;

			} else if (c == '<') {
				// overload
				maybe_cfg_item_start = pos;

			} else if (c == '>') {
				// End of this name.
				std::stringstream s;
				const std::string element_name(text.substr(maybe_cfg_item_start + 1, pos - maybe_cfg_item_start - 1));
				if (support_markups.find(element_name) != support_markups.end()) {
					s << "</" << element_name << ">";
					const std::string end_element_name = s.str();
					size_t end_pos = text.find(end_element_name, pos);
					if (end_pos != std::string::npos) {
						const int size = end_pos - pos - 1;
						const config cfg = convert_to_wml(text.c_str() + pos + 1, size);
						if (!cfg.empty()) {
							std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
							if (item_start != maybe_cfg_item_start) {
								VALIDATE(item_start < maybe_cfg_item_start, null_str);
								// previous
								ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
								ins.first->second.iscfg = false;
								ins.first->second.text = text.substr(item_start, maybe_cfg_item_start - item_start);

								item_start = maybe_cfg_item_start;
							}

							ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
							ins.first->second.iscfg = true;
							ins.first->second.text = element_name;
							ins.first->second.cfg = cfg;

							pos = end_pos + end_element_name.size() - 1;
							item_start = pos + 1;
						}
					}
				}

				state = OTHER;
			}
		}
	}
	if (item_start != size) {
		std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
		ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
		ins.first->second.iscfg = false;
		ins.first->second.text = text.substr(item_start, size - item_start);
	}

	verify_splited_intergate(text, ret);
}

// when tintegrate::insert_str, string will insert in <format>...</format>
// because must not markup-nest, inserted string shoulu be drop markup.
std::string ht_drop_markup(const std::string& text, const std::set<std::string>& support_markups, const std::string& text_markup)
{
	if (text.empty()) {
		return null_str;
	}
	std::map<int, tcfg_string_pair> splited_items;
	split_integrate_src(text, support_markups, splited_items);
	if (splited_items.size() == 1 && !splited_items[0].iscfg) {
		return text;
	}

	std::stringstream ss;
	std::map<int, utils::tcfg_string_pair>::const_iterator it = splited_items.begin();
	std::map<int, utils::tcfg_string_pair>::const_iterator before_it = it;
	for (++ it; ; before_it = it, ++ it) {
		if (before_it->second.iscfg) {
			if (before_it->second.text == text_markup) {
				// string shoulde be in <format>...</format>, require escape.
				std::string stuffed = tintegrate::stuff_escape(before_it->second.cfg["text"].str(), true);
				ss << stuffed;
			}

		} else {
			if (it != splited_items.end()) {
				ss << text.substr(before_it->first, it->first - before_it->first);
			} else {
				ss << text.substr(before_it->first);
			}
		}
		if (it == splited_items.end()) {
			break;
		}
	}
	return ss.str();
}

// @slash: true-->slash, false-->backslash
// get rid of all /.. and /. and //
std::string normalize_path(const std::string& src, bool slash)
{
	// because of thread-safe, must not use static variable.
	char* data = NULL;
	int size = (int)src.size();

	const char separator = slash? '/': '\\';
	const char require_modify_separator = slash? '\\': '/';
	const char* c_str = src.c_str();
	bool has_dot = false;
	bool has_continue_separator = false;
	int sep = -2;

	// 1. replace all require_modify_separator to separator.
	for (int at = 0; at < size; at++) {
		const char ch = c_str[at];
		if (ch == separator) {
			if (data) {
				data[at] = ch;
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			sep = at;
		} else if (ch == require_modify_separator) {
			if (!data) {
				data = (char*)malloc(posix_align_ceil(size + 1, 128));
				if (at) {
					memcpy(data, c_str, at);
				}
				data[size] = '\0';
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			data[at] = separator;
			sep = at;
		} else {
			if (!has_dot && ch == '.' && (sep + 1 == at)) {
				// only consider '.' next to /.
				// filename maybe exist '.', ex: _main.cfg
				has_dot = true;
			}
			if (data) {
				data[at] = ch;
			}
		}
	}

	if (!has_dot && !has_continue_separator) {
		if (!data) {		
			return src;
		}
		std::string ret(data, size);
		free(data);
		return ret;
	}

	if (!data) {
		data = (char*)malloc(posix_align_ceil(size + 1, 128));
		memcpy(data, c_str, size);
		data[size] = '\0';
	}
	
	// 2. delete all /.. and /. and //
	while (1) {
		sep = -1;
		bool exit = true;
		for (int at = 0; at < size - 1; at++) {
			if (data[at] == separator) {
				if (data[at + 1] == '.') {
					if (data[at + 2] == '.') {
						// /..
						VALIDATE(sep != -1, null_str);
						if (size - at - 3) {
							SDL_memcpy(data + sep, data + at + 3, (size - at - 3));
						}
						size -= at - sep + 3;
						data[size] = '\0';
						exit = false;

					} else if (at + 2 >= size || data[at + 2] == '/') {
						// /.    ===>next is '/', or '.' is last char. delete these tow chars.
						if (size - at - 2) {
							SDL_memcpy(data + at, data + at + 2, (size - at - 2));
						}
						size -= 2;
						data[size] = '\0';
						exit = false;
					}
					
				} else if (data[at + 1] == '/') {
					// //    ===>delete second '/'
					if (size - at - 1) {
						SDL_memcpy(data + at, data + at + 1, (size - at - 1));
					}
					size -= 1;
					data[size] = '\0';
					exit = false;
				}

				if (!exit) {
					break;
				}
				sep = at;
			}
		}
		if (exit) {
			break;
		}
	}

	std::string ret(data, size);
	free(data);
	return ret;
}

std::string format_guid(const GUID& guid)
{
	// ==> 3641E31E-36BF-4E03-8879-DE33ADC07D68
	std::stringstream ss;

	ss << std::setw(8) << std::setfill('0') << std::setbase(16) << guid.Data1 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data2 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data3 << "-";
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[0];
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[1] << "-";
	for (int at = 2; at < 8; at ++) {
		ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[at];
	}

	return uppercase(ss.str());
}

void aes_256_encrypt(const uint8_t* key, const uint8_t* in, uint8_t* out, const int bytes)
{
	const int key_bytes = 32;
	VALIDATE(bytes > 0 && (bytes % key_bytes) == 0, null_str);

	AES_KEY enckey;
	AES_set_encrypt_key(key, 256, &enckey);

	// The IV used to encrypt block[n] is ciphertext(block[n-1]) unless n == 0, which is when the input IV is used. 
	// This copy makes sense if you think about calling this API in a loop. 
	// The output of the previous block is the IV for the next block. 
	uint8_t iv[key_bytes];
    memset(iv, 0x00, key_bytes);
	AES_cbc_encrypt(in, out, bytes, &enckey, iv, AES_ENCRYPT);
}

void aes_256_decrypt(const uint8_t* key, const uint8_t* in, uint8_t* out, const int bytes)
{
	const int key_bytes = 32;
	VALIDATE(bytes > 0 && (bytes % key_bytes) == 0, null_str);

	AES_KEY deckey;
	AES_set_decrypt_key(key, 256, &deckey);

	uint8_t iv[key_bytes];
    memset(iv, 0x00, key_bytes);
	AES_cbc_encrypt(in, out, bytes, &deckey, iv, AES_DECRYPT);
}

std::unique_ptr<uint8_t> sha1(const uint8_t* in, const int len)
{
	VALIDATE(in && len > 0, null_str);
	std::unique_ptr<uint8_t> md(new uint8_t[SHA_DIGEST_LENGTH]);
	uint8_t* mdptr = md.get();
    SHA1(in, len, mdptr);

	std::string str = rtc::hex_encode((const char*)mdptr, SHA_DIGEST_LENGTH);

	return md;
}

std::unique_ptr<uint8_t> sha256(const uint8_t* in, const int len)
{
	VALIDATE(in && len > 0, null_str);
	std::unique_ptr<uint8_t> md(new uint8_t[SHA256_DIGEST_LENGTH]);
    SHA256(in, len, md.get());
	return md;
}

} // end namespace utils
