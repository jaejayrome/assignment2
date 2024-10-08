//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                     Fall 2023
//
/// @file
/// @brief resursively traverse directory tree and list all entries
/// @author Jerome Goh
/// @studid 2024-81390
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>
#include <libgen.h> // to remove for normal

#define MAX_DIR 64 ///< maximum number of supported directories

/// @brief output control flags
#define F_TREE 0x1    ///< enable tree view
#define F_SUMMARY 0x2 ///< enable summary
#define F_VERBOSE 0x4 ///< turn on verbose mode

/// @brief struct holding the summary
struct summary
{
  unsigned int dirs;  ///< number of directories encountered
  unsigned int files; ///< number of files
  unsigned int links; ///< number of links
  unsigned int fifos; ///< number of pipes
  unsigned int socks; ///< number of sockets

  unsigned long long size;   ///< total size (in bytes)
  unsigned long long blocks; ///< total number of blocks (512 byte blocks)
};

/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
void panic(const char *msg)
{
  if (msg)
    fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *getNext(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do
  {
    errno = 0;
    next = readdir(dir);
    if (errno != 0)
      perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}

/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  const struct dirent *e1 = *(const struct dirent **)a;
  const struct dirent *e2 = *(const struct dirent **)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type == DT_DIR && e2->d_type != DT_DIR)
    return -1;
  if (e1->d_type != DT_DIR && e2->d_type == DT_DIR)
    return 1;

  // otherwise sorty by name
  return strcmp(e1->d_name, e2->d_name);
}


/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param pstr prefix string printed in front of each entry
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEPTH 10

/*
  @param dn: this would be the name of the current directory
  @param pstr: this would the prefix string thus far
*/
void processDir(const char *dn, const char *pstr, struct summary *stats, unsigned int flags)
{
  DIR *dir;
  struct dirent *dp;

  // Allocate enough space for the complete new path (pstr + "/" + dn)
  unsigned short oldPathLength = strlen(pstr);
  unsigned short newDirNameLength = strlen(dn);
  char *newPath = malloc(oldPathLength + newDirNameLength + 2); // pstr + "/" + dn + null terminator

  // Create the new path
  strcpy(newPath, pstr);
  if (oldPathLength > 0 && pstr[oldPathLength - 1] != '/')
  {
    strcat(newPath, "/");
  }
  strcat(newPath, dn);

  // base case: failed to open the directory at that given path
  dir = opendir(newPath);
  if (dir == NULL)
  {
    free(newPath);
    return;
  }

  // using an array containing a pointer to a struct dirent
  struct dirent **files = NULL;
  int count = 0;

  while ((dp = readdir(dir)) != NULL)
  {
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
    {
      continue;
    }
    files = realloc(files, sizeof(struct dirent *) * (count + 1));
    files[count] = malloc(sizeof(struct dirent));
    memcpy(files[count], dp, sizeof(struct dirent));
    count++;
  }

  // base case: returning if the directory happens to be empty
  if (count == 0)
  {
    free(files);
    closedir(dir);
    return;
  }

  // Sort the entries correctly
  qsort(files, count, sizeof(struct dirent *), dirent_compare);

  // ensures that there is at least one file inside
  qsort((void *)files, count, sizeof(struct dirent *), dirent_compare);

  // traverse through each file that is inside the new array
  for (int i = 0; i < count; i++)
  {
    struct dirent *dp = files[i];
    printf("%s\n", dp->d_name);

    // update the directory statistics
    switch (dp->d_type)
    {
    case DT_REG:
      stats->files++;
      stats->size += dp->d_reclen;
      break;
    case DT_DIR:
      stats->dirs++;
      break;
    case DT_LNK:
      stats->links++;
      break;
    case DT_FIFO:
      stats->fifos++;
      break;
    case DT_SOCK:
      stats->socks++;
      break;
    }
    // recursive step [DFS]: if have nested directory, explore it
    if (dp->d_type == DT_DIR && strcmp(dp->d_name, "..") != 0 && strcmp(dp->d_name, ".") != 0)
    {
      processDir(dp->d_name, newPath, stats, flags);
    }
  }

  closedir(dir);
}

/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error)
  {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-t] [-s] [-v] [-h] [path...]\n"
                  "Gather information about directory trees. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -t        print the directory tree (default if no other option specified)\n"
                  " -s        print summary of directories (total number of files, total file size, etc)\n"
                  " -v        print detailed information for each file. Turns on tree view.\n"
                  " -h        print this help\n"
                  " path...   list of space-separated paths (max %d). Default is the current directory.\n",
          basename(argv0), MAX_DIR);

  exit(EXIT_FAILURE);
}

/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int ndir = 0;

  struct summary tstat;
  unsigned int flags = 0;

  //
  // parse arguments
  //
  for (int i = 1; i < argc; i++)
  {
    if (argv[i][0] == '-')
    {
      // format: "-<flag>"
      if (!strcmp(argv[i], "-t"))
        flags |= F_TREE;
      else if (!strcmp(argv[i], "-s"))
        flags |= F_SUMMARY;
      else if (!strcmp(argv[i], "-v"))
        flags |= F_VERBOSE;
      else if (!strcmp(argv[i], "-h"))
        syntax(argv[0], NULL);
      else
        syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    }
    else
    {
      // anything else is recognized as a directory
      if (ndir < MAX_DIR)
      {
        directories[ndir++] = argv[i];
      }
      else
      {
        printf("Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
      }
    }
  }

  // if no directory was specified, use the current directory
  if (ndir == 0)
    directories[ndir++] = CURDIR;

  //
  // process each directory
  //
  // TODO
  //
  // Pseudo-code
  // - reset statistics (tstat)
  // - loop over all entries in 'directories' (number of entires stored in 'ndir')
  //   - reset statistics (dstat)
  //   - if F_SUMMARY flag set: print header
  //   - print directory name
  //   - call processDir() for the directory
  //   - if F_SUMMARY flag set: print summary & update statistics

  // resets the statistics back to 0
  memset(&tstat, 0, sizeof(tstat));
  for (int i = 0; i < ndir; i++)
  {
    struct summary dstat;
    memset(&dstat, 0, sizeof(dstat));

    if (flags & F_SUMMARY)
    {
      printf("\nDirectory: %s\n", directories[i]);
    }

    // copy the directory name into another string
    char *dirPathCopy = strdup(directories[i]);
    // remove the latest directory
    char *lastComponent = basename(dirPathCopy);
    // seperate the parent path
    char *parentPath = dirname(dirPathCopy);
    parentPath = parentPath == NULL ? "./" : parentPath;

    // first recursive call
    processDir(lastComponent, parentPath, &dstat, flags);

    // increment the total stats based on the directory stats
    tstat.files += dstat.files;
    tstat.dirs += dstat.dirs;
    tstat.links += dstat.links;
    tstat.fifos += dstat.fifos;
    tstat.socks += dstat.socks;
    tstat.size += dstat.size;
    tstat.blocks += dstat.blocks;

    // Print the directory statistics if summary flag is set
    if (flags & F_SUMMARY)
    {
      printf("  # of files:        %d\n", dstat.files);
      printf("  # of directories:  %d\n", dstat.dirs);
      printf("  # of links:        %d\n", dstat.links);
      printf("  # of pipes:        %d\n", dstat.fifos);
      printf("  # of sockets:      %d\n", dstat.socks);
      printf("  total file size:   %llu bytes\n", dstat.size);
      printf("  total blocks:      %llu\n", dstat.blocks);
    }
  }

  // Print grand total if more than one directory
  if ((flags & F_SUMMARY) && (ndir > 1))
  {
    printf("Analyzed %d directories:\n"
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of sockets:      %16d\n",
           ndir, tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks);

    if (flags & F_VERBOSE)
    {
      printf("  total file size:         %16llu\n"
             "  total # of blocks:       %16llu\n",
             tstat.size, tstat.blocks);
    }
  }

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}
