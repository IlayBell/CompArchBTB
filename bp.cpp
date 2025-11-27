/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <vector>

#define FSM_SIZE 2
#define START_TAG_IDX 2
#define PC_SIZE 32
#define FLUSH_NUM 2

// State space for prediction
enum StateSpace {
	ST = 3,
	WT = 2,
	WNT = 1,
	SNT = 0 
};

// L/G Share options
enum ShareSpace {
	NOT_USING_SHARE = 0,
	USING_SHARE_LSB = 1,
	USING_SHARE_MID = 2
};

int pow(int base, int exp) {
	if (exp == 0) {
		return 1;
	}

	return base * pow(base, exp - 1);
}

// LOG 2
int log(int x) {
	if (x == 1) {
		return 0;
	}

	return 1 + log(x / 2);
}

uint32_t extractBits(uint32_t num, int start_idx, int len) {
	uint32_t extracted = 0;

	num = num >> start_idx;

	for (int i = 0; i < len; i++) {
		if (num == 0) {
			break;
		}

		// Copy lsb to extracted
		extracted |= (num & 1);

		// Rapid with 0 at the end (shift reg)
		extracted = extracted << 1;

		// Delete lsb of num
		num = num >> 1;
	}

	return extracted;
	
}

StateSpace cvtFsmState(unsigned int_state) {
	switch (int_state) {
		case 0:
			return SNT;
		case 2:
			return WT;
		case 3:
			return ST;
		default:
			return WNT;
	}

	return WNT;
}

ShareSpace cvtShareState(unsigned int_share) {
	switch(int_share) {
		case 1:
			return USING_SHARE_LSB;
		case 2:
			return USING_SHARE_MID;
		default:
			return NOT_USING_SHARE;
	}

	return NOT_USING_SHARE;
}

int calcTheoSize(int btbSize,
				 int historySize,
				 int tagSize,
				 bool isGlobalHist,
				 bool isGlobalTable) {

	int size = 0;

	// Entry composed of TAG + HISTORY + TARGET + VALID
	int entrySize = tagSize + historySize + PC_SIZE + 1;

	// ONE BLOCK OF FSMS
	int fsmSize = pow(2, historySize) * FSM_SIZE;

	if (isGlobalHist) {
		// GLOBAL HISTORY
		size += historySize; // One time for global history

		if (isGlobalTable) {
			// GLOBAL FSMS
			size += fsmSize; // ONE FSM BLOCK
			return size;
		} 
		
		// LOCAL FSMS
		size += (entrySize - historySize) * btbSize; // GLOBAL HISTORY
		size += btbSize * fsmSize; // FSMS FOR ALL BLOCKS

		return size;
	}

	// LOCAL HISTORY
	if (isGlobalTable) {
		// GLOBAL FSMS

		size += entrySize * btbSize;
		size += fsmSize; // GLOBAL FSMS

		return size;
	} 
		
	// LOCAL FSMS

	size += entrySize * btbSize;
	size += btbSize * fsmSize;

	return size;
}


void updateFSM(StateSpace& state, bool taken, SIM_stats& stats) {
	if (taken) {
		switch (state) {
			case SNT:
				stats.flush_num += FLUSH_NUM;
				state = WNT;
				break;
			case WNT:
				stats.flush_num += FLUSH_NUM;
				state = WT;
				break;
			case WT:
				state = ST;
				break;
			default:
				state = ST;
				break;
		}
	} else {
		switch (state) {
			case ST:
				state = WT;
				stats.flush_num += FLUSH_NUM;
				break;
			case WT:
				state = WNT;
				stats.flush_num += FLUSH_NUM;
				break;
			case WNT:
				state = SNT;
				break;
			default:
				state = SNT;
				break;
		}
	}
}

class BTB_Entry {
	uint32_t tag;
	uint32_t target;
	uint8_t localHistory;
	std::vector<StateSpace> localFSM;

	bool is_empty;

	public:
		BTB_Entry(unsigned historySize, StateSpace defFsmState) {
			this->tag = 0;
			this->target = 0;
			this->localHistory = 0;

			this->is_empty = true;

			for (int i = 0; i < pow(2, historySize); i++) {
				localFSM.at(i) = defFsmState;
			}
		}

		void ResetEntry(uint32_t tag,
						uint32_t target,
						unsigned historySize,
						StateSpace defFsmState) {

				this->tag = tag;
				this->target = target;
				this->localHistory = 0;

				this->is_empty = false;

				for (int i = 0; i < pow(2, historySize); i++) {
					localFSM.at(i) = defFsmState;
				}
		}

		uint32_t getTag() {
			return this->tag;
		}

		bool isEmpty() {
			return this->is_empty;
		}

		StateSpace getLocalState(uint32_t idx) {
			return this->localFSM.at(idx);
		}

		uint32_t getLocalHistory() {
			return this->localHistory;
		}

		bool compareTag(uint32_t tag) {
			return this->getTag() == tag;
		}

		void addLocalHistory(bool taken, int histSize) {
			this->localHistory <<= 1;
			this->localHistory |= taken;

			// DELETE OLD HISTORY
			uint32_t mask = -1; // mask = 32'b111....
			mask = mask >> (PC_SIZE - histSize);

			localHistory &= mask;
		}

		StateSpace& getLocalFSM(uint32_t history) {
			return this->localFSM.at(history);
		}

};

class BTB {
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	StateSpace defFsmState;
	bool isGlobalHist;
	bool isGlobalTable;
	ShareSpace shared;

	// For global history
	uint8_t sharedHistory;

	// For global fsm table
	std::vector<StateSpace> sharedFSM;

	std::vector<BTB_Entry> entries;

	SIM_stats stats;

	public:
		BTB(unsigned btbSize,
			unsigned historySize,
			unsigned tagSize, 
			unsigned fsmState, 
			bool isGlobalHist,
			bool isGlobalTable,
			int Shared) : 	sharedFSM(pow(2, historySize)),
							entries(btbSize, BTB_Entry(historySize, 
													cvtFsmState(fsmState))) 
						   {

				this->btbSize = btbSize;
				this->historySize = historySize;
				this->tagSize = tagSize;
				this->isGlobalHist = isGlobalHist;
				this->isGlobalTable = isGlobalTable;

				this->sharedHistory = 0;
				
				// Defines defualt fsm state as enum const
				this->defFsmState = cvtFsmState(fsmState);

				// Defines share method as enum const
				this->shared = cvtShareState(Shared);

				for (int i = 0; i < pow(2, historySize); i++) {
					sharedFSM.at(i) = this->defFsmState;
				}

				this->stats.flush_num = 0;
				this->stats.br_num = 0;
				this->stats.size = calcTheoSize(this->btbSize,
										   this->historySize,
										   this->tagSize,
										   this->isGlobalHist,
										   this->isGlobalTable);	
		}

		int getBtbSize() {
			return this->btbSize;
		}

		int getTagSize() {
			return this->tagSize;
		}

		bool compareTag(uint32_t idx, uint32_t tag) {
			return this->entries.at(idx).getTag() == tag;
		}

		BTB_Entry& getEntryAtIdx(int idx) {
			return this->entries.at(idx);
		}

		bool isGlobalHistory() {
			return this->isGlobalHist;
		}

		bool isGlobalFSM() {
			return this->isGlobalTable;
		}

		StateSpace getGlobalState(uint32_t idx) {
			return this->sharedFSM.at(idx);
		}

		uint32_t getGlobalHistory() {
			return this->sharedHistory;
		}

		ShareSpace getSharedType() {
			return this->shared;
		}

		int getHistSize() {
			return this->historySize;
		}

		StateSpace getDefFsmState() {
			return this->defFsmState;
		}

		StateSpace& getSharedFSM(uint32_t pc, uint32_t history) {
			uint32_t idx_fsm;
			uint32_t relevantCut;

			// CHECK SHARE STATE
			switch (this->shared) {
				case USING_SHARE_LSB:
					relevantCut = extractBits(pc, 
											  START_TAG_IDX, 
											  this->getHistSize());
					idx_fsm = history ^ relevantCut;
					break;
				
				case USING_SHARE_MID:
					relevantCut = extractBits(pc, 
											  PC_SIZE / 2, 
											  this->getHistSize());

					idx_fsm = history ^ relevantCut;
					break;
				
				// NO USING SHARE
				default: 
					idx_fsm = history;
					break;
			}

			return this->sharedFSM.at(idx_fsm);

		}

		void addGlobalHistory(bool taken, int histSize) {
			this->sharedHistory <<= 1;
			this->sharedHistory |= taken;

			// DELETE OLD HISTORY
			uint32_t mask = -1; // mask = 32'b111....
			mask = mask >> (PC_SIZE - histSize);

			sharedHistory &= mask;
		}

		void updateBrNum() {
			this->stats.br_num += 1;
		}

		SIM_stats& getStatsRef() {
			return this->stats;
		}

};

BTB* btb;

// WHAT WE ACTUALLY SHOULD IMPLEMENT
int BP_init(unsigned btbSize, 
			unsigned historySize,
			unsigned tagSize,
			unsigned fsmState,
			bool isGlobalHist,
			bool isGlobalTable, 
			int Shared){

				btb = new BTB(btbSize, historySize, tagSize, fsmState,
						  isGlobalHist, isGlobalTable, Shared);
				
				
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	int idx_len = log(btb->getBtbSize()); 
	uint32_t idx = extractBits(pc, START_TAG_IDX, idx_len);
	uint32_t tag = extractBits(pc, START_TAG_IDX + idx_len, btb->getTagSize());

	BTB_Entry entry = btb->getEntryAtIdx(idx);

	if (entry.isEmpty() || entry.compareTag(tag)) {
		*dst = pc + 4;
		return false;
	}

	StateSpace state;
	// GLOBAL FSM
	if (btb->isGlobalFSM()) {

		uint32_t history;

		if (btb->isGlobalHistory()) {
			// GLOBAL HISTORY - GSHARE
			history = btb->getGlobalHistory();
		} else {
			//LOCAL HISTORY - LSHARE
			history = entry.getLocalHistory();
		}

		state = btb->getSharedFSM(pc, history);

	} else { // LOCAL FSM

		// GLOBAL HISTORY
		if(btb->isGlobalHistory()) {
			state = entry.getLocalState(btb->getGlobalHistory());
		}

		// LOCAL HISTORY
		if(btb->isGlobalHistory()) {
			state = entry.getLocalState(entry.getLocalHistory());
		}
	}
	
	return (state == ST || state == WT);	
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	// ADD BR+1
	btb->updateBrNum();

	int idx_len = log(btb->getBtbSize()); 
	uint32_t idx = extractBits(pc, START_TAG_IDX, idx_len);
	uint32_t tag = extractBits(pc,
								START_TAG_IDX + idx_len,
								btb->getTagSize());

	BTB_Entry entry = btb->getEntryAtIdx(idx);

	// CHECKS IF EXSITS
	if (entry.isEmpty()) {
		entry.ResetEntry(tag,
							targetPc, 
							btb->getHistSize(),
							btb->getDefFsmState());
	}

	// CHECK FOR COLLISION
	if (!entry.compareTag(tag)) {
		entry.ResetEntry(tag,
							targetPc, 
							btb->getHistSize(),
							btb->getDefFsmState());
	}

	// GLOBAL HISTORY
	if (btb->isGlobalHistory()) {
		btb->addGlobalHistory(taken, btb->getHistSize());

		if (btb->isGlobalFSM()) {
			// UPDATE GLOBAL FSM
			StateSpace& globState = btb->getSharedFSM(pc,
													 btb->getGlobalHistory());

			updateFSM(globState, taken, btb->getStatsRef());

		} else {
			// UPDATE LOCAL FSM
			StateSpace& locState = entry.getLocalFSM(btb->getGlobalHistory());
			updateFSM(locState, taken, btb->getStatsRef());
		}

	} else {
	// LOCAL HISTORY

		// UPDATE LOCAL HISTORY + FSM (L/G)
		entry.addLocalHistory(taken, btb->getHistSize());

		if (btb->isGlobalFSM()) {
			// UPDATE GLOBAL FSM
			StateSpace& globState = btb->getSharedFSM(pc,
													 entry.getLocalHistory());
			updateFSM(globState, taken, btb->getStatsRef());
		} else {
			// UPDATE LOCAL FSM
			StateSpace& locState = entry.getLocalFSM(entry.getLocalHistory());
			updateFSM(locState, taken, btb->getStatsRef());
		}

	}
}

void BP_GetStats(SIM_stats *curStats){
	SIM_stats stats = btb->getStatsRef();
	curStats->br_num =  stats.br_num;
	curStats->flush_num = stats.flush_num;
	curStats->size = stats.size;

	delete btb;
}
