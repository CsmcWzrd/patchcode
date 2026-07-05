#include <iostream>
#include <fstream>
#include <exception>
#include <cstdio>
#include <patchcode_tool.h>
#include <patchcode_file.h>

using namespace std;

static const char *PATCHCODE_SEPARATOR = "<@+@+@>";

void show_help(){
	cout<<"Patchcode command help "<<endl;
	cout<<endl;
	cout<<"patchcode --help : show help"<<endl;
	cout<<"patchcode -h : show help"<<endl;
	cout<<"patchcode [--backup] <filename> : run the patch program "<<endl;
	cout<<"patchcode --undo-patch <filename> : undo using <filename>.undo.patch"<<endl;
	cout<<"diffcode <original-file> <updated-file> [output.patch] : generate a patchcode patch file"<<endl;
	cout<<"diffcode <original-dir> <updated-dir> [output.patch] : generate a directory patchcode patch file"<<endl;
	cout<<"----"<<endl;
	cout<<endl;
	cout<<endl;
	cout<<endl;
	cout<<"Description : Patchcode is a simple patching command that takes input from a file. The file contains "<<endl;
	cout<<"              sections as shown below. Only two operations are supported delete and replace.        "<<endl;
	cout<<"              Add is also considered as replace. Patchcode should only be used for text files.       "<<endl;
	cout<<"              Each section consists of patch details and 3 code-location sections. Patch details are "<<endl;
	cout<<"              strings, except 'enable' whose values are either 'yes' or 'no'. The remaining patch    "<<endl;
	cout<<"              details are self-explanatory.                                                         "<<endl;
	cout<<"              The code-location sections are separated by a line containing "<<PATCHCODE_SEPARATOR<<"."<<endl;
	cout<<"              The first section identifies the start code location. The second section is the updated "<<endl;
	cout<<"              code. The third section identifies the end code location. The patch is done by replacing"<<endl;
	cout<<"              the entire content between the last line of the first section and the first line of the "<<endl;
	cout<<"              third section. Each non-empty code-location section should contain 3 or more lines of  "<<endl;
	cout<<"              code, except the special end-of-file truncate marker described below.                  "<<endl;
	cout<<endl;
	cout<<"              First  section is called 'Start section pattern.'(SS)                                "<<endl;
	cout<<"              Second section is called 'Patch/Change section ' (PS)                                "<<endl;
	cout<<"              Third  section is called 'End section pattern'   (ES)                                "<<endl;
	cout<<endl;
	cout<<"              If SS is empty and ES is empty, it does an append operation on the file.             "<<endl;
	cout<<"              If SS is empty and ES is not empty, it does a prepend operation on the file, after   "<<endl;
	cout<<"              removing the content from the start of the file till ES.                             "<<endl;
	cout<<"              If SS is not empty and ES is empty, it does an append operation on the file, after   "<<endl;
	cout<<"              removing the content from the last line of SS till the end of the file.              "<<endl;
	cout<<"              If SS is not empty and ES is not empty, then it removes all the content in between   "<<endl;
	cout<<"              last line of SS and first line of ES and then replacing it with PS.                  "<<endl;
	cout<<"              If SS is empty and ES contains '----++++endoffile++++----', it truncates the file and"<<endl;
	cout<<"              copies the content of the PS to the empty file.                                      "<<endl;
	cout<<"              Delete is as good as having the PS empty.                                            "<<endl;
	cout<<endl;
	cout<<"              Patchcode supports overlay patching. If a section modifies a code block, you can     "<<endl;
	cout<<"              update the same code block with a newer patch section instead of correcting the old   "<<endl;
	cout<<"              patch section.                                                                       "<<endl;
	cout<<"              Use diffcode to generate patchcode patch files from original/updated files or dirs.  "<<endl;
	cout<<endl;
	cout<<"              Backup files are not created by default. Use --backup to create one backup per     "<<endl;
	cout<<"              target file for the current patchcode run.                                            "<<endl;
	cout<<"              A reverse patch is created as <patchfile>.undo.patch when a patch changes files.     "<<endl;
	cout<<"              Use --undo-patch <patchfile> to apply that generated reverse patch.                  "<<endl;
	cout<<"              Patchcode reports whether each patch segment was applied, already applied, disabled, "<<endl;
	cout<<"              or failed. Applied empty files(*_applied) are created only when changes are applied. "<<endl;
	cout<<endl;
	cout<<"NOTE: Patchcode detects common already-applied cases by comparing the target replacement area with"<<endl;
	cout<<"      the patch change section before rewriting the file.                                         "<<endl;
	cout<<"----"<<endl;
	cout<<endl;
	cout<<endl;
	cout<<endl;
	cout<<"Patchcode segment sample template: "<<endl;
	cout<<endl;
	cout<<"#Comment line, can only be used within patch details area and not within patch sections "<<endl;
	cout<<"filename:src/filename.cpp"<<endl;
	cout<<"author:Author Bugfixer"<<endl;
	cout<<"patchname:Patchcode patchname for Bug 0"<<endl;
	cout<<"timestamp:2022/12/17-14:25:40 GMT"<<endl;
	cout<<"enable:yes"<<endl;
	cout<<"bugs:BugNumber0"<<endl;
	cout<<"extra:Team Extra"<<endl;
	cout<<PATCHCODE_SEPARATOR<<endl;
	cout<<"\t\t\t//Pattern section : Start"<<endl;
	cout<<"\t\t\twhile( line_num < n ) {"<<endl;
	cout<<"\t\t\t\tif( (*i)->line != (*code)[line_num] )"<<endl;
	cout<<"\t\t\t\t\tbreak;"<<endl;
	cout<<PATCHCODE_SEPARATOR<<endl;
	cout<<"\t\t\t//Change (done for the patch)"<<endl;
	cout<<"\t\t\t\t//Patch change 1"<<endl;
	cout<<"\t\t\t\t//Patch change 2"<<endl;
	cout<<"\t\t\t\t//Patch change 3"<<endl;
	cout<<PATCHCODE_SEPARATOR<<endl;
	cout<<"\t\t\t//Pattern section : end"<<endl;
	cout<<"\t\t\t\ti++;"<<endl;
	cout<<"\t\t\t\tline_num++;"<<endl;
	cout<<"\t\t\t\tcur_line_num++;"<<endl;
	cout<<PATCHCODE_SEPARATOR<<endl;
	cout<<endl;
	cout<<endl;
	cout<<endl;
}


static bool file_exists_for_patchcode(const string &path) {
	fstream f(path.c_str(), ios::in | ios::binary);
	return f.is_open();
}

static string undo_patch_filename_for(const string &patch_filename) {
	return patch_filename + ".undo.patch";
}

static bool string_ends_with_patchcode(const string &s, const string &suffix) {
	if( suffix.size() > s.size() )
		return false;
	return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

int main( int argc , char *argv[ ] ){

	if( argc < 2 ){
		cerr<<"Error: Requires patch filename to be supplied "<<endl;
		show_help();
		return 1;
	}

	bool backup_enabled = false;
	bool undo_mode = false;
	string patch_filename;

	for( int i = 1; i < argc; ++i ){
		string arg = argv[i];
		if( arg == "--help" || arg == "-h" ){
			show_help();
			return 0;
		} else if( arg == "--backup" ){
			backup_enabled = true;
		} else if( arg == "--undo-patch" ){
			undo_mode = true;
		} else if( arg.size() > 0 && arg[0] == '-' ){
			cerr<<"Error: Unknown option: "<<arg<<endl;
			show_help();
			return 1;
		} else if( patch_filename.empty() ){
			patch_filename = arg;
		} else {
			cerr<<"Error: Only one patch filename can be supplied"<<endl;
			show_help();
			return 1;
		}
	}

	if( patch_filename.empty() ){
		cerr<<"Error: Requires patch filename to be supplied "<<endl;
		show_help();
		return 1;
	}

	string file_to_open = patch_filename;
	string undo_sidecar = undo_patch_filename_for(patch_filename);
	bool create_undo_patch = !undo_mode;

	if( undo_mode ){
		file_to_open = undo_sidecar;
		if( !file_exists_for_patchcode(file_to_open) &&
			string_ends_with_patchcode(patch_filename, ".undo.patch") &&
			file_exists_for_patchcode(patch_filename) ){
			// Also allow direct use of an undo patch file.
			file_to_open = patch_filename;
		}
		cerr<<"Info: undo mode enabled; using undo patch file: "<<file_to_open<<endl;
	}

	fstream f;
	f.open(file_to_open.c_str(), ios::in | ios::binary );
	
	if( !f.is_open() ){
		cerr<<"Error: Unable to open patch file : "<<file_to_open<<endl;
		if( undo_mode )
			cerr<<"Error: Undo patch was not found. Apply the original patch first so patchcode can create "<<undo_sidecar<<endl;
		return 2;
	}

	patchcode_tool t ( &f, backup_enabled );

	try{
		t.read_patches();
	} catch ( const exception &e ){
		cerr<<"Error: exception while reading the patch file: "<<e.what()<<endl;
		return 3;
	} catch ( ... ){
		cerr<<"Error: unknown exception while reading the patch file "<<endl;
		return 3;
	}

	patchcode_apply_summary summary;
	try{
		summary = t.apply_patches( undo_sidecar, create_undo_patch );
	} catch ( const exception &e ){
		cerr<<"Error: exception while applying patches: "<<e.what()<<endl;
		return 4;
	} catch ( ... ){
		cerr<<"Error: unknown exception while applying patches"<<endl;
		return 4;
	}

	if( summary.applied > 0 ){
		fstream touchfile;
		string touch = patch_filename ;
		touch += "_" + patchcode_file::get_timestamp();
		touch += undo_mode ? "_undo_applied" : "_applied";
		touchfile.open(touch.c_str(), ios::out | ios::trunc );
		touchfile.close();
	}

	if( backup_enabled )
		cerr<<"Info: backup files were created in the same folder before patching when target files existed"<<endl;

	if( summary.failed > 0 ){
		if( summary.applied > 0 )
			cerr<<"Status: patch partially applied with failures"<<endl;
		else
			cerr<<"Status: patch not applied"<<endl;
		return 5;
	}
	if( summary.applied > 0 ){
		cerr<<(undo_mode ? "Status: undo patch applied" : "Status: patch applied")<<endl;
		return 0;
	}
	if( summary.already_applied > 0 && summary.disabled == 0 ){
		cerr<<(undo_mode ? "Status: undo patch already applied" : "Status: patch already applied")<<endl;
		return 0;
	}
	if( summary.disabled > 0 && summary.applied == 0 ){
		cerr<<"Status: patch not applied; all enabled changes were skipped or disabled"<<endl;
		return 0;
	}

	cerr<<"Status: no patch changes applied"<<endl;
	return 0;
}
	
