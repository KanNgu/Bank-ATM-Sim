#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <math.h>


Bank* bank_create()
{
    Bank *bank = (Bank*) malloc(sizeof(Bank));

    if(bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&bank->rtr_addr,sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port=htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd,(struct sockaddr *)&bank->bank_addr,sizeof(bank->bank_addr));

    // Set up the protocol state
    bank->database = hash_table_create(DEFAULT_SIZE);

    return bank;
}

void bank_free(Bank *bank)
{
    if(bank != NULL)
    {
        close(bank->sockfd);
        free(bank);
        hash_table_free(bank->database);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr*) &bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}

void bank_process_local_command(Bank *bank, char *command,
                             FILE *bank_fp){
    regex_t command_regex;
    int reg_compile_code;
    char* command_regex_string = "^\\s*(create-user|deposit|balance)\\s+";

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
            bank_exec(parsed_command, command, bank->database);
        }else{
            // not one of the three valid commands
            printf("%s\n", "Invalid command");
        }
    }
    regfree(&command_regex);
}

void bank_exec(char* command, char* full_command, HashTable *bank_table){
   const char* CREATE = "create-user";
   const char* DEPOSIT = "deposit";

   // determine if the commands are well-formed
   if(!strncmp(command, CREATE, strlen(CREATE))){
       regex_t create_args_regex;
       char* create_args_string = 
            "^\\s*create-user\\s+([a-zA-Z]+)\\s+([0-9]{4})\\s+([0-9]+)\\s*$";
        int create_code;
        create_code = regcomp(&create_args_regex, create_args_string,
                              REG_EXTENDED);

        // ensure compilation was sucessful
        if (create_code){
            fprintf(stderr, "%s\n", "Regex Compilation Unsucessful");
            exit(1);
        }else{
            //execute regex to ensure well formed input
            int exec_error;
            regmatch_t create_matches[4];
            exec_error = regexec(&create_args_regex, full_command, 4,
                create_matches, 0);

            // check for well-formed command
            if(!exec_error){
                int user_start = create_matches[1].rm_so;
                int user_end = create_matches[1].rm_eo;
                int pin_start = create_matches[2].rm_so;
                int pin_end = create_matches[2].rm_eo;
                int cash_start = create_matches[3].rm_so;
                int cash_end = create_matches[3].rm_eo;
                char *user_create_arg = malloc(user_end - user_start + 1);
                char *cash_create_arg = malloc(cash_end - cash_start + 1);
                char pin_create_arg[pin_end - pin_start + 1];

                //extracting the argument username
                strncpy(user_create_arg, &full_command[user_start],
                    user_end - user_start);
                user_create_arg[user_end - user_start] = '\0';

                //extracting the argument amount
                strncpy(cash_create_arg, &full_command[cash_start],
                    cash_end - cash_start);
                cash_create_arg[cash_end - cash_start] = '\0';

                //extracting the argument pin
                strncpy(pin_create_arg, &full_command[pin_start],
                    pin_end - pin_start);
                pin_create_arg[pin_end - pin_start] = '\0';

                //ensure the username and amount are well formed
                if(strlen(user_create_arg) > MAX_USERNAME){
                    printf("%s\n",
                     "Usage: create-user <user-name> <pin> <balance>");
                }else{
                    // create the account if the accountname doesnt exist
                    if(hash_table_find(bank_table, user_create_arg) == NULL){
                        FILE *card_file;
                        char card_filename[strlen(user_create_arg) + 5 + 1];
                        char *cardInfo = malloc(270);
                        strcpy(card_filename, user_create_arg);
                        strcat(card_filename, ".card");
                        card_file = fopen(card_filename, "w");

                        //ensure patron's card was created
                        if(card_file != NULL){
                            //POSSIBLE TODO add secure card features
                            strcat(cardInfo, pin_create_arg);
                            fputs(cardInfo, card_file);
                            free(cardInfo);
                            
                            hash_table_add(bank_table, user_create_arg,
                                cash_create_arg);
                            printf("Created user %s\n", user_create_arg);
                            fclose(card_file);
                        }else{
                           printf("Error creating card file for user %s\n",
                              user_create_arg);
                        }
                    }else{
                        printf("Error: user %s already exists\n",
                           user_create_arg);
                    }
                }
            }else{
                printf("%s\n",
                 "Usage: create-user <user-name> <pin> <balance>");
            }
        }
        regfree(&create_args_regex);

    }else if(!strncmp(command, DEPOSIT, strlen(DEPOSIT))){

        regex_t deposit_args_regex;
        char* deposit_args_string 
            = "^\\s*deposit\\s+([a-zA-Z]+)\\s+([0-9]+)\\s*$";
        int deposit_code;
        deposit_code = regcomp(&deposit_args_regex, deposit_args_string,
                               REG_EXTENDED);

        // ensure compilation was sucessful
        if (deposit_code){
            fprintf(stderr, "%s\n", "Regex Compilation Unsucessful");
            exit(1);
        }else{
            //execute regex to ensure well formed input
            int exec_error;
            regmatch_t deposit_matches[3];
            exec_error = regexec(&deposit_args_regex, full_command, 3,
                deposit_matches, 0);

            // check for well-formed command
            if(!exec_error){
                int user_start = deposit_matches[1].rm_so;
                int user_end = deposit_matches[1].rm_eo;
                int cash_start = deposit_matches[2].rm_so;
                int cash_end = deposit_matches[2].rm_eo;
                char user_deposit_arg[user_end - user_start + 1];
                char cash_deposit_arg[cash_end - cash_start + 1];

                //extracting the argument username
                strncpy(user_deposit_arg, &full_command[user_start],
                    user_end - user_start);
                user_deposit_arg[user_end - user_start] = '\0';

                //extracting the argument amount
                strncpy(cash_deposit_arg, &full_command[cash_start],
                    cash_end - cash_start);
                cash_deposit_arg[cash_end - cash_start] = '\0';

                // using atoi max_int feature to limit deposit amount
                int deposit_value = atoi(cash_deposit_arg);
                char converted_int[cash_end - cash_start + 1];
                sprintf(converted_int, "%d", deposit_value);
                converted_int[cash_end - cash_start] = '\0';

                //ensure the username and amount are well formed
                if(strlen(user_deposit_arg) > MAX_USERNAME){
                    printf("%s\n", "Usage: deposit <user-name> <amt>");
                }else if(strcmp(cash_deposit_arg, converted_int)){
                    // different strings, value must have been greater than int
                    printf("%s\n", "Too rich for this program");
                }else{
                    //make sure user account exists and deposit
                    char *balance_value;
                    balance_value = hash_table_find(bank_table,
                        user_deposit_arg);
                    
                    // add the money to the account
                    if(balance_value != NULL){
                        int new_balance, number_length;
                        int current_balance = atoi(balance_value);

                        //updating the user's balance
                        new_balance = current_balance + deposit_value;
                        number_length = floor(log10(abs(new_balance))) + 1;
                        
                        char *new_balance_string = malloc(number_length + 1);
                        sprintf(new_balance_string, "%d", new_balance);
                        new_balance_string[number_length] = '\0';

                        hash_table_del(bank_table, user_deposit_arg);
                        hash_table_add(bank_table, user_deposit_arg,
                           new_balance_string);

                        printf("$%d added to %s's account\n", deposit_value,
                            user_deposit_arg);
                    }else{
                        printf("%s\n", "No such user");
                    }
                }
            }else{
                printf("%s\n", "Usage: deposit <user-name> <amt>");
            }
        }
        regfree(&deposit_args_regex);
    }else{
        //must be balance according to regex in process function
        regex_t balance_args_regex;
        char* balance_args_string = "^\\s*balance\\s+([a-zA-Z]+)\\s*$";
        int balance_code;
        balance_code = regcomp(&balance_args_regex, balance_args_string,
                               REG_EXTENDED);

        // ensure compilation was sucessful
        if (balance_code){
            fprintf(stderr, "%s\n", "Regex Compilation Unsucessful");
            exit(1);
        }else{
            //execute regex to ensure well formed input
            int exec_error;
            regmatch_t balance_matches[2];
            exec_error = regexec(&balance_args_regex, full_command, 2,
                balance_matches, 0);

            // check for well-formed command
            if(!exec_error){
                int start = balance_matches[1].rm_so;
                int end = balance_matches[1].rm_eo;
                char balance_username_arg[end - start + 1];

                //extracting the username argument
                strncpy(balance_username_arg, &full_command[start],
                    end - start);
                balance_username_arg[end - start] = '\0';

                //ensure the username is well formed
                if(strlen(balance_username_arg) > MAX_USERNAME){
                    printf("%s\n", "Usage: balance <user-name>");
                }else{
                    char *balance_value;
                    balance_value = (char*)hash_table_find(bank_table,
                        balance_username_arg);
                    
                    // grabbing balance if user has been created
                    if(balance_value != NULL){
                        printf("$%s\n", balance_value);
                    }else{
                        printf("%s\n", "No such user");
                    }
                }
            }else{
                printf("%s\n", "Usage: balance <user-name>");
            }
        }
        regfree(&balance_args_regex);
    }
}

void bank_process_remote_command(Bank *bank, char *command,
                                 size_t len, FILE *fp){

    const char *FIND = "find user";

    //given a find user command
    if (!strncmp(command, FIND, strlen(FIND))){
        regex_t command_regex;
        int reg_compile_code;
        char* command_regex_string = "find user ([a-zA-Z]+)";

        //ensure regex compilation
        reg_compile_code = regcomp(&command_regex, command_regex_string,
            REG_EXTENDED);

        if(reg_compile_code){
            // error comp code
            fprintf(stderr, "%s\n", "Regex Compilation Failed.");
            exit(1);
        }else{
            //find matches to detedrmine a command
            regmatch_t command_match[2];
            int exec_error;
            exec_error = regexec(&command_regex, command, 2, command_match, 0);

            //check whether a valid command was inputted
            if(!exec_error){
                int start = command_match[1].rm_so;
                int end = command_match[1].rm_eo;
                char *parsed_command = malloc(end - start + 1);
                char *output = malloc(150);

                //extract the command from the input
                parsed_command[end - start] = '\0';
                strncpy(parsed_command, &command[command_match[1].rm_so],
                        command_match[1].rm_eo - command_match[1].rm_so);

                output = hash_table_find(bank->database, parsed_command);

                printf("The output is %s\n", output);

                // send response back to atm
                if(output != NULL){
                    bank_send(bank, "found", 5);
                }else{
                    bank_send(bank, "not found", 9);
                }
            }
        }
    }
}
