/* 
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include "atm.h"
#include <stdio.h>
#include <stdlib.h>

static const char prompt[] = "ATM: ";

int main(int argc, char**argv)
{
    char user_input[1000];
    ATM *atm = atm_create();
    FILE *atm_fp;
    atm_fp = fopen(argv[1], "r");

    // ensure .bank file can be opened
    if(atm_fp == NULL){
      printf("%s\n", "Error opening bank initialization file");
      return 64;
    }

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000,stdin) != NULL)
    {
        atm_process_command(atm, user_input);
        printf("%s", prompt);
        fflush(stdout);
    }
	return EXIT_SUCCESS;
}
