/*-------------------------------------------------------------------------
 *
 * pg_scws.c
 *	  a text search parser for Chinese
 *
 * Author: Jaimin Pan <jaimin.pan@gmail.com>
 *
 * IDENTIFICATION
 *	  pg_scws.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libscws/scws.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "utils/builtins.h"


PG_MODULE_MAGIC;

/* Output token categories */
/*
* there are 26 types in this parser,alias from a to z
* for full attr explanation,
* Please reference http://www.xunsearch.com/scws/docs.php#attr
*/

/* Start From 1 and LASTNUM is the last number */
#define LASTNUM			26

static const char *const tok_alias[] = {
	"",
	"a",
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p",
	"q",
	"r",
	"s",
	"t",
	"u",
	"v",
	"w",
	"x",
	"y",
	"z"
};

static const char *const lex_descr[] = {
	"",
	"adjective",
	"difference",
	"conjunction",
	"adverb",
	"exclamation",
	"position",
	"word root",
	"head",
	"idiom",
	"abbreviation",
	"head",
	"temp",
	"numeral",
	"noun",
	"onomatopoeia",
	"prepositional",
	"quantity",
	"pronoun",
	"space",
	"time",
	"auxiliary",
	"verb",
	"punctuation",
	"unknown",
	"modal",
	"status"
};

/*
 * types
 */

/* self-defined type */
typedef struct
{
	char	   *buffer;			/* text to parse */
	int			len;			/* length of the text in buffer */
	int			pos;			/* position of the parser */
	scws_t		scws;
	scws_res_t	res;
	scws_res_t	curr;
} ParserState;

/* copy-paste from wparser.h of tsearch2 */
typedef struct
{
	int			lexid;
	char	   *alias;
	char	   *descr;
} LexDescr;

static void pgscws_init();
static char *pgscws_get_tsearch_config_filename(const char *basename, const char *extension);

void		_PG_init(void);
void		_PG_fini(void);

/*
 * prototypes
 */
PG_FUNCTION_INFO_V1(pgscws_start);
Datum		pgscws_start(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pgscws_getlexeme);
Datum		pgscws_getlexeme(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pgscws_end);
Datum		pgscws_end(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pgscws_lextype);
Datum		pgscws_lextype(PG_FUNCTION_ARGS);


static scws_t pgscws_scws = NULL;

/* config */
static bool pgscws_dict_in_memory = false;
static int pgscws_load_dict_mem_mode = 0;

static char * pgscws_charset = "utf8";
static char * pgscws_rules = "rules.utf8.ini";
static char * pgscws_extra_dicts = "dict.utf8.xdb";

static bool pgscws_punctuation_ignore = false;
static bool pgscws_seg_with_duality = false;
static char * pgscws_multi_mode_string = "none";
static int pgscws_multi_mode = SCWS_MULTI_NONE;

static void pgscws_assign_dict_in_memory(bool newval, void *extra);
static bool pgscws_check_charset(char **newval, void **extra, GucSource source);
static void pgscws_assign_charset(const char *newval, void *extra);
static bool pgscws_check_rules(char **newval, void **extra, GucSource source);
static void pgscws_assign_rules(const char *newval, void *extra);
static bool pgscws_check_extra_dicts(char **newval, void **extra, GucSource source);
static void pgscws_assign_extra_dicts(const char *newval, void *extra);
static bool pgscws_check_multi_mode(char **newval, void **extra, GucSource source);
static void pgscws_assign_multi_mode(const char *newval, void *extra);


static void
pgscws_init()
{
	pgscws_scws = scws_new();
	if (!pgscws_scws)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Failed to init Chinese Parser Lib SCWS!\"%s\"",
						 "")));
}

/*
 * Module load callback
 */
void
_PG_init(void)
{
	if (pgscws_scws)
		return;

	pgscws_init();

	DefineCustomBoolVariable("scws.dict_in_memory",
							 "load dicts into memory",
							 NULL,
							 &pgscws_dict_in_memory,
							 false,
							 PGC_BACKEND,
							 0,
							 NULL,
							 pgscws_assign_dict_in_memory,
							 NULL);
	DefineCustomStringVariable("scws.charset",
							   "charset",
							   NULL,
							   &pgscws_charset,
							   "utf8",
							   PGC_USERSET,
							   0,
							   pgscws_check_charset,
							   pgscws_assign_charset,
							   NULL);
	DefineCustomStringVariable("scws.rules",
							   "rules to load",
							   NULL,
							   &pgscws_rules,
							   "rules.utf8.ini",
							   PGC_USERSET,
							   0,
							   pgscws_check_rules,
							   pgscws_assign_rules,
							   NULL);
	DefineCustomStringVariable("scws.extra_dicts",
							   "extra dicts files to load",
							   NULL,
							   &pgscws_extra_dicts,
							   "dict.utf8.xdb",
							   PGC_USERSET,
							   0,
							   pgscws_check_extra_dicts,
							   pgscws_assign_extra_dicts,
							   NULL);

	DefineCustomBoolVariable("scws.punctuation_ignore",
							 "set if scws ignores the puncuation",
							 "set if scws ignores the puncuation, except \\r and \\n",
							 &pgscws_punctuation_ignore,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);
	DefineCustomBoolVariable("scws.seg_with_duality",
							 "segment words with duality",
							 NULL,
							 &pgscws_seg_with_duality,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);
	DefineCustomStringVariable("scws.multi_mode",
							   "set multi mode",
							   "\"none\" for none\n"
							   "\"short\" for prefer short words\n"
							   "\"duality\" for prefer duality\n"
							   "\"zmain\" prefer most important element"
							   "\"zall\" prefer prefer all element",
							   &pgscws_multi_mode_string,
							   "none",
							   PGC_USERSET,
							   0,
							   pgscws_check_multi_mode,
							   pgscws_assign_multi_mode,
							   NULL);
}

/*
 * Module unload callback
 */
void
_PG_fini(void)
{
	if (pgscws_scws)
		scws_free(pgscws_scws);

	pgscws_scws = NULL;
}

/*
 * functions
 */
Datum
pgscws_start(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *) palloc0(sizeof(ParserState));
	scws_t scws = scws_fork(pgscws_scws);

	pst->buffer = (char *) PG_GETARG_POINTER(0);
	pst->len = PG_GETARG_INT32(1);
	pst->pos = 0;

	pst->scws = scws;
	pst->res = NULL;
	pst->curr = NULL;

	scws_set_ignore(scws, (int) pgscws_punctuation_ignore);
	scws_set_duality(scws, (int) pgscws_seg_with_duality);
	scws_set_multi(scws, pgscws_multi_mode);

	scws_send_text(scws, pst->buffer, pst->len);

	PG_RETURN_POINTER(pst);
}

Datum
pgscws_getlexeme(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *) PG_GETARG_POINTER(0);
	char	  **t = (char **) PG_GETARG_POINTER(1);
	int		   *tlen = (int *) PG_GETARG_POINTER(2);
	int			type = -1;

	if (pst->curr == NULL)
		pst->res = pst->curr = scws_get_result(pst->scws);

	/* already done the work, or no sentence */
	if (pst->res == NULL)
	{
		*tlen = 0;
		type = 0;
	}
	/* have results */
	else if (pst->curr != NULL)
	{
		scws_res_t curr = pst->curr;

		/*
		* check the first char to determine the lextype
		* if out of [0,25],then set to 'x',mean unknown type
		* so for Ag,Dg,Ng,Tg,Vg,the type will be unknown
		* for full attr explanation,visit http://www.xunsearch.com/scws/docs.php#attr
		*/
		type = (int)(curr->attr)[0];
		if (type > (int)'x' || type < (int)'a')
			type = (int)'x';
		*tlen = curr->len;
		*t = pst->buffer + curr->off;

		pst->curr = curr->next;

		/* clear for the next calling */
		if (pst->curr == NULL)
		{
			scws_free_result(pst->res);
			pst->res = NULL;
		}
	}

	PG_RETURN_INT32(type);
}

Datum
pgscws_end(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *) PG_GETARG_POINTER(0);

	if (pst->scws)
		scws_free(pst->scws);
	pfree(pst);
	PG_RETURN_VOID();
}

Datum
pgscws_lextype(PG_FUNCTION_ARGS)
{
	LexDescr   *descr = (LexDescr *) palloc(sizeof(LexDescr) * (26 + 1));
	int			i;

	for (i = 1; i <= LASTNUM; i++)
	{
		descr[i - 1].lexid = i;
		descr[i - 1].alias = pstrdup(tok_alias[i]);
		descr[i - 1].descr = pstrdup(lex_descr[i]);
	}

	descr[LASTNUM].lexid = 0;

	PG_RETURN_POINTER(descr);
}


/*
 * check_hook, assign_hook and show_hook subroutines
 */
static bool
pgscws_check_charset(char **newval, void **extra, GucSource source)
{
	char* string = *newval;

	if (pg_strcasecmp(string, "gbk") != 0 &&
		pg_strcasecmp(string, "utf8") != 0)
	{
		GUC_check_errdetail("Charset: \"%s\". Only Support \"gbk\" or \"utf8\"", string);
		return false;
	}

	return true;
}

static void
pgscws_assign_charset(const char *newval, void *extra)
{
	scws_set_charset(pgscws_scws, newval);
}

static bool
pgscws_check_rules(char **newval, void **extra, GucSource source)
{
	char	   *rawstring;
	char	    *myextra;
	char	    *ext;
	char	    *rule_path;

	if (strcmp(*newval, "none") == 0)
		return true;

	/* Need a modifiable copy of string */
	rawstring = pstrdup(*newval);

	ext = strrchr(rawstring, '.');
	if (ext && pg_strcasecmp(ext, ".ini") == 0)
	{
		*ext = '\0';
		ext++;
		rule_path = pgscws_get_tsearch_config_filename(rawstring, ext);
	}
	else
	{
		GUC_check_errdetail("Unrecognized key word: \"%s\". Must end with .ini", rawstring);
		pfree(rawstring);
		return false;
	}

	myextra = strdup(rule_path);
	*extra = (void *) myextra;
	pfree(rule_path);

	return true;
}

static void
pgscws_assign_rules(const char *newval, void *extra)
{
	char *myextra = (char *) extra;

	/* Do nothing for the boot_val default of NULL */
	if (!extra)
		return;

	scws_set_rule(pgscws_scws, myextra);
}

typedef struct
{
	int			mode;
	char		path[MAXPGPATH];
} dict_elem;

/* This is the "extra" state */
typedef struct
{
	int			num;
	dict_elem	dicts[1];
} dict_extra;

static bool
pgscws_check_extra_dicts(char **newval, void **extra, GucSource source)
{
	char	   *rawstring;
	List	   *elemlist;
	ListCell   *l;
	dict_extra *myextra;
	int			num;
	int			i;

	if (strcmp(*newval, "none") == 0)
		return true;

	/* Need a modifiable copy of string */
	rawstring = pstrdup(*newval);

	/* Parse string into list of identifiers */
	if (!SplitIdentifierString(rawstring, ',', &elemlist))
	{
		/* syntax error in list */
		GUC_check_errdetail("List syntax is invalid.");
		pfree(rawstring);
		list_free(elemlist);
		return false;
	}

	num = list_length(elemlist);
	myextra = (dict_extra *) malloc(sizeof(dict_extra) + num * sizeof(dict_elem));
	if (!myextra)
	{
		GUC_check_errdetail("Out of memory. Too many dictionary");
		pfree(rawstring);
		list_free(elemlist);
		return false;
	}

	i = 0;
	foreach(l, elemlist)
	{
		char	   *tok = (char *) lfirst(l);
		char	   *dict_path;

		int load_dict_mode = pgscws_load_dict_mem_mode;
		char * ext = strrchr(tok, '.');
		if (ext && strlen(ext) == 4)
		{
			if (pg_strcasecmp(ext, ".txt") == 0)
				load_dict_mode |= SCWS_XDICT_TXT;
			else if(pg_strcasecmp(ext, ".xdb") == 0)
				load_dict_mode |= SCWS_XDICT_XDB;
			else
			{
				GUC_check_errdetail("Unrecognized key word: \"%s\". Must end with .txt or .xdb", tok);
				pfree(rawstring);
				list_free(elemlist);
				free(myextra);
				return false;
			}

			*ext = '\0';
			ext++;
		}
		else
		{
			GUC_check_errdetail("Unrecognized key word: \"%s\". Must end with .txt or .xdb", tok);
			pfree(rawstring);
			list_free(elemlist);
			free(myextra);
			return false;
		}

		dict_path = pgscws_get_tsearch_config_filename(tok, ext);

		memcpy(myextra->dicts[i].path, dict_path, MAXPGPATH);
		myextra->dicts[i].mode = load_dict_mode;
		i++;
	}
	myextra->num = i;
	*extra = (void *) myextra;

	pfree(rawstring);
	list_free(elemlist);

	return true;
}

static void
pgscws_assign_extra_dicts(const char *newval, void *extra)
{
	dict_extra *myextra = (dict_extra *) extra;
	int i;

	/* Do nothing for the boot_val default of NULL */
	if (!extra)
		return;

	for (i = 0; i < myextra->num; i++)
	{
		int err;
		char	*dict_path = myextra->dicts[i].path;
		int		mode = myextra->dicts[i].mode;

		if (i == 0)
			err = scws_set_dict(pgscws_scws, dict_path, mode);
		else
			err = scws_add_dict(pgscws_scws, dict_path, mode);

		/* ignore error*/
		if (err != 0)
			ereport(NOTICE,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("pg_scws add dict : \"%s\" failed!",
						 dict_path)));
	}
}

static bool
pgscws_check_multi_mode(char **newval, void **extra, GucSource source)
{
	char	   *rawstring;
	List	   *elemlist;
	ListCell   *l;
	int			new_multi_mode = SCWS_MULTI_NONE;
	int		   *myextra;

	if (strcmp(*newval, "none") == 0)
	{
		new_multi_mode = SCWS_MULTI_NONE;
	}
	else if (*newval)
	{
		/* Need a modifiable copy of string */
		rawstring = pstrdup(*newval);

		/* Parse string into list of identifiers */
		if (!SplitIdentifierString(rawstring, ',', &elemlist))
		{
			/* syntax error in list */
			GUC_check_errdetail("List syntax is invalid.");
			pfree(rawstring);
			list_free(elemlist);
			return false;
		}

		foreach(l, elemlist)
		{
			char	   *tok = (char *) lfirst(l);

			if (pg_strcasecmp(tok, "short") == 0)
				new_multi_mode |= SCWS_MULTI_SHORT;
			else if (pg_strcasecmp(tok, "duality") == 0)
				new_multi_mode |= SCWS_MULTI_DUALITY;
			else if (pg_strcasecmp(tok, "zmain") == 0)
				new_multi_mode |= SCWS_MULTI_ZMAIN;
			else if (pg_strcasecmp(tok, "zall") == 0)
				new_multi_mode |= SCWS_MULTI_ZALL;
			else
			{
				GUC_check_errdetail("Unrecognized key word: \"%s\".", tok);
				pfree(rawstring);
				list_free(elemlist);
				return false;
			}
		}

		pfree(rawstring);
		list_free(elemlist);
	}

	myextra = (int *) malloc(sizeof(int));
	if (!myextra)
	{
		GUC_check_errdetail("Out of memory");
		return false;
	}

	*myextra = new_multi_mode;
	*extra = (void *) myextra;

	return true;
}

static void
pgscws_assign_multi_mode(const char *newval, void *extra)
{
	pgscws_multi_mode = *((int *) extra);
}

static void
pgscws_assign_dict_in_memory(bool newval, void *extra)
{
	if (newval)
		pgscws_load_dict_mem_mode = SCWS_XDICT_MEM;
	else
		pgscws_load_dict_mem_mode = 0;
}


/*
 * Given the base name and extension of a tsearch config file, return
 * its full path name.	The base name is assumed to be user-supplied,
 * and is checked to prevent pathname attacks.	The extension is assumed
 * to be safe.
 *
 * The result is a palloc'd string.
 */
static char *
pgscws_get_tsearch_config_filename(const char *basename,
							const char *extension)
{
	char		sharepath[MAXPGPATH];
	char	   *result;

	/*
	 * We limit the basename to contain a-z, 0-9, and underscores.	This may
	 * be overly restrictive, but we don't want to allow access to anything
	 * outside the tsearch_data directory, so for instance '/' *must* be
	 * rejected, and on some platforms '\' and ':' are risky as well. Allowing
	 * uppercase might result in incompatible behavior between case-sensitive
	 * and case-insensitive filesystems, and non-ASCII characters create other
	 * interesting risks, so on the whole a tight policy seems best.
	 */
	if (strspn(basename, "abcdefghijklmnopqrstuvwxyz0123456789_.") != strlen(basename))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid text search configuration file name \"%s\"",
						basename)));

	get_share_path(my_exec_path, sharepath);
	result = palloc(MAXPGPATH);
	snprintf(result, MAXPGPATH, "%s/tsearch_data/%s.%s",
			 sharepath, basename, extension);

	return result;
}
