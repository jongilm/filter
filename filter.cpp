
//////////////////////////////////////////////////////////////////////////////////////////
// filter.cpp - Source code utility (All things for all people).
//
// Jonnie Gilmore 2001/10/31 - Is this utility a Trick or a Treat ? 
//                           - Time will tell!
//
//////////////////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"

#define UNIX_TO_DOS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <windows.h>
#include <winbase.h>
#include <io.h>
#include <conio.h>

#ifndef TRUE
#define TRUE  (1)
#define FALSE (0)
#endif

#ifdef WIN32
#define MSDOS 1
#endif

#if MSDOS
#define R_CNTRL   "rb"
#define W_CNTRL   "wb"
#else
#define rename(x,y) link(x,y)
#define R_CNTRL   "r"
#define W_CNTRL   "w"
#endif

#define COMMENT_BLOCK_PROBLEM 4240

// Actions
static int g_fActionSelected = 0;
static int g_fUnix2Dos = 0;
static int g_fDos2Unix = 0;
static int g_fStripCVSFields = 0;
static int g_fEmptyCVSIdFields = 0;
static int g_fStatistics = 0;
static int g_fNumberOfLinesOfCode = 0;
static int g_fRenameFileToLowercase = 0;
static int g_fStripTrailingBlanks = 0;
static int g_fStripExcessEOLatEOF = 0;
static int g_fBackupRequired = 0;

// Options
static int g_fVerbose = 0;
static int g_fRecursive = 0;
static int g_fNoBackup = 0;
static int g_fPreserveDatestamp = 0;

// Future ideas
static int g_fTouchFilestamp = 0;
static int g_fChangeString = 0;
static int g_fUntabify = 0;
static int g_fTabSize = 0;
static int g_fSetDirectoryToTimeStampOfNewestFile = 0;
static int g_fNumberOfFilesWithAllUppercaseNames = 0;
static int g_fNumberOfFilesWithTildeInTheFilename = 0;

// In Win2000, _MAX_PATH = 260

static char FilenameOfNewestFile[_MAX_PATH] = {0};
static char FilenameOfOldestFile[_MAX_PATH] = {0};
static char FilenameOfLargestFile[_MAX_PATH] = {0};
static char FilenameOfSmallestFile[_MAX_PATH] = {0};
static char FilenameOfLongestFilename[_MAX_PATH] = {0};
static char FilenameOfShortestFilename[_MAX_PATH] = {0};
static char PathnameOfDeepestPathname[_MAX_PATH] = {0};
static char FilenameOfLongestPathname[_MAX_PATH] = {0};

static FILETIME FiletimeOfNewestFile = {0,0};
static FILETIME FiletimeOfOldestFile = {0xFFFFFFFF,0xFFFFFFFF};
static DWORD SizeOfLargestFile = 0;
static DWORD SizeOfSmallestFile = 0xFFFFFFFF;
static size_t LengthOfLongestFilename = 0;
static size_t LengthOfShortestFilename = 0xFFFFFFFF;
static int DepthOfDeepestPathname = 0;
static DWORD LengthOfLongestPathname = 0;

static int NumberOfDirectoriesScanned = 0;
static int NumberOfFilesScanned = 0;
static int NumberOfFilesModified = 0;
static int NumberOfFilesBackedUp = 0;
static DWORD SizeOfAllFilesFound = 0;
static DWORD NumberOfLinesOfCode = 0;
static DWORD NumberOfLinesNonCommentedOfCode = 0;
static int FileModified = 0;

/*
For easy reference...
struct stat {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        _off_t st_size;  // _off_t = long int
        time_t st_atime;
        time_t st_mtime;
        time_t st_ctime;
        };

struct utimbuf 
{ 
   time_t actime;
   time_t modtime;
} ut_buf;

FILETIME structure:
   DWORD dwLowDateTime;
   DWORD dwHighDateTime;

WIN32_FIND_DATA structure:
   DWORD dwFileAttributes;
   FILETIME ftCreationTime;
   FILETIME ftLastAccessTime;
   FILETIME ftLastWriteTime;
   DWORD nFileSizeHigh;
   DWORD nFileSizeLow;
   DWORD dwReserved0;
   DWORD dwReserved1;
   CHAR   cFileName[ MAX_PATH ];
   CHAR   cAlternateFileName[ 14 ];
*/


static int GetFileChecksum(char *szFilename, char *szPath, unsigned int *pChecksum)
{
   FILE *in;
   int ch;
   char szFullFilename[_MAX_PATH];
   unsigned int HashTotal = 0;
   unsigned int ArithmeticTotal = 0;
   unsigned int ByteCount = 0;

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   in = fopen (szFullFilename, R_CNTRL);
   if (!in)
   {
      fprintf (stderr, "\nERROR: Cannot open %s for reading\n", szFullFilename);
      return 1;
   }

   for(;;)
   {       
      ch = getc (in);
      if (ch == EOF)
         break;
      HashTotal ^= ch;
      ArithmeticTotal += ch;
      ByteCount++;
   }   
   (*pChecksum) = HashTotal*7 + ArithmeticTotal*11 + ByteCount*13;

   if (fclose (in) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close infile\n");
      return 6;
   }

   return 0;
}

static int ConvertFormat(char *szFilename, char *szPath, int UnixToDos)
{
   FILE *in;
   FILE *out;
   int ch;
   int rc = 0;
   char temppath [_MAX_PATH];
   struct utimbuf ut_buf;
   char szFullFilename[_MAX_PATH];
   struct stat FileAttribs = {0};

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   //if (UnixToDos)
   //   printf ("Unix2Dos: Cleaning file %s ...\n", szFullFilename);
   //else
   //   printf ("Dos2Unix: Cleaning file %s ...\n", szFullFilename);

   if (stat(szFullFilename,&FileAttribs) == -1)
   {
      fprintf (stderr, "\nERROR: Can't stat '%s'\n", szFullFilename);
      return 11;
   }

   strcpy (temppath, ".");
#if defined (MSDOS)
   strcat (temppath, "\\filter");
   strcat (temppath, "XXXXXX");
   _mktemp (temppath);
#else
   strcat (temppath, "/filter");
   strcat (temppath, "XXXXXX");
   mktemp (temppath);
#endif  
   in = fopen (szFullFilename, R_CNTRL);
   if (!in)
   {
      fprintf (stderr, "\nERROR: Cannot open %s for reading\n", szFullFilename);
      return 1;
   }
   out = fopen (temppath, W_CNTRL);
   if (!out)
   {
      fprintf (stderr, "\nERROR: Cannot open tempfile %s for writing\n", temppath);
      fclose (in);
      return 2;
   }              
   
   for(;;)
   {       
      ch = getc (in);
      if (ch == EOF)
         break;
      if (UnixToDos)
      {
         // Dos wants 0x0D 0x0A
         if (ch == 0x0A)
         {
            if (putc (0x0D, out) == EOF)
            {
               fprintf (stderr, "\nERROR: Cannot write 0x0D to outfile\n");
               rc = 3;
               break;
            }
         }
         if (putc (ch, out) == EOF)
         {
            fprintf (stderr, "\nERROR: Cannot write char to outfile\n");
            rc = 4;
            break;
         }
      }
      else
      {
         if (ch != 0x0D && ch != '\032') // Strip all CR's and EOF's
         {
            if (putc (ch, out) == EOF)
            {
               fprintf (stderr, "\nERROR: Cannot write char to outfile\n");
               rc = 5;
               break;
            }
         }
      }
   }   
   
   if (fclose (in) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close infile\n");
      rc = 6;
   }
   if (fclose (out) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close outfile\n");
      rc = 7;
   }

   if (g_fPreserveDatestamp)
   {
       // Set timestamp to that of the original file
       ut_buf.actime = FileAttribs.st_atime; // Access time
       ut_buf.modtime = FileAttribs.st_mtime; // Modification time
       if (utime (temppath, &ut_buf) == -1)
       {
          fprintf (stderr, "\nERROR: Cannot set timestamp of outfile %s\n",temppath);
          rc = 8;
       }
   }

   if (unlink (szFullFilename) == -1)
   {
      fprintf (stderr, "\nERROR: Cannot delete infile %s\n",szFullFilename);
      rc = 9;
   }

   if (rc)
   {
      unlink (temppath);
      return rc;
   }
   if (rename (temppath,szFullFilename) == -1)
   {
      fprintf (stderr, "\nERROR: Problems renaming '%s' to '%s'\n", temppath, szFullFilename);
      fprintf (stderr, "         However, file '%s' remains\n", temppath);
      rc = 10;
      exit (rc);
   }
   unlink (temppath);
   return 0;
}

static void GatherStatistics(WIN32_FIND_DATA &finddata, char *pRelPath, int Depth )
{
   if (finddata.nFileSizeHigh > 0)
   {
       fprintf (stderr, "\nERROR: Filesize too big for me (%s)\n",finddata.cFileName);
       exit(1);
   }
   SizeOfAllFilesFound += finddata.nFileSizeLow;

   if (CompareFileTime(&finddata.ftLastWriteTime,&FiletimeOfNewestFile) > 0 )
   {
      FiletimeOfNewestFile = finddata.ftLastWriteTime;
      
      strcpy(FilenameOfNewestFile,pRelPath); 
      strcat(FilenameOfNewestFile,"\\"); 
      strcat(FilenameOfNewestFile,finddata.cFileName); 
   }
   if (CompareFileTime(&finddata.ftLastWriteTime,&FiletimeOfOldestFile) < 0)
   {
      FiletimeOfOldestFile = finddata.ftLastWriteTime;
      strcpy(FilenameOfOldestFile,pRelPath); 
      strcat(FilenameOfOldestFile,"\\"); 
      strcat(FilenameOfOldestFile,finddata.cFileName); 
   }
   if (finddata.nFileSizeLow > SizeOfLargestFile)
   {
      SizeOfLargestFile = finddata.nFileSizeLow;
      strcpy(FilenameOfLargestFile,pRelPath); 
      strcat(FilenameOfLargestFile,"\\"); 
      strcat(FilenameOfLargestFile,finddata.cFileName); 
   }
   if (finddata.nFileSizeLow < SizeOfSmallestFile)
   {
      SizeOfSmallestFile = finddata.nFileSizeLow;
      strcpy(FilenameOfSmallestFile,pRelPath); 
      strcat(FilenameOfSmallestFile,"\\"); 
      strcat(FilenameOfSmallestFile,finddata.cFileName); 
   }
   if (strlen(finddata.cFileName) > LengthOfLongestFilename)
   {
      LengthOfLongestFilename = strlen(finddata.cFileName);
      strcpy(FilenameOfLongestFilename,pRelPath); 
      strcat(FilenameOfLongestFilename,"\\"); 
      strcat(FilenameOfLongestFilename,finddata.cFileName); 
   }
   if (strlen(finddata.cFileName) < LengthOfShortestFilename)
   {
      LengthOfShortestFilename = strlen(finddata.cFileName);
      strcpy(FilenameOfShortestFilename,pRelPath); 
      strcat(FilenameOfShortestFilename,"\\"); 
      strcat(FilenameOfShortestFilename,finddata.cFileName); 
   }
   if (Depth > DepthOfDeepestPathname)
   {
      DepthOfDeepestPathname = Depth;
      strcpy(PathnameOfDeepestPathname,pRelPath); 
   }
   if (strlen(pRelPath)+1+strlen(finddata.cFileName) > LengthOfLongestPathname)
   {
      LengthOfLongestPathname = strlen(pRelPath)+1+strlen(finddata.cFileName);
      strcpy(FilenameOfLongestPathname,pRelPath); 
      strcat(FilenameOfLongestPathname,"\\"); 
      strcat(FilenameOfLongestPathname,finddata.cFileName); 
   }
}

static int MakeBackup(char *szFilename, char *szPath)
{
   char szFullFilename[_MAX_PATH];
   char szBackupFilename[_MAX_PATH];

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);
   strcpy(szBackupFilename, szFullFilename);
   strcat(szBackupFilename, ".bak");

   if (!CopyFile(szFullFilename,szBackupFilename,FALSE)) // Overwrite if .bak file already exists!
   {
       fprintf (stderr, "\nERROR: Unable to backup file \"%s\"\n",szBackupFilename);
       return 96;
   }
   NumberOfFilesBackedUp++;
   return 0;
}

static int PosEmbeddedInQuotes(char *pInBuffer, int InBufferSize, int i, char *szFullFilename, int LineComment)
{
    int QuotesBefore = 0;
    int QuotesAfter = 0;
    int j;

    // Look backward as far as the beginning of the line for a double-quote
    for(j=i-1;j>=0;j--)
    {
        if ((pInBuffer[j] == '\r') || (pInBuffer[j] == '\n'))
           break;
        if (pInBuffer[j] == '\"' && (j && (pInBuffer[j-1] != '\\') && (pInBuffer[j-1] != '\'')))
            QuotesBefore++;
    }

    if (QuotesBefore%2)
        return 1; // Unbalanced quotes before the start of comment
    return 0; // No quotes or balanced quotes
}

static int CheckForCommentBlock(char *pInBuffer, int InBufferSize, int i, int *pSizeOfCommentBlock, char *szFullFilename)
{
   int k;
   int LastByteOfComment = -1;
   int LineComment = 0;
   int CommentIsFromBeginningOfLine = 0;

   *pSizeOfCommentBlock = 0;

   if (pInBuffer[i] != '/')
      return 0;

   if ((pInBuffer[i+1] != '*') && (pInBuffer[i+1] != '/'))
       return 0;

   if (pInBuffer[i] == '/' && pInBuffer[i+1] == '/')
      LineComment = 1;

   if (PosEmbeddedInQuotes(pInBuffer,InBufferSize,i,szFullFilename,LineComment))
      return 0;

   if (i==0 || pInBuffer[i-1] == '\n')
      CommentIsFromBeginningOfLine = 1;

   // Look ahead for end of comment block, watching also for Id or Log fields
   for (k=i;k<InBufferSize;k++)
   {
      if (LineComment)
      {
         if ((pInBuffer[k] == '\n') || (pInBuffer[k] == '\r'))
         {
            LastByteOfComment = k-1; // Byte before the LF
            break;
         }
         if (k==InBufferSize-1) // Line comment is at the EOF with no trailing EOL
         {
            LastByteOfComment = k;
            break;
         }
      }
      else // !LineComment
      {
         if (k==InBufferSize-1) // Comment is at the EOF with no trailing EOL
         {                      // and no end of comment has been found.
            break;
         }
         if (pInBuffer[k] == '*' && pInBuffer[k+1] == '/')
         {
            LastByteOfComment = k+1;
            break;
         }
      }
   }
   if (LastByteOfComment == -1)
   {
      fprintf (stderr, "\nWARNING: Missing EndOfComment (offset %d) in %s\n", i, szFullFilename);
      LastByteOfComment = InBufferSize-1;
      //return COMMENT_BLOCK_PROBLEM;
   }

   if (CommentIsFromBeginningOfLine)
   {
      // If the very next char is LF or CRLF, then skip over these too. 
      if (pInBuffer[LastByteOfComment+1] == '\r')
         LastByteOfComment++;
      if (pInBuffer[LastByteOfComment+1] == '\n')
         LastByteOfComment++;
   }
   *pSizeOfCommentBlock = (LastByteOfComment+1) - i;
   
   return 0;
}

static int EmptyTheCVSField( char *pInBuffer, size_t InBufferSize, char *pOutBuffer, size_t &OutBufferSize, size_t &i, int &FileModified, char *szFieldName)
{
   size_t k;

   //if (pInBuffer[i] == '$' && 
   //    memicmp(&pInBuffer[i+1], szFieldName, strlen(szFieldName))==0 && 
   //    pInBuffer[i+strlen(szFieldName)+1] != '$')
   if (pInBuffer[i] == '$' && 
       memicmp(&pInBuffer[i+1], szFieldName, strlen(szFieldName))==0 && 
       pInBuffer[i+strlen(szFieldName)+1] == ':')
   {
      for (k=i+1;k<InBufferSize;k++)
      {
         if (pInBuffer[k] == '\n')
            break;
         if (pInBuffer[k] == '$')
         {
            // NextDollarFound = 1;
            pOutBuffer[OutBufferSize] = '$';
            OutBufferSize++;
            memcpy(&pOutBuffer[OutBufferSize],szFieldName,strlen(szFieldName));
            OutBufferSize += strlen(szFieldName);
            pOutBuffer[OutBufferSize] = '$';
            OutBufferSize++;
            i = k+1; // Char after trailing dollar
            FileModified = 1;
            return 1;
         }
      }
      // If trailing dollar not found, ignore this field and
      // copy to OutBuffer as normal.
   }
   return 0;
}

static int StripCVSFields(char *szFilename, char *szPath)
{
   FILE *in;
   FILE *out;
   int rc = 0;
   struct utimbuf ut_buf;
   char szFullFilename[_MAX_PATH];
   char *pInBuffer;
   size_t InBufferSize;
   char *pOutBuffer;
   size_t OutBufferSize;
   struct stat FileAttribs = {0};

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   if (stat(szFullFilename,&FileAttribs) == -1)
   {
      fprintf (stderr, "\nERROR: Can't stat '%s'\n", szFullFilename);
      return 1;
   }

   ///////////////////////////////////////////
   // Allocate space for entire file
   ///////////////////////////////////////////
   InBufferSize = FileAttribs.st_size;
   pInBuffer = (char *)malloc(InBufferSize+10); // Allow for 10 bytes of explicit look aheads (eg indexing buffer by i+1 etc)
   if (!pInBuffer)
   {
      fprintf (stderr, "\nERROR: Can't alloc 1st %ld bytes of memory for '%s'\n", InBufferSize, szFullFilename);
      return 2;
   }
   memset(pInBuffer,0,InBufferSize+10);

   pOutBuffer = (char *)malloc(InBufferSize+10); // Same size as InBuffer
   if (!pOutBuffer)
   {
      fprintf (stderr, "\nERROR: Can't alloc 2nd %ld bytes of memory for '%s'\n", InBufferSize, szFullFilename);
      free(pInBuffer);
      return 2;
   }
   memset(pOutBuffer,0,InBufferSize+10);

   ///////////////////////////////////////////
   // Read entire file into memory
   ///////////////////////////////////////////
   in = fopen (szFullFilename, R_CNTRL);
   if (!in)
   {
      fprintf (stderr, "\nERROR: Cannot open %s for reading\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 3;
   }
   if (fread(pInBuffer,1,InBufferSize,in) != InBufferSize)
   {
      fprintf (stderr, "\nERROR: Unable to read %s into buffer\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 4;
   }
   if (fclose (in) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close infile\n");
      free(pInBuffer);
      free(pOutBuffer);
      return 5;
   }

   ///////////////////////////////////////////
   // Process data
   ///////////////////////////////////////////
   size_t i;
   OutBufferSize = 0;
   for(i=0;i<InBufferSize;)
   {
      if (g_fStripCVSFields)
      {
          int SizeOfCommentBlock = 0;
          int rc;

          rc = CheckForCommentBlock(pInBuffer, InBufferSize, i, &SizeOfCommentBlock, szFullFilename); 
          if (rc)
          {
              // typically COMMENT_BLOCK_PROBLEM
              free(pInBuffer);
              free(pOutBuffer);
              return rc; 
          }
          if (SizeOfCommentBlock>0)
          {
             int CommentBlockContainsCVSField = 0;
             int k;
      
             for (k=0;k<SizeOfCommentBlock;k++)
             {
                if (pInBuffer[i+k] == '$')
                {
                   if (memicmp(&pInBuffer[i+k+1], "Id",2) || memicmp(&pInBuffer[i+k+1], "Log",3))
                   {
                      CommentBlockContainsCVSField = 1;
                      break;
                   }
                }
             }
             if (CommentBlockContainsCVSField)
             {
                 i += SizeOfCommentBlock; // Set next byte to the one emmediately after the comment block
                 FileModified = 1;
                 continue; // Loop around to the top - it may be another comment block.
             }
          }
      }
      if (g_fEmptyCVSIdFields)
      {
          // There may be an Id field embedded in the code.  eg in quotes as an ident field.
          if (EmptyTheCVSField(pInBuffer,InBufferSize,pOutBuffer,OutBufferSize,i,FileModified,"Id"))
              continue;
          if (EmptyTheCVSField(pInBuffer,InBufferSize,pOutBuffer,OutBufferSize,i,FileModified,"Revision"))
              continue;
          if (EmptyTheCVSField(pInBuffer,InBufferSize,pOutBuffer,OutBufferSize,i,FileModified,"Date"))
              continue;
          if (EmptyTheCVSField(pInBuffer,InBufferSize,pOutBuffer,OutBufferSize,i,FileModified,"Header"))
              continue;
      }
      pOutBuffer[OutBufferSize] = pInBuffer[i];
      i++;
      OutBufferSize++;
   }

   ///////////////////////////////////////////
   // Write buffer back to szFullFilename
   ///////////////////////////////////////////
   out = fopen (szFullFilename, W_CNTRL);
   if (!out)
   {
      fprintf (stderr, "\nERROR: Cannot open file %s for writing\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 6;
   }              
   if (fwrite(pOutBuffer,1,OutBufferSize,in) != OutBufferSize)
   {
      fprintf (stderr, "\nERROR: Unable to write buffer to %s\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 7;
   }
   if (fclose (out) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close outfile\n");
      free(pInBuffer);
      free(pOutBuffer);
      return 8;
   }

   if (g_fPreserveDatestamp)
   {
       // Set timestamp to that of the original file
       ut_buf.actime = FileAttribs.st_atime; // Access time
       ut_buf.modtime = FileAttribs.st_mtime; // Modification time
       if (utime (szFullFilename, &ut_buf) == -1)
       {
          fprintf (stderr, "\nERROR: Cannot set timestamp of outfile\n");
          free(pInBuffer);
          free(pOutBuffer);
          return 9;
       }
   }

   free(pInBuffer);
   free(pOutBuffer);
   return 0;
}

static int StripTrailingBlanks(char *szFilename, char *szPath)
{
   FILE *in;
   FILE *out;
   int rc = 0;
   struct utimbuf ut_buf;
   char szFullFilename[_MAX_PATH];
   char *pInBuffer;
   size_t InBufferSize;
   char *pOutBuffer;
   size_t OutBufferSize;
   struct stat FileAttribs = {0};

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   if (stat(szFullFilename,&FileAttribs) == -1)
   {
      fprintf (stderr, "\nERROR: Can't stat '%s'\n", szFullFilename);
      return 1;
   }

   ///////////////////////////////////////////
   // Allocate space for entire file
   ///////////////////////////////////////////
   InBufferSize = FileAttribs.st_size;
   pInBuffer = (char *)malloc(InBufferSize+10); // Allow for 10 bytes of explicit look aheads (eg indexing buffer by i+1 etc)
   if (!pInBuffer)
   {
      fprintf (stderr, "\nERROR: Can't alloc 1st %ld bytes of memory for '%s'\n", InBufferSize, szFullFilename);
      return 2;
   }
   memset(pInBuffer,0,InBufferSize+10);

   pOutBuffer = (char *)malloc(InBufferSize+10); // Same size as InBuffer
   if (!pOutBuffer)
   {
      fprintf (stderr, "\nERROR: Can't alloc 2nd %ld bytes of memory for '%s'\n", InBufferSize, szFullFilename);
      free(pInBuffer);
      return 2;
   }
   memset(pOutBuffer,0,InBufferSize+10);

   ///////////////////////////////////////////
   // Read entire file into memory
   ///////////////////////////////////////////
   in = fopen (szFullFilename, R_CNTRL);
   if (!in)
   {
      fprintf (stderr, "\nERROR: Cannot open %s for reading\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 3;
   }
   if (fread(pInBuffer,1,InBufferSize,in) != InBufferSize)
   {
      fprintf (stderr, "\nERROR: Unable to read %s into buffer\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 4;
   }
   if (fclose (in) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close infile\n");
      free(pInBuffer);
      free(pOutBuffer);
      return 5;
   }

   ///////////////////////////////////////////
   // Process data
   ///////////////////////////////////////////
   size_t i;
   int UnixFormat = 0;
   OutBufferSize = 0;
   // Determine if Unix or Dos file format
   for(i=0;i<InBufferSize;i++)
   {
      if (pInBuffer[i] == '\n') // If file is in dos format, we would find a CR before the LF
      {
          UnixFormat = 1;
          break;
      }
      if (pInBuffer[i] == '\r') // There shouldn't be any of these in a unix file
      {
          UnixFormat = 0;
          break;
      }
   }

   OutBufferSize = 0;
   for(i=0;i<InBufferSize;)
   {
      if (g_fStripTrailingBlanks)
      {
          if (pInBuffer[i] == '\n' || (pInBuffer[i] == '\r' && pInBuffer[i+1] == '\n'))
          {
              while (OutBufferSize && (pOutBuffer[OutBufferSize-1] == ' ' ||
                                       pOutBuffer[OutBufferSize-1] == '\t') )
              {
                  OutBufferSize--;
              }
          }
      }
      pOutBuffer[OutBufferSize] = pInBuffer[i];
      i++;
      OutBufferSize++;
   }

   // If the last line has does not have an EOL, any trailing spaces would still be there.
   while (OutBufferSize && (pOutBuffer[OutBufferSize-1] == ' ' ||
                            pOutBuffer[OutBufferSize-1] == '\t') )
   {
      OutBufferSize--;
   }

   if (g_fStripExcessEOLatEOF)
   {
       // Strip all trailing EOLs
       while (OutBufferSize && (pOutBuffer[OutBufferSize-1] == '\r' || 
                                pOutBuffer[OutBufferSize-1] == '\n'))
       {
           OutBufferSize--;
       }

       if (OutBufferSize) // If at least 1 byte in the file...
       {
           // Add one (and only one) EOL to EOF
           if (!UnixFormat)
           {
               pOutBuffer[OutBufferSize] = '\r';
               OutBufferSize++;
           }
           pOutBuffer[OutBufferSize] = '\n';
           OutBufferSize++;
       }
   }

   ///////////////////////////////////////////
   // Write buffer back to szFullFilename
   ///////////////////////////////////////////
   out = fopen (szFullFilename, W_CNTRL);
   if (!out)
   {
      fprintf (stderr, "\nERROR: Cannot open file %s for writing\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 6;
   }              
   if (fwrite(pOutBuffer,1,OutBufferSize,in) != OutBufferSize)
   {
      fprintf (stderr, "\nERROR: Unable to write buffer to %s\n", szFullFilename);
      free(pInBuffer);
      free(pOutBuffer);
      return 7;
   }
   if (fclose (out) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close outfile\n");
      free(pInBuffer);
      free(pOutBuffer);
      return 8;
   }

   if (g_fPreserveDatestamp)
   {
       // Set timestamp to that of the original file
       ut_buf.actime = FileAttribs.st_atime; // Access time
       ut_buf.modtime = FileAttribs.st_mtime; // Modification time
       if (utime (szFullFilename, &ut_buf) == -1)
       {
          fprintf (stderr, "\nERROR: Cannot set timestamp of outfile\n");
          free(pInBuffer);
          free(pOutBuffer);
          return 9;
       }
   }

   free(pInBuffer);
   free(pOutBuffer);
   return 0;
}

static int GetNumberOfLinesOfCode(char *szFilename, char *szPath, int *pLOC, int *pNCNB_LOC)
{
   FILE *in;
   int rc = 0;
   char szFullFilename[_MAX_PATH];
   char *pInBuffer;
   size_t InBufferSize;
   struct stat FileAttribs = {0};

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   if (stat(szFullFilename,&FileAttribs) == -1)
   {
      fprintf (stderr, "\nERROR: Can't stat '%s'\n", szFullFilename);
      return 1;
   }

   ///////////////////////////////////////////
   // Allocate space for entire file
   ///////////////////////////////////////////
   InBufferSize = FileAttribs.st_size;
   pInBuffer = (char *)malloc(InBufferSize+10); // Allow for 10 bytes of explicit look aheads (eg indexing buffer by i+1 etc)
   if (!pInBuffer)
   {
      fprintf (stderr, "\nERROR: Can't alloc 1st %ld bytes of memory for '%s'\n", InBufferSize, szFullFilename);
      return 2;
   }
   memset(pInBuffer,0,InBufferSize+10);

   ///////////////////////////////////////////
   // Read entire file into memory
   ///////////////////////////////////////////
   in = fopen (szFullFilename, R_CNTRL);
   if (!in)
   {
      fprintf (stderr, "\nERROR: Cannot open %s for reading\n", szFullFilename);
      free(pInBuffer);
      return 3;
   }
   if (fread(pInBuffer,1,InBufferSize,in) != InBufferSize)
   {
      fprintf (stderr, "\nERROR: Unable to read %s into buffer\n", szFullFilename);
      free(pInBuffer);
      return 4;
   }
   if (fclose (in) == EOF)
   {
      fprintf (stderr, "\nERROR: Cannot close infile\n");
      free(pInBuffer);
      return 5;
   }

   ///////////////////////////////////////////
   // Process data
   ///////////////////////////////////////////
   size_t i;
   for(i=0;i<InBufferSize;)
   {
      int SizeOfCommentBlock = 0;
      int rc;

      rc = CheckForCommentBlock(pInBuffer, InBufferSize, i, &SizeOfCommentBlock, szFullFilename); 
      if (rc)
      {
          // typically COMMENT_BLOCK_PROBLEM
          free(pInBuffer);
          return rc;
      }
      if (SizeOfCommentBlock>0)
      {
         int k;
      
         for (k=0;k<SizeOfCommentBlock;k++)
         {
            if (pInBuffer[i+k] == '\n')
               (*pLOC)++; // i.e. NumberOfLinesOfCode++;
         }
      
         i += SizeOfCommentBlock; // Set next byte to the one emmediately after the comment block
         continue; // Loop around to the top - it may be another comment block.
      }
      if (pInBuffer[i] == '\n')
      {
         (*pLOC)++; // i.e. NumberOfLinesOfCode++;
      
         if (i>0 && pInBuffer[i-1] == '\r') // Dos format
         {
            if (i<2 || pInBuffer[i-2] != '\n') // Check for empty line
               (*pNCNB_LOC)++; // NumberOfLinesNonCommentedOfCode++;
         }
         else
         {
            if (i<1 || pInBuffer[i-1] != '\n') // Check for empty line
               (*pNCNB_LOC)++; // NumberOfLinesNonCommentedOfCode++;
         }
      }
      i++;
   }
   free(pInBuffer);
   return rc;
}

static int RenameFileToLowercase (char *szFilename, char *szPath)
{
   char szFullFilename[_MAX_PATH];
   char szNewFilename[_MAX_PATH];
   char szNewFullFilename[_MAX_PATH];

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   strcpy(szNewFilename,szFilename);
   strupr(szNewFilename);
   if (strcmp(szFilename,szNewFilename)==0) // Filename is all UPPERCASE
   {
      strlwr(szNewFilename);
      if (strcmp(szFilename,szNewFilename)!=0) // Filename is not all lowercase
      {
         strcpy(szNewFullFilename, szPath);
         if (szNewFullFilename[strlen(szNewFullFilename)-1] != '\\')
            strcat(szNewFullFilename, "\\");
         strcat(szNewFullFilename, szNewFilename);

         printf(" Rename \"%s\" to \"%s\"\n",szFullFilename,szNewFilename);

         //sprintf(cmd,"ren \"%s\" \"%s\"",szFullFilename,szNewFullFilename);
         //system(cmd);
         if (rename (szFullFilename,szNewFullFilename) == -1)
         {
            fprintf (stderr, "\nERROR: Problems renaming '%s' to '%s'\n", szFullFilename, szNewFullFilename);
            return 89;
         }
         FileModified = 1;
      }
   }
   return 0;
}

static int ProcessThisFile(char *szFilename, char *szPath, int *pFilesProcessed)
{
   int rc;
   char szFullFilename[_MAX_PATH];
   unsigned int Checksum1;
   unsigned int Checksum2;

   FileModified = 0;

   strcpy(szFullFilename, szPath);
   if (szFullFilename[strlen(szFullFilename)-1] != '\\')
      strcat(szFullFilename, "\\");
   strcat(szFullFilename, szFilename);

   if (g_fBackupRequired && !g_fNoBackup) 
   {
      if (g_fVerbose)
         printf("File%5d: %s\\%s [Backed up]\n", *pFilesProcessed, szPath, szFilename );
      rc = MakeBackup(szFilename,szPath);
      if (rc != 0)
         return rc;
   }

   if (g_fUnix2Dos || g_fDos2Unix)
   {
      rc = GetFileChecksum(szFilename,szPath,&Checksum1);
      if (rc != 0)
         return rc;
      // Doing both of these (regardless of the value of FinalFileInUnixFormat) 
      // guarrantees that the file will be in a known format.
      rc = ConvertFormat(szFilename,szPath,FALSE); // ConvertToUnixFormat
      if (rc != 0)
         return rc;
      rc = ConvertFormat(szFilename,szPath,TRUE); // ConvertToDosFormat
      if (rc != 0)
         return rc;
   }

   if (g_fStripTrailingBlanks || g_fStripExcessEOLatEOF)
   {
      rc = GetFileChecksum(szFilename,szPath,&Checksum1);
      if (rc != 0)
         return rc;
      rc = StripTrailingBlanks(szFilename,szPath);
      if (rc != 0)
         return rc;
   }

   ////////////////////////////////////
   // Do other file processing
   ////////////////////////////////////
   if (g_fStripCVSFields || g_fEmptyCVSIdFields)
   {
      rc = StripCVSFields(szFilename,szPath);
      if (rc != 0)
      {
         if (rc==COMMENT_BLOCK_PROBLEM)
            fprintf (stderr, "[Continuing]\n");
         else
            return rc;
      }
   }

   if (g_fNumberOfLinesOfCode)
   {
      int LOC = 0;
      int NCNB_LOC = 0;
      rc = GetNumberOfLinesOfCode(szFilename,szPath,&LOC,&NCNB_LOC);
      if (rc != 0)
      {
         if (rc==COMMENT_BLOCK_PROBLEM)
            fprintf (stderr, "[Continuing]\n");
         else
            return rc;
      }
      NumberOfLinesOfCode += LOC;
      NumberOfLinesNonCommentedOfCode += NCNB_LOC;
      if (g_fVerbose)
         printf("File%5d:[LOC=%6d,NCNB-LOC=%6d] %s\\%s\n", *pFilesProcessed, LOC, NCNB_LOC, szPath, szFilename);

   }

   if (g_fRenameFileToLowercase)
   {
      rc = RenameFileToLowercase(szFilename,szPath);
      if (rc != 0)
         return rc;
   }

   if (g_fDos2Unix)
   {
      rc = ConvertFormat(szFilename,szPath,FALSE); // ConvertToUnixFormat
      if (rc != 0)
         return rc;
   }
   if (g_fUnix2Dos || g_fDos2Unix || g_fStripTrailingBlanks || g_fStripExcessEOLatEOF)
   {
      rc = GetFileChecksum(szFilename,szPath,&Checksum2);
      if (rc != 0)
         return rc;
      if (Checksum1 != Checksum2)
          FileModified = 1;
   }
   if (FileModified)
   {
      NumberOfFilesModified++;

      if (g_fVerbose)
      {
         printf("File%5d: %s\\%s [Modified: ", *pFilesProcessed, szPath, szFilename );
         if (g_fUnix2Dos)
            printf ("Unix2dos ");
         if (g_fDos2Unix)
            printf ("Dos2unix ");
         if (g_fRenameFileToLowercase)
            printf ("Ren2Lwr ");
         if (g_fStripCVSFields)
            printf ("StripCVS ");
         if (g_fEmptyCVSIdFields)
            printf ("EmptyCVSIds ");
          printf("]\n");
      }
   }
   if (!g_fVerbose)
      printf("\b\b\b\b\b%5d",*pFilesProcessed);
   return 0;
}

static void GetPathAndFileSpec(char *pStartDir, char *szDestPath, char *szDestFilespec )
{
   HANDLE hFind;
   WIN32_FIND_DATA finddata;
   int IsFolder = 0;
   int WildcardsSupplied = 0;
   size_t i;
   int PositionOfLastSeperator = -1;

   if (strchr(pStartDir,'*') || strchr(pStartDir,'?'))
       WildcardsSupplied = 1;
   else
   {
       if (pStartDir[strlen(pStartDir)-1] == '\\')
           IsFolder = 1; // "xxx\"
       if (strlen(pStartDir)==1 && pStartDir[0] == '.')
           IsFolder = 1; // "."
       if (strlen(pStartDir)>=2 && pStartDir[strlen(pStartDir)-2] == '\\' && pStartDir[strlen(pStartDir)-1] == '.')
           IsFolder = 1; // "xxx\."
       else if (strlen(pStartDir)>=2 && pStartDir[strlen(pStartDir)-2] == '.' && pStartDir[strlen(pStartDir)-1] == '.')
           IsFolder = 1; // ".." or "xxx\.."
       else
       {
          hFind = FindFirstFile(pStartDir, &finddata);
          if (hFind != (HANDLE)-1)
             IsFolder = (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
          FindClose(hFind);
       }
   }

   if (IsFolder)
   {
      strcpy(szDestPath,pStartDir);
      strcpy(szDestFilespec,"*.*");
   }
   else 
   {
      // Either is a discrete filename or has wildcard supplied
      for (i=0;i<strlen(pStartDir);i++)
      {
          if (pStartDir[i] == '\\')
              PositionOfLastSeperator = i;
      }
      if (PositionOfLastSeperator == -1)
      {
          // No seperators in pStartDir
          // It must be a discrete filename
          strcpy(szDestPath,".");
          strcpy(szDestFilespec,pStartDir);
      }
      else
      {
          // There is at least one path seperator.
          // We must assume that the sub-string following
          // the last seperator is a filename/filespec.
          strcpy(szDestPath,pStartDir);
          szDestPath[PositionOfLastSeperator] = 0;
          strcpy(szDestFilespec,&pStartDir[PositionOfLastSeperator+1]);
      }
   }
   if (g_fVerbose)
   {
       printf("Path     = \"%s\"\n",szDestPath);
       printf("Filespec = \"%s\"\n",szDestFilespec);
   }
}

typedef struct tDIRENTRY
{
   char StartDir[_MAX_PATH];
   char Filespec[_MAX_PATH];
   WIN32_FIND_DATA finddata;
} tDIRENTRY;
typedef struct tLISTENTRY
{
   tLISTENTRY *pPrev;
   tLISTENTRY *pNext;
   tDIRENTRY Data;
} tLISTENTRY;

static tLISTENTRY *pFirstEntry = NULL;
static tLISTENTRY *pLastEntry = NULL;

static tLISTENTRY *CreateListEntry(tLISTENTRY *pLastEntry,char *StartDir,char *Filespec,WIN32_FIND_DATA *pfinddata)
{
   tLISTENTRY *pNewEntry;
   pNewEntry = (tLISTENTRY *)malloc(sizeof(tLISTENTRY));
   if (!pNewEntry)
   {
      fprintf (stderr, "\nERROR: Unable to alloc memory for list entry\n");
      return NULL;
   }
   memset(pNewEntry,0,sizeof(tLISTENTRY));
   pNewEntry->pPrev = pLastEntry;
   pNewEntry->pNext = 0;
   if (pLastEntry)
       pLastEntry->pNext = pNewEntry;
   if (StartDir)
       strcpy(pNewEntry->Data.StartDir,StartDir);
   if (Filespec)
       strcpy(pNewEntry->Data.Filespec,Filespec);
   if (pfinddata)
       memcpy(&pNewEntry->Data.finddata,pfinddata,sizeof(WIN32_FIND_DATA));
   return pNewEntry;
}

static int dir_scan(char *pStartDir, char *pFilespec, BOOL bRecurse, int *pDepth, int *pFilesFound )
/////////////////////////////////////////////////////////////////
// Scan the directory given. If bRecurse is true, go on to
// recursive call for each of the child directories.
/////////////////////////////////////////////////////////////////
{
   char path[_MAX_PATH];
   BOOL bMore;
   BOOL bIsDir;
   HANDLE hFind;
   WIN32_FIND_DATA finddata;
   int rc;

   if (!pFirstEntry)
   {
      pFirstEntry = CreateListEntry(NULL, NULL, NULL, NULL);
      pLastEntry = pFirstEntry;
   }
   ////////////////////////////////////////////
   // Process specified files in current directory
   ////////////////////////////////////////////
   strcpy(path, pStartDir);
   if (path[strlen(path)-1] != '\\')
      strcat(path, "\\");
   strcat(path, pFilespec);

   //if (g_fVerbose)
   //   printf("Scanning directory: %s\n", path );

   hFind = FindFirstFile(path, &finddata);
   if (hFind == (HANDLE)-1)
      bMore = 0;
   else
      bMore = 1;

   while (bMore)
   {
      bIsDir = (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
      if (!bIsDir)
      {
         NumberOfFilesScanned++;
         (*pFilesFound)++;
         //if (g_fVerbose)
         //   printf("Found file: %s\\%s\n", pStartDir, finddata.cFileName );
         if (g_fStatistics)
            GatherStatistics(finddata,pStartDir,*pDepth);
         pLastEntry = CreateListEntry(pLastEntry, pStartDir, pFilespec, &finddata);
         if (!pLastEntry)
             return 97;
      }
      bMore = FindNextFile(hFind, &finddata);
   }
   FindClose(hFind);

   ////////////////////////////////////////////
   // Process all directories in current directory
   ////////////////////////////////////////////
   strcpy(path, pStartDir);
   if (path[strlen(path)-1] != '\\')
      strcat(path, "\\");
   strcat(path, "*.*");

   hFind = FindFirstFile(path, &finddata);
   if (hFind == (HANDLE)-1)
      bMore = 0;
   else
      bMore = 1;

   while (bMore) 
   {
      bIsDir = (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
      if (bIsDir) 
      {
         if (strcmp(finddata.cFileName, ".") == 0) 
            NumberOfDirectoriesScanned++;
         if ( (strcmp(finddata.cFileName, ".") != 0) && ( strcmp(finddata.cFileName, "..") != 0) ) 
         {
             if (bRecurse) 
             {
                char NewPath[_MAX_PATH];

                strcpy(NewPath,pStartDir);
                if (NewPath[0])
                   if (NewPath[strlen(NewPath)-1] != '\\')
                      strcat(NewPath, "\\");
                strcat(NewPath,finddata.cFileName);

                (*pDepth)++;
                rc = dir_scan(NewPath, pFilespec, bRecurse, pDepth, pFilesFound);
                (*pDepth)--;
                if (rc != 0)
                   return rc;
             }
         }
      }
      bMore = FindNextFile(hFind, &finddata);
   }

   FindClose(hFind);
   return 0;
}

static int ProcessFileList(int *pFilesProcessed)
{
   int rc;
   tLISTENTRY *pCurrentEntry;

   if (!pFirstEntry)
   {
       fprintf (stderr, "\nERROR: List non-existant\n");
       return 21;
   }
   pCurrentEntry = pFirstEntry; // Remembering that the first entry has no data
   *pFilesProcessed = 0;

   for(;;)
   {
       if (!pCurrentEntry->pNext)
           break;
       pCurrentEntry = pCurrentEntry->pNext;
       (*pFilesProcessed)++;
       rc = ProcessThisFile(pCurrentEntry->Data.finddata.cFileName,pCurrentEntry->Data.StartDir,pFilesProcessed);
       if (rc != 0)
          return rc;
   }
   return 0;
}

static int FreeFileList(void)
{
   tLISTENTRY *pCurrentEntry;
   int i;

   if (!pFirstEntry)
   {
       fprintf (stderr, "\nERROR: List non-existant\n");
       return 22;
   }
   pCurrentEntry = pFirstEntry;

   // Go to the end
   for(i=0;;i++)
   {
       if (!pCurrentEntry->pNext)
           break;
       pCurrentEntry = pCurrentEntry->pNext;
   }
   // Free from bottom up, including pFirstEntry
   for(i=0;;i++)
   {
       tLISTENTRY *pPrev;
       pPrev = pCurrentEntry->pPrev;
       free(pCurrentEntry);
       pCurrentEntry = pPrev;
       if (!pCurrentEntry)
           break;
   }
   pFirstEntry = 0;
   return 0;
}

static void ShowHelp(void)
{
   printf("Usage: FILTER [-actions] [-options] filespec1 [filespec2 [filespec3...]]\n");
   printf("Actions:\n");
   printf(" -?        (or -Help)        This help\n");
   printf(" -dos      (or -Unix2dos)    Convert to DOS format\n");
   printf(" -unix     (or -Dos2unix)    Convert to Unix format\n");
   printf(" -cvs      (or -StripCVS)    Remove CVS Id and Log comment blocks,\n");
   printf("                             also empty out embedded Id fields (i.e. -cvsid implied).\n");
   printf(" -cvsids   (or -EmptyCVSIds) Empty out all the single line CVS fields (ie. Id,Date,Revision,Header)\n");
   printf("                             (e.g. \"$I" "d:xxxx $\" to \"$I" "d$\")\n"); // broken string to preventy CVS itself drom detecting the field
   printf(" -blanks   (or -StripBlanks) Strip trailing blanks and ensure single EOL at EOF\n");
   printf(" -loc                        Count the number of lines of code\n");
   printf(" -ren2lwr                    Rename all-uppercase filenames to lowercase\n");
   printf(" -stats    (or -Statistics)  Display directory statistics (default)\n");
   printf("Options:\n");
   printf(" -nobak    (or -NoBackup)    Do not make .bak file\n");
   printf(" -r        (or -Recurse)     Operate recursively\n");
   printf(" -v        (or -Verbose)     Display progress\n");
   printf(" -date     (or -PreserveDates) Preserve the DateTime stamp of the original file\n");
   printf("(Switches are not case sensitive)\n");
}

static char *FormatFileTime(LPFILETIME pFiledate, char *szDest, int iDestSize)
{
    SYSTEMTIME SystemTime;
    char *DayOfWeek[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    if (!szDest || iDestSize<24)
        return NULL;

    if ((pFiledate->dwLowDateTime == 0          && pFiledate->dwHighDateTime == 0         ) ||
        (pFiledate->dwLowDateTime == 0xFFFFFFFF && pFiledate->dwHighDateTime == 0xFFFFFFFF))
    {
        strcpy(szDest,"(Null)");
        return szDest;
    }

    if (!FileTimeToSystemTime(pFiledate,&SystemTime))
        return NULL;
    sprintf(szDest,"%s %4.4u-%2.2u-%2.2u %2.2u:%2.2u:%2.2u", // 23 bytes + NULL = 24 bytes
                      DayOfWeek[SystemTime.wDayOfWeek],
                      SystemTime.wYear,
                      SystemTime.wMonth,
                      SystemTime.wDay,
                      SystemTime.wHour,
                      SystemTime.wMinute,
                      SystemTime.wSecond );
    return szDest;
}

int main(int argc, char* argv[])
{
   int rc;
   int i;
   char szPath[_MAX_PATH];
   char szFilespec[_MAX_PATH];
   int NumberOfPathArgs = 0;

   for (i=1;i<argc;i++)
   {
      if (argv[i][0] == '-' || argv[i][0] == '/')
      {
         ///////////////////////
         // Actions
         ///////////////////////
         if      (stricmp(&argv[i][1],"?")==0 || 
                  stricmp(&argv[i][1],"help")==0) 
         {
            ShowHelp();
            exit(0);
         }
         else if (stricmp(&argv[i][1],"dos")==0 ||
                  stricmp(&argv[i][1],"unix2dos")==0)
         {
            g_fUnix2Dos = 1;
            g_fActionSelected = 1;
            g_fBackupRequired = 1;
         }
         else if (stricmp(&argv[i][1],"unix")==0 ||
                  stricmp(&argv[i][1],"dos2unix")==0)
         {
            g_fDos2Unix = 1;
            g_fActionSelected = 1;
            g_fBackupRequired = 1;
         }
         else if (stricmp(&argv[i][1],"cvs")==0 ||
                  stricmp(&argv[i][1],"stripcvs")==0) 
         {
            g_fStripCVSFields = 1;
            g_fEmptyCVSIdFields = 1;
            g_fActionSelected = 1;
            g_fBackupRequired = 1;
         }
         else if (stricmp(&argv[i][1],"cvsid")==0 ||
                  stricmp(&argv[i][1],"cvsids")==0 ||
                  stricmp(&argv[i][1],"emptycvsids")==0) 
         {
            g_fEmptyCVSIdFields = 1;
            g_fActionSelected = 1;
            g_fBackupRequired = 1;
         }
         else if (stricmp(&argv[i][1],"blanks")==0 ||
                  stricmp(&argv[i][1],"stripblanks")==0) 
         {
            g_fStripTrailingBlanks = 1;
            g_fStripExcessEOLatEOF = 1;
            g_fActionSelected = 1;
            g_fBackupRequired = 1;
         }
         else if (stricmp(&argv[i][1],"stat")==0 ||
                  stricmp(&argv[i][1],"stats")==0 ||
                  stricmp(&argv[i][1],"statistics")==0)
         {
            g_fStatistics = 1;
            g_fActionSelected = 1;
         }
         else if (stricmp(&argv[i][1],"loc")==0)
         {
            g_fNumberOfLinesOfCode = 1;
            g_fActionSelected = 1;
         }
         else if (stricmp(&argv[i][1],"ren2lwr")==0)
         {
            g_fRenameFileToLowercase = 1;
            g_fActionSelected = 1;
         }
         
         ///////////////////////
         // Options
         ///////////////////////
         else if (stricmp(&argv[i][1],"nobak")==0 ||
                  stricmp(&argv[i][1],"noback")==0 ||
                  stricmp(&argv[i][1],"nobackup")==0)
         {
            g_fNoBackup = 1;
         }
         else if (stricmp(&argv[i][1],"v")==0 ||
                  stricmp(&argv[i][1],"verbose")==0)
         {
            g_fVerbose = 1;
         }
         else if (stricmp(&argv[i][1],"r")==0 ||
                  stricmp(&argv[i][1],"recurse")==0)
         {
            g_fRecursive = 1;
         }
         else if (stricmp(&argv[i][1],"date")==0 ||
                  stricmp(&argv[i][1],"preservedate")==0)
         {
            g_fPreserveDatestamp = 1;
         }
      }
   }

   if (!g_fActionSelected)
   {
       //g_fStatistics = 1; // Default action
       //g_fActionSelected = 1;
       return 0;
   }

   /////////////////////////////////
   // Generate file list
   /////////////////////////////////
   for (i=1;i<argc;i++)
   {
      if (argv[i][0] == '-' || argv[i][0] == '/')
         continue;

      NumberOfPathArgs++;
      GetPathAndFileSpec(argv[i], szPath, szFilespec );
      //printf ("Working on Arg%2.2d: %s\n",i,argv[i]);
      //if (g_fVerbose)
      //{
      //   char szAbsolutePath[_MAX_PATH];
      //   _fullpath(szAbsolutePath,argv[i],_MAX_PATH-1);
      //   printf("Acting on path: %s\n", szAbsolutePath );
      //}

      printf("** Generating file list for %s\\%s",szPath,szFilespec);
      int Depth = 1;
      int FilesFound = 0;
      rc = dir_scan(szPath, szFilespec, g_fRecursive, &Depth, &FilesFound);
      if (rc)
          return rc;
      printf(" [%d files]\n",FilesFound);
   }

   /////////////////////////////////
   // Process file list (Do actions)
   /////////////////////////////////
   printf("** Processing files      ");
   if (g_fVerbose) // no dots here.
      printf("\n");
   int FilesProcessed = 0;
   rc = ProcessFileList(&FilesProcessed);
   if (rc)
       return rc;
   if (!g_fVerbose) // Terminate the dots.
      printf("\n");

   /////////////////////////////////
   // Clean up
   /////////////////////////////////
   printf("** Freeing list...\n");
   rc = FreeFileList();
   if (rc)
       return rc;

   /////////////////////////////////
   // Display results
   /////////////////////////////////
   if (g_fStatistics)
   {
      char szTemp[24];
      printf("** Statistics...\n");
      printf("Total size    = %lu bytes\n"     , SizeOfAllFilesFound );
      printf("Newest file   = %s [%s]\n"       , FilenameOfNewestFile, FormatFileTime(&FiletimeOfNewestFile,szTemp,24) );
      printf("Oldest file   = %s [%s]\n"       , FilenameOfOldestFile, FormatFileTime(&FiletimeOfOldestFile,szTemp,24) );
      printf("Largest file  = %s [%lu bytes]\n", FilenameOfLargestFile, SizeOfLargestFile );
      //printf("Smallest file = %s [%lu bytes]\n", FilenameOfSmallestFile, SizeOfSmallestFile  );
      printf("Deepest path  = %s [%d]\n"       , PathnameOfDeepestPathname, DepthOfDeepestPathname );
      printf("Longest path  = %s [%lu chars]\n", FilenameOfLongestPathname, LengthOfLongestPathname );
      printf("Longest name  = %s [%lu chars]\n", FilenameOfLongestFilename, LengthOfLongestFilename );
      //printf("Shortest name = %s [%lu chars]\n", FilenameOfShortestFilename, LengthOfShortestFilename );
      printf("\n");
   }

   if (NumberOfPathArgs<=1) // If >1, then we cannot be sure that we didn't count the same directories twice, so let's not report false information.
       printf("Directories    = %d\n", NumberOfDirectoriesScanned );
   printf("Files found    = %d\n", NumberOfFilesScanned );
   printf("Files backed up= %d\n", NumberOfFilesBackedUp );
   printf("Files modified = %d\n", NumberOfFilesModified );
   if (g_fNumberOfLinesOfCode)
   {
      printf ("Total Lines of Code       (LOC)      = %lu\n",NumberOfLinesOfCode);
      printf ("Non-Comment Non-Blank LOC (NCNB-LOC) = %lu\n",NumberOfLinesNonCommentedOfCode);
      //printf ("Statement Terminating Semi-Colons (STSC) = %lu\n",xxx);
      //printf ("Preprocessor Directive Lines (PDIR) = %lu\n",xxx);
      //printf ("Logical Statements (STMT-LOC) = %lu\n",xxx); // = STSC + "}" closing braces
      //printf ("Blank Lines of Code (BLK-LOC) = %lu\n",xxx); // = STSC + "}" closing braces
      //printf ("Text Comments (TEXT-CMTS) = %lu\n",xxx); // = STSC + "}" closing braces

   }

   printf("Done.\n");

   printf("Press any key to continue");
   getch();
   printf("\n");

   return 0;
}

