#include "bpred.h"

#define TAKEN   true
#define NOTTAKEN false
#define GHR_BITMASK 0x0FFF
#define PHT_ENTRY_BITMASK 0x03
#define STRONGLY_TAKEN 0b11
#define WEAKLY_TAKEN 0b10
#define WEAKLY_NOTTAKEN 0b01
#define STRONGLY_NOTTAKEN 0b00

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

BPRED::BPRED(uint32_t policy) {
    this->policy = BPRED_TYPE_ENUM(policy);
    this->ghr = 0;
    this->pht = std::map<uint16_t, uint8_t>();
    this->stat_num_branches = 0;
    this->stat_num_mispred = 0;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool BPRED::GetPrediction(uint32_t PC){
    ++stat_num_branches;
    switch(policy){
        case BPRED_ALWAYS_TAKEN:
            return TAKEN;
        case BPRED_GSHARE:
            // Retrieve the two bits to decide taken or not
            uint8_t predict = GetPHTEntry(PCxorGHR(PC));
            if(WEAKLY_TAKEN & predict)
                return TAKEN;
            else
                return NOTTAKEN;
    }
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  BPRED::UpdatePredictor(uint32_t PC, bool resolveDir, bool predDir) {
    // If always taken, no need for history overhead
    if(policy == BPRED_ALWAYS_TAKEN)
        return;

    // XOR ghr with PC to index pht
    uint16_t hsh = PCxorGHR(PC);
    // Update the PHT entry with the newly resovled branch direction
    UpdatePHTEntry(hsh, resolveDir);
    // Refresh GHR with resolved branch direction
    UpdateGHR(resolveDir);
}

uint16_t BPRED::PCxorGHR(uint32_t PC)
{
    PC &= GHR_BITMASK;
    return ghr ^ PC;
}

void BPRED::UpdateGHR(bool resolveDir)
{
    // Shift left by one bit
    ghr <<= 1;
    // Add new resolved direction to global history register
    ghr |= int(resolveDir);
    // Trim the last 4 bits, we only track the first 12
    ghr &= GHR_BITMASK;
}

uint8_t BPRED::GetPHTEntry(uint16_t hsh)
{
    // If the entry was not yet initialized, initialize it to 10 (weakly taken)
    if(!pht.count(hsh))
        return WEAKLY_TAKEN;
    // Otherwise retrieve the entry
    else
        return pht[hsh];
}

void BPRED::UpdatePHTEntry(uint16_t hsh, bool resolveDir)
{
    uint8_t entry = GetPHTEntry(hsh);
    // entry <<= 1;
    // // Shift in resolved direction (Taken/Not Taken)
    // entry |= int(resolveDir);
    // If the branch was taken, increment counter
    if(resolveDir)
        entry = SatIncrement(entry, STRONGLY_TAKEN);
    // Otherwise decrement
    else
        entry = SatDecrement(entry);
    // Trim bits beyond the first two
    entry &= PHT_ENTRY_BITMASK;
    // Place into map/ Update map
    pht[hsh] = entry;
}
/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

