#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sfs_api.h"

/* The maximum file name length. We assume that filenames can contain
 * upper-case letters and periods ('.') characters. Feel free to
 * change this if your implementation differs.
 */
#define MAX_FNAME_LENGTH 20   /* Assume at most 20 characters (16.3) */

/* The maximum number of files to attempt to open or create.  NOTE: we
 * do not _require_ that you support this many files. This is just to
 * test the behavior of your code.
 */
#define MAX_FD 100 

/* The maximum number of bytes we'll try to write to a file. If you
 * support much shorter or larger files for some reason, feel free to
 * reduce this value.
 */
#define MAX_BYTES 30000 /* Maximum file size I'll try to create */
#define MIN_BYTES 10000         /* Minimum file size */

/* Just a random test string.
 */
static char test_str[] = "The quick brown fox jumps over the lazy dog.\n";

/* rand_name() - return a randomly-generated, but legal, file name.
 *
 * This function creates a filename of the form xxxxxxxx.xxx, where
 * each 'x' is a random upper-case letter (A-Z). Feel free to modify
 * this function if your implementation requires shorter filenames, or
 * supports longer or different file name conventions.
 * 
 * The return value is a pointer to the new string, which may be
 * released by a call to free() when you are done using the string.
 */
 
char *rand_name() 
{
  char fname[MAX_FNAME_LENGTH];
  int i;

  for (i = 0; i < MAX_FNAME_LENGTH; i++) {
    if (i != 16) {
      fname[i] = 'A' + (rand() % 26);
    }
    else {
      fname[i] = '.';
    }
  }
  fname[i] = '\0';
  return (strdup(fname));
}

/* The main testing program
 */
int
main(int argc, char **argv)
{
  int i, j, k;
  int chunksize;
  int readsize;
  char *buffer;
  char fixedbuf[1024];
  int fds[MAX_FD];
  char *names[MAX_FD];
  int filesize[MAX_FD];
  int nopen;                    /* Number of files simultaneously open */
  int ncreate;                  /* Number of files created in directory */
  int error_count = 0;
  int tmp;

  mksfs(1);                     /* Initialize the file system. */
  
}