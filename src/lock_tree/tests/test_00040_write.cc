/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* We are going to test whether create and close properly check their input. */

#include "test.h"

int r;
toku_lock_tree* lt  = NULL;
toku_ltm*       ltm = NULL;
DB*             db  = (DB*)1;
enum { MAX_LT_LOCKS = 1000 };
uint32_t max_locks = MAX_LT_LOCKS;
uint64_t max_lock_memory = MAX_LT_LOCKS*64;
int  nums[100];

DBT _keys_left[2];
DBT _keys_right[2];
DBT* keys_left[2];
DBT* keys_right[2];

toku_point qleft, qright;
toku_interval query;
toku_range* buf;
unsigned buflen;
unsigned numfound;

static void init_query(void) {  
    init_point(&qleft,  lt);
    init_point(&qright, lt);
    
    qleft.key_payload  = (void *) toku_lt_neg_infinity;
    qright.key_payload = (void *) toku_lt_infinity;

    memset(&query,0,sizeof(query));
    query.left  = &qleft;
    query.right = &qright;
}

static void setup_tree(void) {
    assert(!lt && !ltm);
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    CKERR(r);
    assert(ltm);
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp, NULL, NULL, NULL);
    CKERR(r);
    assert(lt);
    init_query();
}

static void close_tree(void) {
    assert(lt && ltm);
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm);
        CKERR(r);
    lt = NULL;
    ltm = NULL;
}

typedef enum { null = -1, infinite = -2, neg_infinite = -3 } lt_infty;

static DBT* set_to_infty(DBT *dbt, int value) {
    if (value == infinite) return (DBT*)toku_lt_infinity;
    if (value == neg_infinite) return (DBT*)toku_lt_neg_infinity;
    if (value == null) return dbt_init(dbt, NULL, 0);
    assert(value >= 0);
    return                    dbt_init(dbt, &nums[value], sizeof(nums[0]));
}


static void lt_insert(int r_expect, char txn, int key_l,
		      int key_r, bool read_flag) {
    DBT _key_left;
    DBT _key_right;
    DBT* key_left   = &_key_left;
    DBT* key_right  = &_key_right;

    key_left  = set_to_infty(key_left,  key_l);
    key_right = set_to_infty(key_right, key_r);
    {
        assert(key_left);
        assert(!read_flag || key_right);
    }

    TXNID local_txn = (TXNID) (size_t) txn;

    if (read_flag)
        r = toku_lt_acquire_range_read_lock(lt, local_txn,
                                            key_left,
                                            key_right);
    else
        r = toku_lt_acquire_write_lock(lt, local_txn, key_left);
    CKERR2(r, r_expect);
}

static void lt_insert_read(int r_expect, char txn, int key_l, int key_r) {
    lt_insert(r_expect, txn, key_l, key_r, true);
}

static void lt_insert_write(int r_expect, char txn, int key_l) {
    lt_insert(r_expect, txn, key_l, 0, false);
}


static void lt_unlock(char ctxn) {
  int retval;
  retval = toku_lt_unlock_txn(lt, (TXNID) (size_t) ctxn);
  CKERR(retval);
}
              
static void runtest(void) {
    setup_tree();
    lt_insert_write(0, 'a', 1);
    toku_lt_verify(lt);
    lt_insert_write(0, 'a', 5);
    toku_lt_verify(lt);
    lt_insert_write(0, 'a', 20);
    toku_lt_verify(lt);
    lt_insert_write(0, 'b', 10);
    toku_lt_verify(lt);
    lt_unlock('a');
    lt_unlock('b');
    close_tree();
    
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 1);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 1);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_read (0, 'a', 1, 1);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'b', 1, 1);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_read (0, 'b', 1, 1);
    lt_insert_write(DB_LOCK_NOTGRANTED, 'a', 1);
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 3);
    lt_insert_write(0, 'a', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'b', 2, 4);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 3);
    lt_insert_write(0, 'a', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_write (DB_LOCK_NOTGRANTED, 'b', 2);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_read (0, 'b', 3, 3);
    lt_unlock('a');
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_read (0, 'b', 3, 3);
    lt_unlock('a');
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'b', 1);
    lt_insert_write(0, 'b', 2);
    lt_insert_write(0, 'b', 3);
    lt_insert_write(0, 'b', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_write(0, 'a', 6);
    lt_insert_write(0, 'a', 7);
    lt_insert_write(0, 'a', 8);
    lt_insert_write(0, 'a', 9);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'a', 3, 7);
    lt_unlock('a');
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'b', 1);
    lt_insert_write(0, 'b', 2);
    lt_insert_write(0, 'b', 3);
    lt_insert_write(0, 'b', 4);
    lt_insert_write(0, 'b', 5);
    lt_insert_write(0, 'b', 6);
    lt_insert_write(0, 'b', 7);
    lt_insert_write(0, 'b', 8);
    lt_insert_write(0, 'b', 9);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'a', 3, 7);
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 3);
    lt_insert_write(0, 'a', 4);
    lt_insert_read (0, 'a', 3, 7);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'b', 1);
    lt_insert_write(0, 'b', 2);
    lt_insert_write(0, 'b', 3);
    lt_insert_write(0, 'b', 4);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'a', 3, 7);
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 4);
    lt_insert_write(0, 'a', 5);
    lt_insert_write(0, 'a', 3);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'b', 4);
    lt_insert_write(0, 'b', 5);
    lt_insert_write(0, 'a', 3);
    lt_unlock('a');
    lt_unlock('b');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 2);
    lt_insert_write(0, 'a', 3);
    lt_insert_write(0, 'a', 4);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'b', 3, 3);
    lt_unlock('a');
    lt_insert_write(0, 'b', 3);
    lt_insert_read (DB_LOCK_NOTGRANTED, 'a', 3, 3);
    lt_unlock('b');
    lt_insert_read (0, 'a', 3, 3);
    lt_unlock('a');
    close_tree();
    /* ********************* */
    setup_tree();
    lt_insert_write(0, 'a', 1);
    lt_insert_write(0, 'a', 3);
    lt_insert_write(0, 'b', 2);
    lt_unlock('b');
    lt_unlock('a');
    close_tree();
    /* ********************* */
}


static void init_test(void) {
    unsigned i;
    for (i = 0; i < sizeof(nums)/sizeof(nums[0]); i++) nums[i] = i;

    buflen = 64;
    buf = (toku_range*) toku_malloc(buflen*sizeof(toku_range));
}

static void close_test(void) {
    toku_free(buf);
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);

    init_test();

    runtest();

    close_test();

    return 0;
}