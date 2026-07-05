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

struct patchcode_apply_summary {
	int total;
	int applied;
	int already_applied;
	int disabled;
	int failed;
	patchcode_apply_summary();
};

enum SEGMENTS {
	SEG_INFO = 0,
	SEG_START,
	SEG_PATCH,
	SEG_END,
};

class patchcode_tool {
	public:

		patchcode_tool( fstream *f, bool backup_enabled = false );
		~patchcode_tool ();

		void read_patches ( );
		patchcode_apply_summary apply_patches ( const string &undo_patch_filename = string(), bool create_undo_patch = false );
		void print();

	private:
		int line_num;
		patchcode_segments *cur_patch_segment;
		vector_patchcode_segments patch_segments;
		map_patchcode_files patch_files;
		patch_lines cur_patch_code;
		fstream *patch_file;
		bool backup_enabled;

		bool write_undo_patch_file( const string &undo_patch_filename, vector_patchcode_segments &undo_segments );
		void process_info(string &s );
		void process_start ();
		void process_patch();
		void process_end();
};

#endif
