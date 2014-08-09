/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  This file has been borrowed thanks to AprQ2, and is subject to GPL license
  see the licensing of Aprq2 for more, it is GPLv2 or newer
*/


#ifdef USE_CURL
#include "client.h"

extern void CL_SendCommand (void);

cvar_t	*cl_http_downloads;
cvar_t	*cl_http_filelists;
cvar_t	*cl_http_proxy;
cvar_t	*cl_http_max_connections;

#define	HTTPDL_ABORT_NONE 0
#define HTTPDL_ABORT_SOFT 1
#define HTTPDL_ABORT_HARD 2

static CURLM	*multi = NULL;
static int		handleCount = 0;
static int		pendingCount = 0;
static int		abortDownloads = HTTPDL_ABORT_NONE;
static qboolean	downloading_pak = false;
static qboolean	httpDown = false;
void CL_RestartFilesystem( qboolean execAutoexec );



struct MemoryStruct {
  char *memory;
  size_t size;
};


static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int binaryWrite(char *file, char *data, int bytenum)
{

	char tmpfile[512];
	char tmpfile2[512];
	FILE *fh = NULL;
	sprintf(tmpfile,"%s/%s.tmp2",FS_Gamedir(),file);
	sprintf(tmpfile2,"%s/%s",FS_Gamedir(),file);

	//Com_Printf("try to write %d bytes to %s..\n", bytenum, tmpfile);

	fh = fopen(tmpfile,"wb");
	if (!fh) return 1;
	
	fwrite(data,1,bytenum,fh);

	fclose(fh);

	//CL_RestartFilesystem( false );
	//Com_Printf("fetched 100 ok, rename time...\n");

	//Com_Printf("rename %s to %s ..\n", tmpfile,tmpfile2);
	if (rename(tmpfile,tmpfile2) == 0)
		remove(tmpfile);

	return 0;
}


/*
===============
CL_EscapeHTTPPath
=============
*/
//static void CL_EscapeHTTPPath (const char *filePath, char *escaped)


int oldcurlFetch(char url[])
{
  CURL *curl_handle;
  CURLcode res;
  char buf[512];
  char out[2048];
  struct MemoryStruct chunk;
  char *localfile=url;
  char *p;
  int retval=0;

  chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
  chunk.size = 0;    /* no data at this point */

  curl_global_init(CURL_GLOBAL_ALL);

  curl_handle = curl_easy_init();

  if (cls.downloadServer == NULL || url == NULL)
	return 0;

  sprintf(buf,"%s/%s/%s", cls.downloadServer, FS_Gamedir(), url);

  //for debug mio full webpath
  //Com_Printf("HTTP downloading %s ...\n   ",buf);
  Com_Printf("HTTP downloading %s ...\n   ",url);


  curl_easy_setopt(curl_handle, CURLOPT_URL, buf);
  curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "quake2 curl 3.26");
  res = curl_easy_perform(curl_handle);

  if(res != CURLE_OK)
  {
	Com_Printf("404 Error\n");
	retval=1;
  }
  else {
    /* got a file */
	//Com_Printf("  %s has been downloaded.\n", localfile);
    Com_Printf("%lu bytes received\n", (long)chunk.size);

	//sprintf(&buf,"%s/%s",FS_Gamedir(),localfile);
	//Com_Printf("buf is now::::: %s\n\n",buf);

	if (binaryWrite(localfile,chunk.memory,chunk.size) != 0)
	{
		Com_Printf("%s write file error!!!\n", localfile);
		retval=1;
	}



    //printf("%s",chunk);
  }

  curl_easy_cleanup(curl_handle);

  if(chunk.memory)
    free(chunk.memory);

  curl_global_cleanup();

  return retval;
}






struct urls
{
	char *url;
	int id;
	char *next;
	int bytes;
};



static const char *turls[] = {
  "http://rlogin.dk/foobaz",
  "http://rlogin.dk/roflmao",
  "http://www.google.com",
  "http://www.yahoo.com",
  "http://www.ibm.com",
  "http://www.mysql.com",
  "http://www.oracle.com",
  "http://www.ripe.net",
  "http://www.iana.org",
  "http://www.nbc.com",
  "http://slashdot.org",
  "http://www.bloglines.com",
  "http://www.techweb.com",
  "http://www.newslink.org",
  "http://www.un.org",
};

/* max 2 conns to http at a time, don't rape apache servers */
#define MAX 2
#define CNT sizeof(turls)/sizeof(char*)

static size_t cb(char *d, size_t n, size_t l, void *p)
{
  /* take care of the data here, ignored in this example */
  (void)d;
  (void)p;
  return n*l;
}


/* http options */
#define MAXFILES 100
#define URLMAXLEN 250


struct urls *dlqueue_add(char url[])
{
  // remember our queueptr
  static struct urls *p;
  static int queuenum;
  int i;
  static struct urls *tmp;

  Com_Printf("call again... ptr should be const: %p\n",p);

  // first time allocate for all files, 100 url structs
  if (!p) 
  {
	  p = malloc(MAXFILES*(sizeof(struct urls)));
	  if (!p) {
		  Com_Printf("MALLOC FAIL for http downloads!\n");
	      return NULL;
	  }

	  tmp=p;
	  for (i=0;i<MAXFILES;++i)
	  {
		  p->url = malloc(URLMAXLEN);
		  if (!p->url) {
			  Com_Printf("err p url malloc fail\n");
			  return NULL;
		  }
			
		  p++;
	  }
	  p=tmp;
	  cls.dlqueue=p;
  }
  // time to add to dlqueue
  
  strncpy(p->url,url,URLMAXLEN-1);
  p->next=0xDEADBEEF;
  p->id=queuenum;
  p++;

  
  Com_Printf("queued url: %s\n",url);

  queuenum++;

  return p;
}

static struct urls *url;

static void init(CURLM *cm, int i)
{
  CURL *eh = curl_easy_init();

  char buf[1024];
  snprintf(buf,sizeof(buf)-1,"%s/%s/%s", cls.downloadServer, FS_Gamedir(), cls.dlqueue[i].url);

  //for debug mio full webpath
  //Com_Printf("HTTP downloading %s ...\n   ",buf);

  Com_Printf("--- MULTI HTTP downloading\n");

  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
  curl_easy_setopt(eh, CURLOPT_URL, buf);
  curl_easy_setopt(eh, CURLOPT_PRIVATE, buf);
  curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(eh, CURLOPT_USERAGENT, "quake2 curl 3.26");

  Com_Printf("HTTP DL added %s to dlqueue...\n" , buf);
  curl_multi_add_handle(cm, eh);
}

int curlFetch(struct url *ptr, int dlnum)
{

  CURLM *cm;
  CURLMsg *msg;
  long L=100;
  unsigned int C=0;
  int M, Q, U = -2; // originally -1, mio, but reserved, use a unique FD, 515176 seems to work on windows
  fd_set R, W, E;
  struct timeval T;

  // sanity check, dont dl if dlserver supplied is weird..
  if (!cls.downloadServer)
	  return 0;

  Com_Printf("INIT newcurl dl now\n");
  curl_global_init(CURL_GLOBAL_ALL);

  cm = curl_multi_init();

  /* we can optionally limit the total amount of connections this multi handle
     uses */
  curl_easy_setopt(cm, CURLMOPT_MAXCONNECTS, (long)MAX);

  Com_Printf("TIME TO INIT YESSSS!!!\n\n");

  // init multiqueue with all urls
  for (C = 0; C < dlnum; ++C) {
    init(cm, C);
  }

  //init(cm, C, url);

  Com_Printf("[mio] MultiCurl() while loop loaded\n");
  while (U) {
    curl_multi_perform(cm, &U);

    if (U) {
      FD_ZERO(&R);
      FD_ZERO(&W);
      FD_ZERO(&E);

      if (curl_multi_fdset(cm, &R, &W, &E, &M)) {
        Com_Printf("E: curl_multi_fdset\n");
        return 1;
      }
/*      if (L == -1)
        L = 100;

      if (M == -1) {
#ifdef WIN32
        Sleep(L);
#else
        sleep(L / 1000);
#endif
      } else {*/
        //T.tv_sec = L/1000;
        //T.tv_usec = (L%1000)*1000;
		T.tv_sec = 5;
        T.tv_usec = 0;


        if (0 > select(M+1, &R, &W, &E, &T)) {
          Com_Printf("E: select(%i,,,,%li): %i: %s\n",
              M+1, L, errno, strerror(errno));
          return 1;
        }
      
    }

    while ((msg = curl_multi_info_read(cm, &Q))) {
      if (msg->msg == CURLMSG_DONE) {
        char *url;
        CURL *e = msg->easy_handle;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
        Com_Printf("R: %d - %s <%s>\n",
                msg->data.result, curl_easy_strerror(msg->data.result), url);
        curl_multi_remove_handle(cm, e);
        curl_easy_cleanup(e);
      }
      else {
        fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
      }

	  // allow user to use console
	  //CL_SendCommand ();

	  /* not used as we only fetch one file at a time.. 
	  else we need to make a dlqueue... if we want 2 at a time..
	  */
      if (C < CNT) {
        init(cm, C++);
        U++; 
      }
	  /* just to prevent it from remaining at 0 if there are more
                URLs to get */
    }
  }

  curl_multi_cleanup(cm);
  curl_global_cleanup();

  // return 0 for success, or 1 for 404 - to keep calling us!
  return 0;
}


#endif