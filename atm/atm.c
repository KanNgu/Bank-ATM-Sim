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
    atm->in_session = NO_USER;
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

        //check whether a valid command was inputted in the first place
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
        regex_t begin_regex;
        char* begin_regex_string 
            = "^\\s*begin-session\\s+([a-zA-Z]+)\\s*$";
        int begin_comp;
        begin_comp = regcomp(&begin_regex, begin_regex_string,
                               REG_EXTENDED);

        //check if reg exp compiled
        if(begin_comp){
        	fprintf(stderr, "%s\n", "Compilation Unsuccessful");
        }else{
        	//execute regex to ensure well formed input
            int exec_error;
            regmatch_t create_matches[2];
            exec_error = regexec(&begin_regex, full_command, 4,
                create_matches, 0);

            // check for well-formed command
            if(!exec_error){
                int user_start = create_matches[1].rm_so;
                int user_end = create_matches[1].rm_eo;
                char *user_create_arg = malloc(user_end - user_start + 1);

				//extracting the argument username
                strncpy(user_create_arg, &full_command[user_start],
                    user_end - user_start);
                user_create_arg[user_end - user_start] = '\0';  

                //check that a valid username is used
                if(strlen(user_create_arg) > MAX_USERNAME){
                    printf("%s\n", "Usage: begin-session <user-name>");
                }else{
                	// get the card for for this user
                	FILE *card_file;
                	char *filename = malloc(275);
     				strcat(filename, user_create_arg);
     				strcat(filename, ".card");
                	card_file = fopen(filename, "r");

                	//check that card exists
                	if (card_file != NULL){
                		char *find_command = malloc(300);
                		char *received = malloc(1000);
                		strcat(find_command, "find user ");
                		strcat(find_command, user_create_arg);
                        find_command[10 + strlen(user_create_arg)] = '\0';

                		//check that bank has record of this user
                		atm_send(atm, find_command, strlen(find_command));
                		atm_recv(atm, received, 1000);

                        // check if the bank had an account of this user
                		if(!strcmp(received, "found")){
                			printf("%s", "PIN? ");
                			char *inputted_pin = malloc(1000);
                			fgets(inputted_pin, 1000, stdin);
                			regex_t pin_regex;
	       					char* pin_regex_string = "^\\s*([0-9]{4})\\s*$";
	        				int create_code;
	        				create_code = regcomp(&pin_regex,
	        					pin_regex_string, REG_EXTENDED);

	        				// ensure compilation was sucessful
	        				if (create_code){
	            				fprintf(stderr, "%s\n", "Regex Compilation Unsucessful");
	            				exit(1);
	        				}else{
	            				//execute regex to ensure well formed input
	            				int exec_error;
	            				regmatch_t create_matches[1];
	            				exec_error = regexec(&pin_regex, inputted_pin, 4,
	                				create_matches, 0);

	            				// check for well-formed command
	            				if(!exec_error){
	                				int start = create_matches[1].rm_so;
	                				int end = create_matches[1].rm_eo;
	                				char pin_create_arg[end - end + 1];
	                				int possible_pin, pin;
	                				char *pin_cardfile = malloc(5);

	                				//extracting the argument username
	                				strncpy(pin_create_arg, &inputted_pin[start],
	                    				end - start);
	               				 	pin_create_arg[end - start] = '\0';
	               				 	possible_pin = atoi(pin_create_arg);

	               				 	//read .card file for pin
	               				 	//TODO SAFE card reading openssl??
	               				 	fgets(pin_cardfile, 5, card_file);
	               				 	pin_cardfile[4] = '\0';
	               				 	pin = atoi(pin_cardfile);

	               				 	// cbeck if correct pin has been entered
	               				 	if(pin == possible_pin){
	               				 		atm->in_session = USER;
	               				 		strncpy(atm->curr_user, user_create_arg,
	               				 			strlen(user_create_arg));
                                        atm->curr_user[strlen(user_create_arg)] = '\0';

	               				 		printf("%s\n", "Authorized");
	               				 	}else{
	               				 		printf("%s\n", "Not authorized");
	               				 	}
	               				 }else{
	               				 	printf("%s\n", "Not authorized");
	               				 }
	               			}
                		}else{
                			printf("%s\n", "No such user");
                		}
                		free(find_command);
                		free(received);
                	}else{
                		printf("%s\n", "Unable to access <user-name>'s card");
                	}
                } 
        	}else{
        		printf("%s\n", "Usage: begin-session <user-name>");
        	}
       }
   }else if(!strncmp(command, WITHDRAW, strlen(WITHDRAW))){
        regex_t withdraw_regex;
        char* withdraw_regex_string 
            = "^\\s*withdraw\\s+([0-9]+)\\s*$";
        int with_comp;
        with_comp = regcomp(&withdraw_regex, withdraw_regex_string,
                               REG_EXTENDED);

        //check if reg exp compiled
        if(with_comp){
            fprintf(stderr, "%s\n", "Compilation Unsuccessful");
        }else{
            //execute regex to ensure well formed input
            int exec_error;
            regmatch_t create_matches[2];
            exec_error = regexec(&withdraw_regex, full_command, 2,
                create_matches, 0);

            // check for well-formed command
            if(!exec_error){
                int start = create_matches[1].rm_so;
                int end = create_matches[1].rm_eo;
                char *amount_arg = malloc(end - start + 1);

                //extracting the argument username
                strncpy(amount_arg, &full_command[start],
                    end - start);
                amount_arg[end - start] = '\0'; 

                char *withdraw_command = malloc(500);
                char *received = malloc(500);
                int n;
                strcpy(withdraw_command, "withdraw ");
                strcat(withdraw_command, atm->curr_user);
                strcat(withdraw_command, " ");
                strcat(withdraw_command, amount_arg);
                withdraw_command[9 + strlen(atm->curr_user) +
                    1 + strlen(amount_arg)] = '\0';

                atm_send(atm, withdraw_command, strlen(withdraw_command));
                n = atm_recv(atm, received, 10);
                strncpy(received, received, n);
                received[n] = '\0';

                if(!strcmp(received, "-1")){
                    printf("%s\n", "Insufficient funds");
                }else{
                    printf("$%s dispensed\n", received);
                }
            }else{
                printf("%s\n", "Usage: withdraw <amt>");
            }
        }
   }else{
   	    // analyzing the balance. 8 because newline is included in raw
        if (strlen(full_command) == 8){
            char *balance_command = malloc(500);
            char *received = malloc(500);
            int n;
            strcpy(balance_command, "balance ");
            strcat(balance_command, atm->curr_user);
            balance_command[8 + strlen(atm->curr_user)] = '\0';

            //check that bank has record of this user
            atm_send(atm, balance_command, strlen(balance_command));
            n = atm_recv(atm, received, 10);

            //ensuring correct sent length message
            strncpy(received, received, n);
            received[n] = '\0';

            //print balance
            printf("$%s\n", received);

            free(received);
            free(balance_command);
        }else{
            printf("%s\n", "Usage: balance");
        }

   }
}

