/*
    Computer System - Project 2 (Password Cracker)
    Author: Jeremy Tee (856782)
    Purpose:  - crack the passwords of a simple system that has four- and
                six-character passwords. The passwords can contain any ASCII character from 32 (space) to
                126 (∼), 
              - generate smart guesses which will be determine (how smart is determine by using Kullback-Leibler       divergence) 
              - compare a file of guesses and another hashed file, print guess if found
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <memory.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include<time.h> 
#include "sha256.h"
#include "crack.h"
#include "stats.h"

const BYTE allchar[MAX_KEYWORDS] = "abcdefghijklmnopqrstuvwxyz"
                       "0123456789"
                       " ,./;'[]\\-=`<>?:\"{}|~!@#$%^&*()_+"
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

char lowercase_alphabets[MAX_LOWERCASE_ALPHABETS] = "abcdefghijklmnopqrstuvwxyz";

char numbers[MAX_NUMBERS] = "0123456789";

int main(int argc, char *argv[]){
    //generate x amount of guesses
    if (argc == 2){

        //set seed for random int generator (0 - 999)
        srand(time(NULL)); 
        int numOfGuesses = atoi(argv[1]);
        generate_password(numOfGuesses);
    
    //argv[2] = guess file, argv[3] = hashedfile
    }else if(argc == 3){
        BYTE** hashed_file;
        int num_hashes;
        num_hashes = readHashFile(argv[2], &hashed_file);
        compareAllGuesses(argv[1],hashed_file, num_hashes);
        free(hashed_file);
    
    //will not be tested (use to generate guesses for pwd4sha256/ pwd6sha256)
    }else if (argc == 1){
        BYTE **file, **file2; 
        int num_pwd4sha, num_pwd6sha;
        num_pwd4sha = readHashFile(PWD4_FILENAME, &file);
        generateFourCharPass(PWD4_GUESS_LENGTH, file, num_pwd4sha);
        free(file);
        num_pwd6sha = readHashFile(PWD6_FILENAME, &file2);
        generateSixCharPass(PWD6_GUESS_LENGTH, file2, num_pwd6sha);
        free(file2);
    }
    return 0;
}

/* use for argc == 3 
   compare all guesses from a given file, with the given hashedfile 
*/
void compareAllGuesses(char* filename, BYTE** hashed_file, int num_hashes){
    //usage: read from fgets file
    char line[100001];
    bzero(line, 100001);

    //usage: convert line to byte (unsigned char) for formula purposes
    BYTE guess[100001];
    bzero(guess, 100001);

    FILE *fp;
    fp = fopen(filename, "r");
    if(fp != NULL){
        //for own record purposes
        int numOfCorrectGuesses = 0;

        while (fgets(line, sizeof(line), fp) != NULL ){
            //length of guess (without \n character)
            int wordLen = strlen(line) - 1;
            
            //copy from line to guess (convert char to byte)
            memcpy(guess, line, wordLen);
            numOfCorrectGuesses += compareHashes(hashed_file, guess, num_hashes, wordLen);

            //flush buffer after usage
            bzero(line, 100001);
            bzero(guess, 100001);
        }
    } else {
        fprintf(stderr, "File not found");
    }
    fclose(fp);
}

/* read hashfile from filename into the buffer for storage
*/
int readHashFile(char* filename, BYTE*** buffer){
    FILE *fp;

    //to store a line
    BYTE line[SHA256_BLOCK_SIZE];
    bzero(line, SHA256_BLOCK_SIZE);

    //first read to store the total num_hashes
    fp = fopen(filename, "rb");
    int num_hashes = 0;
    if (fp != NULL){
        int read = 0;
        while((read = fread(line, SHA256_BLOCK_SIZE, 1, fp)) > 0){
            num_hashes++;
        }
    }
    fclose(fp);

    //dynamic allocation based on num_hashes
    *buffer = (BYTE**)malloc(num_hashes * sizeof(BYTE *)); 
    if (*buffer == NULL){
        fprintf(stderr, "Cannot allocate memory for hashed_file");
        exit(1);
    }
    for (int i=0; i < num_hashes; i++) {
        (*buffer)[i] = (BYTE*)malloc(SHA256_BLOCK_SIZE+1);
        if ((*buffer)[i] == NULL){
            fprintf(stderr, "Cannot allocate memory for hashed_file");
            exit(1);
        }
    }

    //second read to store hashes in buffer
    fp = fopen(filename, "rb");
    int i = 0;
    if (fp != NULL){
        int read = 0;
        while((read = fread((*buffer)[i], SHA256_BLOCK_SIZE, 1, fp)) > 0){
            i++;
        }
    }
    fclose(fp);
    return num_hashes;
}

/* compare a hashed guess with the hashed file
   if success, it will print the guess and the coresponding hashed index (start from 1)
*/
int compareHashes(BYTE** file, BYTE* guess, int num_hashes, int guess_length){
    int test;
    BYTE* hashed_guess = hashGuess(guess, guess_length);
    for(int i = 0; i < num_hashes; i++){
        test = memcmp(file[i], hashed_guess, SHA256_BLOCK_SIZE);
        if(test == 0){
            printf("%s %d\n", guess, i+1);
            free(hashed_guess);
            return 1;
        }
    }
    free(hashed_guess);
    return 0;
}

/* hash a given guess using the methods from sha256.c provided
*/
BYTE* hashGuess(BYTE* guess, int guess_length){
    BYTE* buffer = (BYTE*)malloc(SHA256_BLOCK_SIZE+1);
    SHA256_CTX ctx;
    sha256_init(&ctx);
	sha256_update(&ctx, guess, guess_length);
	sha256_final(&ctx, buffer);

    return buffer;
}

/* generate smart guess 
   1. use dictionary attack
   2. then, use smart guess
   3. if still not found, brute force it :)
*/
void generate_password(int numGuessRequired){

    //try dictionary attack using common_password.txt
    //if length < 6 append suffix to it
    int numGuessLeft = dictionaryAttack(numGuessRequired);

    //smart guesses
    generate_similar_words(&numGuessLeft);

    //perform brute force after dict attack and smart guess 
    if(numGuessLeft > 0){
        bruteForce(&numGuessLeft);
    }
}

/* try guesses from common_password.txt
*/
int dictionaryAttack(int numGuessRemaining){
    //include '\0'
    int bufferLen = SMART_GUESS_WORD_LEN + 1;

    //read common_password file
    FILE *file = fopen (COMMON_PASS_FILENAME,"r");
    if (file != NULL){
        char line [128]; 
        bzero(line, 128);
        char *buffer   = malloc(bufferLen);

        if (buffer == NULL) {
            fprintf(stderr, "Cannot allocate memory for buffer");
            exit(1);
        }
        
         //perform dictionary attack first
        while (fgets(line, sizeof(line), file) != NULL ){
            int wordLen = strlen(line);

            //slice words if length > SMART_GUESS_WORD_LEN
            if(wordLen > SMART_GUESS_WORD_LEN + 1){
                strncpy(buffer, line, SMART_GUESS_WORD_LEN);

            //if less than or equals to SMART_GUESS_WORD_LEN, copy string without '\n'
            } else { 
                strncpy(buffer, line, wordLen - 1);
            }
        
            //clear line and buffer for next word (safety purpose)
            bzero(line, 128);
            
            //if it is a 6 character guess, output and move on
            if(strlen(buffer) == 6){
                printf("%s\n", buffer);
                numGuessRemaining --;

            // string length < 6, generate suffix to string (digit padding)
            } else {
                int suffix_length =  SMART_GUESS_WORD_LEN - strlen(buffer);
                char* suffix_buff = malloc(suffix_length + 1);
                memset(suffix_buff, 0, suffix_length + 1);
                generate_suffix_and_password(suffix_buff, suffix_length, buffer, &numGuessRemaining);
                free(suffix_buff);
            }
            //reset buffer for next word
            memset(buffer, '\0', bufferLen);
            if (numGuessRemaining == 0){
                return numGuessRemaining;
            }
        }
        fclose(file);
        free(buffer);
        return numGuessRemaining;
    } else{
        perror("Common_password.txt not found");
        return 0;
    }
}

/* smart guess
   82% alphabets guesses
   11% alphanumeric guesses
   5% numeric guesses 
   symbols guesses are ignored (do during bruteforce as % is too small)
*/
void generate_similar_words(int* numGuessRemaining){

    //split numGuessRemaining to the 3 category
    int nAlpha, nAlphaNum, nNumbers;
    (*numGuessRemaining) = calculate_distribution(*numGuessRemaining, &nAlpha, &nAlphaNum, &nNumbers);

    //clear buffer for nAlpha and generate alphabetical guesses
    char charCombination[SMART_GUESS_WORD_LEN][MAX_COMBINATION_PER_CHAR];
    memset(charCombination, '\0', sizeof(charCombination));
    generate_word(nAlpha, ALPHA_PASS);

    //clear buffer for nAlphaNum and generate alphanumeric guesses
    memset(charCombination, '\0', sizeof(charCombination));
    generate_word(nAlphaNum, ALPHANUMERIC_PASS);

    //clear buffer for nNumbers and generate numeric guesses
    memset(charCombination, '\0', sizeof(charCombination));
    generate_word(nNumbers, NUMERIC_PASS);
}

/* calculate distribution of alphabetical, alphanumeric, numeric passwords
*/
int calculate_distribution(int numGuessRemaining, int* nAlpha, int* nAlphaNum, int* nNumbers){
    *nAlpha = round(numGuessRemaining * PERCENTAGE_ALPHABETS_PASSWORD);
    *nAlphaNum = round(numGuessRemaining * PERCENTAGE_ALPHANUMERIC_PASSWORD);
    *nNumbers = numGuessRemaining - *nAlpha - *nAlphaNum;

    //set maximum smart password generation
    if(*nAlpha > MAX_ALPHABETS_COMBINATIONS){
        *nAlpha = MAX_ALPHABETS_COMBINATIONS;
    }
    if(*nAlphaNum > MAX_ALPHANUMERIC_COMBINATIONS){
        *nAlphaNum = MAX_ALPHANUMERIC_COMBINATIONS;
    } 
    if(*nNumbers > MAX_NUMBERS_COMBINATIONS){
        *nNumbers = MAX_NUMBERS_COMBINATIONS;
    }   
    return numGuessRemaining - *nAlpha - *nAlphaNum - *nNumbers;
}

/* generate words based on a given type (alphanumeric, alphabets, numeric)
*/
void generate_word(int numGuessRemaining, int type){

    //buffer for the word generated
    char guess[SMART_GUESS_WORD_LEN+1];
    bzero(guess, SMART_GUESS_WORD_LEN+1);
    while(numGuessRemaining != 0 ){
        //for alphanumeric password purposes only 
        //use to validate if there is a number in the password
        int flag = 0;

        //generate alphabetical password
        if(type == ALPHA_PASS){
            for(int i = 0; i < SMART_GUESS_WORD_LEN; i++){
                //roll twice for each character, first is for which alphabet, second is whether upper or lowercase
                //percentage of these are based on statistics collected
                int roll = randomNumGenerator();
                char character = characterGenerator(roll, i, ALPHA_PASS, flag);
                guess[i] = character;
            }
            printf("%s\n", guess);

        //generate alphanumeric password
        } else if(type == ALPHANUMERIC_PASS){
            for(int i = 0; i < SMART_GUESS_WORD_LEN; i++){
                //first roll to decide whether is num/alphabet
                //if alphabet, roll to decide which alphabet, and roll again to decide if its uppercase
                //if all is alphabet, last one must be num since it is alphanumeric
                int roll = randomNumGenerator();
                char character = characterGenerator(roll, i, ALPHANUMERIC_PASS, flag);
                guess[i] = character;
                if (isdigit(character) > 0){
                    flag = 1;
                }
            }
            printf("%s\n", guess);
            
        //generate numeric passwords
        } else if(type == NUMERIC_PASS){
            for(int i = 0; i < SMART_GUESS_WORD_LEN; i++){

                //roll for each character to decide which number is used based on stats
                int roll = randomNumGenerator();
                char character = characterGenerator(roll, i, NUMERIC_PASS, flag);
                guess[i] = character;
            }
            printf("%s\n", guess);
        }
        numGuessRemaining --;
        bzero(guess, SMART_GUESS_WORD_LEN+1);
    }
}

/* generate character based on roll, index and type of word
   alphabet guess: roll once to decide which alphabet char (26 in total), roll again to decide whether is uppercase
   alphanumeric guess: roll for either numeric or alphabet char, if alphabet roll decide which char and roll 
                        to decide whether is uppercase
   numeric guess: roll once to decide which number

   all the rolls are compared to the statistics obtained to retrieve the right character
*/
char characterGenerator(int roll, int index, int type, int flag){
    char c = '\0';

    //if type is alphabets
    if(type == ALPHA_PASS){
        //select the right character
        for(int i = 0; i < MAX_LOWERCASE_ALPHABETS; i++){
            if(roll <= (alpha_char_distribution[index][i]*1000)){
                c = lowercase_alphabets[i];
                break;
            }
        }
        //check whether this character is suppose to be lowercase
        int isUpper = randomNumGenerator();
        if(isUpper < (char_upper_distribution[0]*1000)){
            return c;
        }
        return toupper(c);
    
    //type is alphanumeric
    } else if (type == ALPHANUMERIC_PASS) {
        //to decide either alphabet or number
        int roll2 = randomNumGenerator();

        //if its last character and still no number, make the last character a number
        if(index == 5 && flag == 0){
            roll2 = NUMERIC_GUESS;
        }
        //it is a character
        if(roll2 < alphaNum_char_num_distribution[0]*1000){

            //select the right character
            for(int i = 0; i < MAX_LOWERCASE_ALPHABETS; i++){
                if(roll <= (alphaNum_char_distribution[index][i]*1000)){
                    c = lowercase_alphabets[i];
                    break;
                }
            }
            //check whether this character is suppose to be lowercase
            int isUpper = randomNumGenerator();
            if(isUpper < (char_upper_distribution[0]*1000)){
                return c;
            }
            return toupper(c);
        
        //it is a number
        } else {
            for(int i = 0; i < MAX_NUMBERS; i++){
                if(roll <= (alphaNum_num_distribution[index][i]*1000)){
                    c = numbers[i];
                    break;
                }
            }
            return c;
        }
    } else if (type == NUMERIC_PASS){
        //get the right number based on roll
        for (int i = 0; i < MAX_NUMBERS; i++){
            if(roll <= (number_distribution[i]*1000)){
                c = numbers[i];
                break;
            }
        }
        return c;
    }
    return c;
}

/* generate suffix (digits) for a given prefix to match length = 6*/
void generate_suffix_and_password(char* suffix, int suffix_length, char* prefix, int* numGuessRemaining){
    //loop for suffix_length
    for(int i = 0; i < suffix_length; i++){
        //generate one character
        int roll = randomNumGenerator();
        for (int j = 0; j < MAX_NUMBERS; j++){
            if(roll <= (number_distribution[j]*1000)){
                suffix[i] = numbers[j];
                break;
            }
        }
    }
    printf("%s%s\n", prefix, suffix);
    (*numGuessRemaining)--;
    return;
}

//generate random number between 0 - 9 
//source: https://www.tutorialspoint.com/c_standard_library/c_function_rand.htm
int randomNumGenerator(){
    //selective spits out a int between 0-9 based 
    //on the distribution of numbers in common_passwords.txt
    int random = rand() % 1000;
    return random;
}

// brute force all possible combinations
void bruteForce(int* numGuessRemaining){
    int len = SMART_GUESS_WORD_LEN;
    BYTE *buffer = malloc((SMART_GUESS_WORD_LEN + 1));

    if (buffer == NULL) {
        fprintf(stderr, "Cannot allocate memory for buffer");
        exit(1);
    }

    int bufLen = len+1;
    memset(buffer, '\0', bufLen);
    for(int i = 0; i < MAX_KEYWORDS; i++){
        for(int j = 0; j < MAX_KEYWORDS; j++){
            for(int k = 0; k < MAX_KEYWORDS; k++){
                for(int l = 0; l < MAX_KEYWORDS; l++){
                    for(int m = 0; m < MAX_KEYWORDS; m++){
                        for(int n = 0; n < MAX_KEYWORDS; n++){
                            buffer[0] = allchar[i];
                            buffer[1] = allchar[j];
                            buffer[2] = allchar[k];
                            buffer[3] = allchar[l];
                            buffer[4] = allchar[m];
                            buffer[5] = allchar[n];
                            printf("%s\n", buffer);
                            (*numGuessRemaining)--;
                            if(*numGuessRemaining == 0){
                                free(buffer);
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    free(buffer);
}

//=====================       ARG0 PURPOSES ========================================

/* Brute force generating all possible 4 keyword password */
void generateFourCharPass(int maxlen, BYTE** file, int numberOfHash){
    int   len      = maxlen;
    BYTE *buffer   = malloc((maxlen + 1));

    if (buffer == NULL) {
        fprintf(stderr, "Cannot allocate memory for buffer");
        exit(1);
    }

    // This for loop generates all 1 letter patterns, then 2 letters, etc,
    // up to the given maxlen.
    // The stride is one larger than len because each line has a '\0'.
    int stride = len+1;
    int bufLen = stride;

    // Initialize buffer
    memset(buffer, '\0', bufLen);
    int numOfCorrectGuesses = 0;

    //brute force 
    for(int i = 0; i < MAX_KEYWORDS; i++){
        for(int j = 0; j < MAX_KEYWORDS; j++){
            for(int k = 0; k < MAX_KEYWORDS; k++){
                for(int l = 0; l < MAX_KEYWORDS; l++){
                    buffer[0] = allchar[i];
                    buffer[1] = allchar[j];
                    buffer[2] = allchar[k];
                    buffer[3] = allchar[l];
                    numOfCorrectGuesses += compareHashes(file, buffer, NUM_PWD4SHA256, PWD4_GUESS_LENGTH);
                    memset(buffer, '\0', bufLen);
                    if(numOfCorrectGuesses == numberOfHash){
                        free(buffer);
                        return;
                    }
                }
            }
        }
    }
    // Clean up
    free(buffer);
}

/* Brute force generating all possible 6 keyword password */
void generateSixCharPass(int maxlen, BYTE** file, int numberOfHash){
    int   len      = maxlen;
    BYTE *buffer   = malloc((maxlen + 1));

    if (buffer == NULL) {
        fprintf(stderr, "Cannot allocate memory for buffer");
        exit(1);
    }

    // This for loop generates all 1 letter patterns, then 2 letters, etc,
    // up to the given maxlen.
    // The stride is one larger than len because each line has a '\0'.
    int stride = len+1;
    int bufLen = stride;

    // get my username from the hash first
    BYTE test[7] = {'t', 'e', 'e', 'j', '1', ' '};
    int numOfCorrectGuesses = 0;
    numOfCorrectGuesses = compareHashes(file, test, NUM_PWD6SHA256, 6);

    // Initialize buffer
    memset(buffer, '\0', bufLen);

    //brute force all T.T
    for(int i = 0; i < MAX_KEYWORDS; i++){
        for(int j = 0; j < MAX_KEYWORDS; j++){
            for(int k = 0; k < MAX_KEYWORDS; k++){
                for(int l = 0; l < MAX_KEYWORDS; l++){
                    for(int m = 0; m < MAX_KEYWORDS; m++){
                        for(int n = 0; n < MAX_KEYWORDS; n++){
                            buffer[0] = allchar[i];
                            buffer[1] = allchar[j];
                            buffer[2] = allchar[k];
                            buffer[3] = allchar[l];
                            buffer[4] = allchar[m];
                            buffer[5] = allchar[n];
                            numOfCorrectGuesses += compareHashes(file, buffer, NUM_PWD6SHA256, 6);
                            memset(buffer, '\0', bufLen);
                            if(numOfCorrectGuesses == numberOfHash){
                                free(buffer);
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    // Clean up
    free(buffer);
}
