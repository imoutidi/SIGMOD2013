#ifndef WORD_H
#define WORD_H

#define MAX_WL ((MAX_WORD_LENGTH+1)/sizeof(unsigned long))


using namespace std;

 struct Word
{
    int length;
    unsigned wbit;
    unsigned wid;
    std::string txt;
    
    Word(string &s, unsigned index):
    wbit(0),
    wid(index)
    {
        txt.resize(MAX_WORD_LENGTH+1);
        txt = s;
        unsigned wi;
        for (wi=0; txt[wi]; wi++) 
        {
            wbit |= 1 << (txt[wi]-'a');
        }
        length = txt.length();
    }
    
    bool equals(string &s)
    {
        if(txt.compare(s)==0)
            return true;
        else
            return false;            
    }
    
//    bitwise operation saving time
     int ltr_dif(Word *w ) {
        return __builtin_popcount(this->wbit ^ w->wbit);
    }

    int ltr_dif(unsigned _wbit) {
        return __builtin_popcount(this->wbit ^ _wbit);
    }
    static int ltr_dif (unsigned lb1, unsigned lb2) {
        return __builtin_popcount(lb1 ^ lb2);
    }
};
#endif