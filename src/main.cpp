#include <iostream>
#include <fstream>
#include <exception>
#include <patchcode_tool.h>
#include <patchcode_file.h>

using namespace std;

static const char *PATCHCODE_SEPARATOR = "<@+@+@>";

void show_help(){
	cout<<"Patchcode command help "<<endl;
	cout<<endl;
	cout<<"patchcode --help : show help"<<endl;
	cout<<"patchcode -h : show help"<<endl;
	cout<<"patchcode <filename> : run the patch program "<<endl;
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
	cout<<"              Backup files are created for each file for each run of patchcode.                    "<<endl;
	cout<<"              Applied empty file(*_applied) is created for each run for a specific patch file to   "<<endl;
	cout<<"              detect multiple patchcode execution for the same patch file.                         "<<endl;
	cout<<endl;
	cout<<"NOTE: Patch only once, otherwise it may lead to unexpected results as delete doesn't say what lines"<<endl;
	cout<<"      are being deleted. Also patchcode doesn't perform a check if the patch has already applied.  "<<endl;
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

int main( int argc , char *argv[ ] ){

	if( argc < 2 ){
		cerr<<"Error: Requires patch filename to be supplied "<<endl;
		show_help();
		return 1;
	}

	string s = argv[1];

	if( s == "--help" || s == "-h" ){
		show_help();
		return 0;
	}

	fstream f;
	f.open(argv[1], ios::in | ios::binary );
	
	if( !f.is_open() ){
		cerr<<"Error: Unable to open patch file : "<<argv[1]<<endl;
		return 2;
	}

	patchcode_tool t ( &f );

	try{
		t.read_patches();
	} catch ( const exception &e ){
		cerr<<"Error: exception while reading the patch file: "<<e.what()<<endl;
		return 3;
	} catch ( ... ){
		cerr<<"Error: unknown exception while reading the patch file "<<endl;
		return 3;
	}

	try{
		t.apply_patches();
	} catch ( const exception &e ){
		cerr<<"Error: exception while applying patches: "<<e.what()<<endl;
		return 4;
	} catch ( ... ){
		cerr<<"Error: unknown exception while applying patches"<<endl;
		return 4;
	}

	fstream touchfile;
	string touch = argv[1] ;
	touch += "_" + patchcode_file::get_timestamp() + "_applied";
	touchfile.open(touch.c_str(), ios::out | ios::trunc );
	touchfile.close();

	cerr<<"Info: bkp files have been created in the same folder before patch was done "<<endl;

	return 0;
}
	
