/* ====================================================================
 * Copyright (c) 1995 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * IT'S CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/****************************************************************************
 * Title       : Bandwidth management
 * File        : mod_bandwidth.c
 * Author      : Yann Stettler (stettler@cohprog.com)
 * Date        : 12 January 2003
 * Version     : 2.0.5 for Apache 1.3+
 *
 * Description :
 *   Provide bandwidth usage limitation either on the whole server or
 *   one a per connection basis based on the size of files, directory
 *   location or remote domain/IP.
 *
 * Revision    : 08/04/97 - "-1" value for LargeFileLimit to disable any
 *                          limits for that kind of files.
 *               01/26/98 - Include in_domain() and in_ip() in this file and
 *                          make them static so it will work with Apache 1.3x
 *               03/29/98 - Change to set bandwidth in VirtualHost directive
 *               07/17/99 - Minor changes to remove warnings at compil time
 *                          
 *                          Allow the use of the network/mask format when
 *                          setting a bandwidth for a remote host/network.
 *                          (Thanks to Sami Kuhmonen for the patch) 
 *
 *                          New directive BandWidthPulse
 *
 *               10/14/00 - Minor bug fixed
 *               12/15/00 - Bug fix when using mmap
 *               08/29/01 - Add a directive to change the data directory
 *                          (Thanks to Awesome Walrus <walrus@amur.ru> )
 *               01/12/03 - Add MaxConnection directive to limit the number
 *                          of simultaneous connections.
 *
 ***************************************************************************
 * Copyright (c)1997 Yann Stettler and CohProg SaRL. All rights reserved.
 * Written for the Apache Group by :
 *
 * Yann Stettler
 * stettler@cohprog.com
 * http://www.cohprog.com/
 * http://www.animanga.com/
 *
 * Based on the original default_handler module and on bw_module 0.2 from
 * Etienne BERNARD (eb@via.ecp.fr) at VIA Centrale Reseaux, ECP, France
 * 
 * Many thanks to Colba Net Inc (Montreal) for their sponsoring of 
 * improvements to this module.
 *
 ***************************************************************************/


/*
 * Instruction :
 * -------------
 *
 * Note : this module was writen for Apache 1.3.x and tested on it
 *
 * Installation :
 *
 * 1) Insert the following line at the end of the "Configuration" file :
 *    Module bandwidth_module      mod_bandwidth.o
 *
 * WARNING : This behaviour is not the same between the various main versions
 *           of Apache. Please, read the instruction on our website for the
 *           instructions concerning the latest release : 
 *           http://www.cohprog.com/v3/bandwidth/doc-en.html
 *
 * 2) Run the "Configure" program and re-compile the server with "make".
 *
 * 3) Create the following directories with "rwx" permission to everybody :
 *    (or rwx for the user under which Apache run : Usualy "nobody")
 *    /tmp/apachebw
 *    /tmp/apachebw/link
 *    /tmp/apachebw/master
 *
 *  /==== by Awesome Walrus <walrus@amur.ru> =====================\
 *    Or you may change this name by using BandWidthDataDir global
 *    configuration directive. See below for details.
 *  \==== by Awesome Walrus <walrus@amur.ru> =====================/
 *
 * Note that if any of those directories doesn't exist, or if they can't
 * be accessed by the server, the module is totaly disabled except for
 * logging an error message in the logfile.
 *
 * Be careful that on some systems the content of the /tmp directory
 * is deleted at boot time or every so often by a cronjob. If that the
 * case, either disable this feature or change the location of the
 * directories used by the module in the sources bellow.
 *
 * Server configuration directive :
 * --------------------------------
 *
 *  /==== by Awesome Walrus <walrus@amur.ru> =====================\
 * -  BandWidthDataDir
 *    Syntax  : BandWidthDataDir <directory>
 *    Default : "/tmp/apachebw"
 *    Context : server config
 *
 *    Sets the name of the directory used by mod_bandwidth to store
 *    its internal temporary information.
 *  \==== by Awesome Walrus <walrus@amur.ru> =====================/
 *
 * -  BandWidthModule 
 *    Syntax  : BandWidthModule <On|Off>
 *    Default : Off
 *    Context : per server config
 *
 *    Enable or disable totaly the whole module. By default, the module is
 *    disable so it is safe to compile it in the server anyway.
 *
 *    PLEASE, NOTE THAT IF YOU SET A BANDWIDTH LIMIT INSIDE A VIRTUALHOST
 *    BLOCK, YOU ALSO __NEED__ TO PUT THE "BandWidthModule On" DIRECTIVE
 *    INSIDE THAT VIRTUALHOST BLOCK !
 *
 *    IF YOU SET BANDWIDTH LIMITS INSIDE DIRECTORY BLOCKS (OUTSIDE OF
 *    ANY VIRTUALHOST BLOCK), YOU ONLY NEED TO PUT THE "BandWidthModule On"
 *    DIRECTIVE ONCE, OUTSIDE OF ANY VIRTUALHOST OR DIRECTORY BLOCK.
 *
 * -  BandWidthPulse
 *    Syntax  : BandWidthPulse <microseconds>
 *    Default :
 *    Context : per server config
 *
 *    Change the algorithm used to calculate bandwidth and transmit data.
 *    In normal mode (old mode), the module try to transmit data in packets
 *    of 1KB. That mean that if the bandwidth available is of 512B, the
 *    module will transmit 1KB, wait 2 seconds, transmit another 1KB and
 *    so one.
 *
 *    Seting a value with "BandWidthPulse", will change the algorithm so
 *    that the server will always wait the same amount of time between
 *    sending packets but the size of the packets will change.
 *    The value is in microseconds.
 *    For example, if you set "BandWidthPulse 1000000" (1 sec) and the
 *    bandwidth available is of 512B, the sever will transmit 512B,
 *    wait 1 second, transmit 512B and so on.
 *
 *    The advantage is a smother flow of data. The disadvantage is
 *    a bigger overhead of data transmited for packet header.
 *    Setting too small a value (bellow 1/5 of a sec) is not realy
 *    useful and will put more load on the system and generate more
 *    traffic for packet header.
 *
 *    Note also that the operating system may do some buffering on
 *    it's own and so defeat the purpose of setting small values.
 *
 *    This may be very useful on especialy crowded network connection :
 *    In normal mode, several seconds may happen between the sending of
 *    a full packet. This may lead to timeout or people may believe that
 *    the connection is hanging. Seting a value of 1000000 (1 sec) would
 *    guarantee that some data are sent every seconds...
 *
 * Directory / Server / Virtual Server configuration directive :
 * -------------------------------------------------------------
 *
 * -  BandWidth
 *    Syntax  : BandWidth <domain|ip|all> <rate>
 *    Default : none
 *    Context : per directory, .htaccess
 *
 *    Limit the bandwidth for files in this directory and
 *    sub-directories based on the remote host <domain> or
 *    <ip> address or for <all> remote hosts.
 *
 *    Ip addresses may now be specified in the network/mask format.
 *    (Ie: 192.168.0.0/21 )
 *
 *    The <rate> is in Bytes/second.
 *    A <rate> of "0" means no bandwidth limit.
 *
 *    Several BandWidth limits can be set for the same
 *    directory to set different limits for different
 *    hosts. In this case, the order of the "BandWidth"
 *    keywords is important as the module will take the
 *    first entry which matches the client address.
 *
 *    Example :
 *       <Directory /home/www>
 *       BandWidth ecp.fr 0
 *       BandWidth 138.195 0
 *       BandWidth all 1024
 *       </Directory>
 *
 *      This will limit the bandwith for directory /home/www and 
 *      all it's subdirectories to 1024Bytes/sec, except for 
 *      *.ecp.fr or 138.195.*.* where no limit is set.
 *
 * -  LargeFileLimit
 *    Syntax  : LargeFileLimit <filesize> <rate>
 *    Default : none
 *    Context : per directory, .htaccess
 *
 *    Set a maximal <rate> (in bytes/sec) to use when transfering
 *    a file of <filesize> KBytes or more.
 *
 *    Several "LargeFileLimit" can be set for various files sizes
 *    to create range. The rate used for a given file size will be
 *    the one of the matching range.
 *
 *    A <rate> of "0" mean that there isn't any limit based on
 *    the size.
 *
 *    A <rate> of "-1" mean that there isn't any limit for that type
 *    of file. It's override even a BandWidth limit. I found this usefull
 *    to give priority to very small files (html pages, very small pictures)
 *    while seting limits for larger files... (users with their video files
 *    can go to hell ! :)
 *
 *    Example :
 *    If the following limits are set :
 *       LargeFileLimit 200 3072
 *       LargeFileLimit 1024 2048
 *
 *       That's mean that a file of less than 200KBytes won't be
 *       limited based on his size. A file with a size between
 *       200KBytes (included) and 1023Kbytes (included) will be
 *       limited to 3072Bytes/sec and a file of 1024Kbytes or more
 *       will be limited to 2048Bytes/sec.
 *
 * -  MinBandWidth
 *    Syntax  : MinBandWidth <domain|ip|all> <rate>
 *    Default : all 256
 *    Context : per directory, .htaccess
 *
 *    Set a minimal bandwidth to use for transfering data. This
 *    over-ride both BandWidth and LargeFileLimit rules as well
 *    as the calculated rate based on the number of connections.
 *
 *    The first argument is used in the same way as the first
 *    argument of BandWidth.
 *
 *    <rate> is in bytes per second.
 *
 *    A rate of "0" explicitly means to use the default minimal
 *    value (256 Bytes/sec).
 *
 *    A rate of "-1" means that the minimal rate is equal to the
 *    actual rate defined by BandWidth and LargeFileLimit.
 *    In fact, that means that the final rate won't depend
 *    of the number of connections but only on what was defined.
 *
 *    Example :
 *    If BandWidth is set to "3072" (3KBytes/sec) and MinBandWidth
 *    is set to "1024" (1KBytes/sec) that means :
 *       - if there is one connection, the file will be transfered
 *         at 3072 Bytes/sec.
 *       - if there is two connections, each files will be transfered
 *         at 1536 Bytes/sec. 
 *       - if there is three or more connections, each files will be
 *         transfered at 1024 Bytes/sec. (Minimal of 1024 Bytes/sec).
 *
 *    If MinBandWidth is set to "-1" that means :
 *       - if there is one connection, the file will be transfered
 *         at 3072 Bytes/sec.
 *       - if there is two or more connections, each files will be
 *         transfered at 3072 Bytes/sec. In effect, the rate doesn't
 *         depend anymore on the number of connections but only on
 *         the configuration values.
 *
 *    Note that the total transfer rate will never exceed your physical
 *    bandwidth limitation.
 *
 * Note : If both a "BandWidth" and a "LargeFileLimit" limit apply,
 *        the lowest one will be used. (But never lower than the
 *        "MinBandWidth" rate)
 *
 *        If both a virtual server limit is defined and another
 *        apply for a directory under this virtual server, the
 *        directory limit will over-ride it.
 *
 *        If a limit is defined outside a Directory or VirtualHost
 *        directive, it will act as default on a per virtual server
 *        basis. (Ie: each virtual server will have that limit,
 *        _independantly_ of the other servers)
 *
 * -  MaxConnection
 *    Syntax  : MaxConnection <connections>
 *    Default : 0 (illimited)
 *    Context : per directory, .htaccess 
 *
 *    Restrict the number of maximum simultanous connections. If the
 *    limit is reached, new connections will be rejected.
 "
 *    A value of 0 mean that there isn't any limits.
 *
 * Implementation notes :
 * ----------------------
 * 
 * 1) This module isn't called when serving a cgi-script. In this
 *    case, it's the functions in mod_cgi.c that handle the connection.
 *
 *    That's not a bug : I didn't want to change the cgi_module and
 *    I was too lazy to do it anyway.
 *
 * 2) The rate of data transmission is only calculated. It isn't
 *    measured. Which mean that this module calculates the final
 *    rate that should apply for a file and simply divides this
 *    number by the number of actual connections subject to the
 *    same "per directory" directives.
 * 
 * 3) On the "+" side, the module regulate the speed taking
 *    into account the actual time that was needed to send the
 *    data. Which also mean that if data are read by the client
 *    at a slowest rate than the limit, no time will be lost and
 *    data will be sent as fast as the client can read them (but
 *    no faster than the limited rate :) ...
 *
 * 4) Some kind of buffering is done as side effect. Data are
 *    sent in packet of 1024 Bytes which seems a good value
 *    for TCP/IP.
 *    If another packet size is wanted, change the value of
 *    "#define PACKET" in the codes bellow.
 *
 * 5) The default value for MinBandWidth is defined by :
 *    "#define MIN_BW_DEFAULT" (in Bytes/sec)
 *
 * 6) Don't define "BWDEBUG" for a production server :
 *    this would log a _lot_ of useless informations for
 *    debuging purpose.
 *
 */

#include <time.h>
#include "httpd.h"
#include "http_core.h"
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_main.h"

#define MIN_BW_DEFAULT  256        /* Minimal bandwidth defaulted to 256Bps */
#define PACKET 1024                /* Sent packet of 1024 bytes             */

/* #define MASTER_DIR  "/tmp/apachebw/master"
 * #define LINK_DIR    "/tmp/apachebw/link"
 */

#define MASTER_DIR  "master"
#define LINK_DIR    "link"

/* Define BWDEBUG for debuging purpose only ! */
/* #define BWDEBUG */
#ifdef BWDEBUG
#undef BWDEBUG
#endif

#define BANDWIDTH_DISABLED             1<<0
#define BANDWIDTH_ENABLED              1<<1

#ifdef USE_MMAP_FILES
#include <unistd.h>
#include <sys/mman.h>

/* mmap support for static files based on ideas from John Heidemann's
 * patch against 1.0.5.  See
 * <http://www.isi.edu/~johnh/SOFTWARE/APACHE/index.html>.
 */

/* Files have to be at least this big before they're mmap()d.  This is to
 * deal with systems where the expense of doing an mmap() and an munmap()
 * outweighs the benefit for small files.
 */
#ifndef MMAP_THRESHOLD
#ifdef SUNOS4
#define MMAP_THRESHOLD          (8*1024)
#else
#define MMAP_THRESHOLD          0
#endif
#endif
#endif

/* Limits for bandwidth and minimal bandwidth based on directory */
typedef struct {
  char *from;
  long rate;
} bw_entry;

/* Limits for bandwidth based on file size */
typedef struct {
  long size;
  long rate;
} bw_sizel;

/* Per directory configuration structure */
typedef struct {
  array_header *limits;
  array_header *minlimits;
  array_header *sizelimits;
  int maxconnection;
  char  *directory;
} bandwidth_config;

/* Per server configuration structure */
typedef struct {
  int   state;
} bandwidth_server_config;

int bw_handler (request_rec *);

module bandwidth_module;

static long int BWPulse=0;

char bandwidth_data_dir[MAX_STRING_LEN]="/tmp/apachebw";

/***************************************************************************
 * Configuration functions                                                 *
 ***************************************************************************/

static void *create_bw_config(pool *p, char *path) {
  bandwidth_config *new=
	(bandwidth_config *) ap_palloc(p, sizeof(bandwidth_config));
  new->limits=ap_make_array(p,20,sizeof(bw_entry));
  new->minlimits=ap_make_array(p,20,sizeof(bw_entry));
  new->sizelimits=ap_make_array(p,10,sizeof(bw_sizel));
  new->directory=ap_pstrdup(p, path);
  new->maxconnection=0;
  return (void *)new; 
}

static void *create_bw_server_config(pool *p, server_rec *s) {
  bandwidth_server_config *new;

  new = (bandwidth_server_config *)ap_pcalloc(p, sizeof(bandwidth_server_config));

  new->state = BANDWIDTH_DISABLED;

  return (void *)new;
}

/***************************************************************************
 * Directive functions                                                     *
 ***************************************************************************/

static const char *bandwidthmodule(cmd_parms *cmd, bandwidth_config *dconf, int flag) {
   bandwidth_server_config *sconf;

   sconf = (bandwidth_server_config *)ap_get_module_config(cmd->server->module_config, &bandwidth_module);
   sconf->state = (flag ? BANDWIDTH_ENABLED : BANDWIDTH_DISABLED);

   return NULL;
}

static const char *set_bandwidth_data_dir(cmd_parms *cmd, void *dummy, char *arg) {
    arg = ap_os_canonical_filename(cmd->pool, arg);

    if (!ap_is_directory(arg)) {
        return "BandWidthDataDir must be a valid directory";
    }
    ap_cpystrn(bandwidth_data_dir, arg, sizeof(bandwidth_data_dir));
    return NULL;
}

static const char *setpulse(cmd_parms *cmd, bandwidth_config *dconf, char *pulse) {
   long int temp;

   if (pulse && *pulse && isdigit(*pulse))
     temp = atol(pulse);
   else
     return "Invalid argument";

   if (temp<0)
     return "Pulse must be a number of microseconds/s";

   BWPulse=temp;

   return NULL;
}

static const char *MaxConnection(cmd_parms *cmd, void *s, char *maxc) { 
   bandwidth_config *conf=(bandwidth_config *)s;
   int temp;
 
   if (maxc && *maxc && isdigit(*maxc))
     temp = atoi(maxc); 
   else 
     return "Invalid argument";
 
   if (temp<0)
     return "Connections must be a number of simultaneous connections allowed/s";
 
   conf->maxconnection=temp;
 
   return NULL;
} 

static const char *bandwidth(cmd_parms *cmd, void *s, char *from, char *bw) {
  bandwidth_config *conf=(bandwidth_config *)s;
  bw_entry *a;
  long int temp;

  if (bw && *bw && isdigit(*bw))
    temp = atol(bw);
  else
    return "Invalid argument";

  if (temp<0)
    return "BandWidth must be a number of bytes/s";

  a = (bw_entry *)ap_push_array(conf->limits);
  a->from = ap_pstrdup(cmd->pool,from);
  a->rate = temp;
  return NULL;
}

static const char *minbandwidth(cmd_parms *cmd, void *s, char *from, char *bw) {
  bandwidth_config *conf=(bandwidth_config *)s;
  bw_entry *a;
  long int temp;

  if (bw && *bw && (*bw=='-' || isdigit(*bw)))
    temp = atol(bw);
  else
    return "Invalid argument";

  a = (bw_entry *)ap_push_array(conf->minlimits);
  a->from = ap_pstrdup(cmd->pool,from);
  a->rate = temp;
  return NULL;
}

static const char *largefilelimit(cmd_parms *cmd, void *s, char *size, char *bw) {
  bandwidth_config *conf=(bandwidth_config *)s;
  bw_sizel *a;
  long int temp, tsize;

  if (bw && *bw && (*bw=='-' || isdigit(*bw)))
    temp = atol(bw);
  else
    return "Invalid argument";

  if (size && *size && isdigit(*size))
    tsize = atol(size);
  else
    return "Invalid argument";

  /*
  if (temp<0)
    return "BandWidth must be a number of bytes/s";
  */

  if (tsize<0)
    return "File size must be a number of Kbytes";

  a = (bw_sizel *)ap_push_array(conf->sizelimits);
  a->size = tsize;
  a->rate = temp;
  return NULL;
}

static command_rec bw_cmds[] = {
{ "BandWidth", bandwidth, NULL, RSRC_CONF | OR_LIMIT, TAKE2,
    "a domain (or ip, or all for all) and a bandwidth limit (in bytes/s)" },
{ "MinBandWidth", minbandwidth, NULL, RSRC_CONF | OR_LIMIT, TAKE2,
    "a domain (or ip, or all for all) and a minimal bandwidth limit (in bytes/s)" },
{ "LargeFileLimit", largefilelimit, NULL, RSRC_CONF | OR_LIMIT, TAKE2,
    "a filesize (in Kbytes) and a bandwidth limit (in bytes/s)" },
{ "MaxConnection", MaxConnection, NULL, RSRC_CONF | OR_LIMIT, TAKE1,
    "A number of allowed simultaneous connections" },
{ "BandWidthModule",   bandwidthmodule,  NULL, OR_FILEINFO, FLAG, 
      "On or Off to enable or disable (default) the whole bandwidth module" },
{ "BandWidthPulse", setpulse, NULL, OR_FILEINFO, TAKE1,
    "a number of microseconds" },
{ "BandWidthDataDir", set_bandwidth_data_dir, NULL, RSRC_CONF, TAKE1,
    "A writable directory where temporary bandwidth info is to be stored" },
{ NULL }
};

/***************************************************************************
 * Internal functions                                                      *
 ***************************************************************************/

#ifdef USE_MMAP_FILES
struct mmap {
    void *mm;
    size_t length;
};

static void mmap_cleanup (void *mmv)
{
    struct mmap *mmd = mmv;

    munmap(mmd->mm, mmd->length);
}
#endif

static int in_domain(const char *domain, const char *what) {
    int dl=strlen(domain);
    int wl=strlen(what);

    if((wl-dl) >= 0) {
        if (strcasecmp(domain,&what[wl-dl]) != 0) return 0;

        /* Make sure we matched an *entire* subdomain --- if the user
         * said 'allow from good.com', we don't want people from nogood.com
         * to be able to get in.
         */

        if (wl == dl) return 1; /* matched whole thing */
        else return (domain[0] == '.' || what[wl - dl - 1] == '.');
    } else
        return 0;
}

static int in_ip(char *domain, char *what) {

    /* Check a similar screw case to the one checked above ---
     * "allow from 204.26.2" shouldn't let in people from 204.26.23
     */
    int a, b, c, d, e; 
    unsigned long in, out, inmask, outmask;

    if (sscanf(domain, "%i.%i.%i.%i/%i", &a, &b, &c, &d, &e) < 5) {
       int l = strlen(domain);
       if (strncmp(domain,what,l) != 0) return 0;
       if (domain[l - 1] == '.') return 1;
       return (what[l] == '\0' || what[l] == '.');
    }
    in = (a<<24)|(b<<16)|(c<<8)|d;
    inmask = 0xFFFFFFFF ^ ((1<<(32-e))-1);

    if (sscanf(what, "%i.%i.%i.%i", &a, &b, &c, &d) < 4)
      return 0;

    out = (a<<24)|(b<<16)|(c<<8)|d;

    return ((in&inmask) == (out&inmask));
}

static int is_ip(const char *host)
{
    while ((*host == '.') || (*host=='/') || isdigit(*host))
        host++;
    return (*host == '\0');
}

static long get_bw_rate(request_rec *r, array_header *a) {
  bw_entry *e = (bw_entry *)a->elts;
  const char *remotehost = NULL;
  int i;

  remotehost = ap_get_remote_host(r->connection, r->per_dir_config, REMOTE_HOST);

  for (i=0; i< a->nelts; i++) {
    if (!strcmp(e[i].from,"all"))
      return e[i].rate;

    if (in_ip(e[i].from, r->connection->remote_ip))
      return e[i].rate;
    if ((remotehost!=NULL) && !is_ip(remotehost)) {
      if (in_domain(e[i].from,remotehost))
	return e[i].rate;
    }
  }
  return 0;
}

static long get_bw_filesize(request_rec *r, array_header *a, off_t filesize) {
  bw_sizel *e = (bw_sizel *)a->elts;
  int i;
  long int tmpsize=0, tmprate=0;

  if (!filesize)
    return(0);
 
  filesize /= 1024;

  for (i=0; i< a->nelts; i++) {
    if (e[i].size <= filesize )
       if (tmpsize <= e[i].size) {
          tmpsize=e[i].size;
          tmprate=e[i].rate;
       }
  }

  return(tmprate);
}

static int current_connection(char *filename) {
   struct stat mdata;

   /*
    * Here, we will do the main work. And that won't take
    * long !
    */

    if (stat(filename, &mdata) < 0) 
       return(1);   /* strange... should not occure */

    return(mdata.st_nlink-1);

    /*
     * I told you it wouldn't be long... :)
     */
}

static struct timeval timediff(struct timeval *a, struct timeval *b)
{
   struct timeval rslt, tmp;

   tmp=*a;
  
   if ((rslt.tv_usec = tmp.tv_usec - b->tv_usec) < 0) {
      rslt.tv_usec += 1000000;
      --(tmp.tv_sec);
   }
   if ((rslt.tv_sec = tmp.tv_sec - b->tv_sec) < 0) {
      rslt.tv_usec=0;
      rslt.tv_sec=0;
   }
   return(rslt);
}

/***************************************************************************
 * Main function, module core                                              *
 ***************************************************************************/

static int handle_bw(request_rec *r) {
  int rangestatus, errstatus;
  FILE *f;
  bandwidth_config *conf =
     (bandwidth_config *)ap_get_module_config(r->per_dir_config, &bandwidth_module);
  bandwidth_server_config *sconf =
     (bandwidth_server_config *)ap_get_module_config(r->server->module_config, &bandwidth_module);
  long int bw_rate=0, bw_min=0, bw_f_rate=0, cur_rate;
  int nolimit=0, bwlast=0, fd;
  long int tosend, bytessent, filelength;
  struct stat fdata;
  struct timeval opt_time, last_time, now, timespent, timeout;
  char masterfile[128], filelink[128], *directory;

#ifdef USE_MMAP_FILES
    caddr_t mm;
#endif

  /* This handler has no use for a request body (yet), but we still
   * need to read and discard it if the client sent one.
   */
  if ((errstatus = ap_discard_request_body(r)) != OK)
      return errstatus;

  /* Return as fast as possible if request is not a GET or if module not
     enabled */
  if (r->method_number != M_GET || sconf->state == BANDWIDTH_DISABLED)
     return DECLINED;

  if (!conf->directory) {
     directory=(char *)ap_document_root(r);
  } else {
     directory=conf->directory;
  }

  bw_rate=get_bw_rate(r,conf->limits);
  bw_min=get_bw_rate(r, conf->minlimits);
  bw_f_rate=get_bw_filesize(r, conf->sizelimits , r->finfo.st_size);

  if (!directory) return DECLINED;

  if ((bw_rate==0 && bw_f_rate==0) || bw_f_rate < 0) {
     if (!conf->maxconnection) {
         return DECLINED;
     } else {
         nolimit=1;
     }
  }

  if (r->finfo.st_mode == 0 || (r->path_info && *r->path_info)) {
      ap_log_reason("File does not exist", r->filename, r);
      return NOT_FOUND;
  }

  ap_update_mtime (r, r->finfo.st_mtime);
  ap_set_last_modified(r);
  ap_set_etag(r);
  if (((errstatus = ap_meets_conditions(r)) != OK)
      || (errstatus = ap_set_content_length (r, r->finfo.st_size))) {
          return errstatus;
  }

  /* 
   * Create name of the master file.
   * Will use the inode and device number of the "limited"
   * directory.
   */

  if (stat(directory, &fdata) < -1) {
     /* Dunno if this may happen... but well... */
     return DECLINED;
  }
  sprintf(masterfile,"%s/%s/%d:%ld",  bandwidth_data_dir, MASTER_DIR, (short int)fdata.st_dev, (long int)fdata.st_ino);

  /*
   * Check if master file already exist, else create it.
   * Then we create a hardlink to it using the pid as name.
   */
  if ((fd=open(masterfile, O_RDONLY|O_CREAT, 0777)) < 0) {
     ap_log_printf(r->server, "mod_bandwidth : Can't create/access master file %s",masterfile);
     return DECLINED;
  }
  close(fd);

  /*
   * Check if the maximum number of connections allowed is reached
   */
  if (conf->maxconnection && (conf->maxconnection <= current_connection(masterfile))) {
/*     return HTTP_SERVICE_UNAVAILABLE; */
       return FORBIDDEN;
  }

  sprintf(filelink,"%s/%s/%d", bandwidth_data_dir, LINK_DIR, getpid());
  if (link(masterfile, filelink) < 0) {
     ap_log_printf(r->server, "mod_bandwidth : Can't create hard link %s", filelink);
     return DECLINED;
  }

  f = ap_pfopen(r->pool, r->filename,"r");

  if (f == NULL) {
     ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
                "file permissions deny server access: %s", r->filename);
     unlink(filelink); 
     return FORBIDDEN;
  }

  /*
   * Calculate bandwidth for this file based on location limits and
   * file size limit.
   *
   * The following rules applies :
   * 1) File size limit over-rule location limit if it's slower.
   * 2) Whatever the resuling limit and number of connections, never
   *    go bellow the minimal limit for this location.
   * 3) A file size limit of zero mean that we don't care about the
   *    size of the file for this purpose.
   *
   */

#ifdef BWDEBUG
  ap_log_printf(r->server, "mod_bandwidth : Directory : %s Rate : %d Minimum : %d Size rate : %d", directory, bw_rate, bw_min, bw_f_rate);
#endif

  if (bw_f_rate && (bw_rate > bw_f_rate || !bw_rate))
      bw_rate=bw_f_rate;

  if (bw_min < 0)
     bw_min=bw_rate;
  else if (! bw_min)
     bw_min=MIN_BW_DEFAULT;

#ifdef USE_MMAP_FILES
  ap_block_alarms();
  if ((r->finfo.st_size >= MMAP_THRESHOLD)
      && ( !r->header_only)) {
    /* we need to protect ourselves in case we die while we've got the
       * file mmapped */
      mm = mmap (NULL, r->finfo.st_size, PROT_READ, MAP_PRIVATE,
                  fileno(f), 0);
  } else {
      mm = (caddr_t)-1;
  }

  if (mm == (caddr_t)-1) {
      ap_unblock_alarms();

      if (r->finfo.st_size >= MMAP_THRESHOLD) {
          ap_log_error(APLOG_MARK, APLOG_CRIT, r->server,
                      "mmap_handler: mmap failed: %s", r->filename);
      }
#endif

  rangestatus = ap_set_byterange(r);
  ap_send_http_header(r);
 
  if (!r->header_only) {
      if (!rangestatus) {
          filelength=r->finfo.st_size;
          bytessent=0;
          bwlast=0;
          while(!bwlast) {

             cur_rate=(long int)bw_rate / current_connection(masterfile);
             if (cur_rate < bw_min)
                cur_rate=bw_min;

             if (BWPulse <= 0) {
                /*
                 * I feel rather foolish to use select and use the 
                 * 1/1000000 of sec for my calculation. But as long as I
                 * am at it, let's do it well...
                 *
                 * Note that my loop don't take into account the time
                 * spent outside the send_fd_length function... but in
                 * this case, I feel that I have the moral right to do so :)
                 */

                if (nolimit) {
                   opt_time.tv_sec=0;
                   opt_time.tv_usec=0;
                } else {
                   opt_time.tv_sec=(long int) PACKET / cur_rate;
                   opt_time.tv_usec=(long int)PACKET*1000000/cur_rate-opt_time.tv_sec*1000000;
                }
                tosend=PACKET;
                if (tosend+bytessent >= filelength) {
                   tosend=filelength-bytessent;
                   bwlast=1;
                }
             } else {
                opt_time.tv_sec=(long int)BWPulse/1000000;
                opt_time.tv_usec=BWPulse-opt_time.tv_sec*1000000;

                if (nolimit)
                   tosend=filelength;
                else
                   tosend=(long int)((double)BWPulse/(double)1000000*(double)cur_rate);
                if (tosend+bytessent >= filelength) {
                   tosend=filelength-bytessent;
                   bwlast=1;
                }
             }  
#ifdef BWDEBUG
             ap_log_printf(r->server, "mod_bandwidth : Current rate : %d [%d/%d] Min : %d",
                cur_rate, bw_rate, current_connection(masterfile), bw_min);
#endif

             gettimeofday(&last_time, (struct timezone *) 0);

             ap_send_fd_length(f, r, tosend);
             bytessent += tosend;
             if (r->connection->aborted)
                break;
             if (!bwlast) {
                /* We sleep... */
                gettimeofday(&now, (struct timezone *) 0);
                timespent=timediff(&now, &last_time);
                timeout=timediff(&opt_time, &timespent);

#ifdef BWDEBUG
                    ap_log_printf(r->server, "mod_bandwidth : Sleep : %ld/%ld (Op time : %ld/%ld Spent : %ld/%ld)",
                      timeout.tv_sec, timeout.tv_usec, opt_time.tv_sec, opt_time.tv_usec,
                      timespent.tv_sec, timespent.tv_usec);
#endif

                select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout);
             }
          }
      } else {
          long offset, length;
          while (ap_each_byterange(r, &offset, &length)) {
              fseek(f, offset, SEEK_SET);

              filelength=length;
              bytessent=0;
              bwlast=0;
              while(!bwlast) {

                 cur_rate=(long int)bw_rate / current_connection(masterfile);
                 if (cur_rate < bw_min)
                    cur_rate=bw_min;

                 if (BWPulse <= 0) {
                    if (nolimit) {
                      opt_time.tv_sec=0;
                      opt_time.tv_usec=0;
                    } else {
                      opt_time.tv_sec=(long int) PACKET / cur_rate;
                      opt_time.tv_usec=(long int)PACKET*1000000/cur_rate-opt_time.tv_sec*1000000;
                    }

                    tosend=PACKET;
                    if (tosend+bytessent >= filelength) {
                       tosend=filelength-bytessent;
                       bwlast=1;
                    }
                 } else {
                    opt_time.tv_sec=(long int)BWPulse/1000000;
                    opt_time.tv_usec=BWPulse-opt_time.tv_sec*1000000;
 
                    if (nolimit)
                       tosend=filelength;
                    else 
                       tosend=(long int)((double)BWPulse/(double)1000000*(double)cur_rate);
                    if (tosend+bytessent >= filelength) {
                       tosend=filelength-bytessent;
                       bwlast=1;
                    }
                 }  

#ifdef BWDEBUG
                 ap_log_printf(r->server, "mod_bandwidth : Current rate : %d [%d/%d] Min : %d",
                   cur_rate, bw_rate, current_connection(masterfile), bw_min);
#endif 
                 gettimeofday(&last_time, (struct timezone *) 0);
   
                 ap_send_fd_length(f, r, tosend);
                 bytessent += tosend;
                 if (r->connection->aborted)
                    break;
                 if (!bwlast) {
                    /* We sleep... */
                    gettimeofday(&now, (struct timezone *) 0);
                    timespent=timediff(&now, &last_time);
                    timeout=timediff(&opt_time, &timespent);

#ifdef BWDEBUG
                    ap_log_printf(r->server, "mod_bandwidth : Sleep : %ld/%ld (Op time : %ld/%ld Spent : %ld/%ld)", 
                      timeout.tv_sec, timeout.tv_usec, opt_time.tv_sec, opt_time.tv_usec,
                      timespent.tv_sec, timespent.tv_usec);
#endif
                    select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout);
                 }
              }
          }
      }
  }

#ifdef USE_MMAP_FILES
    } else {
        struct mmap *mmd;

        mmd = ap_palloc (r->pool, sizeof (*mmd));
        mmd->mm = mm;
        mmd->length = r->finfo.st_size;
        ap_register_cleanup (r->pool, (void *)mmd, mmap_cleanup, mmap_cleanup);
        ap_unblock_alarms();

        rangestatus = ap_set_byterange(r);
        ap_send_http_header (r);

        if (!r->header_only) {
           if (!rangestatus) {
              filelength=r->finfo.st_size;
              bytessent=0;
              bwlast=0;
              while(!bwlast) {

                cur_rate=(long int)bw_rate / current_connection(masterfile);
                if (cur_rate < bw_min)
                   cur_rate=bw_min;

                if (BWPulse <= 0) {
                   /*
                    * I feel rather foolish to use select and use the 
                    * 1/1000000 of sec for my calculation. But as long as I
                    * am at it, let's do it well...
                    *
                    * Note that my loop don't take into account the time
                    * spent outside the send_fd_length function... but in
                    * this case, I feel that I have the moral right to do so :)
                    */

                   if (nolimit) {
                      opt_time.tv_sec=0;
                      opt_time.tv_usec=0;
                   } else {
                      opt_time.tv_sec=(long int) PACKET / cur_rate;
                      opt_time.tv_usec=(long int)PACKET*1000000/cur_rate-opt_time.tv_sec*1000000;
                   }

                   tosend=PACKET;
                   if (tosend+bytessent >= filelength) {
                      tosend=filelength-bytessent;
                      bwlast=1;
                   }
                } else {
                   opt_time.tv_sec=(long int)BWPulse/1000000;
                   opt_time.tv_usec=BWPulse-opt_time.tv_sec*1000000;

                   if (nolimit)
                      tosend=filelength;
                   else
                      tosend=(long int)((double)BWPulse/(double)1000000*(double)cur_rate);
                   if (tosend+bytessent >= filelength) {
                      tosend=filelength-bytessent;
                      bwlast=1;
                   }
                }

#ifdef BWDEBUG
             ap_log_printf(r->server, "mod_bandwidth : Current rate : %d [%d/%d] Min : %d Sending : %d",
                cur_rate, bw_rate, current_connection(masterfile), bw_min, tosend);
#endif
                gettimeofday(&last_time, (struct timezone *) 0);

                ap_send_mmap(mm, r, bytessent, tosend);
                bytessent += tosend;
                if (r->connection->aborted)
                   break;
                if (!bwlast) {
                   /* We sleep... */
                   gettimeofday(&now, (struct timezone *) 0);
                   timespent=timediff(&now, &last_time);
                   timeout=timediff(&opt_time, &timespent);

#ifdef BWDEBUG
                    ap_log_printf(r->server, "mod_bandwidth : Sleep : %ld/%ld (Op time : %ld/%ld Spent : %ld/%ld)",
                      timeout.tv_sec, timeout.tv_usec, opt_time.tv_sec, opt_time.tv_usec,
                      timespent.tv_sec, timespent.tv_usec);
#endif
                   select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout);
                }
             }
         } else {
             long offset, length;
             while (ap_each_byterange(r, &offset, &length)) {

                filelength=length;
                bytessent=0;
                bwlast=0;
                while(!bwlast) {

                   cur_rate=(long int)bw_rate / current_connection(masterfile);
                   if (cur_rate < bw_min)
                      cur_rate=bw_min;

                   if (BWPulse <= 0) { 
                      if (nolimit) {
                        opt_time.tv_sec=0;
                        opt_time.tv_usec=0;
                      } else {
                         opt_time.tv_sec=(long int) PACKET / cur_rate;
                         opt_time.tv_usec=(long int)PACKET*1000000/cur_rate-opt_time.tv_sec*1000000;
                      }
   
                      tosend=PACKET;
                      if (tosend+bytessent >= filelength) {
                         tosend=filelength-bytessent;
                         bwlast=1;
                      }
                   } else {
                      opt_time.tv_sec=(long int)BWPulse/1000000;
                      opt_time.tv_usec=BWPulse-opt_time.tv_sec*1000000;

                      if (nolimit)
                         tosend=filelength;
                      else
                         tosend=(long int)((double)BWPulse/(double)1000000*(double)cur_rate);
                      if (tosend+bytessent >= filelength) {
                         tosend=filelength-bytessent;
                         bwlast=1;
                      }
                   }  

#ifdef BWDEBUG
                 ap_log_printf(r->server, "mod_bandwidth : Current rate : %d [%d/%d] Min : %d Sending : $d",
                   cur_rate, bw_rate, current_connection(masterfile), bw_min , tosend);
#endif 
                   gettimeofday(&last_time, (struct timezone *) 0);
   
                   ap_send_mmap(mm, r, offset+bytessent, tosend);
                   bytessent += tosend;
                   if (r->connection->aborted)
                      break;
                   if (!bwlast) {
                      /* We sleep... */
                      gettimeofday(&now, (struct timezone *) 0);
                      timespent=timediff(&now, &last_time);
                      timeout=timediff(&opt_time, &timespent);

#ifdef BWDEBUG
                    ap_log_printf(r->server, "mod_bandwidth : Sleep : %ld/%ld (Op time : %ld/%ld Spent : %ld/%ld)", 
                      timeout.tv_sec, timeout.tv_usec, opt_time.tv_sec, opt_time.tv_usec,
                      timespent.tv_sec, timespent.tv_usec);
#endif
                      select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &timeout);
                   }
                }
            }
        }
    }
    }
#endif

    ap_pfclose(r->pool, f);

    /* Remove the hardlink */
     unlink(filelink);

    return OK;
}

static handler_rec bw_handlers[] = {
{ "*/*", handle_bw },
{ NULL }
};

module bandwidth_module = {
   STANDARD_MODULE_STUFF,
   NULL,                        /* initializer */
   create_bw_config,            /* bw config creater */
   NULL,                        /* bw merger --- default is to override */
   create_bw_server_config,     /* server config */
   NULL,                        /* merge server config */
   bw_cmds,                     /* command table */
   bw_handlers,                 /* handlers */
   NULL,                        /* filename translation */
   NULL,                        /* check_user_id */
   NULL,                        /* check auth */
   NULL,                        /* check access */
   NULL,                        /* type_checker */
   NULL,                        /* fixups */
   NULL                         /* logger */
};
