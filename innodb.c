/***********************************************************************
Copyright (c) 2008 Innobase Oy. All rights reserved.
Copyright (c) 2008 Oracle. All rights reserved.
Copyright (c) 2010 Neal Richter. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef UNIV_DEBUG_VALGRIND
#include <valgrind/memcheck.h>
#endif

#define DATABASE	"memcache_innodb"
#define TABLE		"ib_kv1"


/*********************************************************************
Create an InnoDB database (sub-directory). */
static
ib_err_t
innodb_create_database(
/*============*/
	const char*	name)
{
	ib_bool_t	err;

	err = ib_database_create(name);
	assert(err == IB_TRUE);

	return(DB_SUCCESS);
}

/*********************************************************************
CREATE TABLE T(
	vchar_key	VARCHAR(128),
	blob_value	VARCHAR(n),
	updated		BIGINT DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
	PRIMARY KEY(vchar_key); */
static
ib_err_t
innodb_create_table(
/*=========*/
	const char*	dbname,			/* in: database name */
	const char*	name)			/* in: table name */
{
	ib_trx_t	ib_trx;
	ib_id_t		table_id = 0;
	ib_err_t	err = DB_SUCCESS;
	ib_tbl_sch_t	ib_tbl_sch = NULL;
	ib_idx_sch_t	ib_idx_sch = NULL;
	char		table_name[IB_MAX_TABLE_NAME_LEN];

#ifdef __WIN__
	sprintf(table_name, "%s/%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif
	/* Pass a table page size of 0, ie., use default page size. */
	err = ib_table_schema_create(
		table_name, &ib_tbl_sch, IB_TBL_COMPACT, 0);

	assert(err == DB_SUCCESS);

	err = ib_table_schema_add_col(
		ib_tbl_sch, "vchar_key",
		IB_VARCHAR, IB_COL_NONE, 0, 128);
	assert(err == DB_SUCCESS);

	err = ib_table_schema_add_col(
		ib_tbl_sch, "blob_value",
		IB_BLOB, IB_COL_NONE, 0, 0);
	assert(err == DB_SUCCESS);

	err = ib_table_schema_add_col(
		ib_tbl_sch, "updated",
		IB_INT, IB_COL_NONE, 0, 8);

	assert(err == DB_SUCCESS);

	err = ib_table_schema_add_index(ib_tbl_sch, "PRIMARY", &ib_idx_sch);
	assert(err == DB_SUCCESS);

	/* Set prefix length to 0. */
	err = ib_index_schema_add_col( ib_idx_sch, "vchar_key", 0);
	assert(err == DB_SUCCESS);

	err = ib_index_schema_set_clustered(ib_idx_sch);
	assert(err == DB_SUCCESS);

	err = ib_index_schema_set_unique(ib_idx_sch);
	assert(err == DB_SUCCESS);

	/* create table */
	ib_trx = ib_trx_begin(IB_TRX_REPEATABLE_READ);
	err = ib_schema_lock_exclusive(ib_trx);
	assert(err == DB_SUCCESS);

	err = ib_table_create(ib_trx, ib_tbl_sch, &table_id);
	if (err != DB_SUCCESS) {
		fprintf(stderr, "Warning: table create failed: %s\n",
				ib_strerror(err));
		err = ib_trx_rollback(ib_trx);
	} else {
		err = ib_trx_commit(ib_trx);
	}
	assert(err == DB_SUCCESS);

	if (ib_tbl_sch != NULL) {
		ib_table_schema_delete(ib_tbl_sch);
	}

	return(err);
}

/*********************************************************************
Open a table and return a cursor for the table. */
static
ib_err_t
innodb_open_table(
/*=======*/
	const char*	dbname,		/* in: database name */
	const char*	name,		/* in: table name */
	ib_trx_t	ib_trx,		/* in: transaction */
	ib_crsr_t*	crsr)		/* out: innodb cursor */
{
	ib_err_t	err = DB_SUCCESS;
	char		table_name[IB_MAX_TABLE_NAME_LEN];

#ifdef __WIN__
	sprintf(table_name, "%s/%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif
	err = ib_cursor_open_table(table_name, ib_trx, crsr);
	assert(err == DB_SUCCESS);

	return(err);
}

/*********************************************************************
UPDATE T SET blob_value = 'some_value' WHERE vchar_key = 'some_key'
 ON DUPLICATE KEY INSERT VALUES('some_key', 'some_value'); 
with implicit  updated = CURRENT_TIMESTAMP
*/

static
ib_err_t
innodb_row_upsert(
/*===============*/
	ib_crsr_t	crsr,
        char *  key_text,
        int     key_length,
        void *  value_data,
        int     value_length
)
{
	ib_err_t	err;
	int		res = ~0;
	ib_tpl_t	key_tpl;
	ib_tpl_t	old_tpl = NULL;
	ib_tpl_t	new_tpl = NULL;

	/* Create a tuple for searching an index. */
	key_tpl = ib_sec_search_tuple_create(crsr);
	assert(key_tpl != NULL);

	ptr = (char*) malloc(8192);

	/* Set the value to look for. */
	err = ib_col_set_value(key_tpl, 0, key_text, key_length);
	assert(err == DB_SUCCESS);

	/* Search for the key using the cluster index (PK) */
	err = ib_cursor_moveto(crsr, key_tpl, IB_CUR_GE, &res);
	assert(err == DB_SUCCESS
	       || err == DB_END_OF_INDEX
	       || err == DB_RECORD_NOT_FOUND);

	if (key_tpl != NULL) {
		ib_tuple_delete(key_tpl);
	}

	/* Match found */
	if (res == 0) {
		ib_i64_t	updated;
		const char*	first;
		ib_ulint_t	data_len;
		ib_ulint_t	first_len;
		ib_col_meta_t	col_meta;

		/* Create the tuple instance that we will use to update the
		table. old_tpl is used for reading the existing row and
		new_tpl will contain the update row data. */

		old_tpl = ib_clust_read_tuple_create(crsr);
		assert(old_tpl != NULL);

		new_tpl = ib_clust_read_tuple_create(crsr);
		assert(new_tpl != NULL);

		err = ib_cursor_read_row(crsr, old_tpl);
		assert(err == DB_SUCCESS);

		/* Get the first column value. */
		first = ib_col_get_value(old_tpl, 0);
		first_len = ib_col_get_meta(old_tpl, 0, &col_meta);

		/* There are no SQL_NULL values in our test data. */
		assert(first != NULL);

		/* Copy the old contents to the new tuple. */
		err = ib_tuple_copy(new_tpl, old_tpl);

		/* Update the updated column in the new tuple with now(). */
		data_len = ib_col_get_meta(old_tpl, 2, &col_meta);
		assert(data_len != IB_SQL_NULL);
		err = ib_tuple_read_i64(old_tpl, 2, &updated);
		assert(err == DB_SUCCESS);
		updated = (ib_i64_t) ((long int) time(0));  //CURRENT_TIMESTAMP

		/* Set the new updated value in the new tuple. */
		err = ib_tuple_write_i64(new_tpl, 2, updated);
		assert(err == DB_SUCCESS);

		/* Set the blob value in the new tuple. */
		err = ib_col_set_value(new_tpl, 1, value_data, value_length);
		assert(err == DB_SUCCESS);

		/* INSERT the row */
		err = ib_cursor_update_row(crsr, old_tpl, new_tpl);
		assert(err == DB_SUCCESS
		       || err == DB_DUPLICATE_KEY);

		/* Reset the old and new tuple instances. */
		old_tpl = ib_tuple_clear(old_tpl);
		assert(old_tpl != NULL);

		new_tpl = ib_tuple_clear(new_tpl);
		assert(new_tpl != NULL);
	}
        else  // no old value found
        {
		err = ib_col_set_value(new_tpl, 0, key_text, key_length);
		assert(err == DB_SUCCESS);

		err = ib_col_set_value(new_tpl, 1, value_data, value_length);
		assert(err == DB_SUCCESS);

		err = ib_tuple_write_i64(new_tpl, 2, (ib_i64_t) ((long int)time(0)));
		assert(err == DB_SUCCESS);

		err = ib_cursor_insert_row(crsr, new_tpl);
		assert(err == DB_SUCCESS || err == DB_DUPLICATE_KEY);

		new_tpl = ib_tuple_clear(new_tpl);
		assert(tpl != NULL);
        }

	if (old_tpl != NULL) {
		ib_tuple_delete(old_tpl);
	}
	if (new_tpl != NULL) {
		ib_tuple_delete(new_tpl);
	}

	return(err);
}

/* TODO innodb_get_row() */

/*********************************************************************
Drop the table. */
static
ib_err_t
innodb_drop_table(
/*=======*/
	const char*	dbname,			/* in: database name */
	const char*	name)			/* in: table to drop */
{
	ib_err_t	err;
	char		table_name[IB_MAX_TABLE_NAME_LEN];

#ifdef __WIN__
	sprintf(table_name, "%s/%s", dbname, name);
#else
	snprintf(table_name, sizeof(table_name), "%s/%s", dbname, name);
#endif
	err = ib_table_drop(table_name);

	return(err);
}

