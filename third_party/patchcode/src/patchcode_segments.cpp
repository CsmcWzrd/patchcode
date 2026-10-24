#include <patchcode_segments.h>

static const char *PATCHCODE_SEPARATOR = "<@+@+@>";

patchcode_segments::patchcode_segments() {
	enable = false;
}

void patchcode_segments::print() {
	cerr<<"filename:"<<filename<<endl;
	cerr<<"author:"<<author<<endl;
	cerr<<"patchname:"<<patchname<<endl;
	cerr<<"timestamp:"<<timestamp<<endl;
	cerr<<"bugs:"<<bugs<<endl;
	cerr<<"extra:"<<extra<<endl;
	string s = "yes";
	if( !enable )
		s = "no";
	cerr<<"enable:"<<s<<endl;
	print_patchcode( start );
	print_patchcode( change );
	print_patchcode( end );
	cerr<<PATCHCODE_SEPARATOR<<endl<<endl;
}


void patchcode_segments::print_patchcode(patch_lines &code) {
	patch_lines::iterator i = code.begin();
	cerr<<PATCHCODE_SEPARATOR<<endl;
	while ( i != code.end() ){
		cerr<<*i;
		i++;
	}
}
