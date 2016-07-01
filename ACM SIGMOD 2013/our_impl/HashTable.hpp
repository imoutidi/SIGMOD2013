#ifndef HASHED_TABLE
#define HASHED_TABLE


#define BPU (sizeof(unsigned long)*8)
//#include <iostream>

using namespace std;

class Hash_Table
{
public:
    vector<unsigned> indexes;
private:
    vector<unsigned long> hash_entry;
    unsigned noe;
    unsigned num_items;
    unsigned capacity;
    bool entry_lock;
    
public:
    Hash_Table (unsigned cApacity, bool eNtry_lock=false):
    noe(0),
    capacity(cApacity),
    entry_lock(eNtry_lock)
    
    {
        num_items = capacity/BPU;
        //we have done the division check if the result is bigger
        //the capacity has to be > BPU + 1
        //so we can has another item
        if(capacity%BPU) 
            num_items+=1;
        //change the capacity, accordingly to mactch the num_items
        capacity = num_items*BPU;
        hash_entry.resize(num_items,0);
        
    }
    ~Hash_Table(){
        hash_entry.clear();
    }
    bool insert (unsigned at_index){
        //compute the level of the entry 0,1,2,3...
        unsigned entry_lvl = at_index / BPU;
        //loop the table for the right position of the entry
        unsigned entry_pos = at_index % BPU;
        
        unsigned long hash = 1L << entry_pos;
       
        if (at_index < capacity){
            //word with the same letters already in 
            if(hash_entry[entry_lvl] & hash)
                return false;
            else{
                //make the bit the pos flagged as 1 
                hash_entry[entry_lvl] |= hash;
                noe++;
                if(entry_lock)
                    indexes.push_back(at_index);
                return true;
                }
        }
//        we are over capacity
        else{
            
            unsigned tmp = num_items;
//            make some space, more levels
//            2^10 more places, as the index is exactly bigger than the capacity
            capacity = at_index + (1<<10);
            num_items = capacity/BPU;
            //we have done the division check if the result is bigger
            //the capacity has to be > BPU + 1
            //so we can has another item
            if(capacity%BPU) 
                num_items+=1;
            //change the capacity, accordingly to mactch the num_items
            capacity = num_items*BPU;
            hash_entry.resize(num_items,0);
            //mark the bit in the position full
            hash_entry[entry_lvl] |= hash;
            noe++;
            if(entry_lock)
                indexes.push_back(at_index);
            return true;
        }
        
    }
    bool exists (unsigned at_index) {
        if (at_index>=capacity) 
            return false;
        //find hash table metrics
        unsigned entry_lvl = at_index / BPU;
        unsigned entry_pos  = at_index % BPU;
        unsigned long hash = 1L << entry_pos;
        
        //if we have the entry hashed bitwise check
        if (hash_entry[entry_lvl] & hash)
            return true;
        else
            return false;
    }
    unsigned size() const{
        return noe;
    }
    void clear()
    {
        noe=0;
        indexes.clear();
        hash_entry.clear();
    }
    
};

#endif
