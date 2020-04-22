#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

#define DICTSIZE 4096                     /* allow 4096 entries in the dict  */
#define ENTRYSIZE 32

unsigned char dict[DICTSIZE][ENTRYSIZE];  /* of 32 chars max; the first byte */
                                          /* is string length; index 0xFFF   */
                                          /* will be reserved for padding    */
                                          /* the last byte (if necessary)    */

// These are provided below
int read12(FILE *infil);
int write12(FILE *outfil, int int12);
void strip_lzw_ext(char *fname);
void flush12(FILE *outfil);
void encode(FILE *in, FILE *out, char eORd);
void decode(FILE *in, FILE *out, char dORd);
int compareArray(char A1[],char A2[],int size);
void addToDict(char w[] , int position, int size);
void clearW(char w[]);


int main(int argc, char *argv[]) {
    //error if no argument is passed
  	if(argc == 1){
    		fprintf(stderr, "\nError: No input file specified!\n");
    		exit(1);
  	}
  	
   char *filename = argv[1];
  	FILE *inputf = fopen(filename, "r");
  	
	//error if the file does not exist
  	if (inputf == NULL){
    		fprintf(stderr, "Read error: file not found or cannot be read\n");
    		exit(2);
  	}

	//error if the second argument is not given
   if(argc < 3){
      fprintf(stderr, "Invalid Usage, expected: RLE {filename} [e | d]\n"); 
      exit(4);
   }
  	char pointer = *argv[2];     
   
   char eORd;
   int i = strlen(filename);
   if(filename[i-4] == '.' && filename[i-3] == 'L' && filename[i-2] == 'Z' && filename[i-1] == 'W' ){
      eORd = 'd';
      strip_lzw_ext(filename);
   }else{
      eORd = 'e';
      strcat(filename, ".LZW");
   }    
   FILE *outputf;
   outputf = fopen(filename, "w");
 
	//error if the second argument is neither 'e' nor 'd'.
   if (pointer=='e'){
    	encode(inputf, outputf,eORd);
  	}else if (pointer=='d'){
    	decode(inputf, outputf, eORd);
	}else{
      fprintf(stderr, "Invalid Usage, expected: RLE {filename} [e | d]\n");
      exit(4);
  	}
}

/*****************************************************************************/
/* encode() performs the Lempel Ziv Welch compression from the algorithm in  */
/* the assignment specification. The strings in the dictionary have to be    */
/* handled carefully since 0 may be a valid character in a string (we can't  */
/* use the standard C string handling functions, since they will interpret   */
/* the 0 as the end of string marker). Again, writing the codes is handled   */
/* by a separate function, just so I don't have to worry about writing 12    */
/* bit numbers inside this algorithm.                                        */
void encode(FILE *in, FILE *out, char eORd) {
	if(eORd != 'e'){
		fprintf(stderr, "Error: File could not be encoded\n");
		exit(5);
	}   
   int i=0;
   int curr =256;
   while(i<curr){
      dict[i][0] = 1;
      dict[i][1] = i;
      i++;
   }
   int k = fgetc(in);
   char w[ENTRYSIZE];
   int code;
   while(k != EOF){
      w[0]++;
      w[w[0]] = k;
      int boolean = TRUE;
      for (i=0; i<DICTSIZE-1; i++){
         if(compareArray(w, dict[i], ENTRYSIZE)){
            k = fgetc(in);
            if(w[0]<ENTRYSIZE-1 && curr<DICTSIZE-1){
               w[0] ++;
               w[w[0]] = k;
            }else{
               boolean = FALSE;
            }
            code=i;
         }
      }
      write12(out, code);
      if(boolean){
			addToDict(w, curr, ENTRYSIZE-1);
			curr++;
      }
		clearW(w);
   }

   write12(out, DICTSIZE-1);
   flush12(out);
   exit(0);
}

/*****************************************************************************/
/* decode() performs the Lempel Ziv Welch decompression from the algorithm   */
/* in the assignment specification.                                          */
void decode(FILE *in, FILE *out, char eORd) {
	
	if(eORd != 'd'){
		fprintf(stderr, "Error: File could not be decoded\n");
		exit(5);
	}
   int i=0;
   int curr =256;
   while(i<curr){
      dict[i][0] = 1;
      dict[i][1] = i;
      i++;
   }

   char w[ENTRYSIZE];
   int k=read12(in);

   for(i=1; i<=dict[k][0]; i++){
      if(dict[k][i] != 0x0000){
         fputc(dict[k][i], out); 
      }
   }
   for(i=0; i<ENTRYSIZE; i++){
      w[i] = dict[k][i];
   }
   while(k!=0x0fff){
      k = read12(in);
      if(dict[k][0] != 0){
         for(i=1; i<=dict[k][0]; i++){
               fputc(dict[k][i], out);   
         }
         w[0]++;
         if(w[0]<ENTRYSIZE && curr<(DICTSIZE-1)){
            w[w[0]] = dict[k][1];
				addToDict(w, curr, w[0]);
            curr++;
         }
      }else if(k!=0x0fff && k>255){
         w[0]++;
         w[w[0]] = w[1];
         for(i=1; i<=w[0]; i++){
            fputc(w[i], out);  
         }
			addToDict(w, curr, w[0]);
			curr++;
      }

		clearW(w);  
      for(i=0; i<=dict[k][0]; i++){
			w[i] = dict[k][i];
      }
   }
   exit(0);
}

/*****************************************************************************/
/* read12() handles the complexities of reading 12 bit numbers from a file.  */
/* It is the simple counterpart of write12(). Like write12(), read12() uses  */
/* static variables. The function reads two 12 bit numbers at a time, but    */
/* only returns one of them. It stores the second in a static variable to be */
/* returned the next time read12() is called.                                */
int read12(FILE *infil){
   static int number1 = -1, number2 = -1;
   unsigned char hi8, lo4hi4, lo8;
   int retval;

   if(number2 != -1)                        /* there is a stored number from   */
      {                                     /* last call to read12() so just   */
      retval = number2;                    /* return the number without doing */
      number2 = -1;                        /* any reading                     */
      }
   else                                     /* if there is no number stored    */
      {
      if(fread(&hi8, 1, 1, infil) != 1)    /* read three bytes (2 12 bit nums)*/
         return(-1);
      if(fread(&lo4hi4, 1, 1, infil) != 1)
         return(-1);
      if(fread(&lo8, 1, 1, infil) != 1)
         return(-1);

      number1 = hi8 * 0x10;                /* move hi8 4 bits left            */
      number1 = number1 + (lo4hi4 / 0x10); /* add hi 4 bits of middle byte    */

      number2 = (lo4hi4 % 0x10) * 0x0100;  /* move lo 4 bits of middle byte   */
                                             /* 8 bits to the left              */
      number2 = number2 + lo8;             /* add lo byte                     */

      retval = number1;
      }

   return(retval);
}

/*****************************************************************************/
/* write12() handles the complexities of writing 12 bit numbers to file so I */
/* don't have to mess up the LZW algorithm. It uses "static" variables. In a */
/* C function, if a variable is declared static, it remembers its value from */
/* one call to the next. You could use global variables to do the same thing */
/* but it wouldn't be quite as clean. Here's how the function works: it has  */
/* two static integers: number1 and number2 which are set to -1 if they do   */
/* not contain a number waiting to be written. When the function is called   */
/* with an integer to write, if there are no numbers already waiting to be   */
/* written, it simply stores the number in number1 and returns. If there is  */
/* a number waiting to be written, the function writes out the number that   */
/* is waiting and the new number as two 12 bit numbers (3 bytes total).      */
int write12(FILE *outfil, int int12){
   static int number1 = -1, number2 = -1;
   unsigned char hi8, lo4hi4, lo8;
   unsigned long bignum;

   if(number1 == -1)                         /* no numbers waiting             */
   {
      number1 = int12;                      /* save the number for next time  */
      return(0);                            /* actually wrote 0 bytes         */
   }

   if(int12 == -1)                           /* flush the last number and put  */
      number2 = 0x0FFF;                      /* padding at end                 */
   else
      number2 = int12;

   bignum = number1 * 0x1000;                /* move number1 12 bits left      */
   bignum = bignum + number2;                /* put number2 in lower 12 bits   */

   hi8 = (unsigned char) (bignum / 0x10000);                     /* bits 16-23 */
   lo4hi4 = (unsigned char) ((bignum % 0x10000) / 0x0100);       /* bits  8-15 */
   lo8 = (unsigned char) (bignum % 0x0100);                      /* bits  0-7  */

   fwrite(&hi8, 1, 1, outfil);               /* write the bytes one at a time  */
   fwrite(&lo4hi4, 1, 1, outfil);
   fwrite(&lo8, 1, 1, outfil);

   number1 = -1;                             /* no bytes waiting any more      */
   number2 = -1;

   return(3);                                /* wrote 3 bytes                  */
}

/** Write out the remaining partial codes */
void flush12(FILE *outfil){
   write12(outfil, -1);                      /* -1 tells write12() to write    */
}                                          /* the number in waiting          */

/** Remove the ".LZW" extension from a filename */
void strip_lzw_ext(char *fname){
   char *end = fname + strlen(fname);

   while (end > fname && *end != '.' && *end != '\\' && *end != '/') {
      --end;
   }
   if ((end > fname && *end == '.') &&
      (*(end - 1) != '\\' && *(end - 1) != '/')) {
      *end = '\0';
   }
}


//compares two arrays and returns true if they are same
int compareArray(char A1[],char A2[],int size)	{
	int i;
	for(i=0; i<size; i++){
		if(A1[i]!=A2[i]){
			return FALSE;
		}
	}
	return TRUE;
}

void addToDict(char w[], int position, int size){
	int i;
	for(i=0; i<=size; i++){
		dict[position][i] = w[i];
	}
}

void clearW(char w[]){
	int i;
	for(i=0; i<ENTRYSIZE; i++){
		w[i] = 0;
	}     
}