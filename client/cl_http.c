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

	Com_Printf("try to write %d bytes to %s..\n", bytenum, tmpfile);

	fh = fopen(tmpfile,"wb");
	if (!fh) return 1;
	
	fwrite(data,1,bytenum,fh);

	fclose(fh);

	//CL_RestartFilesystem( false );
	Com_Printf("fetched 100% ok, rename time...\n");

	Com_Printf("rename %s to %s ..\n", tmpfile,tmpfile2);
	if (rename(tmpfile,tmpfile2) == 0)
		remove(tmpfile);

	return 0;
}


/*
===============
CL_EscapeHTTPPath

Properly escapes a path with HTTP %encoding. libcurl's function
seems to treat '/' and such as illegal chars and encodes almost
the entire URL...
===============
*/
static void CL_EscapeHTTPPath (const char *filePath, char *escaped)
{
	size_t	i, len;
	char	*p;

	p = escaped;

	len = strlen (filePath);
	for (i = 0; i < len; i++)
	{
		if (!isalnum (filePath[i]) && filePath[i] != ';' && filePath[i] != '/' &&
			filePath[i] != '?' && filePath[i] != ':' && filePath[i] != '@' && filePath[i] != '&' &&
			filePath[i] != '=' && filePath[i] != '+' && filePath[i] != '$' && filePath[i] != ',' &&
			filePath[i] != '[' && filePath[i] != ']' && filePath[i] != '-' && filePath[i] != '_' &&
			filePath[i] != '.' && filePath[i] != '!' && filePath[i] != '~' && filePath[i] != '*' &&
			filePath[i] != '\'' && filePath[i] != '(' && filePath[i] != ')')
		{
			sprintf (p, "%%%02x", filePath[i]);
			p += 3;
		}
		else
		{
			*p = filePath[i];
			p++;
		}
	}
	p[0] = 0;

	//using ./ in a url is legal, but all browsers condense the path and some IDS / request
	//filtering systems act a bit funky if http requests come in with uncondensed paths.
	len = strlen(escaped);
	p = escaped;
	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, len - (p - escaped) - 1);
		len -= 2;
	}
}

int curlFetch(char url[])
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

  // sanity check, dont download if there is no server to dl from
  if (cls.downloadServer == NULL)
	return 0;

  //strcpy(&buf,"http://rlogin.dk/games/filds/");

  sprintf(buf,"%s/%s/%s", cls.downloadServer, FS_Gamedir(), url);

  Com_Printf("HTTP downloading %s ...\n   ",buf);

//CL_EscapeHTTPPath (const char *filePath, char *escaped);
  //CL_EscapeHTTPPath(buf,out);

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
    Com_Printf("%lu bytes retrieved\n", (long)chunk.size);

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


/*
====================
CL_RestartFilesystem
 
Flush caches and restart the VFS.
====================
*/
void VID_Restart_f (void);
//extern int silentSubsystem;
qboolean CL_IsDisconnected(void)
{
	return (cls.state <= ca_disconnected);
}

void CL_RestartFilesystem( qboolean execAutoexec )
{
    connstate_t	cls_state;

    if ( cls.state == ca_uninitialized ) {
        //FS_Restart();
		//miofixme
		if (execAutoexec) {
			FS_ExecConfig("autoexec.cfg");
			Cbuf_Execute();
		}
        return;
    }

    Com_DPrintf( "CL_RestartFilesystem()\n" );

    /* temporary switch to loading state */
    cls_state = cls.state;
    if ( cls.state == ca_active ) {
//        cls.state = ca_loading;
    }

	S_Shutdown();
	VID_Shutdown();

    //FS_Restart();
	//miofixme

	if (execAutoexec) {
		//silentSubsystem = CVAR_SYSTEM_VIDEO|CVAR_SYSTEM_SOUND;
		FS_ExecConfig("autoexec.cfg");
		Cbuf_Execute();
		//silentSubsystem = 0;
	}

#ifndef VID_INITFIRST
	S_Init();	
	VID_Restart_f();
	VID_CheckChanges();
#else
	VID_Restart_f();
	VID_CheckChanges();
	S_Init();
#endif

    if ( cls_state == ca_disconnected ) {
        //UI_OpenMenu( UIMENU_MAIN );
    } else if ( cls_state > ca_connected ) {
		memset(cl.sound_precache, 0, sizeof(cl.sound_precache));
		CL_RegisterSounds();
        CL_PrepRefresh();
    }

    /* switch back to original state */
    cls.state = cls_state;
}



#endif