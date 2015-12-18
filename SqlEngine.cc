/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "Bruinbase.h"
#include "SqlEngine.h"
#include "BTreeIndex.h"

using namespace std;

// external functions and variables for load file and sql command parsing 
extern FILE* sqlin;
int sqlparse(void);


RC SqlEngine::run(FILE* commandline)
{
    fprintf(stdout, "Bruinbase> ");

    // set the command line input and start parsing user input
    sqlin = commandline;
    sqlparse();  // sqlparse() is defined in SqlParser.tab.c generated from
               // SqlParser.y by bison (bison is GNU equivalent of yacc)

    return 0;
}

RC SqlEngine::select(int attr, const string& table, const vector<SelCond>& cond)
{
    RecordFile rf;   // RecordFile containing the table
    RecordId   rid;  // record cursor for table scanning
    
    IndexCursor cursor; // cursor for traversing
    BTreeIndex indexTree; // index is stored here
   
    RC      rc;
    int     key;     
    string  value;
    int     count;
    int     start; // first element
    int     diff;
    int     condHasVal;

    // open the table file
    if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
        fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
        return rc;
    }

    // scan the table file from the beginning
    rid.pid = rid.sid = 0;
    count = 0;
    start = 0; //Can be negative also
    condHasVal = 0;
//    int jj;
//    cout<<"attr: "<<attr<<endl;
    //cin >>jj;
    bool indexUse = false;
    for(unsigned i = 0; i<cond.size(); i++){
        if (cond[i].attr == 1 && cond[i].comp!=SelCond::NE){
            indexUse = true;
            break;
        }
    }

    if (not(indexUse)){
        for(unsigned i = 0; i<cond.size(); i++){
            if( (attr == 2)||(attr == 3)||(cond[i].attr == 2)||(cond[i].attr == 3) ){
                indexUse = false;
                break;
            }
            else indexUse = true;
        }
        if (cond.size()==0 && !((attr == 2)||(attr == 3)))
            indexUse = true;
    }

 //   cout << "Index Use: " <<indexUse<<endl;
    //cin >>jj;
        
//    if (not(indexUse)) goto bypass;

    
    if((indexTree.open(table+".idx",'r')==0)) {
        for(unsigned i = 0; i<cond.size(); i++){
            if(cond[i].attr == 1){
                int prio = 0;
                switch(cond[i].comp){
                case SelCond::GT:
                    if (start < atoi(cond[i].value)+1)
                        start = atoi(cond[i].value)+1;
                    break;
                case SelCond::GE:
                    if (start < atoi(cond[i].value))
                        start = atoi(cond[i].value);
                    break;
                case SelCond::EQ:
                    start = atoi(cond[i].value);
                    prio = 1;
                    break;
                }
                if (prio==1) break;
            }
            else if(cond[i].attr ==2)
                condHasVal = 1;
        }
        indexTree.locate(start, cursor);
  //      cout<<"Start: "<<start;
 //       int iii=0;
        int ntup = 1;
      //  cin >> iii;
        while ((indexTree.readForward(cursor, key, rid)) != RC_END_OF_TREE) {  
            if (attr == 2 || attr == 3 || condHasVal == 1){
                //rid.pid -=1; 
                // cout <<"attr="<<attr<<endl;
              //  cout <<"rid.Pid="<<rid.pid<<endl;
              //  cout <<"rid.Sid="<<rid.sid<<endl;
              //  cout <<"Cursor.pid:" << cursor.pid << endl;
              //  cout <<"Cursor.eid:" << cursor.eid << endl;
                int p;
                // cout <<"Should not have enterred here 1"<<endl;
                // cout<<"attr: "<<attr<<endl;
                // cout<<"cond has value?: "<<condHasVal<<endl;
                // cin >>jj;

                if((p=rf.read(rid, key, value)) < 0){
                //    cout <<p<<endl;
                  //  cout <<"Cursor.pid:" << cursor.pid << endl;
                    //cout <<"Cursor.eid:" << cursor.eid << endl;
                    fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
                    goto exit_select;
                }
            }
            //cout<<"key: "<<key<<endl;
            //cout<<"value: "<<value<<endl;

            for (unsigned i = 0; i < cond.size(); i++) {
                // compute the difference between the tuple value and the condition value
    //            cout <<"Should not have enterred here 2"<<endl;
                switch (cond[i].attr) {
                case 1:
                    diff = key - atoi(cond[i].value);
                    break;
                case 2:
                    diff = strcmp(value.c_str(), cond[i].value);
                    break;
                }

                if(cond[i].attr == 1) {
                    switch (cond[i].comp) {
                    case SelCond::EQ: //what if two ==s? then one may satisfy this, other won't.
                        if (diff != 0) goto exit_select;
                        break;
                    case SelCond::NE:
                        if (diff == 0) goto ntuple; //what if there is only one <> then? i am not using index then
                        break;
                    case SelCond::GT:
                        if (diff <= 0) goto exit_select; //cout << "Exception E1"<<endl; //should never occur
//                        cin>>iii;
                        break;
                    case SelCond::LT:
                        if (diff >= 0) goto exit_select;
                        break;
                    case SelCond::GE:
                        if (diff < 0) goto exit_select; //cout <<"Exception E2"<<endl; //should never occur
  //                      cin>>iii;
                        break;
                    case SelCond::LE:
                        if (diff > 0) goto exit_select;
                        break;
                    }
                }
                else {
                    switch (cond[i].comp) {
                    case SelCond::EQ:
                        if (diff != 0) goto ntuple;
                        break;
                    case SelCond::NE:
                        if (diff == 0) goto ntuple; 
                        break;
                    case SelCond::GT:
                        if (diff <= 0) goto ntuple; 
                        break;
                    case SelCond::LT:
                        if (diff >= 0) goto ntuple;
                        break;
                    case SelCond::GE:
                        if (diff < 0) goto ntuple;
                        break;
                    case SelCond::LE:
                        if (diff > 0) goto ntuple;
                        break;
                    }
                }
            }
            // the condition is met for the tuple. 
            // increase matching tuple counter
            count++;

            // print the tuple 
            switch (attr) {
            case 1:  // SELECT key
                fprintf(stdout, "%d\n", key);
                break;
            case 2:  // SELECT value
                fprintf(stdout, "%s\n", value.c_str());
                break;
            case 3:  // SELECT *
                fprintf(stdout, "%d '%s'\n", key, value.c_str());
                break;
            }
            ntuple:
                ntup = 1;
            
        }
        // print matching tuple count if "select count(*)"
        if (attr == 4) {
            fprintf(stdout, "%d\n", count);
        }
        rc = 0;
    }

    else {
        bypass:
        while (rid < rf.endRid()) {
        // read the tuple
            
            if ((rc = rf.read(rid, key, value)) < 0) {
                // cout<<"rid.pid: "<<rid.pid<<endl;
                // cout<<"rid.sid: "<<rid.sid<<endl;
                fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
                goto exit_select;
            }

            // check the conditions on the tuple
            for (unsigned i = 0; i < cond.size(); i++) {
            // compute the difference between the tuple value and the condition value
                switch (cond[i].attr) {
                case 1:
	                diff = key - atoi(cond[i].value);
	                break;
                case 2:
	                diff = strcmp(value.c_str(), cond[i].value);
	                break;
                }

            // skip the tuple if any condition is not met
                switch (cond[i].comp) {
                case SelCond::EQ:
	                if (diff != 0) goto next_tuple;
	                break;
                case SelCond::NE:
	                if (diff == 0) goto next_tuple;
	                break;
                case SelCond::GT:
	                if (diff <= 0) goto next_tuple;
	                break;
                case SelCond::LT:
	                if (diff >= 0) goto next_tuple;
	                break;
                case SelCond::GE:
	                if (diff < 0) goto next_tuple;
	                break;
                case SelCond::LE:
	                if (diff > 0) goto next_tuple;
	                break;
                }
            }   

            // the condition is met for the tuple. 
            // increase matching tuple counter
            count++;

            // print the tuple 
            switch (attr) {
            case 1:  // SELECT key
                fprintf(stdout, "%d\n", key);
                break;
            case 2:  // SELECT value
                fprintf(stdout, "%s\n", value.c_str());
                break;
            case 3:  // SELECT *
                fprintf(stdout, "%d '%s'\n", key, value.c_str());
                break;
            }   

            // move to the next tuple
            next_tuple:
            ++rid;
        }

        // print matching tuple count if "select count(*)"
        if (attr == 4) {
            fprintf(stdout, "%d\n", count);
        }
        rc = 0;

        // close the table file and return
        exit_select:
        rf.close();
        return rc;
    }
}

RC SqlEngine::load(const string& table, const string& loadfile, bool index)
{
  RecordFile rf;   // RecordFile containing the table
  RecordId   rid;  // record cursor for table scanning
  FILE * pFile;    // File containing the loadfile
  //cout <<"Not even there"<<endl;
  BTreeIndex indexTree;

  RC     rc;
  int    key;     
  string value;
  int    count;
  char mystring [100];

  // open or create the table file
  //cout <<"Opening Record File"<<endl;
  if ((rc = rf.open(table + ".tbl", 'w')) < 0) {
    fprintf(stderr, "Error: table %s could not be created\n", table.c_str());
    return rc;
  }

  // creating B+Tree Index
  if(index){
//    cout <<"Opening index table"<<endl;
    indexTree.open(table + ".idx", 'w');
  }

  // append to the table file
  //rid = rf.endRid();
  count = 0;

  // until the loadfile ends, read one tuple from 

   pFile = fopen (loadfile.c_str(), "r");
    if (pFile == NULL) perror ("Error opening file");
    else {
       // int ai=0, ii=0;
        while(!feof(pFile)){
	       if ( fgets (mystring , 100 , pFile) != NULL ){
         //     ii++;
		      parseLoadLine(mystring, key, value);
		      rf.append(key, value, rid);
              //cout<<"Load: rid.pid: "<<rid.pid<<endl;
              //cout<<"Load: rid.sid: "<<rid.sid<<endl;
              if(index){
  //              cout<<"Going to insert key "<< key <<"and rid "<< rid.pid<<" "<<rid.sid<<" into the tree"<<endl;
                indexTree.insert(key, rid);
              }
              // if ((ii%10)==2){
              //   cout<<"ii: " << ii << endl;
              //   if (ii>75)
              //       indexTree.printTree(3);
              //   cin>>ai;
              // }

            }
	   }     
	fclose (pFile);
    if(index){
    //    cout<<"Closing the index table"<<endl;
        indexTree.close();
    }
   }

  // close the table file and return
  exit_select:
  rf.close();
  return 0;
}

RC SqlEngine::parseLoadLine(const string& line, int& key, string& value)
{
    const char *s;
    char        c;
    string::size_type loc;
    
    // ignore beginning white spaces
    c = *(s = line.c_str());
    while (c == ' ' || c == '\t') { c = *++s; }

    // get the integer key value
    key = atoi(s);

    // look for comma
    s = strchr(s, ',');
    if (s == NULL) { return RC_INVALID_FILE_FORMAT; }

    // ignore white spaces
    do { c = *++s; } while (c == ' ' || c == '\t');
    
    // if there is nothing left, set the value to empty string
    if (c == 0) { 
        value.erase();
        return 0;
    }

    // is the value field delimited by ' or "?
    if (c == '\'' || c == '"') {
        s++;
    } else {
        c = '\n';
    }

    // get the value string
    value.assign(s);
    loc = value.find(c, 0);
    if (loc != string::npos) { value.erase(loc); }

    return 0;
}
