#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <regex.h>
#include <math.h>

ATM* atm_create()
{
    ATM *atm = (ATM*) malloc(sizeof(ATM));
    if(atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&atm->rtr_addr,sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port=htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd,(struct sockaddr *)&atm->atm_addr,sizeof(atm->atm_addr));

    // Set up the protocol state
    // TODO set up more, as needed

    atm->in_session = 1;
    atm->curr_user = malloc(MAX_USERNAME);
    
    return atm;
}

void atm_free(ATM *atm)
{
    if(atm != NULL)
    {
        close(atm->sockfd);
        free(atm->curr_user);
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, data_len, 0,
                  (struct sockaddr*) &atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

void atm_process_command(ATM *atm, char *command){

	regex_t command_regex;
    int reg_compile_code;
    char* command_regex_string = "^\\s*(begin-session|balance|withdraw|end-session)\\s+";

    //ensure regex compilation
    reg_compile_code = regcomp(&command_regex, command_regex_string,
        REG_EXTENDED);

    if(reg_compile_code){
        // error comp code
        fprintf(stderr, "%s\n", "Regex Compilation Failed.");
        exit(1);
    }else{
        //find matches to determine a command
        regmatch_t command_match[2];
        int exec_error;
        exec_error = regexec(&command_regex, command, 2, command_match, 0);

        //check whether a valid command was inputted
        if(!exec_error){
            int start = command_match[1].rm_so;
            int end = command_match[1].rm_eo;
            char parsed_command[end - start + 1];
            
            //extract the command from the input
            parsed_command[end - start] = '\0';
            strncpy(parsed_command, &command[command_match[1].rm_so],
                    command_match[1].rm_eo - command_match[1].rm_so);

            // check whether a correct command was inputted given the current state
            if(atm->in_session == NO_USER && !strcmp(parsed_command, "begin-session")){
            	atm_exec(atm, parsed_command, command);
            }else if (atm->in_session == USER && strcmp(parsed_command, "begin-session")){
            	atm_exec(atm, parsed_command, command);
            }else{
            	//print appropriate failure message
            	if(atm->in_session == USER){
            		printf("%s\n", "A user is already logged in");
            	}else{
            		printf("%s\n", "No user logged in");
            	}
            }
        }else{
            // not one of the three valid commands
            printf("%s\n", "Invalid command");
        }
    }
    regfree(&command_regex);	
}


void atm_exec(ATM *atm, char* command, char* full_command){
   const char *BEGIN = "begin-session";
   const char *WITHDRAW = "withdraw";
   const char *END = "end-session";

   // determine if the commands are well-formed
   if(!strncmp(command, END, strlen(END))){
   		//ending the session
   		memset(atm->curr_user, 0, MAX_USERNAME);
   		atm->in_session = NO_USER;
   		printf("%s\n", "User logged out");
   }else if(!strncmp(command, BEGIN, strlen(BEGIN))){
        regex_t deposit_args_regex;
        char* deposit_args_string 
            = "^\\s*deposit\\s+([a-zA-Z]+)\\s+([0-9]+)\\s*$";
        int deposit_code;
        deposit_code = regcomp(&deposit_args_regex, deposit_args_string,
                               REG_EXTENDED);

   }else if(!strncmp(command, WITHDRAW, strlen(WITHDRAW))){
   		//this is withdraw
   }else{
   	//this is balance
   }
}

