#ifndef ___PATCHCODE_SEGMENTS__H__
#define ___PATCHCODE_SEGMENTS__H__

#include <iostream>
#include <string>
#include <vector>

using namespace std;

typedef vector<string> patch_lines;

class patchcode_segments {

	public:

		patch_lines start;
		patch_lines end;
		patch_lines change;

		string filename;
		string author;
		string patchname;
		string timestamp;
		string bugs;
		string extra;
		bool   enable;

		patchcode_segments();
		void print();
		void print_patchcode( patch_lines & );
};

#endif
