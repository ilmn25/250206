#include <stdio.h>
#include <stdbool.h>
#include <string.h>

char COMMAND[10][100];

void getInput() {
    char input[100];
    scanf(" %[^\n]", input); //get
    
    for (int i = 0; i < 10; i++) {
        COMMAND[i][0] = '\0';
    } //wipe
    
    int i = 0; //seperate by space
    char *token = strtok(input, " ");
    while (token != NULL && i < 10) {
        strcpy(COMMAND[i++], token);
        token = strtok(NULL, " ");
    }
}

bool checkArgCount(int count) {
    int i = 0;
    while (COMMAND[i][0] != '\0') i++;
    if (i == count) {
        return true;
    }
    printf("input error: wrong argument count for %s, expected %d, received %d\n", COMMAND[0], count-1, i-1);
    return false;
}

void addParking() {
    printf("addParking"); 
}

void addReservation() {
    printf("addReservation"); 
}

void bookEssentials() {
    printf("bookEssentials"); 
}

void addEvent() {
    printf("addEvent"); 
}

void importBatch() {
    printf("importBatch"); 
}

void printBookings() {
    printf("printBookings"); 
}

int main() {
    printf("~~ WELCOME TO PolyU ~~\n");
    while (true) { 
        
        printf("Please enter booking: "); 
        
        getInput();
        
        if (strcmp(COMMAND[0], "endProgram") == 0) {
            break;
        } else if (strcmp(COMMAND[0], "addParking") == 0) { 
            addParking();
        } else if (strcmp(COMMAND[0], "addReservation") == 0) { 
            addReservation();
        } else if (strcmp(COMMAND[0], "bookEssentials") == 0) { 
            bookEssentials();
        } else if (strcmp(COMMAND[0], "addEvent") == 0) { 
            addEvent();
        } else if (strcmp(COMMAND[0], "importBatch") == 0) { 
            importBatch();
        } else if (strcmp(COMMAND[0], "printBookings") == 0) { 
            printBookings();
        } else {
            printf("invalid, entered: %s\n", COMMAND[0]);
        }
    }
    
    printf("Bye!\n");
    return 0;
}