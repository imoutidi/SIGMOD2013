#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <core.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <queue>


#include "word.hpp"
#include "trie.hpp"
#include "wordTrie.hpp"
#include "HashTable.hpp"

#define NUM_THREADS  24

enum PHASE {
    PH_IDLE, PH_01, PH_02, PH_FINISHED
};
using namespace std;
struct Query
{
	QueryID         query_id;
        unsigned        numWords;
	Word*           words[MAX_QUERY_LENGTH];
	MatchType       match_type;
	unsigned    match_dist;
};

// Keeps all query ID results associated with a dcoument
struct Document
{
	DocID                   doc_id;
	unsigned int            num_res;
        char                    *str;
        Hash_Table              *words;
	vector<QueryID>         matching_query_ids;
};

void Prepare();
static void* Thread(void *param);
void ParseDoc(Document &doc);


/* Threading */
static volatile PHASE mPhase; ///< Indicates in which phase the threads should be.
static pthread_t mThreads[NUM_THREADS]; 
static pthread_mutex_t mParsedDocs_mutex; 
static pthread_mutex_t mActiveQ_mutex; 
static pthread_cond_t mActiveQ_cond; 
static pthread_mutex_t mPendingDocs_mutex; 
static pthread_cond_t mPendingDocs_cond; 
static pthread_mutex_t mReadyDocs_mutex; 
static pthread_cond_t mReadyDocs_cond; 
static pthread_barrier_t mBarrier;

static Word_Trie word_DB;

static Hash_Table query_words[2]
{
    Hash_Table(1 << 10, 0), Hash_Table(1 << 10, 0)
};
static vector<Query> active_queries;
static vector<QueryID> ham_queries;
static vector<QueryID> edit_queries;

// Keeps all currently available results that has not been returned yet
static vector<Document> ready_docs;
static vector<Document> active_docs;
static vector<Document> parsed_docs[NUM_THREADS];

unsigned last_check_doc[NUM_THREADS];


// Computes edit distance between a null-terminated string "a" with length "na"
//  and a null-terminated string "b" with length "nb" 
int EditDistance(string &a, int na, string &b, int nb)
{
	int oo=0x7F;

	static int T[2][MAX_WORD_LENGTH+1];

	int ia, ib;

	int cur=0;
	ia=0;

	for(ib=0;ib<=nb;ib++)
		T[cur][ib]=ib;

	cur=1-cur;

	for(ia=1;ia<=na;ia++)
	{
		for(ib=0;ib<=nb;ib++)
			T[cur][ib]=oo;

		int ib_st=0;
		int ib_en=nb;

		if(ib_st==0)
		{
			ib=0;
			T[cur][ib]=ia;
			ib_st++;
		}

		for(ib=ib_st;ib<=ib_en;ib++)
		{
			int ret=oo;

			int d1=T[1-cur][ib]+1;
			int d2=T[cur][ib-1]+1;
			int d3=T[1-cur][ib-1]; if(a[ia-1]!=b[ib-1]) d3++;

			if(d1<ret) ret=d1;
			if(d2<ret) ret=d2;
			if(d3<ret) ret=d3;

			T[cur][ib]=ret;
		}

		cur=1-cur;
	}

	int ret=T[1-cur][nb];

	return ret;
}

// Computes Hamming distance between a null-terminated string "a" with length "na"
//  and a null-terminated string "b" with length "nb" 
unsigned int HammingDistance(string &a, int na, string &b, int nb)
{
	int j, oo=0x7F;
	if(na!=nb) return oo;
	
	unsigned int num_mismatches=0;
	for(j=0;j<na;j++) if(a[j]!=b[j]) num_mismatches++;
	
	return num_mismatches;
}


ErrorCode InitializeIndex(){
/* Create the mThreads, which will enter the waiting state. */
    pthread_mutex_init(&mPendingDocs_mutex, NULL);
    pthread_mutex_init(&mActiveQ_mutex, NULL);
    pthread_cond_init(&mActiveQ_cond, NULL);
    pthread_mutex_init(&mParsedDocs_mutex, NULL);
    pthread_cond_init(&mPendingDocs_cond, NULL);
    pthread_mutex_init(&mReadyDocs_mutex, NULL);
    pthread_cond_init(&mReadyDocs_cond, NULL);
    pthread_barrier_init(&mBarrier, NULL, NUM_THREADS);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    mPhase = PH_IDLE;

    for (long t = 0; t < NUM_THREADS; t++) {
        int rc = pthread_create(&mThreads[t], &attr, Thread, (void *) t);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    return EC_SUCCESS;    
}

ErrorCode DestroyIndex(){
    pthread_mutex_lock(&mPendingDocs_mutex);
    mPhase = PH_FINISHED;
    pthread_cond_broadcast(&mPendingDocs_cond);
    pthread_mutex_unlock(&mPendingDocs_mutex);

    for (long t = 0; t < NUM_THREADS; t++) {
        pthread_join(mThreads[t], NULL);
    }

    return EC_SUCCESS;
}

ErrorCode StartQuery(QueryID query_id, const char* query_str, MatchType match_type, unsigned int match_dist)
{
	Query query;
        const char *c2=query_str-1;
        unsigned words_in_query = 0;
        int i;
        char wtxt[MAX_WORD_LENGTH+1];
        Word *word ;

        do{
             i=0; do {wtxt[i++] = *++c2;} while (*c2!=' ' && *c2 );
            wtxt[--i] = 0;

            string txt(wtxt,0,MAX_WORD_LENGTH);
            word_DB.insert(txt, &word);
            query.words[words_in_query] = word;
            words_in_query++;
            
        }while(*c2);
        
        if(match_type!= MT_EXACT_MATCH)
        {
            if (match_type == MT_EDIT_DIST)
                edit_queries.push_back(query_id);
            else
                ham_queries.push_back(query_id);
        }
        
	query.query_id = query_id;
	query.numWords = words_in_query;
	query.match_type = match_type;
	query.match_dist = match_dist;
        active_queries.push_back(query);
        
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode EndQuery(QueryID query_id)
{
	// Remove this query from the active query set
	unsigned int i, n=active_queries.size();
	for(i=0;i<n;i++)
	{
		if(active_queries[i].query_id==query_id)
		{
			active_queries[i].numWords=0;
			break;
		}
	}
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode MatchDocument(DocID doc_id, const char* doc_str)
{

    char *new_doc_str = (char *) malloc((1 + strlen(doc_str)));
    if (!new_doc_str) {
        fprintf(stderr, "Could not allocate memory. \n");
        fflush(stderr);
        return EC_FAIL;
    }
    strcpy(new_doc_str, doc_str);  
        
    Document doc;
    doc.doc_id = doc_id;
    doc.str = new_doc_str;
    doc.words = new Hash_Table(0,1);
        
    pthread_mutex_lock(&mPendingDocs_mutex);
    active_docs.push_back(doc);
    mPhase = PH_01;
    pthread_cond_broadcast(&mPendingDocs_cond);
    pthread_mutex_unlock(&mPendingDocs_mutex);
        
        
    return EC_SUCCESS;
}
void ParseDoc(Document &doc)
{
        //parse the new document
        
        char txt[MAX_WORD_LENGTH+1];
        int i;
        char *c2;
        Word* nw ;
        c2 = doc.str - 1;
        do {
            i=0; do {txt[i++] = *++c2;} while (*c2!=' ' && *c2 );
            txt[--i] = 0;
           string wtxt(txt,0,MAX_WORD_LENGTH+1);
    //        insert the word to the summing index
            word_DB.insert(wtxt, &nw);
    //      insert the word id in the documents word list
            doc.words->insert(nw->wid);
        } while(*c2);
        
        
       	
}

///////////////////////////////////////////////////////////////////////////////////////////////

ErrorCode GetNextAvailRes(DocID* p_doc_id, unsigned int* p_num_res, QueryID** p_query_ids)
{
	// Get the first undeliverd resuilt from "docs" and return it
        pthread_mutex_lock(&mPendingDocs_mutex);
        if (mPhase == PH_01) mPhase = PH_02;
        pthread_cond_broadcast(&mPendingDocs_cond);
        pthread_mutex_unlock(&mPendingDocs_mutex);
        
        pthread_mutex_lock(&mReadyDocs_mutex);
        while (ready_docs.empty())
                pthread_cond_wait(&mReadyDocs_cond, &mReadyDocs_mutex);
        
        
	if(ready_docs.size()==0) return EC_NO_AVAIL_RES;
	
        *p_doc_id=ready_docs[0].doc_id; 
        *p_num_res=ready_docs[0].matching_query_ids.size(); 
        
        QueryID *q_query_ids = (QueryID*) malloc(*p_num_res * sizeof(QueryID));
        for(unsigned i=0 ; i< *p_num_res; i++)
        {
            q_query_ids[i] = ready_docs[0].matching_query_ids[i];
        }
        
        *p_query_ids = q_query_ids;
        
        ready_docs.erase(ready_docs.begin());
        

        pthread_cond_broadcast(&mReadyDocs_cond);
        pthread_mutex_unlock(&mReadyDocs_mutex);
	return EC_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// elenxei tis lekseis apo ta docs edw kalei tis exact edit kai hamming distances
/////////////////////////////////////////////////////////////////////////////
void Prepare(long myThreadId){
   
    for (unsigned i = 0 ; i< parsed_docs[myThreadId].size(); i++)//+=NUM_THREADS )
    {
        pthread_mutex_lock(&mParsedDocs_mutex);
        Document &doc = parsed_docs[myThreadId][i];
        pthread_mutex_unlock(&mParsedDocs_mutex);
        
        pthread_mutex_lock(&mActiveQ_mutex);
        for(unsigned j =0 ; j< active_queries.size();j++)
        {
            Query &query = active_queries[j];
            
            unsigned matched = 0;
            bool a = false;
            if(query.numWords==0)continue;
            if(query.match_type == MT_EXACT_MATCH)
            {
                unsigned k =0;
                while( k < query.numWords )
                {
                        if (doc.words->exists(query.words[k]->wid))
                            ++matched;
                        else
                            break;
                        k++;
                }
            }
            else if (query.match_type == MT_EDIT_DIST)
            {
                unsigned k =0;
                a = false;
                
                while( k < query.numWords )
                {
                    for (unsigned index=0; index < doc.words->indexes.size(); index++)
                    {
                        Word *nw  = word_DB.getWord(doc.words->indexes[index]);
                        unsigned res = EditDistance(query.words[k]->txt, query.words[k]->length, nw->txt, nw->length);
                        if(res <= query.match_dist)
                        {
                                ++matched;
                                a=true;//if it fails no need to check further
                                break;//if not either
                        }
                    }
                    if(!a) break;
                    k++;
                }
            }
            else if (query.match_type == MT_HAMMING_DIST)
            {
                unsigned k =0;
                a = false;
                while( k < query.numWords )
                {
                    for (unsigned index=0; index < doc.words->indexes.size(); index++)
                    {
                        Word *nw  = word_DB.getWord(doc.words->indexes[index]);
                        unsigned res = HammingDistance(query.words[k]->txt, query.words[k]->length, nw->txt, nw->length);
                        if(res <= query.match_dist)
                        {
                            ++matched;
                            a = true;
                            break;
                        }
                    }
                    if(!a) break;
                    k++;
                }
            }
            if(query.numWords <= matched) {
                doc.matching_query_ids.push_back(query.query_id);
                
            }
            
        }
        pthread_mutex_unlock(&mActiveQ_mutex);
        
        pthread_mutex_lock(&mReadyDocs_mutex);
        ready_docs.push_back(doc);
        
        pthread_cond_broadcast(&mReadyDocs_cond);
        pthread_mutex_unlock(&mReadyDocs_mutex);
        
    }
    
    last_check_doc[myThreadId] = active_docs.size();
    
    
}

void* Thread(void *param) {
    const long myThreadId = (long) param;

    while (1) {
        pthread_barrier_wait(&mBarrier);
        /** PHASE 01 */
        while (1) {
            pthread_mutex_lock(&mPendingDocs_mutex);
                while (active_docs.empty() && mPhase < PH_02)
                    pthread_cond_wait(&mPendingDocs_cond, &mPendingDocs_mutex);

                if (active_docs.empty() || mPhase == PH_FINISHED) {
                    pthread_cond_broadcast(&mPendingDocs_cond);
                    pthread_mutex_unlock(&mPendingDocs_mutex);
                    break;
                }

                /* Get a document from the pending list */
                Document doc = active_docs.back();
                active_docs.pop_back();
            pthread_cond_broadcast(&mPendingDocs_cond);
            pthread_mutex_unlock(&mPendingDocs_mutex);

            /* Parse the document and append it to mParsedDocs */
            
            pthread_mutex_lock(&mParsedDocs_mutex);
            ParseDoc(doc);

            if (doc.doc_id < NUM_THREADS)
                parsed_docs[doc.doc_id].push_back(doc);
            else
                parsed_docs[doc.doc_id % NUM_THREADS].push_back(doc);
            pthread_mutex_unlock(&mParsedDocs_mutex);
        }

        /* Finish detected, thread should exit. */
        if (mPhase == PH_FINISHED) return NULL; //break;
        pthread_barrier_wait(&mBarrier);

       
        Prepare(myThreadId);
        pthread_barrier_wait(&mBarrier);
        parsed_docs[myThreadId].clear();

    }


    return NULL;
}
