CREATE FUNCTION pgscws_start(internal, int4)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pgscws_getlexeme(internal, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pgscws_end(internal)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION pgscws_lextype(internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE TEXT SEARCH PARSER scws (
    START    = pgscws_start,
    GETTOKEN = pgscws_getlexeme,
    END      = pgscws_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = pgscws_lextype
);

CREATE TEXT SEARCH CONFIGURATION scwscfg (PARSER = scws);

COMMENT ON TEXT SEARCH CONFIGURATION scwscfg IS 'configuration for chinese language';

ALTER TEXT SEARCH CONFIGURATION scwscfg ADD MAPPING FOR n,v,a,i,e,l WITH simple;
