#include "BTreeNode.h"
#include <iostream>
#include <string.h>
#include <math.h>

using namespace std;

/* 
 * Constructor: Initialize the buffer
 */
BTLeafNode::BTLeafNode()
{ 
	//cout<<"Constructing the Leaf Node"<<endl;
	memset(buffer, '\0', PageFile::PAGE_SIZE);
	int keyCount = 0;
	memcpy(buffer, &keyCount, sizeof(int));	
	setNextNodePtr(-1);
	//cout <<getNextNodePtr()<<endl;
	//cout<<"Leaving from the leaf construction"<<endl;
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf)
{ 
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf)
{ 	//cout<<"Writing the leaf into the Pid: "<<pid<<endl;
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount()
{
	int keyCount;
	memcpy(&keyCount, buffer, sizeof(int)); //first integer is the number of keys
	return keyCount;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid)
{ 	//cout<<"At leaf Insert"<<endl;
	int eid, decr, offset;
	int keyCount = getKeyCount();
	locate(key, eid);
	
	decr = sizeof(KeyRecID);
	offset = sizeof(int) + (eid+1)*decr;

	if (eid<keyCount){
		memmove(buffer+offset, buffer+offset-decr, (keyCount-eid)*decr); // + sizeof(PageId)
	}
	
	KeyRecID keyrid;
	keyrid.key = key;
	keyrid.rid = rid;
	keyCount = keyCount+1;
	memcpy(buffer+offset-decr, &keyrid, decr);
	memcpy(buffer, &keyCount, sizeof(int));

	//cout <<"Leaf Looks like:"<<endl;
	//cout <<keyCount;
	// for (int j=0; j<keyCount; ++j){
	// 	int k;
	// 	RecordId r;
	// 	memcpy(&r, buffer+sizeof(int)+j*decr, sizeof(r));
	// 	memcpy(&k, buffer+(j+1)*decr, sizeof(int));

	// 	//cout<<" || "<<r.pid<<r.sid<<" | "<<k;
	// }
	//cout<<endl;
	return 0; 
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, BTLeafNode& sibling, int& siblingKey)
{	//cout <<"At leaf insert and Split"<<endl;
	if (sibling.getKeyCount()>0){
		//cout<<"Sibling is not empty"<<endl;
		return RC_INVALID_ATTRIBUTE; //RC_INVALID_SIBLING
	}
	//cout<<"Ye to print hoja"<<endl;
	RecordId sibRid;
	PageId pid;
	int pidSize = sizeof(pid);
	
	int keyCount = getKeyCount(), sibKey, sibKeyCount, eid;
	int intSize = sizeof(int), keySize = sizeof(key), incr = sizeof(KeyRecID);
	//cout<<"KeyCount: "<<keyCount<<endl;
	int keys2keep = ceil(keyCount/2.0);
	
	//cout<<"Have to keep "<<keys2keep<<"in the first leaf"<<endl;
	locate(key, eid);

	if (eid<keys2keep){
		for (int i=keys2keep-1; i<keyCount; ++i){
			readEntry(i, sibKey, sibRid);
			//cout<<"inserting key"<<sibKey<<" and rid"<<rid.pid<<rid.sid<<"into sibling"<<endl;
			sibling.insert(sibKey, sibRid);
		}
		memset(buffer+intSize+incr*(keys2keep-1), '\0', incr*(keyCount-keys2keep+1));
		keyCount = keys2keep-1;
		memcpy(buffer, &keyCount, intSize);
		insert(key, rid);
	}
	else {
		for (int i=keys2keep; i<keyCount; ++i){
			readEntry(i, sibKey, sibRid);
			sibling.insert(sibKey, sibRid);
		}
		memset(buffer+intSize+incr*(keys2keep), '\0', incr*(keyCount-keys2keep));
		keyCount = keys2keep;
		memcpy(buffer, &keyCount, intSize);
		sibling.insert(key, rid);
	}
	
	sibling.readEntry(0, siblingKey, sibRid); //sibRid returned is useless
	
	return 0; 
}

/*
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid)
{ 	//cout<<"At Leaf Node locate"<<endl;
	RecordId rid;
	int key;
	int keyCount = getKeyCount();
	//cout<<"KeyCount of the leaf is:" <<keyCount<<endl;
	for (eid=0; eid<keyCount; ++eid){
		readEntry(eid, key, rid);
		if (key == searchKey){
			//cout<<"returning a eid: "<<eid<<endl;
			return 0;
		}
		else if (key > searchKey){
			//cout<<"returning b eid: "<<eid<<endl;
			return RC_NO_SUCH_RECORD;
		}
	}
	//cout<<"returning c eid: "<<eid<<endl;
	return RC_NO_SUCH_RECORD; 
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid)
{	//cout<<"At Leaf ReadEntry"<<endl;
	int keyCount = getKeyCount();
	//cout<<"Leaf Key Count is: "<<keyCount << endl;
	if(eid < 0 || eid >= keyCount)
		return RC_INVALID_CURSOR;

	KeyRecID keyrid;
	int offset = sizeof(int) + eid*sizeof(keyrid);

	memcpy(&keyrid, buffer+offset, sizeof(keyrid));
	key = keyrid.key;
	rid = keyrid.rid;
	//cout<<"Key at eid:"<<eid<<"is:"<<key<<endl;
	//cout<<"Rid at eid:"<<eid<<"is:"<<rid.pid<<rid.sid<<endl;

	return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr()
{ 	//cout<<"At leaf getNextNodePtr"<<endl;
	PageId pid;
	int pidSize = sizeof(pid);
	int offset = PageFile::PAGE_SIZE - pidSize;
	memcpy (&pid, buffer+offset, pidSize);
	//cout<<"returning: "<<pid<<endl;
	return pid;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid)
{ 	//cout<<"At leaf setNextNodePtr"<<endl;
	int pidSize = sizeof(pid);
	int offset = PageFile::PAGE_SIZE - pidSize;
	memcpy (buffer+offset, &pid, pidSize);

	return 0;
}



BTNonLeafNode::BTNonLeafNode()
{ 	//cout<<"constructing BTNonLeafNode"<<endl;
	memset(buffer, '\0', PageFile::PAGE_SIZE);
	int keyCount = 0;
	memcpy(buffer, &keyCount, sizeof(int));	
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{ 	//cout<<"writing non leaf node into pid: "<<pid<<endl;
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{
	int keyCount;
	memcpy(&keyCount, buffer, sizeof(int)); //first integer is the number of keys
	// cout<<"keyCount is: "<<keyCount<<endl;
	return keyCount;
}

/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{	//cout<<"At BTNonLeafNode insert"<<endl;
	int keySize = sizeof(int), intSize = sizeof(int), pidSize = sizeof(PageId);
	int keyCount = getKeyCount();
	int eid, decr = keySize+pidSize, offset=0;

	locate(key, eid);
	//cout<<"Found eid: "<<eid<<endl;
	offset = intSize + pidSize + (eid+1)*decr;
	memmove(buffer+offset, buffer+offset-decr, (keyCount-eid)*decr); // + pidSize);

	keyCount = keyCount+1;
	memcpy(buffer, &keyCount, intSize);
	memcpy(buffer+offset-decr, &key, keySize);
	memcpy(buffer+offset-decr+keySize, &pid, pidSize);
	
	return 0; 
}

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{ 	//cout<<"At BTNonLeafNode insertAndSplit"<<endl;
	//int ii;
	//cin >>ii;

	int keyCount = getKeyCount(), sibKey;
	PageId sibPid;
	int intSize = sizeof(int), keySize = sizeof(key), pidSize = sizeof(PageId);
	int incr = keySize+pidSize;
	int keys2keep = ceil(keyCount/2.0);
	
	int key1, pid1, key2, pid2;
	memcpy(&key1, buffer+intSize+pidSize+incr*(keys2keep-1), keySize);
	memcpy(&pid1, buffer+intSize+pidSize+incr*(keys2keep-1)+keySize, pidSize);
	memcpy(&key2, buffer+intSize+pidSize+incr*(keys2keep), keySize);
	memcpy(&pid2, buffer+intSize+pidSize+incr*(keys2keep)+keySize, pidSize);
	
	int sibStart;
	if (key<key1){
		midKey = key1;
		for (int i=keys2keep; i<keyCount; ++i){
			readEntry(i, sibKey, sibPid);
			if (sibling.getKeyCount()==0)	
				sibling.initializeRoot(pid1, sibKey, sibPid);
			else sibling.insert(sibKey, sibPid);
			//sibling.insert(sibKey, sibPid);
		}
		memset(buffer + intSize + pidSize + (keys2keep-1)*incr, '\0', incr*(keyCount-keys2keep+1));
		keyCount = keys2keep-1;
		memcpy(buffer, &keyCount, intSize);
		insert(key, pid);
	}
	else if (key1<key && key<key2){
		midKey = key;
		for (int i=keys2keep; i<keyCount; ++i){
			readEntry(i, sibKey, sibPid);
			if (sibling.getKeyCount()==0)	
				sibling.initializeRoot(pid, sibKey, sibPid);
			else sibling.insert(sibKey, sibPid);
		}
		memset(buffer + intSize + pidSize + keys2keep*incr, '\0', incr*(keyCount-keys2keep));
		memcpy(buffer, &keys2keep, intSize);
	}
	else if (key>key2){
		midKey = key2;
		for (int i=keys2keep+1; i<keyCount; ++i){
			readEntry(i, sibKey, sibPid);
			if (sibling.getKeyCount()==0)
				sibling.initializeRoot(pid2, sibKey, sibPid);
			else sibling.insert(sibKey, sibPid);
		}
		sibling.insert(key, pid);
		memset(buffer + intSize + pidSize + keys2keep*incr, '\0', incr*(keyCount-keys2keep));
		memcpy(buffer, &keys2keep, intSize);
	}
	return 0; 
}

/*
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTNonLeafNode::locate(int searchKey, int& eid)
{ 	//cout<<"At BTNonLeafNode locate"<<endl;
	PageId pid;
	int key;
	int keyCount = getKeyCount();
	for (eid=0; eid<keyCount; ++eid){
		readEntry(eid, key, pid);
		if (key == searchKey) 
			return 0;
		else if (key > searchKey)
			return RC_NO_SUCH_RECORD;
	}
	return RC_NO_SUCH_RECORD;
}

/*
 * Read the (key, pid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param pid[OUT] the PageId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::readEntry(int eid, int& key, PageId& pid)
{	//cout<<"At BTNonLeafNode readEntry"<<endl;
	int keyCount = getKeyCount();
	int pidSize = sizeof(PageId), intSize = sizeof(int), keySize=sizeof(key);
	if(eid < 0 || eid >= keyCount)
		return RC_INVALID_CURSOR;
	int offset = intSize + pidSize + eid*(keySize+pidSize);

	memcpy(&key, buffer+offset, keySize);
	memcpy(&pid, buffer+offset+keySize, pidSize);

	return 0;
}

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{	//cout<<"At BTNonLeafNode locateChildPtr"<<endl;
	int pidSize = sizeof(PageId), keySize = sizeof(int), intSize = sizeof(int);
	int keyCount = getKeyCount(), keyCand, offset = intSize+pidSize;
	
	for (int i=0; i<keyCount; i++){
		memcpy(&keyCand, buffer+offset, keySize);
		if (i==keyCount-1 && keyCand <= searchKey){
			memcpy(&pid, buffer+offset+keySize , pidSize); //right pointer
			//cout<<"returning right pointer: "<<pid<<endl;
			return 0;
		}
		else if (keyCand > searchKey){
			memcpy(&pid, buffer+offset-pidSize , pidSize); //left pointer
			//cout<<"returning left pointer: "<<pid<<endl;
			return 0;
		}
		offset += keySize + pidSize;
	}
	cout<<"Error: BTNonLeafNode::locateChildPtr: Could not locate child pointer";
	return -1;
}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{	//cout<<"At BTNonLeafNode initializeRoot"<<endl;
	int pidSize = sizeof(PageId), keySize = sizeof(int), intSize = sizeof(int);
	int keyCount = 1;
	memcpy(buffer, &keyCount, intSize);	
	memcpy(buffer+intSize, &pid1, pidSize);
	memcpy(buffer+intSize+pidSize, &key, keySize);
	memcpy(buffer+intSize+pidSize+keySize, &pid2, pidSize); 
	return 0; 
}
