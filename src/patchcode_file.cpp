#include <patchcode_file.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

using std::chrono::system_clock;
using namespace std;


static bool pc_is_path_separator(char c) {
	return c == '/' || c == '\\';
}

static string pc_parent_path(string path) {
	for( size_t i = 0; i < path.size(); ++i ) {
		if( path[i] == '\\' )
			path[i] = '/';
	}
	while( path.size() > 1 && pc_is_path_separator(path[path.size() - 1]) )
		path.erase(path.size() - 1);
	size_t pos = path.find_last_of('/');
	if( pos == string::npos )
		return string();
	if( pos == 0 )
		return string("/");
	return path.substr(0, pos);
}

static bool pc_directory_exists(const string &path) {
	if( path.empty() )
		return true;
#if defined(_WIN32)
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool pc_make_directory(const string &path) {
	if( path.empty() || pc_directory_exists(path) )
		return true;
#if defined(_WIN32)
	return _mkdir(path.c_str()) == 0 || pc_directory_exists(path);
#else
	return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST || pc_directory_exists(path);
#endif
}

static bool pc_ensure_directory_tree(const string &dir) {
	if( dir.empty() || pc_directory_exists(dir) )
		return true;

	string normalized = dir;
	for( size_t i = 0; i < normalized.size(); ++i ) {
		if( normalized[i] == '\\' )
			normalized[i] = '/';
	}

	string current;
	size_t pos = 0;
	if( !normalized.empty() && normalized[0] == '/' ) {
		current = "/";
		pos = 1;
	} else if( normalized.size() >= 2 && normalized[1] == ':' ) {
		current = normalized.substr(0, 2);
		pos = 2;
		if( pos < normalized.size() && normalized[pos] == '/' ) {
			current += "/";
			pos++;
		}
	}

	while( pos <= normalized.size() ) {
		size_t next = normalized.find('/', pos);
		string part = normalized.substr(pos, next == string::npos ? string::npos : next - pos);
		if( !part.empty() ) {
			if( !current.empty() && !pc_is_path_separator(current[current.size() - 1]) )
				current += "/";
			current += part;
			if( !pc_make_directory(current) )
				return false;
		}
		if( next == string::npos )
			break;
		pos = next + 1;
	}
	return true;
}

static bool pc_ensure_parent_directory(const string &filename) {
	string parent = pc_parent_path(filename);
	if( parent.empty() )
		return true;
	return pc_ensure_directory_tree(parent);
}

static bool read_physical_line(istream &in, string &line) {
	line.clear();
	char c = 0;
	while (in.get(c)) {
		line.push_back(c);
		if (c == '\n')
			break;
	}
	return !line.empty();
}

codeline::codeline() {
	cr = false;
	lf = false;
	original = false;
}

patchcode_file::patchcode_file  (string f){
	filename = f;
	backup_done = false;
	file_read   = false;
}

patchcode_file::~patchcode_file (){
	clear_code();
}

codeline *patchcode_file::make_codeline( const string &line, bool original ) {
	codeline *c = new codeline();
	c->line = line;
	c->original = original;
	if( !line.empty() && line[line.size() - 1] == '\n' )
		c->lf = true;
	if( line.size() >= 2 && line[line.size() - 2] == '\r' && line[line.size() - 1] == '\n' )
		c->cr = true;
	else if( !line.empty() && line[line.size() - 1] == '\r' )
		c->cr = true;
	return c;
}

void patchcode_file::clear_code ( ){
	file_sourcecode::iterator i = source.begin();
	while( i != source.end() ){
		delete *i;
		i = source.erase(i);
	}
	source.clear();
}

int patchcode_file::find_pattern( patch_lines *code ){
	if( code == 0 || code->empty() )
		return 0;

	const size_t pattern_size = code->size();
	const size_t source_size = source.size();
	if( pattern_size > source_size )
		return -1;

	for( size_t pos = 0; pos + pattern_size <= source_size; ++pos ){
		bool matched = true;
		for( size_t line_num = 0; line_num < pattern_size; ++line_num ){
			if( source[pos + line_num]->line != (*code)[line_num] ){
				matched = false;
				break;
			}
		}
		if( matched )
			return static_cast<int>(pos + pattern_size); // zero-indexed insertion point after the pattern
	}
	return -1;
}


void patchcode_file::delete_code( int start, int end ){
	if( start < 0 )
		start = 0;
	if( end < 0 )
		end = 0;
	if( start > static_cast<int>(source.size()) )
		start = static_cast<int>(source.size());
	if( end > static_cast<int>(source.size()) )
		end = static_cast<int>(source.size());
	if( end <= start )
		return;

	file_sourcecode::iterator first = source.begin() + start;
	file_sourcecode::iterator last = source.begin() + end;
	for( file_sourcecode::iterator i = first; i != last; ++i )
		delete *i;
	source.erase(first, last);
}

void patchcode_file::insert_code( int at, patch_lines *code ){
	if( code == 0 || code->empty() )
		return;
	if( at < 0 )
		at = 0;
	if( at > static_cast<int>(source.size()) )
		at = static_cast<int>(source.size());

	patch_lines::iterator i = code->begin();
	file_sourcecode::iterator j = source.begin() + at;

	while( i != code->end() ){
		codeline *c = make_codeline( *i, false );
		j = source.insert( j, c );
		j++;
		i++;
	}
	return;
}

void patchcode_file::append_code( patch_lines *code ){
	if( code == 0 )
		return;
	patch_lines::iterator i = code->begin();
	while( i != code->end() ){
		codeline *c = make_codeline( *i, false );
		source.push_back( c );
		i++;
	}
	return;
}

void patchcode_file::prepend_code( patch_lines *code ){
	insert_code( 0, code );
}
		

patch_lines patchcode_file::copy_code_range( int start, int end ){
	patch_lines result;
	if( start < 0 )
		start = 0;
	if( end < 0 )
		end = 0;
	if( start > static_cast<int>(source.size()) )
		start = static_cast<int>(source.size());
	if( end > static_cast<int>(source.size()) )
		end = static_cast<int>(source.size());
	if( end <= start )
		return result;

	for( int i = start; i < end; ++i )
		result.push_back( source[static_cast<size_t>(i)]->line );
	return result;
}

bool patchcode_file::range_equals_code( int start, int end, patch_lines *code ){
	if( code == 0 )
		return false;
	if( start < 0 )
		start = 0;
	if( end < 0 )
		end = 0;
	if( start > static_cast<int>(source.size()) )
		start = static_cast<int>(source.size());
	if( end > static_cast<int>(source.size()) )
		end = static_cast<int>(source.size());
	if( end < start )
		return false;
	if( static_cast<size_t>(end - start) != code->size() )
		return false;
	for( int i = start; i < end; ++i ){
		if( source[static_cast<size_t>(i)]->line != (*code)[static_cast<size_t>(i - start)] )
			return false;
	}
	return true;
}

bool patchcode_file::tail_equals_code( patch_lines *code ){
	if( code == 0 )
		return false;
	if( code->size() > source.size() )
		return false;
	const size_t offset = source.size() - code->size();
	for( size_t i = 0; i < code->size(); ++i ){
		if( source[offset + i]->line != (*code)[i] )
			return false;
	}
	return true;
}

void patchcode_file::prepare_undo_segment( patchcode_segments *original_segment,
						 patchcode_segments *undo_segment,
						 patch_lines &undo_start,
						 patch_lines &undo_change,
						 patch_lines &undo_end ){
	if( original_segment == 0 || undo_segment == 0 )
		return;

	undo_segment->filename = original_segment->filename;
	undo_segment->author = string("patchcode undo generator");
	undo_segment->patchname = string("Undo: ") + original_segment->patchname;
	undo_segment->timestamp = patchcode_file::get_timestamp();
	undo_segment->bugs = original_segment->bugs.empty() ? string("undo") : original_segment->bugs;
	undo_segment->extra = string("Automatically generated undo patch for: ") + original_segment->patchname;
	undo_segment->enable = true;
	undo_segment->start = undo_start;
	undo_segment->change = undo_change;
	undo_segment->end = undo_end;
}

int patchcode_file::apply_patch( patchcode_segments *segment, patchcode_segments *undo_segment ){
	if( segment == 0 )
		return 100;
	patch_lines *start  = &segment->start;
	patch_lines *end    = &segment->end;
	patch_lines *change = &segment->change;

	patch_lines undo_start = *start;
	patch_lines undo_end = *end;
	patch_lines undo_change;

	if( start->size() == 0 && end->size () == 0 ){
		// Append-only patch. Treat a matching file tail as already applied.
		if( tail_equals_code( change ) )
			return 10;
		if( undo_segment ){
			if( source.empty() ){
				undo_start.clear();
				undo_end.clear();
				undo_end.push_back("----++++endoffile++++----\n");
				undo_change.clear();
			} else {
				// Use the complete original file as the start anchor so undo removes only the appended payload.
				undo_start = copy_code_range( 0, static_cast<int>(source.size()) );
				undo_end.clear();
				undo_change.clear();
			}
			prepare_undo_segment( segment, undo_segment, undo_start, undo_change, undo_end );
		}
		append_code( change );
	} else if( start->size() == 0 && end->size() == 1   ){
		if ( (*end)[0].find("----++++endoffile++++----") != string::npos ){
			if( range_equals_code( 0, static_cast<int>(source.size()), change ) )
				return 10;
			undo_change = copy_code_range( 0, static_cast<int>(source.size()) );
			if( undo_segment )
				prepare_undo_segment( segment, undo_segment, undo_start, undo_change, undo_end );
			clear_code();
			append_code( change );
		} else {
			cerr<<"Warning: ignoring patch ( use ----++++endoffile++++---- to truncate) : "<<segment->filename<<" "<<segment->author<<" "<<segment->patchname<<" "<<segment->bugs<<endl;
			return 5;
		}
	} else if( start->size() == 0 && end->size() > 0 ){
		// Prepend/replace the content before the end marker.
		int startline = 0;
		int endline = find_pattern( end );
		if( endline < 0 )
			return 1;
		endline -= static_cast<int>(end->size());
		if( range_equals_code( startline, endline, change ) )
			return 10;
		undo_change = copy_code_range( startline, endline );
		if( undo_segment )
			prepare_undo_segment( segment, undo_segment, undo_start, undo_change, undo_end );
		delete_code ( startline, endline );
		prepend_code( change );
	} else if( start->size() > 0 && end->size() == 0 ){
		// Replace everything after the start marker.
		int startline = find_pattern( start );
		if( startline < 0 )
			return 2;
		int endline   = static_cast<int>(source.size());
		if( range_equals_code( startline, endline, change ) )
			return 10;
		undo_change = copy_code_range( startline, endline );
		if( undo_segment )
			prepare_undo_segment( segment, undo_segment, undo_start, undo_change, undo_end );
		delete_code ( startline, endline );
		append_code( change );
	} else {
		// Replace the code between the start marker and end marker.
		int startline = find_pattern( start );
		if( startline < 0 )
			return 3;
		int endline   = find_pattern( end );
		if( endline < 0 )
			return 4;
		endline -= static_cast<int>(end->size());
		if( endline < startline )
			return 6;
		if( range_equals_code( startline, endline, change ) )
			return 10;
		undo_change = copy_code_range( startline, endline );
		if( undo_segment )
			prepare_undo_segment( segment, undo_segment, undo_start, undo_change, undo_end );
		delete_code ( startline, endline );
		insert_code ( startline, change );
	}
	return 0;
}

string patchcode_file::get_timestamp(){
	system_clock::time_point now = system_clock::now();
	time_t nowtime;
	nowtime = system_clock::to_time_t ( now );
	string stringtime = ctime(&nowtime);

	replace(stringtime.begin(), stringtime.end(), ' ', '_');
	replace(stringtime.begin(), stringtime.end(), ':', '_');
	replace(stringtime.begin(), stringtime.end(), '\t', '_');
	replace(stringtime.begin(), stringtime.end(), '\n', '_');
	replace(stringtime.begin(), stringtime.end(), '\'', '_');
	return stringtime;
}


void patchcode_file::read_file(){
	if( file_read ){
		return;
	}

	fstream sourcefile;
	sourcefile.open(filename.c_str(), ios::in | ios::binary ) ;
	clear_code();

	if( !sourcefile.is_open() ){
		cerr<<"Warning: Source file not found, creating a new file when patch is written: "<<filename<<endl;
		file_read = true;
		return;
	}

	string s;
	while( read_physical_line(sourcefile, s) ){
		codeline *cl = make_codeline( s, true );
		source.push_back( cl );
	}

	sourcefile.close();
	file_read = true;
	return;
}

void patchcode_file::create_backup(){
	if( backup_done ){
		return;
	}
	fstream s,d;
	string stringtime = patchcode_file::get_timestamp();
	s.open (filename.c_str(), ios::in | ios::binary);
	if( !s.is_open() ){
		cerr<<"Warning: Backup skipped because source file is not open: "<<filename<<endl;
		backup_done = true;
		return;
	}

	string ofilename = filename + "_bkp_" + stringtime;
	d.open (ofilename.c_str(), ios::out | ios::trunc | ios::binary );
	if( !d.is_open() ){
		cerr<<"Error: Destination not open: "<<ofilename<<endl;
		s.close();
		return;
	}

	const int buf_size = 65536;
	char *c = new char[buf_size];
	while( s ){
		s.read( c , buf_size );
		streamsize bytes = s.gcount();
		if( bytes > 0 ){
			d.write( c, bytes );
			if( d.bad() || d.fail() ){
				cerr<<"Error: Write failed during backup"<<endl;
				delete []c;
				d.close();
				s.close();
				return;
			}
		}
	}

	delete []c;
	d.flush();
	d.close();
	s.close();
	backup_done = true;
	return;
}

void patchcode_file::rewrite_file(){
	if( !pc_ensure_parent_directory(filename) ){
		cerr<<"Error: Unable to create parent directory for file to patch: "<<filename<<endl;
		return;
	}

	fstream sourcefile;
	sourcefile.open(filename.c_str(), ios::out | ios::trunc | ios::binary );

	if( sourcefile.is_open() ){
		file_sourcecode::iterator i = source.begin();
		while( i != source.end() ){
			sourcefile<<(*i)->line;
			i++;
		}
		sourcefile.close();
	}
	else {
		cerr<<"Error: Unable to open file to patch: "<<filename<<endl;
		return;
	}
}
