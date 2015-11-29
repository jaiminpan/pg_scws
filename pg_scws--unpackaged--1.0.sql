-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_scws" to load this file. \quit

ALTER EXTENSION pg_scws ADD function pgscws_start(internal,integer);
ALTER EXTENSION pg_scws ADD function pgscws_getlexeme(internal,internal,internal);
ALTER EXTENSION pg_scws ADD function pgscws_end(internal);
ALTER EXTENSION pg_scws ADD function pgscws_lextype(internal);
ALTER EXTENSION pg_scws ADD text search parser scws;
