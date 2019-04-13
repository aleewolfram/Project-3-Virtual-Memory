// ------------------------------------------------------------
// Program to translate Virtual Addresses to Physical Addresses
// Alexandria Wolfram -- 04/13/19
// ------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define BUFLEN 256
#define FRAME_SIZE 256
#define PAGE_TABLE_SIZE 256
#define TLB_SIZE 16

//------Main Memory------
int physicalMemory[FRAME_SIZE][FRAME_SIZE]; //total 65,536 bytes

//------Page Table------
int pageTablePages[PAGE_TABLE_SIZE]; //pages
int pageTableFrames[PAGE_TABLE_SIZE]; //frames
int pageFaults = 0;
int pageTableFrameCounter = 0;

//------TBL------
int page_num_TLB[TLB_SIZE]; //pages
int frame_num_TLB[TLB_SIZE]; //frames
int tlbHit = 0;
int tlbFrameCounter = 0;

int frameCounter = 0;

//------Holds Final Values------
int virtualAddresses[1000];
int physicalAddresses[1000];
int8_t values[1000];
int8_t value;

//------File------
FILE* fadd;
FILE* fcorr;
FILE* fbin;
char address[10];
int logic_add;

//------Binary File Holder------
signed char buffer[BUFLEN];

//-------------------------------------------------------------------
unsigned int getpage(size_t x) { return (0xff00 & x) >> 8; }
unsigned int getoffset(unsigned int x) { return (0xff & x); }

void getpage_offset(unsigned int x) {
  unsigned int page = getpage(x);
  unsigned int offset = getoffset(x);
  printf("x is: %u, page: %u, offset: %u, address: %u, paddress: %u\n", x, page, offset,
         (page << 8) | getoffset(x), page * 256 + offset);
}

void translationLookasideBuffer(int pageNumber, int numFrame) //using FIFO
{
   int tlbCount = 0;
   for(int i = 0; i < tlbFrameCounter; i++)
   {
     tlbCount++;
       if(page_num_TLB[i] == pageNumber){
           break;
       }
   }

   if(tlbCount == tlbFrameCounter)
   {
       if(tlbFrameCounter < TLB_SIZE)
       {
         frame_num_TLB[tlbFrameCounter] = numFrame;
         page_num_TLB[tlbFrameCounter] = pageNumber;
       }
       else
       {
        for(int i = 0; i < (TLB_SIZE-1); i++)
         {
           frame_num_TLB[i] = frame_num_TLB[i+1];
           page_num_TLB[i] = page_num_TLB[i+1];
         }

        frame_num_TLB[tlbFrameCounter-1] = numFrame;
        page_num_TLB[tlbFrameCounter-1] = pageNumber;
       }
   }
   else
   {
       for(int i = tlbCount; i < (tlbFrameCounter-1); i++)
       {
         frame_num_TLB[i] = frame_num_TLB[i+1];
         page_num_TLB[i] = page_num_TLB[i+1];
       }

       if(tlbFrameCounter < TLB_SIZE)
       {
         page_num_TLB[tlbFrameCounter] = pageNumber;
         frame_num_TLB[tlbFrameCounter] = numFrame;
       }
       else
       {
         page_num_TLB[tlbFrameCounter-1] = pageNumber;
         frame_num_TLB[tlbFrameCounter-1] = numFrame;
       }
   }

   if(tlbFrameCounter < TLB_SIZE)
   {
     tlbFrameCounter++;
   }
}

void loadToMemory(int pageNumber)
{
   fseek(fbin, pageNumber * BUFLEN, SEEK_SET);
   fread(buffer, sizeof(signed char), BUFLEN, fbin);

   //reads into physical memory
   for(int i = 0; i < BUFLEN; i++)
   {
     physicalMemory[frameCounter][i] = buffer[i];
   }

   pageTablePages[pageTableFrameCounter] = pageNumber;
   pageTableFrames[pageTableFrameCounter] = frameCounter;

   frameCounter++;
   pageTableFrameCounter++;
}

void getPageFrame(int logic_add, int addNum)
{
   unsigned int pageNumber = getpage(logic_add);
   unsigned int offset = getoffset(logic_add);

   int numFrame = 999;

   //try TLB first
   for(int i = 0; i < TLB_SIZE; i++)
   {
     if(page_num_TLB[i] == pageNumber)
      {
         numFrame = frame_num_TLB[i];
         tlbHit++;
      }
   }

   //if not in TLB, then try page table
   if(numFrame == 999)
   {
       for(int i = 0; i < pageTableFrameCounter; i++)
       {
         if(pageTablePages[i] == pageNumber)
           numFrame = pageTableFrames[i];
       }

       if(numFrame == 999) //page fault: if not in TBL or page table, load to physical memory
       {
         loadToMemory(pageNumber);
         pageFaults++;
         numFrame = (frameCounter-1);
       }
   }

   value = physicalMemory[numFrame][offset];
   translationLookasideBuffer(pageNumber, numFrame); // put into TLB

   virtualAddresses[addNum] = logic_add;
   physicalAddresses[addNum] = (numFrame << 8) | offset;
   values[addNum] = value;
}

int main(int argc, char *argv[])
{
   if (argc != FILE_ERROR)
   {
     fprintf(stderr, "File Error!'\n");  exit(FILE_ERROR);
   }

   fadd = fopen(argv[1], "r");
   if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }
   fcorr = fopen("correct.txt", "r");
   if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }
   fbin = fopen("BACKING_STORE.bin", "rb");
   if (fbin == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }

   unsigned int virt_add, phys_add, value;  // read from file correct.txt
   char buf[BUFLEN]; // read from file correct.txt

   int currentAddress = 0;

   while ( fgets(address, 10, fadd) != NULL)
   {
      logic_add = atoi(address);
      fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add, buf, buf, &phys_add, buf, &value);  // read from file correct.txt

      getPageFrame(logic_add,currentAddress);
      unsigned int page = getpage(logic_add);
      unsigned int offset = getoffset(logic_add);

      assert(physicalAddresses[currentAddress] == phys_add);
      printf("logical: %5u (page:%3u, offset:%3u) ---> physical: %5u -- passed ---> value: %d\n", virtualAddresses[currentAddress], page, offset, physicalAddresses[currentAddress], values[currentAddress]);

       currentAddress++;
   }

   printf("Page Fault Rate: %.1f%%\n",(pageFaults/1000.0)*100);
   printf("TLB Hit Rate: %.1f%%\n", (tlbHit/1000.0)*100);

   fclose(fadd);
   fclose(fcorr);
   fclose(fbin);
   return 0;
}
