#include <iostream>
#include <fstream>
#include <string.h>
#include <iomanip>
#include <stdlib.h>
#include <math.h>
#include <sstream>
#include <algorithm>

using namespace std;

int L1_read = 0;
int L1_readmiss = 0;
int L1_write = 0;
int L1_writemiss = 0;
int L1_WB = 0;
int L1_swap_requests = 0;
int L1_swaps = 0;
int L2_read = 0;
int L2_readmiss = 0;
int L2_write = 0;
int L2_writemiss = 0;
int L2_WB = 0;
bool L2_exists;

int vc_use = 0;
int vc_check = 0;

long int invalid = 0x100000000;

void LRU_Update(int,int,int,bool);
void count_update(bool, bool);
void OnMiss_update(int, long int, long int, long int, bool, bool);
void ReadOrWrite(int, long int, bool);

/*DEFINE A BASIC STRUCTURE CALLED block*/
struct block
{
	long int tag;
	long int block_address;
	bool dirty_bit;
	int MRU_count;
	int max_MRU;
	bool areBlocksEmpty;
};

typedef struct block Block;

/*DDEFINE A 2D block ARRAY USING POINTERS*/
Block** InitCache (int cache_size, int cache_assoc)
{
	Block**  matrix = new Block *[cache_size];

	for (int i=0; i<cache_size; i++)
	{
		matrix[i] = new Block [cache_assoc];
	}

	return matrix;
}


/*INITIALIZE THE GENERIC CACHE MODULE*/
class Cache
{
	public:
	int Size;
	int Blocksize;
	int Assoc;
    int VC_Blocks;
	int sets;
	int off_bits;
	int ind_bits;
	bool vc_enabled;
	Block** cache_actual;
	Block** victim_cache;

	void initCache(int cache_size, int block, int assoc, int vc_blocks)
	{
        vc_enabled = false;

		Size = cache_size;
		Blocksize = block;
		Assoc = assoc + 1; /* THIS IS TO MAKE THE FIRST CACHE BLOCK A COUNTER BLOCK */
		VC_Blocks= vc_blocks + 1; /* THIS IS TO MAKE THE FIRST VC BLOCK A COUNTER BLOCK */

		/* #set, #index, #offset calculation */
		sets = Size/((assoc)*Blocksize);
		ind_bits = int (log2(sets));
		off_bits = int (log2(Blocksize));

		/* INITIALIZE THE 2D STRUCT ARRAY - THE ACTUAL CACHE MEMORY LAYOUT*/
		cache_actual = InitCache(sets, Assoc);

		for (int i=0; i<sets; i++)
		{
			for(int j=0; j<Assoc; j++)
			{
				cache_actual[i][j].tag = invalid;
				cache_actual[i][j].dirty_bit = false;
				cache_actual[i][j].MRU_count = 0;
				cache_actual[i][j].max_MRU = 0;
				cache_actual[i][j].areBlocksEmpty = true;
			}
		}
		
		if(vc_blocks) /* IF USER REQUESTS VICTIM CACHE */
        {
            vc_enabled = true;

            /*INITIALIZE THE VICTIM CACHE */
            victim_cache = InitCache(1,VC_Blocks);

            for(int v = 0; v<VC_Blocks; v++)
            {
                victim_cache[0][v].block_address = invalid;
                victim_cache[0][v].tag = invalid;
                victim_cache[0][v].MRU_count = 0;
				victim_cache[0][v].max_MRU = 0;
				victim_cache[0][v].dirty_bit = false;
				victim_cache[0][v].areBlocksEmpty = true;
            }

        }

    }
};

Cache cache[2]; /* ARRAY OF CACHES */

void LRU_Update(int index, int set_num, int i, bool is_VC)
{
    if(is_VC) // IF IT'S A VC BLOCK UPDATE
    {
        Block vic_temp = cache[index].victim_cache[0][i];

        /* Set MRU update */
        for(int j=1; j<cache[index].VC_Blocks; j++)
        {
            if (cache[index].victim_cache[0][j].MRU_count > vic_temp.MRU_count)
                cache[index].victim_cache[0][j].MRU_count--;
        }

        /* Self MRU update */
        vic_temp.MRU_count = cache[index].victim_cache[0][0].max_MRU;

        cache[index].victim_cache[0][i] = vic_temp;
    }
    else // IF IT'S A REGULAR CACHE BLOCK UPDATE
    {
        Block ac_temp = cache[index].cache_actual[set_num][i];

        /* Set MRU update */
        for(int j=1; j<cache[index].Assoc; j++)
        {
            if (cache[index].cache_actual[set_num][j].MRU_count > ac_temp.MRU_count)
                cache[index].cache_actual[set_num][j].MRU_count--;
        }

        /* Self MRU update */
        ac_temp.MRU_count = cache[index].cache_actual[set_num][0].max_MRU;

        cache[index].cache_actual[set_num][i] = ac_temp;
       
    }
}

void count_update(bool hit, bool isRead, int index)
{
   if (index == 0)
    {
        if(isRead)
		{
		 L1_read++;
		 if (!hit)
			L1_readmiss++;
		}
        else
		{
		 L1_write++;
		 if (!hit)
			L1_writemiss++;
		}
    }
    else
    {
        if(isRead)
		{
		 L2_read++;
		 if (!hit)
			L2_readmiss++;
		}
        else
		{
		 L2_write++;
		 if (!hit)
			L2_writemiss++;
		}
    }
}

int VC_update (Block &bl_actual, long int tag_recvd, long int addr)
{
    int check = 0;
    for(int v = 1; v<cache[0].VC_Blocks; v++)
        {
            Block bl_victim = cache[0].victim_cache[0][v];
            if(bl_victim.block_address == addr) /*IF BLOCK IS FOUND IN VC, THEN SWAP AND UPDATE LRU */
            {
                /* SWAP */
                L1_swaps++;

                swap(bl_actual.MRU_count,bl_victim.MRU_count);  /* THIS IS TO CONSERVE THE RESPECTIVE LRU VALUES */
                swap(bl_actual,bl_victim);

                cache[0].victim_cache[0][v] = bl_victim;

				LRU_Update(0,0,v,true); /* VC LRU UPDATE */
                check+=2;
                break;
            }
            else if (bl_victim.block_address == invalid) /* IF A VC BLOCK IS EMPTY, POPULATE IT */
            {
                /* POPULATING VC */
                bl_victim = bl_actual;
                cache[0].victim_cache[0][0].max_MRU++;
                bl_victim.MRU_count = cache[0].victim_cache[0][0].max_MRU; /* VC LRU UPDATE */               

                cache[0].victim_cache[0][v] = bl_victim;

                check++;
                break;
            }

        }

        if(check == 0) /* IF IT'S A VC MISS AND VC DOESN'T HAVE EMPTY BLOCKS */
        {
            /* A VC WRITEBACK NEEDS TO BE DONE AND THE EVICTED BLOCK FROM L1 NEEDS TO BE PUT IN VC */
            for(int v = 1; v<cache[0].VC_Blocks; v++)
            {
                Block bl_victim = cache[0].victim_cache[0][v];

                if(bl_victim.MRU_count == 1) /* LOOKING FOR THE LRU BLOCK IN VC */
                {
                    /* VC WRITEBACK TO L2 NEEDS TO BE IMPLEMENTED only in case of dirty block */
                    if (bl_victim.dirty_bit)
                    {
                        L1_WB++;
                        if (L2_exists)
                            ReadOrWrite(1, bl_victim.block_address, false); /* CALLING ReadOrWrite ON L2 */
                    }

                    /* COPYING L1 BLOCK TO VC AND DOING VC LRU UPDATE*/
                    bl_victim = bl_actual;
                    bl_victim.MRU_count = 1;
                    bl_actual.dirty_bit = false;

                    cache[0].victim_cache[0][v] = bl_victim;
                    LRU_Update(0, 0, v, true);

                    break;
                }
            }
        }
    return check;
}

/* CACHE UPDATE ON MISS */
void OnMiss_update(int index, long int set_num, long int tag_recvd, long int addr, bool hit, bool isRead)
{
	  if(cache[index].cache_actual[set_num][0].areBlocksEmpty)  /* IF THERE ARE EMPTY BLOCKS IN THE CACHE*/
		{
		  /* POPULATING EMPTY BLOCKS IN CACHE */
		  for(int i=1; i<cache[index].Assoc; i++)
			{
				Block bl_actual = cache[index].cache_actual[set_num][i];

				if(bl_actual.tag == invalid)
				{
					bl_actual.tag = tag_recvd;
					bl_actual.block_address = addr;

					cache[index].cache_actual[set_num][0].max_MRU++;
					bl_actual.MRU_count = cache[index].cache_actual[set_num][0].max_MRU;

					/* IF IT'S A WRITE */
					if (!isRead)
                    {
                        bl_actual.dirty_bit = true;
                    }

                    cache[index].cache_actual[set_num][i] = bl_actual;

					/* IF IT'S THE LAST INVALID BLOCK */
					if(i == (cache[index].Assoc-1))
					{
						cache[index].cache_actual[set_num][0].areBlocksEmpty = false;
					}

                    count_update(hit,isRead, index);

                    if (!index && L2_exists)
                        ReadOrWrite(1, addr, true); 	/* CALLING ReadOrWrite ON L2 TO UPDATE IT*/
        
					break;
				}
			}
		}

	  /* IF NO EMPTY BLOCKS IN CACHE */
	  else
	  {
        int check = 0;
		for (int i=1; i<cache[index].Assoc; i++)
		{
		    /* CHECKING FOR THE LRU BLOCK IN CACHE */
			if(cache[index].cache_actual[set_num][i].MRU_count == 1)
			{
			    Block bl_actual = cache[index].cache_actual[set_num][i];

                if(cache[index].vc_enabled) /* IF VC IS ENABLED IN THE CACHE */
                {
                    L1_swap_requests++;
					
                    /* CHECKING FOR VC HIT OR VC EMPTY BLOCK */
                    check = VC_update (bl_actual, tag_recvd, addr);
                }
                else
                {
                    /* IN THIS CASE, L1 WILL WRITE DIRECTLY TO MEMORY/L2 and L2 WILL WRITE DIRECTLY TO MEMORY */
                   if (bl_actual.dirty_bit)
                        {
                            if(!index)
                            {
                                L1_WB++;
                                if(L2_exists)
                                    ReadOrWrite(1, bl_actual.block_address, false);
                                bl_actual.dirty_bit = false;
                            }
                            else
                            {
                                L2_WB++;
                                bl_actual.dirty_bit = false;
                            }
                        }
                }

                /* INSERTING THE BLOCK INTO CACHE */

                /* IF IT'S A READ/WRITE TO L1, THE BLOCK FIRST NEEDS TO BE ALLOCATED IN L2 AND THEN IN L1 */
                if(!index && L2_exists && check!=2)
                    ReadOrWrite(1, addr, true);  /* CALLING ReadOrWrite ON L2 */

                bl_actual.tag = tag_recvd;
                bl_actual.block_address = addr;

                /* IF IT'S A WRITE */
				if (!isRead )
                {                    
                    bl_actual.dirty_bit = true;
                }
				else
				{
				    if (!index)
                    {
                        if(check!=2)
                            bl_actual.dirty_bit = false;
                    }
                    else
                        bl_actual.dirty_bit = false;
				}

                cache[index].cache_actual[set_num][i] = bl_actual;

				LRU_Update(index, set_num, i, false);
				count_update(hit,isRead, index);
                break;
			}
		}
	  }
}


/* READ OR WRITE A BLOCK */
void ReadOrWrite(int index, long int address, bool isRead)
{
    long int addr;
    if (!index)
         addr = address >> cache[index].off_bits;
    else
         addr = address;

	long int mask = pow(2,cache[index].ind_bits) - 1;
	long int set_num = addr & mask;
	long int tag_recvd = addr >> cache[index].ind_bits;

	bool hit = false;

	/*SEARCH SET FOR BLOCK*/
	for(int i=1; i<cache[index].Assoc; i++)
	{
		if(cache[index].cache_actual[set_num][i].tag == tag_recvd) /* IF IT'S A HIT */
		{
			hit = true;			

			if(!isRead)
			    cache[index].cache_actual[set_num][i].dirty_bit = true;

            LRU_Update(index, set_num, i, false);
            count_update(hit, isRead, index);

            break;
		}
	}

	if(!hit)
	{
	    hit = false;
		OnMiss_update(index,set_num,tag_recvd,addr,hit,isRead);
	}

}

/* Main function */
int main(int argc, char* argv[])
{
	bool isRead;
	ifstream input;
	char ch;
	int blocksize, L1_size, L2_size, L1_assoc, L2_assoc, L1_vc_blocks;
	int ctr = 1;
	L2_exists = false;
	int ind = 0;

    if (argc == 8)
    {
        blocksize = atoi(argv[1]);
        L1_size = atoi(argv[2]);
        L1_assoc = atoi(argv[3]);
        L1_vc_blocks = atoi(argv[4]);
        L2_size = atoi(argv[5]);
        L2_assoc = atoi(argv[6]);
    }


	if(L2_size)
		L2_exists = true;

	/* Initialize caches */
	cache[0].initCache(L1_size, blocksize, L1_assoc, L1_vc_blocks);
	if(L2_exists)
		cache[1].initCache(L2_size, blocksize, L2_assoc, 0);

	/* Read data from file and update caches */
	input.open(argv[7], ios::in);
	input.get(ch);

	while (true)
	{
		long int addr_in_hex;
		string addr;

		if (ch == 'w' || ch == 'r')
		{
			input.get();
			if(ch == 'r')
			{
				isRead = true;
				char cr = 'x';
				while((cr != 'w' && cr!= 'r') && !input.eof() )
				{
					input.get(cr);
					if(cr != 'w' || cr != 'r')
                    {
                        addr += cr;
                    }
				}
				ch = cr;
				istringstream buffer (addr);
				buffer >> hex >> addr_in_hex;
				ctr++;
				ReadOrWrite(0, addr_in_hex, isRead);
			}

			else
			{
				isRead = false;
				char c = 'o';
				while(c != 'w' && c!= 'r' && !input.eof())
				{
					input.get(c);
					if(c != 'w' || c != 'r')
                    {
                        addr += c;
                    }
				}
				ch = c;
				istringstream buffer (addr);
				buffer >> hex >> addr_in_hex;
				ctr++;
				ReadOrWrite(0, addr_in_hex, isRead);
			}
		}

		if(input.eof())
            break;

	}

	while (ind<2)
	{
	    for(int i=0; i<cache[ind].sets;i++)
          for(int k = 1; k< cache[ind].Assoc; k++)
            for(int j= k+1; j<cache[ind].Assoc; j++)
                if(cache[ind].cache_actual[i][k].MRU_count<cache[ind].cache_actual[i][j].MRU_count)
                {
                    Block temp = cache[ind].cache_actual[i][k];
                    cache[ind].cache_actual[i][k] = cache[ind].cache_actual[i][j];
                    cache[ind].cache_actual[i][j] = temp;
                }
        if(L2_exists)
            ind++;
        else
            break;
	}

	if(cache[0].vc_enabled)
    {
	 for(int k = 1; k< cache[0].VC_Blocks; k++)
        for(int j= k+1; j<cache[0].VC_Blocks; j++)
            if(cache[0].victim_cache[0][k].MRU_count<cache[0].victim_cache[0][j].MRU_count)
            {
                Block temp = cache[0].victim_cache[0][k];
                cache[0].victim_cache[0][k] = cache[0].victim_cache[0][j];
                cache[0].victim_cache[0][j] = temp;
            }
    }

	cout<<"===== Simulator configuration =====";
	cout<<"\n  BLOCKSIZE:     "<<blocksize<<"\n  L1_SIZE:                  "<<L1_size<<"\n  L1_ASSOC:                  "<<L1_assoc<<"\n  VC_NUM_BLOCKS:     "<<L1_vc_blocks<<"\n  L2_SIZE:     "<<L2_size;
	cout<<"\n  L2_ASSOC:     "<<L2_assoc<<"\n  trace_file:     "<<argv[7];
	cout<<"\n\n ===== L1 contents =====";

	for(int i = 0; i<cache[0].sets;i++)
	{
		cout<<"\n  set   "<<i<<":   ";
		for(int j=1;j<cache[0].Assoc; j++)
		{
			if(cache[0].cache_actual[i][j].dirty_bit == true)
				cout<<hex<<cache[0].cache_actual[i][j].tag<<dec<<" D  ";
			else
				cout<<hex<<cache[0].cache_actual[i][j].tag<<dec<<"    ";
		}
	}

	if(cache[0].vc_enabled)
	{
		cout<<"\n\n===== VC contents ===== \n set   0:";
		for(int i=1; i<cache[0].VC_Blocks; i++)
        {
            if (cache[0].victim_cache[0][i].dirty_bit == true)
                cout<<"   "<<hex<<cache[0].victim_cache[0][i].block_address<<dec<<" D ";
            else
                cout<<"   "<<hex<<cache[0].victim_cache[0][i].block_address<<dec;
        }
	}

	if(L2_size)
	{
		cout<<"\n\n ===== L2 contents =====";
		for(int i = 0; i<cache[1].sets;i++)
		{
			cout<<"\n  set   "<<i<<":   ";
			for(int j=1;j<cache[1].Assoc; j++)
			{
				if(cache[1].cache_actual[i][j].dirty_bit == true)
					cout<<hex<<cache[1].cache_actual[i][j].tag<<dec<<" D  ";
				else
					cout<<hex<<cache[1].cache_actual[i][j].tag<<dec<<"    ";
			}
		}
	}

	cout<<"\n\n===== Simulation results =====";
	cout<<"\n a. number of L1 reads:     "<<L1_read<<"\n b. number of L1 read misses:     "<<L1_readmiss<<"\n c. number of L1 writes:     "<<L1_write;
	cout<<"\n d. number of L1 write misses:     "<<L1_writemiss<<"\n e. number of swap requests:     "<<L1_swap_requests;
	cout<<"\n f. swap request rate:     "<< fixed <<setprecision(4) <<(float)(L1_swap_requests)/(L1_read + L1_write)<<"\n g. number of swaps:     "<<L1_swaps;
	cout<<"\n h. combined L1+VC miss rate:     "<< fixed <<setprecision(4) <<(float)(L1_readmiss + L1_writemiss - L1_swaps)/(L1_read + L1_write);
	cout<<"\n i. number writebacks from L1/VC:     "<<L1_WB;
	cout<<"\n j. number of L2 reads:     "<<L2_read<<"\n k. number of L2 read misses:     "<<L2_readmiss<<"\n l. number of L2 writes:     "<<L2_write;
	cout<<"\n m. number of L2 write misses:     "<<L2_writemiss;
	if (L2_exists)
        cout<<"\n n. L2 miss rate:     "<< fixed <<setprecision(4) <<(float)(L2_readmiss)/(L2_read);
    else
        cout<<"\n n. L2 miss rate:     0.0000";

	cout<<"\n o. number of writebacks from L2:     "<<L2_WB;

	if(L2_exists)
		cout<<"\n p. total memory traffic:     "<<L2_readmiss + L2_writemiss + L2_WB<<"\n";
	else
		cout<<"\n p. total memory traffic:     "<<L1_readmiss + L1_writemiss + L1_WB - L1_swaps<<"\n";


	return 0;
}
