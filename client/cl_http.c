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
char *httpdirfix(char *s);
char *gdirfix(char *s);

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

	//Com_Printf("[mio] try to write %d bytes to %s..\n", bytenum, tmpfile);

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
	qboolean started; // has download been started? dont start again
};


/* max 2 conns to http at a time, don't rape apache servers */
#define MAX 2


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

//fixme: dlqueue_free
struct urls *dlqueue_add(char url[])
{
  // remember our queueptr
  static struct urls *p;
  static int queuenum;
  int i;
  static struct urls *tmp;
	
  //Com_Printf("[mio] try to add %s to dlqueue...\n",url);

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

  // only add dl queue if it doesn't already exist!
  if (!in_dlqueue(url))
  {
	  strncpy(p->url,url,URLMAXLEN-1);
	  p->next=0xDEADBEEF;
	  p->id=queuenum;
	  p->started=false;
	  p++;

	  //Com_Printf("queued url: %s\n",url);

	  cls.dlqueue_files = queuenum;
	  if (cls.dlqueue_files == 0)
		  cls.dlqueue_files++;

	  queuenum++;
  }

  return p; 
}

/* need to fix HTTP url with these 2 functions */
char *httpdirfix(char *s)
{
	if (!s) return NULL;
	
	if (s[strlen(s)-1]=='/')
		s[strlen(s)-1]='\0';

	return s;
}
char *gdirfix(char *s)
{
	if (s[0]=='.' && s[1]=='/')
	return s+2;
}

// remember our struct location, static ptr
static struct MemoryStruct *memPtr;

static void init(CURLM *cm, int i)
{
  CURL *eh = curl_easy_init();

  // dont redo downloads!
  // only start a download if it is not already marked as started!
  if (!cls.dlqueue[i].started) 
  {
	  char buf[1024];
	  char tmp[256];
	  char *priv=malloc(256);

	  struct MemoryStruct *chunk=malloc(sizeof (struct MemoryStruct));
	  extern struct MemoryStruct *memPtr;
	  memPtr=chunk;
	  if (!chunk)
		  Com_Printf("E: dlqueue malloc err!!!\n");
	  else
	  {
		  chunk->memory=malloc(1);
		  chunk->size=0;

		  snprintf(buf,sizeof(buf)-1,"%s/%s/%s", httpdirfix(cls.downloadServer), 
			  gdirfix(FS_Gamedir()), cls.dlqueue[i].url);


			  //we got a working file handle we can hand over to curl
			  cls.dlqueue[i].started=true;

			  //for debug mio full webpath
			  //Com_Printf("HTTP downloading %s ...\n   ",buf);

			  //Com_Printf("--- MULTI HTTP downloading, got %d\n", i);

			  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
			  curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
			  curl_easy_setopt(eh, CURLOPT_URL, buf);

			  strncpy(priv,cls.dlqueue[i].url,255);
			  curl_easy_setopt(eh, CURLOPT_PRIVATE, priv);
			  curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
			  curl_easy_setopt(eh, CURLOPT_USERAGENT, "quake2 curl 3.26");
			  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
			  curl_easy_setopt(eh, CURLOPT_WRITEDATA, chunk);
			  //chunk is normally a FILE * but we feed it a struct function instead...
			  //writememorycallback func takes a struct and writes it
 
			  //Com_Printf("HTTP DL added %s to dlqueue...\n" , buf);
			  curl_multi_add_handle(cm, eh);
	  }
  }
}

/* need to reconsider this, it is being called all the time, not quite what we wanted...
   need to check if curdlnum > lastdlnum and only run if we have received full dlqueue

  FIXME dont run every time, only if dlqueue is full!!!
 */
int curlFetch(struct url *ptr, int dlnum)
{

  CURLM *cm;
  CURLMsg *msg;
  long L=100;
  unsigned int C=0;
  int M, Q, U = -1; // originally -1, mio, but reserved, use a unique FD, 515176 seems to work on windows
  fd_set R, W, E;
  struct timeval T;
  CURLMcode	ret;
  qboolean got404=false;

  // sanity check, dont dl if dlserver supplied is weird..
  if (!cls.downloadServer)
	  return 0;

  //Com_Printf("INIT newcurl dl now\n");
  curl_global_init(CURL_GLOBAL_ALL);

  cm = curl_multi_init();

  /* we can optionally limit the total amount of connections this multi handle
     uses */
  curl_easy_setopt(cm, CURLMOPT_MAXCONNECTS, (long)MAX);

  //Com_Printf("[mio] init all multiqueue dls at once now!!\n\n");
  //Com_Printf("[mio curlFetch] %d in queue\n",dlnum);
  // init multiqueue with all urls
  for (C = 0; C < dlnum; ++C) {
    init(cm, C);
  }

  //init(cm, C, url);

  //Com_Printf("[mio] MultiCurl() while loop loaded\n");
  while (U) {
    ret = curl_multi_perform(cm, &U);

	/*  newhandlecount = U , so we just finished a dl, see what returncode was.. */
    if (U) {
      FD_ZERO(&R);
      FD_ZERO(&W);
      FD_ZERO(&E);

		if (ret != CURLM_OK)
		{
			Com_Printf ("curl_multi_perform error, Aborting HTTP downloads.\n");
			return 1;
		}

      if (curl_multi_fdset(cm, &R, &W, &E, &M)) {
        Com_Printf("E: curl_multi_fdset\n");
        return 1;
      }
      if (L == -1)
        L = 100;

      if (M == -1) {
		  /* obviously we need to sleep a 
		  short while to we DO NOT RUN OUT OF FDs! */
#ifdef WIN32
        Sleep(L);
#else
        sleep(L / 1000);
#endif
      } else {
        T.tv_sec = L/1000;
        T.tv_usec = (L%1000)*1000;
		//T.tv_sec = 5;
        //T.tv_usec = 0;
        if (0 > select(M+1, &R, &W, &E, &T)) {
          Com_Printf("E: select(%i,,,,%li): %i: %s\n",
              M+1, L, errno, strerror(errno));
          return 1;
        }
	  }
      
    }

    while ((msg = curl_multi_info_read(cm, &Q))) {

	  // allow user to use console
	  CL_SendCommand ();

      if (msg->msg == CURLMSG_DONE)
	  {
		long responseCode;
        extern struct MemoryStruct *memPtr;
		char *url;
		double recvsize;
		double totaltime;
		char *localfile;
		char *fullurl;
		int remainingFiles;
		static int finishcnt;
		
        CURL *e = msg->easy_handle;
		curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &url);
		curl_easy_getinfo(msg->easy_handle, CURLINFO_SIZE_DOWNLOAD, &recvsize);
		curl_easy_getinfo(msg->easy_handle, CURLINFO_TOTAL_TIME, &totaltime);
		curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &localfile);
		curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &fullurl);

		//buggy for now, disabled
		//remainingFiles=cls.dlqueue_files-finishcnt;
		remainingFiles=0;

		if (responseCode == 404)
		{
			// we got at least 1 404 error, so resume with udp downloads to fetch those
			// not all files are avail on http, some may be in paks
			// so we can resume those via udp...
			Com_Printf("[HTTP] %s [404 Not Found] [x remaining files]\n", 
				localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
		
			got404=true;
			cls.downloadnow=false; // dont download over http again...
		}
		else if (responseCode == 200)
		{
			// only save if successfully downloaded!

			Com_Printf("[HTTP] %s [%.f kB, %.0f kB/sec] [x remaining files]\n", 
				localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
		

			//int binaryWrite(char *file, char *data, int bytenum)
			//int binaryWrite(localfile, char *data, recvsize)
			//data received:msg->data.result
			//msg->easy_handle
			
			//Com_Printf("memPtr bytes: %d\n",memPtr->size);
			binaryWrite(localfile, memPtr->memory, memPtr->size);
		}
		
		finishcnt++;

		if (memPtr && memPtr->memory)
		free(memPtr->memory);

        curl_multi_remove_handle(cm, e);
        curl_easy_cleanup(e);
      }
      else {
        Com_Printf("E: CURLMsg (%d)\n", msg->msg);
		return 1;
      }

	  /* not used as we only fetch one file at a time.. 
	  else we need to make a dlqueue... if we want 2 at a time..
	  */
      if (C < dlnum) {
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
  if (got404)
	  return 1;
  else
	  return 0;
}

/* check if file is in queue already */
qboolean in_dlqueue(char *s)
{
	int i;
	for (i=0;i<cls.dlqueue_files;++i)
		if (!strcmp(s,cls.dlqueue[i].url))
			return true;

	return false;

}

/* for debugging purposes only */
dlqueue_print()
{
	int i;
	for (i=0;i<cls.dlqueue_files;++i)
		Com_Printf("[queue #%d]: %s\n",i, cls.dlqueue[i].url);
}

#endif