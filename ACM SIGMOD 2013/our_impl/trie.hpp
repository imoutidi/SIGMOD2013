#ifndef TRIE_H
#define TRIE_H


using namespace std;

class Node {
public:
    
    vector<Node*> mChildren;
    void* wrd;
    
    Node(): wrd(NULL) 
    { 
        mMarker = false;
        mChildren.resize(26, nullptr); 
    }
    ~Node() {}
    bool wordMarker() { return mMarker; }
    void setWordMarker() { mMarker = true; }
    Node* findChild(const char c) const;
    Node* appendChild(const char s) { return mChildren[s - 'a'] = new Node(); }
    vector<Node*> children() { return mChildren; }
  

private:
    bool mMarker;
};

class Trie {
public:
    Trie();
    ~Trie();
    bool insert(string &s, Word **rtrn);
    bool contains(string &s, Word **rtrn) const;
    unsigned size() const;
private:
    Node* root;
    unsigned total;
    
};

Node* Node::findChild(const char c) const
{
        return mChildren[c-'a'];
}

Trie::Trie()
{
    root = new Node();
    total = 0;
}

Trie::~Trie()
{
    // Free memory
}

bool Trie::insert(string &s, Word **rtrn )
{
    Node *current = root, *child;

    for ( unsigned i = 0; s[i]; i++ )
    {        
        child = current->findChild(s[i]);
        if ( child != NULL )
        {
            current = child;
        }
        else
        {
            current = current->appendChild(s[i]);
        }
    }
    if(current->wordMarker())
    {
        *rtrn = (Word*)current->wrd;
        return false;
    }
    else
    {

        *rtrn = new Word(s, total);
        current->wrd = (void*)*rtrn;
        total++;
        current->setWordMarker();
        return true;
    }
    return false;
}


bool Trie::contains(string &s, Word **rtrn) const
{
    Node* current = root;

    for ( unsigned i = 0; s[i]; i++ )
    {
        Node* tmp = current->findChild(s[i]);
        if ( tmp == NULL )
            return false;
        current = tmp;
    }

    if ( current->wordMarker() )
    {
        *rtrn = (Word*)current->wrd;
        return true;
    }
    else
        return false;
}
unsigned Trie::size() const{
    return this->total;
}


#endif
