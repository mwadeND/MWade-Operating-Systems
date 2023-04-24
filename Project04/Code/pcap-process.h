

#ifndef __PCAP_PROCESS_H
#define __PCAP_PROCESS_H

#include <stdint.h>
#include <pthread.h>


#include "packet.h"

#define DEFAULT_TABLE_SIZE  512
#define MIN_PKT_SIZE        128

/* Global Counters for Summary */

/* How many packets have we seen? */
extern uint32_t        gPacketSeenCount;

/* How many total bytes have we seen? */
extern uint64_t        gPacketSeenBytes;        

/* How many hits have we had? */
extern uint32_t        gPacketHitCount;

/* How much redundancy have we seen? */
extern uint64_t        gPacketHitBytes;

/* Simple data structure for tracking redundancy */
struct PacketEntry
{
    struct Packet * ThePacket;

    /* How many times has this been a hit? */
    uint32_t        HitCount;

    /* How much data would we have saved? */
    uint32_t        RedundantBytes;
};

// /* Our big table for recalling packets */
// extern struct PacketEntry *    BigTable; 
// extern int    BigTableSize;
// extern int    BigTableNextToReplace;

// 4 tables
int SmallTableSize;
struct PacketEntry * Table1;
int Table1NextToReplace;
pthread_mutex_t Table1Lock;
struct PacketEntry * Table2;
int Table2NextToReplace;
pthread_mutex_t Table2Lock;
struct PacketEntry * Table3;
int Table3NextToReplace;
pthread_mutex_t Table3Lock;
struct PacketEntry * Table4;
int Table4NextToReplace;
pthread_mutex_t Table4Lock;

struct HashOption
{
    struct PacketEntry * Table;
    int NextToReplace;
    pthread_mutex_t TableLock;
};


// stack
#define STACK_MAX_SIZE          10

pthread_mutex_t     StackLock;
struct Packet *  StackItems[STACK_MAX_SIZE];
int                 StackSize;


pthread_cond_t NotEmptyCond;
pthread_cond_t NotFullCond;

int KeepGoing;

char initializeProcessing (int TableSize);

void processPacket (struct Packet * pPacket);

void tallyProcessing ();

#endif
