#ifndef ___PATCHCODE_FILE__H__
#define ___PATCHCODE_FILE__H__

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>
#include <patchcode_segments.h>

using namespace std;


class codeline {
	public:
		codeline();
		bool cr;
		bool lf;
		bool original;
		string line;
};

typedef vector<codeline*> file_sourcecode;

class patchcode_file {
	public :
		string  filename;
		file_sourcecode source;
		bool backup_done;
		bool file_read;

		patchcode_file  (string filename);
		~patchcode_file ();

		void create_backup();
		void read_file();
		int  apply_patch( patchcode_segments *segment, patchcode_segments *undo_segment = 0 );
		void rewrite_file();

		static string get_timestamp();

	private:

		int  find_pattern ( patch_lines *code );
		patch_lines copy_code_range( int start, int end );
		bool range_equals_code( int start, int end, patch_lines *code );
		bool tail_equals_code( patch_lines *code );
		void prepare_undo_segment( patchcode_segments *original_segment, patchcode_segments *undo_segment, patch_lines &undo_start, patch_lines &undo_change, patch_lines &undo_end );
		void clear_code   ();
		void delete_code  ( int start, int end );
		void insert_code  ( int at, patch_lines *code );
		void append_code  ( patch_lines *code );
		void prepend_code ( patch_lines *code );
		codeline *make_codeline( const string &line, bool original );
};

#endif
