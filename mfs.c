/* Name: Meghan Tennant
     ID: 1000746825   */


#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10     // Mav shell only supports five arguments
#define BUFFERSIZE 4             // Max size needed for buffer
#define BYTESPERSEC_OFFSET 11    // Offset bytes for BPB_BytesPerSec
#define BYTESPERSEC_SIZE 2       // Number of bytes in BPB_BytesPerSec
#define SECPERCLUS_OFFSET 13     // Offset bytes for BPB_SecPerClus
#define SECPERCLUS_SIZE 1        // Number of bytes in BPB_SecPerClus
#define RSVDSECCNT_OFFSET 14     // Offset bytes for BPB_RsvdSecCnt
#define RSVDSECCNT_SIZE 2        // Number of bytes in BPB_RsvdSecCnt
#define NUMFATS_OFFSET 16        // Offset bytes for BPB_NumFats
#define NUMFATS_SIZE 1           // Number of bytes in BPB_NumFats
#define FATSZ32_OFFSET 36        // Offset bytes for BPB_Fatsz32
#define FATSZ32_SIZE 4           // Number of bytes in BPB_Fatsz32
#define DIRNAME_SIZE 11          // Length of directory name

struct Fat32Img
  {
    char BS_OEMName[8];
    int16_t BPB_BytesPerSec;
    int8_t BPB_SecPerClus;
    int16_t BPB_RsvdSecCnt;
    int16_t BPB_NumFATS;
    int16_t BPB_RootEntCnt;
    char BS_VolLab[11];
    int32_t BPB_FATSz32;
    int32_t BPB_RootClus;
    int32_t rootDirSectors;
    int32_t firstDataSector;
    int32_t firstSectorofClusters;
    int32_t rootDirAddress;
  };

  struct __attribute__((__packed__)) DirectoryEntry
  {
    unsigned char DIR_Name[DIRNAME_SIZE];
    uint8_t DIR_Atrr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
  };

static void handle_signal (int sig )
{

  
   //Determine which of the three signals were caught and 
   //print an appropriate message.
  
  int status;
  switch( sig )
  {
    case SIGINT:
    {
        //ignore
    } 
    break;

    case SIGTSTP: 
    {
        waitpid(-1, &status, WNOHANG);
    }
    break;

    default: 
    break;

  }

}


/*
  *Function   : getBpbBytes
  *Parameters : The file pointer, offset bytes of wanted boot sector info, and byte size.
  *Returns    : Returns the reverse bytes of the boot wanted boot sector info
  *Description: Find the wanted boot sector info and covert it to big endian
*/
static unsigned int getBpbBytes(FILE* file, int offset, int size)
{
  unsigned char buffer[BUFFERSIZE];
  int i;
  int bytes = 0;
  //get BPB_RsvdSecCnt
  fseek(file, offset, SEEK_SET);
  bytes = fread(&buffer, sizeof(uint8_t), size, file);
  //Convert from little endian to big endian
  unsigned int reverseByte =0;
  for(i=bytes-1; i>=0; i--)
  {
    reverseByte = (reverseByte << 8) |buffer[i];
  }
  return reverseByte;

}
/*
  *Function   : LBAToOffset
  *Parameters : The current fat32 struct and sector number that points to a block of data
  *Returns    : The value of the address for that block of address
  *Description: Finds the starting address of a block of data given the sector number
  *corresponding to that data block.
*/
int LBAToOffset(struct Fat32Img fat32, uint32_t sector)
{
  return ((sector - 2) * fat32.BPB_BytesPerSec) + (fat32.BPB_BytesPerSec * fat32.BPB_RsvdSecCnt) 
    +(fat32.BPB_NumFATS * fat32.BPB_FATSz32 * fat32.BPB_BytesPerSec);
}

/*
  *Function   : NextLB
  *Parameters : The current fat32 struct, fat32 img file pointer, and sector number that 
                points the current block of data.
  *Returns    : The next block of data
  *Description: Given a logical block address, look up into the first FAT and return the
                the logical block addres of the block in the file. If there is no further 
                blocks, then return -1;
*/
int16_t NextLB(FILE* file, struct Fat32Img fat32, int sector)
{
  int FATAddress = (fat32.BPB_BytesPerSec * fat32.BPB_RsvdSecCnt) + (sector * 4);
  //printf("Fataddress: &d\n", FATAddress);
  int16_t val;
  fseek(file, FATAddress, SEEK_SET);
  fread(&val, 2, 1, file);
  return val;
}

void printDir(struct DirectoryEntry dirc)
{
  char buffer[11];
  strcpy(buffer, dirc.DIR_Name);
  buffer[11] = '\0';
  printf("%s, %x\n", buffer, dirc.DIR_Atrr);
}
int main()
{
  struct sigaction act;

  struct Fat32Img fat32;
  struct DirectoryEntry dir[16];
  int main_offset;
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  FILE *file;

  int is_open = 0;   //0 = Fat32 image is not open, 1= fat32 image is open
  

  while( 1 )
  {

    //Zero out the sigaction struct so that we dont get garbage

    memset (&act, '\0', sizeof(act));
    
    
    //Set the handler to use the function handle_signal()

    act.sa_handler = &handle_signal;

    // Print out the msh prompt
    printf ("msh> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    //Parse input 
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code when done

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }

    //commands
    if(token[0] == NULL)
    {
        continue;
    }
    else if(strcmp(token[0], "open") == 0)
    {
      //open <filename> 
      if(!(file = fopen(token[1], "rb+")))
      {
        printf("Error: File System Image Not Found!\n");
        continue;
      }
      else if(is_open == 1)
      {
        printf("File System Image Is Already Open!\n");
        continue;
      }
      //Get fat32 boot sector information
      fat32.BPB_BytesPerSec = getBpbBytes(file, BYTESPERSEC_OFFSET, BYTESPERSEC_SIZE);
      fat32.BPB_RsvdSecCnt = getBpbBytes(file, RSVDSECCNT_OFFSET, RSVDSECCNT_SIZE);
      fat32.BPB_SecPerClus = getBpbBytes(file, SECPERCLUS_OFFSET, SECPERCLUS_SIZE);
      fat32.BPB_NumFATS = getBpbBytes(file, NUMFATS_OFFSET, NUMFATS_SIZE);
      fat32.BPB_FATSz32 = getBpbBytes(file, FATSZ32_OFFSET, FATSZ32_SIZE);
      // get root directory address and fill in all directory information
      fat32.rootDirAddress = (fat32.BPB_NumFATS * fat32.BPB_FATSz32 * fat32.BPB_BytesPerSec)
        + (fat32.BPB_RsvdSecCnt * fat32.BPB_BytesPerSec);

        fseek(file, fat32.rootDirAddress, SEEK_SET);
        int i;
        for(i = 0; i < 16; i++)
        {
          //32 bytes each
          fread(&dir[i], sizeof(dir[i]), 1, file);
        } 
        
        is_open = 1;
    }
    else if(strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0 )
    {
        return 0;
    }
    else if(strcmp(token[0], "close") == 0 )
    {
        fclose(file);
        is_open = 0;
    }
    else if(strcmp(token[0], "info") == 0)
    {
      if(is_open != 1)
      {
        printf("Must open FAT32 image first!\n");
        continue;
      }
      //Print boot sector info
      printf("BPB_BytesPerSec: %d\n", fat32.BPB_BytesPerSec);
      printf("BPB_BytesPerSec: %x\n\n", fat32.BPB_BytesPerSec);
      printf("BPB_SecPerClus: %d\n", fat32.BPB_SecPerClus);
      printf("BPB_SecPerClus: %x\n\n", fat32.BPB_SecPerClus);
      printf("BPB_RsvdSecCnt: %d\n", fat32.BPB_RsvdSecCnt);
      printf("BPB_RsvdSecCnt: %x\n\n", fat32.BPB_RsvdSecCnt);
      printf("BPB_NumFATS: %d\n", fat32.BPB_NumFATS);
      printf("BPB_NumFATS: %x\n\n", fat32.BPB_NumFATS);
      printf("BPB_FATSz32: %d\n", fat32.BPB_FATSz32);
      printf("BPB_FATSz32: %x\n\n", fat32.BPB_FATSz32);  
    }
    else if(strcmp(token[0], "stat") == 0 )
    {
        //stat <filename> or <directory name>
    }
    else if(strcmp(token[0],"get") == 0)
    {
      if(is_open != 1)
      {
        printf("Must open FAT32 image first!\n");
        continue;
      }

      FILE* fp = fopen(token[1], "wb");

      int found = 17;
      int i = 0;
      char expanded_name[12];
      memset( expanded_name, ' ', 12 );

      char *tmpToken = strtok( token[1], "." );

      strncpy( expanded_name, tmpToken, strlen( tmpToken ) );

      tmpToken = strtok( NULL, "." );

      if( tmpToken )
      {
        strncpy( (char*)(expanded_name+8), tmpToken, strlen(tmpToken) );
      }

      expanded_name[11] = '\0';
      
      for( i = 0; i < 11; i++ )
      {
        expanded_name[i] = toupper( expanded_name[i] );
      }
      for(i = 0; i < 16 ;i++)
      {
        char stringName[12];
        memset(stringName, 0, 12);
        strncpy(stringName, dir[i].DIR_Name, 11);
        if(strcmp(stringName, expanded_name) == 0)
        {
          found = i;
          break;
        }
      }

      if(found == 17)
      {
        printf("file not found");
        continue;
      }

      unsigned int reverseByteCluster =0;
      for(i=sizeof(dir[found].DIR_FirstClusterLow)-1; i>=0; i--)
      {
        reverseByteCluster = (reverseByteCluster << 8) |dir[found].DIR_FirstClusterLow;
      }
      unsigned int reverseByteSize =0;
      for(i=sizeof(dir[found].DIR_FileSize)-1; i>=0; i--)
      {
        reverseByteSize = (reverseByteSize << 8) |dir[found].DIR_FileSize;
      }
      uint16_t cluster = dir[found].DIR_FirstClusterLow;
      int size = dir[found].DIR_FileSize;
      int offset = LBAToOffset(fat32, cluster);
      printf("cluster: %d, size: %d, offset: %d\n", cluster, size, offset );
      fseek(file, offset, SEEK_SET);
      
      uint8_t buffer[512];
      fread(&buffer, 512, 1, file);
      fwrite(&buffer, 512, 1, fp);
      size = size - 512;

      while(size > 0)
      {
        cluster = NextLB(file, fat32, cluster);
        int addr = LBAToOffset(fat32, cluster);
        fseek(file, addr, SEEK_SET);
        fread(&buffer, 512, 1, file);
        fwrite(&buffer, 512, 1, fp);
        size = size - 512;
      }
      fclose(fp);
    }
    else if(strcmp(token[0],"put") == 0)
    {
        if(is_open != 1)
      {
        printf("Must open FAT32 image first!\n");
        continue;
      }

      FILE* fp = fopen(token[1], "rb");

      int found = 17;
      int i = 0;
      char expanded_name[12];
      memset( expanded_name, ' ', 12 );

      char *tmpToken = strtok( token[1], "." );

      strncpy( expanded_name, tmpToken, strlen( tmpToken ) );

      tmpToken = strtok( NULL, "." );

      if( tmpToken )
      {
        strncpy( (char*)(expanded_name+8), tmpToken, strlen(tmpToken) );
      }

      expanded_name[11] = '\0';
      
      for( i = 0; i < 11; i++ )
      {
        expanded_name[i] = toupper( expanded_name[i] );
      }
      for(i = 0; i < 16 ;i++)
      {
        if(dir[i].DIR_Atrr == 0X0f)
        {
          found = i;
          printf("found\n");
          break;
        }
      }

      if(found == 17)
      {
        printf("file not found");
        continue;
      }

      uint16_t cluster = dir[found].DIR_FirstClusterHigh;
      fseek(fp, 0, SEEK_END);
      int size = ftell(fp);
      fseek(fp,0,SEEK_SET);
      int offset = LBAToOffset(fat32, cluster);
      printf("cluster: %d, size: %d, offset: %d\n", cluster, size, offset );
      fseek(file, offset, SEEK_SET);
      
      uint8_t buffer[512];
      fread(&buffer, 512, 1, fp);
      fwrite(&buffer, 512, 1, file);
      size = size - 512;

      while(size > 0)
      {
        cluster = NextLB(file, fat32, cluster);
        int addr = LBAToOffset(fat32, cluster);
        fseek(file, addr, SEEK_SET);
        fread(&buffer, 512, 1, fp);
        fwrite(&buffer, 512, 1, file);
        size = size - 512;
      }

      strcpy(dir[found].DIR_Name, expanded_name);
      dir[found].DIR_FirstClusterLow = cluster;
      dir[found].DIR_FileSize = size; 
      dir[found].DIR_Atrr = 0X20;
      fclose(fp);
    }
    else if(strcmp(token[0], "cd") == 0)
    {   
      if(is_open != 1)
      {
        printf("Must open FAT32 image first!\n");
        continue;
      }
        //cd <directory> 
        int i;
        int count = 0; //number of tokens from path
        char* tokens[MAX_NUM_ARGUMENTS]; //holds the tokens from path

        char *working_token = strdup(token[1]);
        // Pointer to point to the token
        // parsed by strsep
        char *ptr;
        // Tokenize the input stringswith whitespace used as the delimiter
        while ( ( (ptr = strsep(&working_token, "/") ) != NULL) && 
                  (count<MAX_NUM_ARGUMENTS))
        {
          tokens[count] = strndup( ptr, MAX_COMMAND_SIZE );
          if( strlen( tokens[count] ) == 0 )
          {
            tokens[count] = NULL;
          }
            count++;
        }

      char expanded_name[12];

      //cd into each directory
      for(i=0; i<count; i++)
      {
        char * temptoken[strlen(tokens[i])];
        strcpy(temptoken, tokens[i]);
        int found = 17;
        int j = 0;
        
        memset( expanded_name, ' ', 12 );
        if(strcmp(temptoken, "..") == 0)
        {
          strcpy(expanded_name, "..         ");
        }
        else if(strcmp(temptoken, ".") == 0)
        {
          strcpy(expanded_name, ".          ");
        }
        else
        {
          //Make input folder match fat32 style
          char *tmpToken = strtok( tokens[i], "." );

          strncpy( expanded_name, tmpToken, strlen( tmpToken ) );

          tmpToken = strtok( NULL, "." );

          if( tmpToken )
          {
            strncpy( (char*)(expanded_name+8), tmpToken, strlen(tmpToken) );
          }

          expanded_name[11] = '\0';

          for( j = 0; j < 11; j++ )
          {
            expanded_name[j] = toupper( expanded_name[j] );
          }
        }

        for(j = 0; j < 16 ;j++)
        {
          
          char stringName[12];
          memset(stringName, 0, 12);
          strncpy(stringName, dir[j].DIR_Name, 11);
          if(strcmp(stringName, expanded_name) == 0)
          {
            found = j;
            break;
          }
        }

        if(found == 17)
        {
          printf("file not found\n");
          continue;
        }

        int offset;
        //find subdirectories
        if(dir[found].DIR_Atrr == 0x10 || dir[found].DIR_Name[0] == '.')
        {
          if(dir[found].DIR_FirstClusterLow == 0)
          {
            offset = fat32.rootDirAddress;
          }
          else
          {
            offset = LBAToOffset(fat32, dir[found].DIR_FirstClusterLow);
          }
          
          fseek(file, offset, SEEK_SET);
          int i;
          for(i = 0; i < 16; i++)
          {
            //32 bytes each
            fread(&dir[i], sizeof(dir[i]), 1, file);
          } 
          main_offset = dir[found].DIR_FirstClusterLow;

        }

       }
    }
    else if(strcmp(token[0],"ls") == 0)
    {
      int i;
      for( i = 0; i < 16; i++)
      {
        char name[sizeof(dir[i].DIR_Name) + 1];
        memcpy(name, dir[i].DIR_Name, sizeof(dir[i].DIR_Name));
        name[sizeof(dir[i].DIR_Name)] = '\0';

        printf("%s  %x  %d  %d\n", name, dir[i].DIR_Atrr, dir[i].DIR_FirstClusterLow, dir[i].DIR_FirstClusterHigh);
        // if((dir[i].DIR_Atrr == 0x01 || dir[i].DIR_Atrr == 0x10 || dir[i].DIR_Atrr == 0x20) && dir[i].DIR_Name[0] != -27)
        // {
        //   printf("%s\n", name);
        // }
      }
    }
    else if(strcmp(token[0],"read") == 0)
    {
      //read <filename> <position> <number of bytes>
      if(is_open != 1)
      {
        printf("Must open FAT32 image first!\n");
        continue;
      }

      int offset;
      int numBytes;
      int i;
      int found = 17;
      char expanded_name[12];
      if(token[1] != NULL || token[2] != NULL || token[3] != NULL)
      {
        //get file name in fat32 style
        memset( expanded_name, ' ', 12 );
        char *tmpToken = strtok( token[1], "." );
        strncpy( expanded_name, tmpToken, strlen( tmpToken ) );
        tmpToken = strtok( NULL, "." );
        if( tmpToken )
        {
          strncpy( (char*)(expanded_name+8), tmpToken, strlen(tmpToken) );
        }
        expanded_name[11] = '\0';
        for( i = 0; i < 11; i++ )
        {
          expanded_name[i] = toupper( expanded_name[i] );
        }
        for(i = 0; i < 16 ;i++)
        {
          char stringName[12];
          memset(stringName, 0, 12);
          strncpy(stringName, dir[i].DIR_Name, 11);
          if(strcmp(stringName, expanded_name) == 0)
          {
            found = i;
            break;
          }
        }
        if(found == 17)
        {
          printf("file not found");
          continue;
        }

        //get off set bytes and number of bytes to read
        offset = atoi(token[2]);
        numBytes = atoi(token[3]);
        
        //read file
        uint8_t value;
        int cluster = dir[found].DIR_FirstClusterLow;
        int offsetFile = LBAToOffset(fat32, cluster);
        fseek(file, offsetFile, SEEK_SET);

        while(offset > fat32.BPB_BytesPerSec){
          cluster = NextLB(file, fat32, cluster);
          offset -= fat32.BPB_BytesPerSec;
        }

        offsetFile = LBAToOffset(fat32, cluster);
        fseek(file, offsetFile + offset, SEEK_SET);
        int i = 0;
        for(i = 0; i < numBytes; i++){
          fread(&value, 1, 1, file);
          printf("%d ", value);
        }
        printf("\n");   
      }
    }
    
    /* 
    Install the handler for SIGINT and SIGTSTP and check the 
    return value.
  */ 
    else if (sigaction(SIGINT , &act, NULL) < 0) 
    {
        perror ("sigaction: ");
    }
    else if (sigaction(SIGTSTP , &act, NULL) < 0) 
    {

        perror ("sigaction: ");
    }
    else
    {      
        printf("%s: Command not found\n", token[0]);  
    }
    free( working_root );
  }
  return 0;
}
