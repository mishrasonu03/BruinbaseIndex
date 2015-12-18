/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <iostream>
#include <string.h>
#include <math.h> 
#include "BTreeIndex.h"
#include "BTreeNode.h"


using namespace std;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
    treeHeight = 0;
    cont = -1;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{   
    if (pf.open(indexname, mode) == 0){
        if (pf.endPid()>0){
            pf.read(0, buffer);            
            memcpy(&rootPid, buffer, sizeof(PageId));
            memcpy(&treeHeight, buffer+sizeof(PageId), sizeof(int));
        }
        else {
            treeHeight = 0;
            rootPid = -1;
            close();
            return open(indexname, mode);
        }
        return 0;
    }   
    else return -1;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{   
    memcpy(buffer, &rootPid, sizeof(PageId));
    memcpy(buffer+sizeof(PageId), &treeHeight, sizeof(int));
    pf.write(0, buffer);

    return pf.close();
}

RC BTreeIndex::printTree(PageId root)
{   
    BTNonLeafNode temp;
    temp.read(rootPid, pf);
    cout <<"Root Looks like:"<<endl;
    cout <<temp.getKeyCount();
    for (int j=0; j<temp.getKeyCount(); ++j){
        int k;
        PageId p2;
        temp.readEntry(j, k, p2);
        cout<<" || "<<k<<" | "<<p2;
    }
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{   //cout<<"At BTreeIndex Insert"<<endl;
    if(treeHeight == 0){
        //cout<<"Tree height is 0"<<endl;
        if (rootPid >=0)
            cout << "Error: BTreeIndex::insert: Root ID set an non-existent node\n";

        //cout<<"Root Pid is: " << rootPid<<endl;
        treeHeight ++;
        
        BTLeafNode leaf = BTLeafNode();
        leaf.insert(key, rid);
        //cout<<"writing into pid: "<<rootPid<<endl;
        rootPid = pf.endPid();
        leaf.write(rootPid, pf);
        return 0;
    }
    else {
        //cout<<"Tree height is: "<<treeHeight<<endl;
        int sibMidKey = INT_MAX;
        PageId newPid;
        //cout<<"Going into Insert Helper Function"<<endl;
        insertHelper(rootPid, key, rid, treeHeight, newPid, sibMidKey);   
        if (sibMidKey < INT_MAX){
            //cout<<"Should not get executed for xsmall"<<endl;
            treeHeight ++;
            BTNonLeafNode newRoot = BTNonLeafNode();
            newRoot.initializeRoot(rootPid, sibMidKey, newPid);
            rootPid = pf.endPid();
            newRoot.write(rootPid, pf);            
            return 0;
        }
        else return 0;
    }
}

/*
 */
RC BTreeIndex::insertHelper(PageId root, int key, const RecordId& rid, int height, PageId& newPid, int& sibMidKey)
{
    if (height==1){
        BTLeafNode leaf = BTLeafNode();
        leaf.read(root, pf);
        if (leaf.getKeyCount() < MAXKEYS){
            leaf.insert(key, rid);
            leaf.write(root, pf);
                return 0;
        }
        else{
            newPid = pf.endPid();
            BTLeafNode sibling = BTLeafNode();
            sibling.setNextNodePtr(leaf.getNextNodePtr());
            leaf.setNextNodePtr(newPid);
            if (leaf.insertAndSplit(key, rid, sibling, sibMidKey)==0){
                sibling.write(newPid, pf);
                leaf.write(root, pf);
                return 0;
            }
            cout<<"Error: BTreeIndex::insertHelper: Cannot insertAndSplit into leaf node\n";
            return -1;
        }
    }
    else{
        BTNonLeafNode nonLeaf = BTNonLeafNode();
        nonLeaf.read(root, pf);
        PageId childPid;
        nonLeaf.locateChildPtr(key, childPid);
        insertHelper(childPid, key, rid, height-1, newPid, sibMidKey);
        
        if (sibMidKey < INT_MAX){
            if(nonLeaf.getKeyCount() < MAXKEYS){
                nonLeaf.insert(sibMidKey, newPid);
                sibMidKey = INT_MAX;
                newPid = -1;
                nonLeaf.write(root, pf);
                return 0;
            }
            else{
                BTNonLeafNode sibling = BTNonLeafNode();
                int tempSibMidKey;
                nonLeaf.insertAndSplit(sibMidKey, newPid, sibling, tempSibMidKey);
                newPid = pf.endPid();
                sibling.write(newPid, pf);
                nonLeaf.write(root, pf);
                sibMidKey = tempSibMidKey;
                return 0;
            }
        }
        else return 0;
    }        
}


/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor)
{
    int eid, height = treeHeight;
    RC rc;

    if (height==0){
        cout <<"Error: BTreeIndex::locate: Locating in empty node\n";
        return -1;
    }
    else {
        BTNonLeafNode nonLeaf = BTNonLeafNode();
        PageId nodePid=rootPid;
        while (height>1){
            nonLeaf.read(nodePid, pf);
            nonLeaf.locateChildPtr(searchKey, nodePid);
            height--;
        }
        int eid;
        BTLeafNode leaf = BTLeafNode();
        leaf.read(nodePid, pf);
        rc = leaf.locate(searchKey, eid);
        cursor.pid = nodePid;
        cursor.eid = eid;
        return rc;
    }
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid)
{   
    if (cont!=cursor.pid){
        readFwdNode.read(cursor.pid, pf);
        cont = cursor.pid;
    }
    
    if(cursor.eid == readFwdNode.getKeyCount()){
        cursor.pid = readFwdNode.getNextNodePtr();
        if (cursor.pid < 0)
            return RC_END_OF_TREE;
        cursor.eid = 0;
        readFwdNode.read(cursor.pid, pf);
        cont = cursor.pid;
    }

    readFwdNode.readEntry(cursor.eid, key, rid);
    cursor.eid++; 
    return 0;
}
