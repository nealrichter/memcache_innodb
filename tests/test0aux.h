#ifndef _TEST0AUX_H
#define TEST0AUX_H

#include <stdio.h>
#include "innodb.h"

/*********************************************************************
Read a value from an integer column in an InnoDB tuple. */

ib_u64_t
read_int_from_tuple(
/*================*/
						/* out: column value */
	ib_tpl_t		tpl,		/* in: InnoDB tuple */
	const ib_col_meta_t*	col_meta,	/* in: col meta data */
	int			i);		/* in: column number */

/*********************************************************************
Print all columns in a tuple. */

void
print_tuple(
/*========*/
	FILE*		stream,			/* in: Output stream */
	const ib_tpl_t	tpl);			/* in: Tuple to print */

/*********************************************************************
Setup the InnoDB configuration parameters. */

void
test_configure(void);
/*================*/

/*********************************************************************
Generate random text upto max size. */
int
gen_rand_text(
/*==========*/
	char*		ptr,		/* in,out: text written here */
	int		max_size);	/* in: max size of ptr */

#endif /* _TEST0AUX_H */
