#ifndef WORD_TRIE
#define WORD_TRIE


using namespace std;

class Word_Trie
{
    Trie             trie;
    vector<Word*>       words;
    unsigned            capacity;
    pthread_mutex_t     mutex;

    void lock()   {
        pthread_mutex_lock(&mutex); 
    }

    void unlock() {
        pthread_mutex_unlock(&mutex); 
    }

public:
    Word_Trie () { pthread_mutex_init(&mutex,   NULL);}

    Word *getWord (unsigned wid) const { return words[wid]; }

    unsigned size() const { return words.size(); }

    /**
     * Inserts the word with text: `wtxt`.
     * Actually a new word is inserted and space is allocated, ONLY
     * when the word does not already exist in our storage.
     */
    bool insert (string &wtxt, Word** word_in) {
        if (trie.contains(wtxt, word_in)) return false;

        lock();
        if (trie.insert(wtxt, word_in)) {
            words.push_back(*word_in);
            unlock();
            return true;
        }

        unlock();
        return false;
    }

};

#endif
