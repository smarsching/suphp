/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
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
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * mod_suphp is based on mod_cgi from the original Apache sources
 * Code for authorization environment variables is partly taken from mod_php
 *
 * mod_suphp was written by Sebastian Marsching <sebastian@marsching.com>
 * Feel free to contact me if you have bug-reports or suggestions
 */

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "http_conf_globals.h"

#ifndef PATH_TO_SUPHP
#define PATH_TO_SUPHP "/usr/sbin/suphp"
#endif

module MODULE_VAR_EXPORT suphp_module;

/* KLUDGE --- for back-combatibility, we don't have to check ExecCGI
 * in ScriptAliased directories, which means we need to know if this
 * request came through ScriptAlias or not... so the Alias module
 * leaves a note for us.
 */

static int is_scriptaliased(request_rec *r)
{
    const char *t = ap_table_get(r->notes, "alias-forced-type");
    return t && (!strcasecmp(t, "cgi-script"));
}


/*
 * We have to use an own version of ap_call_exec() because suExec
 * does not allow to execute setuid-root programs
 */

static char **suphp_create_argv(pool *p, char *path, char *user, char *group,
                          char *av0, const char *args)
{
    int x, numwords;
    char **av;
    char *w;
    int idx = 0;

    /* count the number of keywords */

    for (x = 0, numwords = 1; args[x]; x++) {
        if (args[x] == '+') {
            ++numwords;
        }
    }

    if (numwords > APACHE_ARG_MAX - 5) {
        numwords = APACHE_ARG_MAX - 5;  /* Truncate args to prevent overrun */
    }
    av = (char **) ap_palloc(p, (numwords + 5) * sizeof(char *));

    if (path) {
        av[idx++] = path;
    }
    if (user) {
        av[idx++] = user;
    }
    if (group) {
        av[idx++] = group;
    }

    av[idx++] = av0;

    for (x = 1; x <= numwords; x++) {
        w = ap_getword_nulls(p, &args, '+');
        ap_unescape_url(w);
        av[idx++] = ap_escape_shell_cmd(p, w);
    }
    av[idx] = NULL;
    return av;
}

int suphp_call_exec(request_rec *r, child_info *pinfo, char *argv0,
			     char **env, int shellcmd)
{
    int pid = 0;

/*

#if defined(RLIMIT_CPU)  || defined(RLIMIT_NPROC) || \
    defined(RLIMIT_DATA) || defined(RLIMIT_VMEM) || defined (RLIMIT_AS)

    core_dir_config *conf;
    conf = (core_dir_config *) ap_get_module_config(r->per_dir_config,
						    &core_module);

#endif

#ifdef RLIMIT_CPU
    if (conf->limit_cpu != NULL) {
        if ((setrlimit(RLIMIT_CPU, conf->limit_cpu)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit: failed to set CPU usage limit");
	}
    }
#endif
#ifdef RLIMIT_NPROC
    if (conf->limit_nproc != NULL) {
        if ((setrlimit(RLIMIT_NPROC, conf->limit_nproc)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit: failed to set process limit");
	}
    }
#endif
#if defined(RLIMIT_AS)
    if (conf->limit_mem != NULL) {
        if ((setrlimit(RLIMIT_AS, conf->limit_mem)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit(RLIMIT_AS): failed to set memory "
			 "usage limit");
	}
    }
#elif defined(RLIMIT_DATA)
    if (conf->limit_mem != NULL) {
        if ((setrlimit(RLIMIT_DATA, conf->limit_mem)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit(RLIMIT_DATA): failed to set memory "
			 "usage limit");
	}
    }
#elif defined(RLIMIT_VMEM)
    if (conf->limit_mem != NULL) {
        if ((setrlimit(RLIMIT_VMEM, conf->limit_mem)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit(RLIMIT_VMEM): failed to set memory "
			 "usage limit");
	}
    }
#endif

*/

//	    execve("/usr/sbin/suphp",
//		   suphp_create_argv(r->pool, NULL, NULL, NULL, "suphp", r->args),
//		   env);
	    execle(PATH_TO_SUPHP, "suphp", NULL, env); 
    return (pid);
}



/* Configuration stuff */

#define DEFAULT_LOGBYTES 10385760
#define DEFAULT_BUFBYTES 1024

#define CONFIG_MODE_SERVER 1
#define CONFIG_MODE_DIRECTORY 2
#define CONFIG_MODE_COMBO 3	/* Shouldn't ever happen. */

#define SUPHP_ENGINE_OFF 0
#define SUPHP_ENGINE_ON 1
#define SUPHP_ENGINE_UNDEFINED 2

typedef struct {
    int cmode;
    char *logname;
    long logbytes;
    int bufbytes;
    int engine;
    char *php_config;
} suphp_conf;

static void *suphp_create_dir_config(pool *p, char *dirspec)
{
 suphp_conf *cfg;
 cfg = (suphp_conf *) ap_pcalloc(p, sizeof(suphp_conf));
 cfg->php_config = NULL;
 cfg->cmode = CONFIG_MODE_DIRECTORY;
 return (void *) cfg;
}

static void *suphp_merge_dir_config(pool *p, void *base_conf, void *new_conf)
{
 suphp_conf *parent = (suphp_conf *) base_conf;
 suphp_conf *child = (suphp_conf *) new_conf;
 suphp_conf *merged = (suphp_conf *) ap_pcalloc(p, sizeof(suphp_conf));
 
 merged->php_config = child->php_config ? child->php_config : parent->php_config;
 
 return merged;
}

static void *suphp_create_server_config(pool *p, server_rec *s)
{
    suphp_conf *cfg = (suphp_conf *) ap_pcalloc(p, sizeof(suphp_conf));

    cfg->logname = NULL;
    cfg->logbytes = DEFAULT_LOGBYTES;
    cfg->bufbytes = DEFAULT_BUFBYTES;
    cfg->engine = SUPHP_ENGINE_UNDEFINED;

    return (void *) cfg;
}

static void *suphp_merge_server_config(pool *p, void *base_conf, void *new_conf)
{
    suphp_conf *merged = (suphp_conf *) ap_pcalloc(p, sizeof(suphp_conf));
    suphp_conf *parent = (suphp_conf *) base_conf;
    suphp_conf *child = (suphp_conf *) new_conf;
    
    if (child->logname)
    {
     merged->logname = child->logname;
     merged->logbytes = child->logbytes;
     merged->bufbytes = child->bufbytes;
    }
    else
    {
     merged->logname = parent->logname;
     merged->logbytes = parent->logbytes;
     merged->bufbytes = parent->bufbytes;
    }

    if (child->engine != SUPHP_ENGINE_UNDEFINED)
     merged->engine = child->engine;
    else
     merged->engine = parent->engine;
    
    return (void *) merged;
}

static const char *suphp_handle_cmd_engine(cmd_parms *cmd, void *mconfig, int flag)
{
 suphp_conf *cfg = (suphp_conf *) ap_get_module_config(cmd->server->module_config, &suphp_module);
 
 if (flag)
  cfg->engine = SUPHP_ENGINE_ON;
 else
  cfg->engine = SUPHP_ENGINE_OFF;
 
 return NULL;
}

static const char *suphp_handle_cmd_config(cmd_parms *parms, void *mconfig, const char *arg)
{
 suphp_conf *cfg = (suphp_conf *) mconfig;
 cfg->php_config = (char*)ap_pstrdup(parms->pool, arg);
 return NULL;
}

static const command_rec suphp_cmds[] =
{
    {"suPHP_Engine", suphp_handle_cmd_engine, NULL, RSRC_CONF, FLAG, "Whether PHP is on or off, default is off"},
    {"suPHP_ConfigPath", suphp_handle_cmd_config, NULL, OR_OPTIONS, TAKE1, "Where the php.ini resists, default is to use PHP's default configuration"},
    {NULL}
};

static int log_scripterror(request_rec *r, suphp_conf * conf, int ret,
			   int show_errno, char *error)
{
    FILE *f;
    struct stat finfo;

    ap_log_rerror(APLOG_MARK, show_errno|APLOG_ERR, r, 
		"%s: %s", error, r->filename);

    if (!conf->logname ||
	((stat(ap_server_root_relative(r->pool, conf->logname), &finfo) == 0)
	 &&   (finfo.st_size > conf->logbytes)) ||
         ((f = ap_pfopen(r->pool, ap_server_root_relative(r->pool, conf->logname),
		      "a")) == NULL)) {
	return ret;
    }

    /* "%% [Wed Jun 19 10:53:21 1996] GET /cgi-bin/printenv HTTP/1.0" */
    fprintf(f, "%%%% [%s] %s %s%s%s %s\n", ap_get_time(), r->method, r->uri,
	    r->args ? "?" : "", r->args ? r->args : "", r->protocol);
    /* "%% 500 /usr/local/apache/cgi-bin */
    fprintf(f, "%%%% %d %s\n", ret, r->filename);

    fprintf(f, "%%error\n%s\n", error);

    ap_pfclose(r->pool, f);
    return ret;
}

static int log_script(request_rec *r, suphp_conf * conf, int ret,
		  char *dbuf, const char *sbuf, BUFF *script_in, BUFF *script_err)
{
    array_header *hdrs_arr = ap_table_elts(r->headers_in);
    table_entry *hdrs = (table_entry *) hdrs_arr->elts;
    char argsbuffer[HUGE_STRING_LEN];
    FILE *f;
    int i;
    struct stat finfo;

    if (!conf->logname ||
	((stat(ap_server_root_relative(r->pool, conf->logname), &finfo) == 0)
	 &&   (finfo.st_size > conf->logbytes)) ||
         ((f = ap_pfopen(r->pool, ap_server_root_relative(r->pool, conf->logname),
		      "a")) == NULL)) {
	/* Soak up script output */
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0)
	    continue;
#if defined(WIN32) || defined(NETWARE)
        /* Soak up stderr and redirect it to the error log.
         * Script output to stderr is already directed to the error log
         * on Unix, thanks to the magic of fork().
         */
        while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, r, 
                          "%s", argsbuffer);            
        }
#else
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0)
	    continue;
#endif
	return ret;
    }

    /* "%% [Wed Jun 19 10:53:21 1996] GET /cgi-bin/printenv HTTP/1.0" */
    fprintf(f, "%%%% [%s] %s %s%s%s %s\n", ap_get_time(), r->method, r->uri,
	    r->args ? "?" : "", r->args ? r->args : "", r->protocol);
    /* "%% 500 /usr/local/apache/cgi-bin" */
    fprintf(f, "%%%% %d %s\n", ret, r->filename);

    fputs("%request\n", f);
    for (i = 0; i < hdrs_arr->nelts; ++i) {
	if (!hdrs[i].key)
	    continue;
	fprintf(f, "%s: %s\n", hdrs[i].key, hdrs[i].val);
    }
    if ((r->method_number == M_POST || r->method_number == M_PUT)
	&& *dbuf) {
	fprintf(f, "\n%s\n", dbuf);
    }

    fputs("%response\n", f);
    hdrs_arr = ap_table_elts(r->err_headers_out);
    hdrs = (table_entry *) hdrs_arr->elts;

    for (i = 0; i < hdrs_arr->nelts; ++i) {
	if (!hdrs[i].key)
	    continue;
	fprintf(f, "%s: %s\n", hdrs[i].key, hdrs[i].val);
    }

    if (sbuf && *sbuf)
	fprintf(f, "%s\n", sbuf);

    if (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0) {
	fputs("%stdout\n", f);
	fputs(argsbuffer, f);
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0)
	    fputs(argsbuffer, f);
	fputs("\n", f);
    }

    if (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
	fputs("%stderr\n", f);
	fputs(argsbuffer, f);
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0)
	    fputs(argsbuffer, f);
	fputs("\n", f);
    }

    ap_bclose(script_in);
    ap_bclose(script_err);

    ap_pfclose(r->pool, f);
    return ret;
}

/****************************************************************
 *
 * Actual suPHP handling...
 */


struct suphp_child_stuff {
#ifdef TPF
    TPF_FORK_CHILD t;
#endif
    request_rec *r;
    int nph;
    int debug;
    char *argv0;
};

static int suphp_child(void *child_stuff, child_info *pinfo)
{
    struct suphp_child_stuff *cld = (struct suphp_child_stuff *) child_stuff;
    request_rec *r = cld->r;
    char *argv0 = cld->argv0;
    int child_pid;
    const char *authorization=NULL;
    char *tmp=NULL;
    char *auth_user=NULL;
    char *auth_password=NULL;
    suphp_conf *cfg = (suphp_conf *) ap_get_module_config(r->per_dir_config, &suphp_module);

#ifdef DEBUG_SUPHP
#ifdef OS2
    /* Under OS/2 need to use device con. */
    FILE *dbg = fopen("con", "w");
#else
    FILE *dbg = fopen("/dev/tty", "w");
#endif
    int i;
#endif

    char **env;

    RAISE_SIGSTOP(SUPHP_CHILD);
#ifdef DEBUG_SUPHP
    fprintf(dbg, "Attempting to exec %s as %ssuPHP child (argv0 = %s)\n",
	    r->filename, cld->nph ? "NPH " : "", argv0);
#endif

    /*
     * Set environment variables with authorization information like mod_php
     */
    
    if (r->headers_in)
    {
     authorization = ap_table_get(r->headers_in, "Authorization");
    }
    if (authorization 
        && !strcasecmp(ap_getword(r->pool, &authorization, ' '), "Basic"))
    {
     tmp = ap_uudecode(r->pool, authorization);
     auth_user = ap_getword_nulls_nc(r->pool, &tmp, ':');
/*
     if (auth_user)
     {
      auth_user = estrdup(auth_user);
     }
*/
     auth_password = tmp;
/*
     if (auth_password)
     {
      auth_password = estrdup(auth_password);
     }
*/
     if (auth_user)
     {
      ap_table_setn(r->subprocess_env, "PHP_AUTH_USER", auth_user);
     }
     if (auth_password)
     {
      ap_table_setn(r->subprocess_env, "PHP_AUTH_PW", auth_password);
     }
    }
    
    if (cfg->php_config)
    {
     ap_table_setn(r->subprocess_env, "PHP_CONFIG", cfg->php_config);
    }
     
    ap_add_cgi_vars(r);
    env = ap_create_environment(r->pool, r->subprocess_env);

#ifdef DEBUG_SUPHP
    fprintf(dbg, "Environment: \n");
    for (i = 0; env[i]; ++i)
	fprintf(dbg, "'%s'\n", env[i]);
#endif


    ap_chdir_file(r->filename);
    if (!cld->debug)
	ap_error_log2stderr(r->server);

    /* Transumute outselves into the script.
     * NB only ISINDEX scripts get decoded arguments.
     */

#ifdef TPF
    return (0);
#else
    ap_cleanup_for_exec();

    child_pid = suphp_call_exec(r, pinfo, argv0, env, 0);
    return (child_pid);

    /* Uh oh.  Still here.  Where's the kaboom?  There was supposed to be an
     * EARTH-shattering kaboom!
     *
     * Oh, well.  Muddle through as best we can...
     *
     * Note that only stderr is available at this point, so don't pass in
     * a server to aplog_error.
     */

    ap_log_error(APLOG_MARK, APLOG_ERR, NULL, "exec of %s failed", r->filename);
    exit(0);
    /* NOT REACHED */
    return (0);
#endif  /* TPF */
}

static int suphp_handler(request_rec *r)
{
    int retval, nph, dbpos = 0;
    char *argv0, *dbuf = NULL;
    BUFF *script_out, *script_in, *script_err;
    char argsbuffer[HUGE_STRING_LEN];
    int is_included = !strcmp(r->protocol, "INCLUDED");
    void *sconf = r->server->module_config;
    suphp_conf *conf =
    (suphp_conf *) ap_get_module_config(sconf, &suphp_module);
    suphp_conf *mconf = (suphp_conf *) ap_get_module_config(r->server->module_config, &suphp_module);

    struct suphp_child_stuff cld;

    if ((mconf->engine == SUPHP_ENGINE_OFF) || (mconf->engine == SUPHP_ENGINE_UNDEFINED))
     return DECLINED;
    
    if (r->method_number == M_OPTIONS) {
	/* 99 out of 100 CGI scripts, this is all they support */
	r->allowed |= (1 << M_GET);
	r->allowed |= (1 << M_POST);
	return DECLINED;
    }

    if ((argv0 = strrchr(r->filename, '/')) != NULL)
	argv0++;
    else
	argv0 = r->filename;

//    nph = !(strncmp(argv0, "nph-", 4));
    nph = 0;

//    if (!(ap_allow_options(r) & OPT_EXECCGI) && !is_scriptaliased(r))
//	return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
//			       "Options ExecCGI is off in this directory");
//    if (nph && is_included)
//	return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
//			       "attempt to include NPH CGI script");
//
    if (r->finfo.st_mode == 0)
	return log_scripterror(r, conf, NOT_FOUND, APLOG_NOERRNO,
			       "script not found or unable to stat");
    if (S_ISDIR(r->finfo.st_mode))
	return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
			       "attempt to invoke directory as script");

    if ((retval = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)))
	return retval;

    ap_add_common_vars(r);
    cld.argv0 = argv0;
    cld.r = r;
    cld.nph = nph;
    cld.debug = conf->logname ? 1 : 0;
#ifdef TPF
    cld.t.filename = r->filename;
    cld.t.subprocess_env = r->subprocess_env;
    cld.t.prog_type = FORK_FILE;
#endif   /* TPF */

#ifdef CHARSET_EBCDIC
    /* The included MIME headers must ALWAYS be in text/ebcdic format.
     * Only after reading the MIME headers, we check the Content-Type
     * and switch to the necessary conversion mode.
     * Until then (and in case an nph- script was called), use the
     * configured default conversion:
     */
    ap_bsetflag(r->connection->client, B_EBCDIC2ASCII, r->ebcdic.conv_out);
#endif /*CHARSET_EBCDIC*/

    /*
     * we spawn out of r->main if it's there so that we can avoid
     * waiting for free_proc_chain to cleanup in the middle of an
     * SSI request -djg
     */
    if (!ap_bspawn_child(r->main ? r->main->pool : r->pool, suphp_child,
			 (void *) &cld, kill_after_timeout,
			 &script_out, &script_in, &script_err)) {
	ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
		    "couldn't spawn child process: %s", r->filename);
	return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Transfer any put/post args, CERN style...
     * Note that we already ignore SIGPIPE in the core server.
     */

    if (ap_should_client_block(r)) {
	int dbsize, len_read;

	if (conf->logname) {
	    dbuf = ap_pcalloc(r->pool, conf->bufbytes + 1);
	    dbpos = 0;
	}

	ap_hard_timeout("copy script args", r);

	while ((len_read =
		ap_get_client_block(r, argsbuffer, HUGE_STRING_LEN)) > 0) {
	    if (conf->logname) {
		if ((dbpos + len_read) > conf->bufbytes) {
		    dbsize = conf->bufbytes - dbpos;
		}
		else {
		    dbsize = len_read;
		}
		memcpy(dbuf + dbpos, argsbuffer, dbsize);
		dbpos += dbsize;
	    }
	    ap_reset_timeout(r);
	    if (ap_bwrite(script_out, argsbuffer, len_read) < len_read) {
		/* silly script stopped reading, soak up remaining message */
		while (ap_get_client_block(r, argsbuffer, HUGE_STRING_LEN) > 0) {
		    /* dump it */
		}
		break;
	    }
	}

	ap_bflush(script_out);

	ap_kill_timeout(r);
    }

    ap_bclose(script_out);

    /* Handle script return... */
    if (script_in && !nph) {
	const char *location;
	char sbuf[MAX_STRING_LEN];
	int ret;

	if ((ret = ap_scan_script_header_err_buff(r, script_in, sbuf))) {
	    return log_script(r, conf, ret, dbuf, sbuf, script_in, script_err);
	}

	location = ap_table_get(r->headers_out, "Location");

	if (location && location[0] == '/' && r->status == 200) {

	    /* Soak up all the script output */
	    ap_hard_timeout("read from script", r);
	    while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0) {
		continue;
	    }
#if defined(WIN32) || defined(NETWARE)
            /* Soak up stderr and redirect it to the error log.
             * Script output to stderr is already directed to the error log
             * on Unix, thanks to the magic of fork().
             */
            while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, r, 
                              "%s", argsbuffer);            
            }
#else
	    while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
	        continue;
	    }
#endif
	    ap_kill_timeout(r);


	    /* This redirect needs to be a GET no matter what the original
	     * method was.
	     */
	    r->method = ap_pstrdup(r->pool, "GET");
	    r->method_number = M_GET;

	    /* We already read the message body (if any), so don't allow
	     * the redirected request to think it has one.  We can ignore 
	     * Transfer-Encoding, since we used REQUEST_CHUNKED_ERROR.
	     */
	    ap_table_unset(r->headers_in, "Content-Length");

	    ap_internal_redirect_handler(location, r);
	    return OK;
	}
	else if (location && r->status == 200) {
	    /* XX Note that if a script wants to produce its own Redirect
	     * body, it now has to explicitly *say* "Status: 302"
	     */
	    return REDIRECT;
	}

	ap_send_http_header(r);
	if (!r->header_only) {
	    ap_send_fb(script_in, r);
	}
	ap_bclose(script_in);

	ap_soft_timeout("soaking script stderr", r);
#if defined(WIN32) || defined(NETWARE)
        /* Script output to stderr is already directed to the error log
         * on Unix, thanks to the magic of fork().
         */
        while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, r, 
                          "%s", argsbuffer);            
        }
#else
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
	    continue;
	}
#endif
	ap_kill_timeout(r);
	ap_bclose(script_err);
    }

    if (script_in && nph) {
	ap_send_fb(script_in, r);
    }

    return OK;			/* NOT r->status, even if it has changed. */
}

static const handler_rec suphp_handlers[] =
{
    {"x-httpd-php", suphp_handler},
    {NULL}
};

module MODULE_VAR_EXPORT suphp_module =
{
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    suphp_create_dir_config,	/* dir config creater */
    suphp_merge_dir_config,	/* dir merger --- default is to override */
    suphp_create_server_config,	/* server config */
    suphp_merge_server_config,	/* merge server config */
    suphp_cmds,			/* command table */
    suphp_handlers,		/* handlers */
    NULL,			/* filename translation */
    NULL,			/* check_user_id */
    NULL,			/* check auth */
    NULL,			/* check access */
    NULL,			/* type_checker */
    NULL,			/* fixups */
    NULL,			/* logger */
    NULL,			/* header parser */
    NULL,			/* child_init */
    NULL,			/* child_exit */
    NULL			/* post read-request */
};
