#include "DirectoryManager.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "State.h"

extern pthread_t core0;

void DirectoryManager::handleLLCReadHit(uint32_t addr,Block * dstB, Block* srcB){
	int id = this->getDirectoryId(addr);
	pthread_t core= pthread_self();
	dstB->data = srcB->data;
	dstB->state = srcB->state;
	switch(srcB->state){
		case State::Modified:{
			this->directoryTable[id].state = State::Modified;
			this->directoryTable[id].owner = pthread_self()==core0? Core0:Core1;
			srcB->state  = State::Invalid;
			srcB->valid = false;
			break;
		}
		case State::Exclusive:{
			Cache * otherCache = core == core0? this->core1Cache:this->core0Cache;
			otherCache->setStateAll(addr, State::Shared);
			dstB->state = State::Shared;
			srcB->state = State::Shared;
			this->directoryTable[id].state = State::Shared;
			this->directoryTable[id].owner = Both;
			break;
		}
		case State::Invalid:{
			dstB->state = State::Exclusive;
			srcB->state = State::Exclusive;
			this->directoryTable[id].state = State::Exclusive;
			this->directoryTable[id].owner = pthread_self()==core0? Core0:Core1;
			break;
		}
		case State::Shared:{
			fprintf(stderr,"miss on a shared cache line\n");
			exit(-1);
		}
	}
}

void DirectoryManager::handleLLCReadMiss(uint32_t addr, Block* b){
	int id = this->getDirectoryId(addr);
	pthread_t core = pthread_self();
	switch(this->directoryTable[id].state){
		case State::Modified:{
			Cache * otherCache = core == core0? this->core1Cache:this->core0Cache;
			std::vector<uint8_t> data(this->blockSize);
			otherCache->findSetStateAll(addr,State::Shared,data);
			this->directoryTable[id].state = State::Shared;
			this->directoryTable[id].owner = Both;
			b->data = data;
			b->state = State::Shared;
			break;
		}
		case State::Exclusive:{
			Cache * otherCache = core == core0? this->core1Cache:this->core0Cache;
			otherCache->setStateAll(addr,State::Shared);
			this->directoryTable[id].state = State::Shared;
			this->directoryTable[id].owner = Both;
			for (uint32_t i = 0;i<this->blockSize;i++){
				b->data[i] = this->memory->getByteNoCache(addr+i);
			}
			b->state = State::Shared;
			break;
		}
		case State::Invalid:{
			this->directoryTable[id].state = State::Exclusive;
			this->directoryTable[id].owner = pthread_self()==core0? Core0:Core1;
			for (uint32_t i = 0;i<this->blockSize;i++){
				b->data[i] = this->memory->getByteNoCache(addr+i);
			}
			b->state = State::Exclusive;
			break;
		}
		case State::Shared:
			fprintf(stderr,"LLC read miss on a shared cache line\n");
			exit(-1);
	}
}

void DirectoryManager::handlePrivateWriteHit(uint32_t addr,Block*b){
	pthread_t core = pthread_self();
	switch(b->state){
		case State::Exclusive:{
			Cache * temp = core==core0?core0Cache:core1Cache;
			temp = temp->lowerCache;
			// invalid all cache except other core's private cache
			while(temp!=nullptr){
				temp->invalidCacheBlock(addr);
				temp = temp->lowerCache;
			}
			break;
		}
		case State::Shared:{
			Cache * temp = core==core0?core0Cache:core1Cache;
			temp = temp->lowerCache;
			// invalid all cache except other core's private cache
			while(temp!=nullptr){
				temp->invalidCacheBlock(addr);
				temp = temp->lowerCache;
			}
			temp = core!=core0?core0Cache:core1Cache;
			// invalid all othre core's private cache
			while(temp!=nullptr){
				temp->invalidCacheBlock(addr);
				temp = temp->lowerCache;
			}
			break;
		}
		case State::Modified:
		case State::Invalid:
		break;
	}
	b->state = State::Modified;
	int id  = this->getDirectoryId(addr);
	this->directoryTable[id].owner = core == core0?Core0:Core1;
	this->directoryTable[id].state = State::Modified;
}

void DirectoryManager::hanldePrivateInvalid(uint32_t addr,State state){
	pthread_t core = pthread_self();
	int direcId = this->getDirectoryId(addr);
	if(state == State::Exclusive){
		Cache * temp = (core == core0?this->core0Cache:this->core1Cache);
		// if the private cache of this core has other copy of this cache line, return
		while(temp->lowerCache!=nullptr){
			if(temp->inCache(addr))
				return;
			temp = temp->lowerCache;
		}
		// else, find it in LLC.
		int id = this->LLC->getBlockId(addr);
		if(id!=-1){
			// if it is in LLC, the state should be set to Invalid
			this->LLC->blocks[id].state = State::Invalid;
			this->directoryTable[direcId].state = State::Invalid;
			this->directoryTable[direcId].owner = Llc;
		}
		else{
			// if not in LLC, the directory state should be invalid and the owner is none
			this->directoryTable[direcId].state = State::Invalid;
			this->directoryTable[direcId].owner = None;
		}
	}
	else{
		Cache * temp = (core == core0?this->core0Cache:this->core1Cache);
		// if the private cache of this core has other copy of this cache line, return
		while(temp->lowerCache!=nullptr){
			if(temp->inCache(addr))
				return;
			temp = temp->lowerCache;
		}
		// else, set another core's and LLC's copy to E
		temp = (core != core0?this->core0Cache:this->core1Cache);
		temp->setStateAll(addr,State::Exclusive);

		this->directoryTable[direcId].state = State::Exclusive;
		this->directoryTable[direcId].owner = core==core0? Core1:Core0;
	}
}

int DirectoryManager::getDirectoryId(uint32_t addr){
	return (addr-this->startAddr)>>(this->blockSize);
}

bool DirectoryManager::inDirectory(uint32_t addr){
	return (addr>=this->startAddr)&&(addr<this->startAddr+this->size);
}