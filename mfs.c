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

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10     // Mav shell only supports five arguments

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

int main()
{
  struct sigaction act;

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  pid_t child_pid;

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
    }
    else if(strcmp(token[0], "exit") == 0 || strcmp(token[0], "quit") == 0 )
    {
        return 0;
    }
    else if(strcmp(token[0], "close") == 0 )
    {

    }
    else if(strcmp(token[0], "info") == 0)
    {
        //• BPB_BytesPerSec
        //• BPB_SecPerClus
        //• BPB_RsvdSecCnt
        //• BPB_NumFATS
        //• BPB_FATSz32
    }
    else if(strcmp(token[0], "stat") == 0 )
    {
        //stat <filename> or <directory name>
    }
    else if(strcmp(token[0],"get") == 0)
    {
        //get <filename> 
    }
    else if(strcmp(token[0],"put") == 0)
    {
        //put <filename> 
    }
    else if(strcmp(token[0], "cd") == 0)
    {   
        //cd <directory> 

        //chdir(token[1]);
    }
    else if(strcmp(token[0],"ls") == 0)
    {
        
    }
    else if(strcmp(token[0],"read") == 0)
    {
        //read <filename> <position> <number of bytes>
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
