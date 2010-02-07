/***********************************************************************
Copyright (c) 2008 Innobase Oy. All rights reserved.
Copyright (c) 2008 Oracle. All rights reserved.
Copyright (c) 2009 Oracle. All rights reserved.

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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#ifdef __WIN__
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "test0aux.h"

/* Runtime config */
static const char log_group_home_dir[] = "log";
static const char data_file_path[] = "ibdata1:32M:autoextend";

static
void
create_directory(
/*=============*/
	const char*	path)
{
#ifdef __WIN__
	BOOL		ret;

	ret = CreateDirectory((LPCTSTR) path, NULL);

	if (ret == 0 && GetLastError() != ERROR_ALREADY_EXISTS) {
		fprintf(stderr, "CreateDirectory failed for: %s\n", path);
		exit(EXIT_FAILURE);
	}
#else
	int		ret;

	/* Try and create the log sub-directory */
	ret = mkdir(path, S_IRWXU);

	/* Note: This doesn't catch all errors. EEXIST can also refer to
	dangling symlinks. */
	if (ret == -1 && errno != EEXIST) {
		perror(path);
		exit(EXIT_FAILURE);
	}
#endif
}

/*****************************************************************
Read a value from an integer column in an InnoDB tuple. */

ib_u64_t
read_int_from_tuple(
/*================*/
						/* out: column value */
	ib_tpl_t		tpl,		/* in: InnoDB tuple */
	const ib_col_meta_t*	col_meta,	/* in: col meta data */
	int			i)		/* in: column number */
{
	ib_u64_t		ival = 0;

	switch (col_meta->type_len) {
	case 1: {
		ib_u8_t		v;

		ib_col_copy_value(tpl, i, &v, sizeof(v));

		ival = (col_meta->attr & IB_COL_UNSIGNED) ? v : (ib_i8_t) v;
		break;
	}
	case 2: {
		ib_u16_t	v;

		ib_col_copy_value(tpl, i, &v, sizeof(v));
		ival = (col_meta->attr & IB_COL_UNSIGNED) ? v : (ib_i16_t) v;
		break;
	}
	case 4: {
		ib_u32_t	v;

		ib_col_copy_value(tpl, i, &v, sizeof(v));
		ival = (col_meta->attr & IB_COL_UNSIGNED) ? v : (ib_i32_t) v;
		break;
	}
	case 8: {
		ib_col_copy_value(tpl, i, &ival, sizeof(ival));
		break;
	}
	default:
		assert(IB_FALSE);
	}

	return(ival);
}

/*********************************************************************
Print all columns in a tuple. */

void
print_int_col(
/*==========*/
	FILE*		stream,
	const ib_tpl_t	tpl,
	int		i,
	ib_col_meta_t*	col_meta)
{
	ib_err_t	err = DB_SUCCESS;

	switch (col_meta->type_len) {
	case 1: {
		if (col_meta->attr & IB_COL_UNSIGNED) {
			ib_u8_t		u8;

			err = ib_tuple_read_u8(tpl, i, &u8);
			fprintf(stream, "%u", u8);
		} else {
			ib_i8_t		i8;

			err = ib_tuple_read_i8(tpl, i, &i8);
			fprintf(stream, "%d", i8);
		}
		break;
	}
	case 2: {
		if (col_meta->attr & IB_COL_UNSIGNED) {
			ib_u16_t	u16;

			err = ib_tuple_read_u16(tpl, i, &u16);
			fprintf(stream, "%u", u16);
		} else {
			ib_i16_t	i16;

			err = ib_tuple_read_i16(tpl, i, &i16);
			fprintf(stream, "%d", i16);
		}
		break;
	}
	case 4: {
		if (col_meta->attr & IB_COL_UNSIGNED) {
			ib_u32_t	u32;

			err = ib_tuple_read_u32(tpl, i, &u32);
			fprintf(stream, "%u", u32);
		} else {
			ib_i32_t	i32;

			err = ib_tuple_read_i32(tpl, i, &i32);
			fprintf(stream, "%d", i32);
		}
		break;
	}
	case 8: {
		if (col_meta->attr & IB_COL_UNSIGNED) {
			ib_u64_t	u64;

			err = ib_tuple_read_u64(tpl, i, &u64);
			fprintf(stream, "%lu", u64);
		} else {
			ib_i64_t	i64;

			err = ib_tuple_read_i64(tpl, i, &i64);
			fprintf(stream, "%ld",  i64);
		}
		break;
	}
	default:
		assert(0);
		break;
	}
	assert(err == DB_SUCCESS);
}

/**********************************************************************
Print character array of give size or upto 256 chars */

void
print_char_array(
/*=============*/
	FILE*		stream,	/* in: stream to print to */
	const char*	array,	/* in: char array */
	int		len)	/* in: length of data */
{
	int		i;
	const char*	ptr = array;

	for (i = 0; i < len; ++i) {
		fprintf(stream, "%c", *(ptr + i));
	}
}

/*********************************************************************
Print all columns in a tuple. */

void
print_tuple(
/*========*/
	FILE*		stream,
	const ib_tpl_t	tpl)
{
	int		i;
	int		n_cols = ib_tuple_get_n_cols(tpl);

	for (i = 0; i < n_cols; ++i) {
		ib_ulint_t	data_len;
		ib_col_meta_t	col_meta;

		data_len = ib_col_get_meta(tpl, i, &col_meta);

		/* Skip system columns. */
		if (col_meta.type == IB_SYS) {
			continue;
		/* Nothing to print. */
		} else if (data_len == IB_SQL_NULL) {
			fprintf(stream, "|");
			continue;
		} else {
			switch (col_meta.type) {
			case IB_INT: {
				print_int_col(stream, tpl, i, &col_meta);
				break;
			}
			case IB_FLOAT: {
				float	v;

				ib_tuple_read_float(tpl, i, &v);
				fprintf(stream, "%f", v);
				break;
			}
			case IB_DOUBLE: {
				double	v;

				ib_tuple_read_double(tpl, i, &v);
				fprintf(stream, "%lf", v);
				break;
			}
			case IB_BLOB:
			case IB_VARCHAR: {
				const char*	ptr;

				ptr = ib_col_get_value(tpl, i);
				fprintf(stream, "%d:", (int) data_len);
				print_char_array(stream, ptr, (int) data_len);
				break;
			}
			default:
				assert(IB_FALSE);
				break;
			}
		}
		fprintf(stream, "|");
	}
	fprintf(stream, "\n");
}

/*********************************************************************
Setup the InnoDB configuration parameters. */

void
test_configure(void)
/*================*/
{
	ib_bool_t	ret;

	create_directory(log_group_home_dir);

#ifndef __WIN__
	ret = ib_cfg_set_text("flush_method", "O_DIRECT");
	assert(ret);
#else
	ret = ib_cfg_set_text("flush_method", "async_unbuffered");
	assert(ret);
#endif

	ret = ib_cfg_set_int("mirrored_log_groups", 2);
	assert(ret);

	ret = ib_cfg_set_int("log_files_in_group", 2);
	assert(ret);

	ret = ib_cfg_set_int("log_file_size", 32 * 1024 * 1024);
	assert(ret);

	ret = ib_cfg_set_int("log_buffer_size", 24 * 16384);
	assert(ret);

	ret = ib_cfg_set_int("buffer_pool_size", 5 * 1024 * 1024);
	assert(ret);

	ret = ib_cfg_set_int("additional_mem_pool_size", 4 * 1024 * 1024);
	assert(ret);

	ret = ib_cfg_set_int("flush_log_at_trx_commit", 0);
	assert(ret);

	ret = ib_cfg_set_int("file_io_threads", 4);
	assert(ret);

	ret = ib_cfg_set_int("lock_wait_timeout", 60);
	assert(ret);

	ret = ib_cfg_set_int("open_files", 300);
	assert(ret);

	ret = ib_cfg_set_bool_on("doublewrite");
	assert(ret);

	ret = ib_cfg_set_bool_on("checksums");
	assert(ret);

	ret = ib_cfg_set_bool_on("rollback_on_timeout");
	assert(ret);

	ret = ib_cfg_set_bool_on("print_verbose_log");
	assert(ret);

	ret = ib_cfg_set_bool_on("file_per_table");
	assert(ret);

	ret = ib_cfg_set_text("data_home_dir", ".");
	assert(ret);

	ret = ib_cfg_set_text("log_group_home_dir", log_group_home_dir);

	if (!ret) {
		fprintf(stderr,
			"syntax error in log_group_home_dir, or a "
			  "wrong number of mirrored log groups\n");
		exit(1);
	}


	ret = ib_cfg_set_text("data_file_path", data_file_path);

	if (!ret) {
		fprintf(stderr,
			"InnoDB: syntax error in data_file_path\n");
		exit(1);
	}
}

/*********************************************************************
Generate random text upto max size. */
int
gen_rand_text(
/*==========*/
	char*		ptr,		/* in,out: text written here */
	int		max_size)	/* in: max size of ptr */
{
	int		i;
	int		len = 0;
	static char 	txt[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			        "abcdefghijklmnopqrstuvwxyz"
			        "0123456789";

	do {
#ifdef __WIN__
		len = rand() % max_size;
#else
		len = random() % max_size;
#endif
	} while (len == 0);

	for (i = 0; i < len; ++i, ++ptr) {
#ifdef __WIN__
		*ptr = txt[rand() % (sizeof(txt) - 1)];
#else
		*ptr = txt[random() % (sizeof(txt) - 1)];
#endif
	}

	return(len);
}

