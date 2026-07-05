#include <patchcode_file.h>

using std::chrono::system_clock;
using namespace std;

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
		
int patchcode_file::apply_patch( patchcode_segments *segment ){
	if( segment == 0 )
		return 100;
	patch_lines *start  = &segment->start;
	patch_lines *end    = &segment->end;
	patch_lines *change = &segment->change;

	if( start->size() == 0 && end->size () == 0 ){
		// append the code without replacing code
		append_code( change );
	} else if( start->size() == 0 && end->size() == 1   ){
		if ( (*end)[0].find("----++++endoffile++++----") != string::npos ){
			clear_code();
			append_code( change );
		} else {
			cerr<<"Warning: ignoring patch ( use ----++++endoffile++++---- to truncate) : "<<segment->filename<<" "<<segment->author<<" "<<segment->patchname<<" "<<segment->bugs<<endl;
			return 5;
		}
	} else if( start->size() == 0 && end->size() > 0 ){
		// prepend the code after removing any content before the end marker
		int startline = 0;
		int endline = find_pattern( end );
		if( endline < 0 )
			return 1;
		endline -= static_cast<int>(end->size());
		delete_code ( startline, endline );
		prepend_code( change );
	} else if( start->size() > 0 && end->size() == 0 ){
		// append the code after replacing everything after the start marker
		int startline = find_pattern( start );
		if( startline < 0 )
			return 2;
		int endline   = static_cast<int>(source.size());
		delete_code ( startline, endline );
		append_code( change );
	} else {
		// replace the code between the start marker and end marker
		int startline = find_pattern( start );
		if( startline < 0 )
			return 3;
		int endline   = find_pattern( end );
		if( endline < 0 )
			return 4;
		endline -= static_cast<int>(end->size());
		if( endline < startline )
			return 6;
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
