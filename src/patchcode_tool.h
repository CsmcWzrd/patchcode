#ifndef ___PATCHCODE_TOOL__H__
#define ___PATCHCODE_TOOL__H__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
using namespace std;


#include <patchcode_segments.h>
#include <patchcode_file.h>

typedef vector<patchcode_segments*> vector_patchcode_segments;
typedef map<string,patchcode_file*> map_patchcode_files;

enum SEGMENTS {
	SEG_INFO = 0,
	SEG_START,
	SEG_PATCH,
	SEG_END,
};

class patchcode_tool {
	public:

		patchcode_tool( fstream *f );
		~patchcode_tool ();

		void read_patches ( );
		void apply_patches ( );
		void print();

	private:
		int line_num;
		patchcode_segments *cur_patch_segment;
		vector_patchcode_segments patch_segments;
		map_patchcode_files patch_files;
		patch_lines cur_patch_code;
		fstream *patch_file;

		void process_info(string &s );
		void process_start ();
		void process_patch();
		void process_end();
};

#endif
