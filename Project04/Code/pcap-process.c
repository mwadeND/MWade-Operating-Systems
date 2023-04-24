
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

// #include <pthread.h>


#include "pcap-process.h"

/* How many packets have we seen? */
uint32_t        gPacketSeenCount;

/* How many total bytes have we seen? */
uint64_t        gPacketSeenBytes;        

// seen lock
pthread_mutex_t SeenLock;

/* How many hits have we had? */
uint32_t        gPacketHitCount;

/* How much redundancy have we seen? */
uint64_t        gPacketHitBytes;

// hit lock
pthread_mutex_t HitLock;

// /* Our big table for recalling packets */
// struct PacketEntry *    BigTable; 
// int    BigTableSize;
// int    BigTableNextToReplace;

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

// stack
#define STACK_MAX_SIZE          10

pthread_mutex_t     StackLock;
struct Packet *  StackItems[STACK_MAX_SIZE];
int                 StackSize = 0;

pthread_cond_t NotEmptyCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t NotFullCond = PTHREAD_COND_INITIALIZER;


void initializeProcessingStats ()
{
    pthread_mutex_init(&SeenLock, 0);
    pthread_mutex_init(&HitLock, 0);

    pthread_mutex_lock(&SeenLock);
    pthread_mutex_lock(&HitLock);
    gPacketSeenCount = 0;
    gPacketSeenBytes = 0;        
    gPacketHitCount  = 0;
    gPacketHitBytes  = 0;  
    pthread_mutex_unlock(&SeenLock);  
    pthread_mutex_unlock(&HitLock);  
}

char initializeProcessing (int TableSize) // no real need for locks here bc only called from main thread once before child threads exist
{
    initializeProcessingStats();

    pthread_mutex_init(&Table1Lock, 0);
    pthread_mutex_init(&Table2Lock, 0);
    pthread_mutex_init(&Table3Lock, 0);
    pthread_mutex_init(&Table4Lock, 0);


    pthread_mutex_lock(&Table1Lock);
    pthread_mutex_lock(&Table2Lock);
    pthread_mutex_lock(&Table3Lock);
    pthread_mutex_lock(&Table4Lock);

    /* Allocate our big table */
    Table1 = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TableSize);
    Table2 = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TableSize);
    Table3 = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TableSize);
    Table4 = (struct PacketEntry *) malloc(sizeof(struct PacketEntry) * TableSize);

    if(Table1 == NULL || Table2 == NULL || Table3 == NULL || Table4 == NULL)
    {
        printf("* Error: Unable to create the new tables\n");
        return 0;
    }

    for(int j=0; j<TableSize; j++)
    {
        Table1[j].ThePacket = NULL;
        Table1[j].HitCount  = 0;
        Table1[j].RedundantBytes = 0;
        Table2[j].ThePacket = NULL;
        Table2[j].HitCount  = 0;
        Table2[j].RedundantBytes = 0;
        Table3[j].ThePacket = NULL;
        Table3[j].HitCount  = 0;
        Table3[j].RedundantBytes = 0;
        Table4[j].ThePacket = NULL;
        Table4[j].HitCount  = 0;
        Table4[j].RedundantBytes = 0;
    }

    SmallTableSize = TableSize;
    Table1NextToReplace = 0;
    Table2NextToReplace = 0;
    Table3NextToReplace = 0;
    Table4NextToReplace = 0;


    pthread_mutex_unlock(&Table1Lock);
    pthread_mutex_unlock(&Table2Lock);
    pthread_mutex_unlock(&Table3Lock);
    pthread_mutex_unlock(&Table4Lock);

    return 1;
}

void resetAndSaveEntry (int nEntry, struct PacketEntry * Table)
{
    if(nEntry < 0 || nEntry >= SmallTableSize)
    {
        printf("* Warning: Tried to reset an entry in the table - entry out of bounds (%d)\n", nEntry);
        return;
    }

    if(Table[nEntry].ThePacket == NULL)
    {
        return;
    }
    pthread_mutex_lock(&HitLock); // NOTE: This lock may slow the program but only if there is a lot of replacment 
    gPacketHitCount += Table[nEntry].HitCount;
    gPacketHitBytes += Table[nEntry].RedundantBytes;
    pthread_mutex_unlock(&HitLock);

    discardPacket(Table[nEntry].ThePacket);

    Table[nEntry].HitCount = 0;
    Table[nEntry].RedundantBytes = 0;
    Table[nEntry].ThePacket = NULL;
}

void processPacket (struct Packet * pPacket)
{
    uint16_t        PayloadOffset;

    PayloadOffset = 0;

    /* Do a bit of error checking */
    if(pPacket == NULL)
    {
        printf("* Warning: Packet to assess is null - ignoring\n");
        return;
    }

    if(pPacket->Data == NULL)
    {
        printf("* Error: The data block is null - ignoring\n");
        return;
    }

    // printf("STARTFUNC: processPacket (Packet Size %d)\n", pPacket->LengthIncluded);

    /* Step 1: Should we process this packet or ignore it? 
     *    We should ignore it if:
     *      The packet is too small
     *      The packet is not an IP packet
     */

    /* Update our statistics in terms of what was in the file */
    pthread_mutex_lock(&SeenLock);
    gPacketSeenCount++;
    gPacketSeenBytes += pPacket->LengthIncluded;
    pthread_mutex_unlock(&SeenLock);

    /* Is this an IP packet (Layer 2 - Type / Len == 0x0800)? */

    if(pPacket->LengthIncluded <= MIN_PKT_SIZE)
    {
        discardPacket(pPacket);
        return;
    }

    if((pPacket->Data[12] != 0x08) || (pPacket->Data[13] != 0x00))
    {
        // printf("Not IP - ignoring...\n");
        discardPacket(pPacket);
        return;
    }

    /* Adjust the payload offset to skip the Ethernet header 
        Destination MAC (6 bytes), Source MAC (6 bytes), Type/Len (2 bytes) */
    PayloadOffset += 14;

    /* Step 2: Figure out where the payload starts 
         IP Header - Look at the first byte (Version / Length)
         UDP - 8 bytes 
         TCP - Look inside header */

    if(pPacket->Data[PayloadOffset] != 0x45)
    {
        /* Not an IPv4 packet - skip it since it is IPv6 */
        printf("  Not IPV4 - Ignoring\n");
        discardPacket(pPacket);
        return;
    }
    else
    {
        /* Offset will jump over the IPv4 header eventually (+20 bytes)*/
    }

    /* Is this a UDP packet or a TCP packet? */
    if(pPacket->Data[PayloadOffset + 9] == 6)
    {
        /* TCP */
        uint8_t TCPHdrSize;

        TCPHdrSize = ((uint8_t) pPacket->Data[PayloadOffset+9+12] >> 4) * 4;
        PayloadOffset += 20 + TCPHdrSize;
    }
    else if(pPacket->Data[PayloadOffset+9] == 17)
    {
        /* UDP */

        /* Increment the offset by 28 bytes (20 for IPv4 header, 8 for the UDP header)*/
        PayloadOffset += 28;
    }
    else 
    {
        /* Don't know what this protocol is - probably not helpful */
        discardPacket(pPacket);
        return;
    }

    // printf("  processPacket -> Found an IP packet that is TCP or UDP\n");

    uint16_t    NetPayload;

    NetPayload = pPacket->LengthIncluded - PayloadOffset;

    pPacket->PayloadOffset = PayloadOffset;
    pPacket->PayloadSize = NetPayload;

    /* Step 2: Do any packet payloads match up? */

    int j;

    // I have not done hashing in a while, I hope this works
    int hash = pPacket->Data[PayloadOffset] % 4;

    struct HashOption hashed; 
    switch (hash) {
        case 0:
            hashed.Table = Table1;
            hashed.TableLock = Table1Lock;
            hashed.NextToReplace = Table1NextToReplace;
            break;
        case 1:
            hashed.Table = Table2;
            hashed.TableLock = Table2Lock;
            hashed.NextToReplace = Table2NextToReplace;
            break;
        case 2:
            hashed.Table = Table3;
            hashed.TableLock = Table3Lock;
            hashed.NextToReplace = Table3NextToReplace;
            break;
        case 3:
            hashed.Table = Table4;
            hashed.TableLock = Table4Lock;
            hashed.NextToReplace = Table4NextToReplace;
            break;
    }

    pthread_mutex_lock(&hashed.TableLock);

    for(j=0; j<SmallTableSize; j++)
    {
        if(hashed.Table[j].ThePacket != NULL)
        {
            int k;

            /* Are the sizes the same? */
            if(hashed.Table[j].ThePacket->PayloadSize != pPacket->PayloadSize)
            {
                continue;
            }

            /* OK - same size - do the bytes match up? */
            for(k=0; k<hashed.Table[j].ThePacket->PayloadSize; k++)
            {
                if(hashed.Table[j].ThePacket->Data[k+PayloadOffset] != pPacket->Data[k+PayloadOffset])
                {
                    /* Nope - they are not the same */
                    break;
                }
            }

            /* Did we break out with a mismatch? */
            if(k < hashed.Table[j].ThePacket->PayloadSize)
            {
                continue;
            }
            else 
            {
                /* Whoot, whoot - the payloads match up */
                hashed.Table[j].HitCount++;
                hashed.Table[j].RedundantBytes += pPacket->PayloadSize;

                /* The packets match so get rid of the matching one */
                discardPacket(pPacket);
                return;
            }
        }
        else 
        {
            /* We made it to an empty entry without a match */
            
            hashed.Table[j].ThePacket = pPacket;
            hashed.Table[j].HitCount = 0;
            hashed.Table[j].RedundantBytes = 0;
            break;
        }
    }

    /* Did we search the entire table and find no matches? */
    if(j == SmallTableSize)
    {
        /* Kick out the "oldest" entry by saving its entry to the global counters and 
           free up that packet allocation 
         */
        resetAndSaveEntry(hashed.NextToReplace, hashed.Table);

        /* Take ownership of the packet */
        hashed.Table[hashed.NextToReplace].ThePacket = pPacket;

        /* Rotate to the next one to replace */
        hashed.NextToReplace = (hashed.NextToReplace+1) % SmallTableSize;
    }

    pthread_mutex_unlock(&hashed.TableLock);

    /* All done */
}

void tallyProcessing ()
{
    pthread_mutex_lock(&Table1Lock);
    pthread_mutex_lock(&Table2Lock);
    pthread_mutex_lock(&Table3Lock);
    pthread_mutex_lock(&Table4Lock);
    for(int j=0; j<SmallTableSize; j++)
    {
        resetAndSaveEntry(j, Table1);
        resetAndSaveEntry(j, Table2);
        resetAndSaveEntry(j, Table3);
        resetAndSaveEntry(j, Table4);
    }
    pthread_mutex_unlock(&Table1Lock);
    pthread_mutex_unlock(&Table2Lock);
    pthread_mutex_unlock(&Table3Lock);
    pthread_mutex_unlock(&Table4Lock);

}





