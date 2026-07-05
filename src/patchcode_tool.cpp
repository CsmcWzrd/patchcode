#include <patchcode_tool.h>

static const char *PATCHCODE_SEPARATOR = "<@+@+@>";

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

static string strip_eol(const string &line) {
	string s = line;
	while( !s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r') )
		s.erase(s.size() - 1);
	return s;
}

static bool is_separator_line(const string &line) {
	return strip_eol(line) == PATCHCODE_SEPARATOR;
}

patchcode_apply_summary::patchcode_apply_summary() {
	total = 0;
	applied = 0;
	already_applied = 0;
	disabled = 0;
	failed = 0;
}

static void write_segment_payload(ostream &out, patch_lines &code) {
	patch_lines::iterator i = code.begin();
	while( i != code.end() ) {
		out << *i;
		if( !(*i).empty() && (*i)[(*i).size() - 1] != '\n' && (*i)[(*i).size() - 1] != '\r' )
			out << endl;
		i++;
	}
}

static void write_patch_segment(ostream &out, patchcode_segments *segment) {
	if( segment == 0 )
		return;
	out << "filename:" << segment->filename << endl;
	out << "author:" << segment->author << endl;
	out << "patchname:" << segment->patchname << endl;
	out << "timestamp:" << segment->timestamp << endl;
	out << "enable:" << (segment->enable ? "yes" : "no") << endl;
	out << "bugs:" << segment->bugs << endl;
	out << "extra:" << segment->extra << endl;
	out << PATCHCODE_SEPARATOR << endl;
	write_segment_payload(out, segment->start);
	out << PATCHCODE_SEPARATOR << endl;
	write_segment_payload(out, segment->change);
	out << PATCHCODE_SEPARATOR << endl;
	write_segment_payload(out, segment->end);
	out << PATCHCODE_SEPARATOR << endl << endl;
}

patchcode_tool::patchcode_tool( fstream *f, bool enable_backup ){
	patch_file = f;
	cur_patch_segment = 0;
	line_num = 1;
	backup_enabled = enable_backup;
}

patchcode_tool::~patchcode_tool() {
	if( patch_file )
		patch_file->close();

	if( cur_patch_segment ) {
		delete cur_patch_segment;
		cur_patch_segment = 0;
	}

	vector_patchcode_segments::iterator i = patch_segments.begin();
	while( i != patch_segments.end() ){
		delete *i;
		i++;
	}
	patch_segments.clear();

	map_patchcode_files::iterator j = patch_files.begin();
	while( j != patch_files.end() ){
		delete j->second;
		j++;
	}
	patch_files.clear();
}

void patchcode_tool::process_info ( string &s ){
	string line = strip_eol(s);

	if( line.length() == 0 || line[0] == '#' ){
		return;
	}

	stringstream ss (line);
	string name;
	string value;
	getline ( ss, name, ':' );
	if( name.length() == 0 ){
		cerr<<"Warning: String "<<line<<" has no name component "<<name<<endl;
		return;
	}
	getline ( ss, value );
	if( value.length() == 0 ){
		cerr<<"Warning: String "<<line<<" has no value component "<<value<<endl;
		return;
	}

	if ( cur_patch_segment == 0 ){
		cur_patch_segment = new patchcode_segments();
	}

	if( name == "filename" ){
		cur_patch_segment->filename = value;
	} else if ( name == "author" ){
		cur_patch_segment->author = value;
	} else if ( name == "patchname" ){
		cur_patch_segment->patchname = value;
	} else if ( name == "timestamp" ){
		cur_patch_segment->timestamp = value;
	} else if ( name == "enable" ){
		if( value == "yes" || value == "true" )
			cur_patch_segment->enable = true;
		else  {
			cur_patch_segment->enable = false;
			cerr<<"Warning: Patch segment will not be applied "<< cur_patch_segment->filename<< " "
				<<cur_patch_segment->patchname<<" " <<cur_patch_segment->author<<" "<<cur_patch_segment->bugs<<endl;
		}
	} else if ( name == "bugs" ){
		cur_patch_segment->bugs  = value;
	} else if ( name == "extra"){
		cur_patch_segment->extra = value;
	} else {
		// Unknown metadata fields are ignored for forward compatibility.
	}
}

void patchcode_tool::process_start (){
	if( cur_patch_segment ){
		cur_patch_segment->start = cur_patch_code;
		cur_patch_code.clear();
	}else {
		cerr<<"Error: Current patch segment object not present before start section at line "<<line_num<<endl;
	}
}

void patchcode_tool::process_patch (){
	if( cur_patch_segment ){
		cur_patch_segment->change = cur_patch_code;
		cur_patch_code.clear();
	}else {
		cerr<<"Error: Current patch segment object not present before change section at line "<<line_num<<endl;
	}
}

void patchcode_tool::process_end (){
	if( cur_patch_segment ){
		cur_patch_segment->end = cur_patch_code;
		if( cur_patch_segment->filename.length() == 0 ||
			cur_patch_segment->patchname.length() == 0 ||
			cur_patch_segment->author.length() == 0 ||
			cur_patch_segment->timestamp.length() == 0 ||
			cur_patch_segment->bugs.length() == 0 ) {

			cerr<<"Error: Patch meta data not valid, cleaning up patch segment at line "<<line_num<<endl;
			delete cur_patch_segment;
			cur_patch_segment = 0;
			cur_patch_code.clear();
			return;
		}
		patch_segments.push_back ( cur_patch_segment );
		cur_patch_code.clear();
		cur_patch_segment = 0;
	}else {
		cerr<<"Error: Current patch segment object not present before end section at line "<<line_num<<endl;
	}
}

void patchcode_tool::read_patches() {
	if( patch_file == 0 || !patch_file->is_open() )
		return;

	SEGMENTS segment = SEG_INFO;
	string line;

	while( read_physical_line(*patch_file, line) ){
		if( is_separator_line(line) ){
			if( segment == SEG_INFO ){
				segment = SEG_START;
				cur_patch_code.clear();
			} else if( segment == SEG_START ){
				process_start();
				segment = SEG_PATCH;
			} else if( segment == SEG_PATCH ){
				process_patch();
				segment = SEG_END;
			} else if( segment == SEG_END ){
				process_end();
				segment = SEG_INFO;
			}
		} else if( segment == SEG_INFO ){
			process_info( line );
		} else {
			cur_patch_code.push_back( line );
		}
		line_num++;
	}

	if( segment != SEG_INFO ){
		cerr<<"Error: Patch file ended before a patch segment was closed with "<<PATCHCODE_SEPARATOR<<endl;
	}

	return;
}


bool patchcode_tool::write_undo_patch_file( const string &undo_patch_filename, vector_patchcode_segments &undo_segments ) {
	if( undo_patch_filename.empty() || undo_segments.empty() )
		return false;

	fstream out;
	out.open( undo_patch_filename.c_str(), ios::out | ios::trunc | ios::binary );
	if( !out.is_open() ) {
		cerr<<"Error: Unable to create undo patch file: "<<undo_patch_filename<<endl;
		return false;
	}

	out << "# patchcode undo patch generated by patchcode" << endl;
	out << "# Apply with: patchcode --undo-patch <original-patch-file>" << endl;
	out << "# Separator: " << PATCHCODE_SEPARATOR << endl << endl;

	// Undo needs to run in the opposite order of the original patch application.
	for( vector_patchcode_segments::reverse_iterator it = undo_segments.rbegin(); it != undo_segments.rend(); ++it )
		write_patch_segment( out, *it );

	out.close();
	cerr<<"Info: undo patch file created: "<<undo_patch_filename<<endl;
	return true;
}

patchcode_apply_summary patchcode_tool::apply_patches( const string &undo_patch_filename, bool create_undo_patch ) {
	patchcode_apply_summary summary;
	vector_patchcode_segments undo_segments;

	vector_patchcode_segments::iterator i = patch_segments.begin();
	while (  i != patch_segments.end() ){
		summary.total++;
		cerr<<endl;
		cerr<<"Info: Applying patch : "<<(*i)->patchname<<endl;
		map_patchcode_files::iterator mapiter = patch_files.find( (*i)->filename );
		patchcode_file *file = 0;
		if( mapiter != patch_files.end() ){
			file = mapiter->second;
		} else {
			file = new patchcode_file( (*i)->filename) ;
			patch_files.insert( pair<string, patchcode_file *>((*i)->filename , file));
		}

		if( (*i)->enable ) {
			if( backup_enabled )
				file->create_backup();
			file->read_file();

			patchcode_segments *undo_segment = 0;
			if( create_undo_patch )
				undo_segment = new patchcode_segments();

			int patch_result = file->apply_patch( (*i), undo_segment );
			if ( patch_result == 0 ){
				summary.applied++;
				cerr<<"Status: applied patch "<<(*i)->patchname<<" to "<<(*i)->filename<<endl;
				file->rewrite_file();
				if( undo_segment )
					undo_segments.push_back( undo_segment );
			} else if( patch_result == 10 ) {
				summary.already_applied++;
				cerr<<"Status: patch already applied; no file change needed "<<(*i)->patchname<<" to "<<(*i)->filename<<endl;
				if( undo_segment )
					delete undo_segment;
			} else {
				summary.failed++;
				cerr<<"Status: patch not applied (code location not found or invalid; status "<<patch_result<<") "<<(*i)->patchname<<" to "<<(*i)->filename<<endl;
				if( undo_segment )
					delete undo_segment;
			}
		} else {
			summary.disabled++;
			cerr<<"Status: patch not applied (patch disabled, change enable to 'yes' to apply) "<<(*i)->patchname<<" to "<<(*i)->filename<<endl;
		}
		cerr<<"----"<<endl;
		cerr<<endl;
		i++;
	}

	if( create_undo_patch && summary.applied > 0 )
		write_undo_patch_file( undo_patch_filename, undo_segments );

	vector_patchcode_segments::iterator u = undo_segments.begin();
	while( u != undo_segments.end() ){
		delete *u;
		u++;
	}
	undo_segments.clear();

	cerr<<"Summary: "<<summary.total<<" patch segment"<<(summary.total == 1 ? "" : "s")
		<<" processed; "<<summary.applied<<" applied, "
		<<summary.already_applied<<" already applied, "
		<<summary.disabled<<" disabled, "
		<<summary.failed<<" failed."<<endl;

	return summary;
}


void patchcode_tool::print(){
	vector_patchcode_segments::iterator i = patch_segments.begin();
	cerr<<"Info: Patch segments parsed output "<<endl;
	while (  i != patch_segments.end() ){
		(*i)->print();
		i++;
	}
	return;
}
