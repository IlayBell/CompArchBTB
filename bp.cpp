/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <vector>

#define FSM_SIZE 4

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

class BTB_Entry {
	uint32_t tag;
	uint32_t target;
	uint8_t local_history;
	std::vector<StateSpace> local_fsm;

	BTB_Entry(uint32_t tag,
			  uint32_t target,
			  unsigned historySize,
			  StateSpace defFsmState) : local_fsm(pow(2, historySize)) {
										
			this->tag = tag;
			this->target = target;
			this->local_history = 0;

			for (int i = 0; i < pow(2, historySize); i++) {
				local_fsm.at(i) = defFsmState;
			}
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
	uint8_t shared_history;

	// For global fsm table
	std::vector<StateSpace> shared_fsm;

	std::vector<BTB_Entry> entries;

	public:
		BTB(unsigned btbSize,
			unsigned historySize,
			unsigned tagSize, 
			unsigned fsmState, 
			bool isGlobalHist,
			bool isGlobalTable,
			int Shared) : entries(btbSize), 
						  shared_fsm(pow(2, historySize)) {

				this->btbSize = btbSize;
				this->historySize = historySize;
				this->tagSize = tagSize;
				this->isGlobalHist = isGlobalHist;
				this->isGlobalTable = isGlobalTable;

				this->shared_history = 0;
				
				// Defines defualt fsm state as enum const
				switch (fsmState) {
					case 0:
						this->defFsmState = SNT;
						break;
					case 2:
						this->defFsmState = WT;
						break;
					case 3:
						this->defFsmState = ST;
						break;
					default:
						this->defFsmState = WNT;
						break;
				}
				
				// Defines share method as enum const
				switch(Shared) {
					case 1:
						this->shared = USING_SHARE_LSB;
						break;
					case 2:
						this->shared = USING_SHARE_MID;
						break;
					default:
						this->shared = NOT_USING_SHARE;
						break;

				}

				for (int i = 0; i < pow(2, historySize); i++) {
					shared_fsm.at(i) = this->defFsmState;
				}
		}

		int getBtbSize() {
			return this->btbSize;
		}


};

BTB btb;

// WHAT WE ACTUALLY SHOULD IMPLEMENT
int BP_init(unsigned btbSize, 
			unsigned historySize,
			unsigned tagSize,
			unsigned fsmState,
			bool isGlobalHist,
			bool isGlobalTable, 
			int Shared){

				btb = BTB(btbSize, historySize, tagSize, fsmState,
						  isGlobalHist, isGlobalTable, Shared);
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	int idx_len = log(btb.getBtbSize()); 

	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	return;
}

void BP_GetStats(SIM_stats *curStats){
	return;
}

uint32_t extract_bits(uint32_t num, int start_idx, int len) {
	uint32_t extracted = 0;

	num = num >> start_idx;

	for (int i = 0; i < len; i++) {
		extracted += num % 2;
		extracted = extracted << 1;
		num = num >> 1;
	}

	return extracted;
	
}
