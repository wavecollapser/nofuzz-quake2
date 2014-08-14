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
  			  //chunk is normally a FILE * but we feed it a struct function instead...
			  //writememorycallback func takes a struct and writes it
			  curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
			  curl_easy_setopt(eh, CURLOPT_WRITEDATA, chunk);
 
			  //Com_Printf("HTTP DL added %s to dlqueue...\n" , buf);
			  curl_multi_add_handle(cm, eh);
	  }
  }
}

/* 
  FIXME dont run every time, only if dlqueue is full!!!
 */
int curlFetch(struct url *ptr, int dlnum)
{

  CURLM *cm;
  CURLMsg *msg;
  long L=100;
  unsigned int C=0;
  int M, Q, U = -1;
  fd_set R, W, E;
  struct timeval T;
  CURLMcode	ret;
  qboolean got404=false;

  if (!cls.downloadServer)
	  return 0;

  curl_global_init(CURL_GLOBAL_ALL);

  cm = curl_multi_init();
  
  curl_easy_setopt(cm, CURLMOPT_MAXCONNECTS, (long)MAX);

  for (C = 0; C < dlnum; ++C)
      init(cm, C);
  

  while (U) 
  {
      ret = curl_multi_perform(cm, &U);

      if (U)
	  {
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
		  short while so we DO NOT RUN OUT OF FDs! */
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

		remainingFiles=0;

		if (responseCode == 404)
		{
			//Com_Printf("[HTTP] %s [404 Not Found] [x remaining files]\n", 
				//localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
			Com_Printf("[HTTP] %s [404 Not Found]\n", 
				localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
			got404=true;
			cls.downloadnow=false; // dont download over http again...
		}
		else if (responseCode == 200)
		{
			/*
			Com_Printf("[HTTP] %s [%.f kB, %.0f kB/sec] [x remaining files]\n", 
				localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
			*/
			Com_Printf("[HTTP] %s [%.f kB, %.0f kB/sec]\n", 
				localfile, recvsize/1000,recvsize/(1000*totaltime),remainingFiles);
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

      if (C < dlnum) 
	  {
        init(cm, C++);
        U++; 
      }
    }
  }

  curl_multi_cleanup(cm);
  curl_global_cleanup();

  return got404 ? 1:0;  
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