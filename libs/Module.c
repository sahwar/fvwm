/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
** Module.c: code for modules to communicate with fvwm
*/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <errno.h>

#include "fvwmlib.h"
#include "Module.h"


/************************************************************************
 *
 * Reads a single packet of info from fvwm. Prototype is:
 * unsigned long header[HEADER_SIZE];
 * unsigned long *body;
 * int fd[2];
 * void DeadPipe(int nonsense);  * Called if the pipe is no longer open  *
 *
 * ReadFvwmPacket(fd[1],header, &body);
 *
 * Returns:
 *   > 0 everything is OK.
 *   = 0 invalid packet.
 *   < 0 pipe is dead. (Should never occur)
 *   body is a malloc'ed space which needs to be freed
 *
 **************************************************************************/
int ReadFvwmPacket(int fd, unsigned long *header, unsigned long **body)
{
  int count,total,count2,body_length;
  char *cbody;

  errno = 0;
  if((count = read(fd,header,HEADER_SIZE*sizeof(unsigned long))) >0)
  {
    if(header[0] == START_FLAG)
    {
      body_length = header[2]-HEADER_SIZE;
      *body = (unsigned long *)
        safemalloc(body_length * sizeof(unsigned long));
      cbody = (char *)(*body);
      total = 0;
      while(total < body_length*sizeof(unsigned long))
      {
        errno = 0;
        if((count2=
            read(fd,&cbody[total],
                 body_length*sizeof(unsigned long)-total)) >0)
        {
          total += count2;
        }
        else if(count2 < 0)
        {
          DeadPipe(errno);
        }
      }
    }
    else
      count = 0;
  }
  if(count <= 0)
    DeadPipe(errno);
  return count;
}

/************************************************************************
 *
 * SendText - Sends arbitrary text/command back to fvwm
 *
 ***********************************************************************/
void SendText(int *fd, char *message, unsigned long window)
{
  char *p, *buf;
  unsigned int len;

  if (!message)
    return;

  /* Get enough memory to store the entire message.
   */
  len = strlen(message);
  p = buf = alloca(sizeof(long) * (3 + 1 + (len / sizeof(long))));

  /* Put the message in the buffer, and... */
  *((unsigned long *)p) = window;
  p += sizeof(unsigned long);

  *((unsigned long *)p) = len;
  p += sizeof(unsigned long);

  strcpy(p, message);
  p += len;

  len = 1;
  memcpy(p, &len, sizeof(unsigned long));
  p += sizeof(unsigned long);

  /* Send it!  */
  write(fd[0], buf, p - buf);
}


void SetMessageMask(int *fd, unsigned long mask)
{
  char set_mask_mesg[50];

  sprintf(set_mask_mesg,"SET_MASK %lu\n",mask);
  SendText(fd,set_mask_mesg,0);
}

/***************************************************************************
 * Gets a module configuration line from fvwm. Returns NULL if there are
 * no more lines to be had. "line" is a pointer to a char *.
 *
 * Changed 10/19/98 by Dan Espen:
 *
 * - The "isspace"  call was referring to  memory  beyond the end of  the
 * input area.  This could have led to the creation of a core file. Added
 * "body_size" to keep it in bounds.
 **************************************************************************/
void GetConfigLine(int *fd, char **tline)
{
  static int first_pass = 1;
  int count,done = 0;
  int body_size;
  static char *line = NULL;
  unsigned long header[HEADER_SIZE];

  if(line != NULL)
    free(line);

  if(first_pass)
  {
    SendInfo(fd,"Send_ConfigInfo",0);
    first_pass = 0;
  }

  while(!done)
  {
    count = ReadFvwmPacket(fd[1],header,(unsigned long **)&line);
    /* DB(("Packet count is %d", count)); */
    if (count <= 0)
      *tline = NULL;
    else {
      *tline = &line[3*sizeof(long)];
      body_size = header[2]-HEADER_SIZE;
      /* DB(("Config line (%d): `%s'", body_size, body_size ? *tline : "")); */
      while((body_size > 0)
            && isspace(**tline)) {
        (*tline)++;
        --body_size;
      }
    }

/*   fprintf(stderr,"%x %x\n",header[1],M_END_CONFIG_INFO);*/
    if(header[1] == M_CONFIG_INFO)
      done = 1;
    else if(header[1] == M_END_CONFIG_INFO)
    {
      done = 1;
      if(line != NULL)
        free(line);
      line = NULL;
      *tline = NULL;
    }
  }
}
