#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

#include "gt_include.h"


#define ROWS 256
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 4
#define NUM_GROUPS NUM_CPUS
#define PER_GROUP_COLS (SIZE/NUM_GROUPS)

#define NUM_THREADS 128
#define PER_THREAD_ROWS (SIZE/NUM_THREADS)

// NEW
int scheduler_type;
int matrices_sizes[4] = {32, 64, 128, 256};
int weights[4] = {25, 50, 75, 100};
long int runtime[NUM_THREADS];
long int exe_time[NUM_THREADS];

/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;


typedef struct __uthread_arg
{
	matrix_t *_A, *_B, *_C;
	unsigned int reserved0;

	unsigned int tid;
	unsigned int gid;
	int start_row; /* start_row -> (start_row + PER_THREAD_ROWS) */
	int start_col; /* start_col -> (start_col + PER_GROUP_COLS) */
	
	// NEW
	int weight;
	int matrix_size;

}uthread_arg_t;
	
struct timeval tv1;

static void generate_matrix(matrix_t *mat, int val, int matrix_size)
{

	int i,j;
	mat->rows = matrix_size;
	mat->cols = matrix_size;

	// CHANGED
	for(i = 0; i < mat->rows;i++)
		for( j = 0; j < mat->cols; j++ )
		{
			mat->m[i][j] = val;
		}
	return;
}

static void print_matrix(matrix_t *mat, int matrix_size)
{
	int i, j;

	// CHANGED
	for(i=0;i<matrix_size;i++)
	{
		for(j=0;j<matrix_size;j++)
			printf(" %d ",mat->m[i][j]);
		printf("\n");
	}

	return;
}

static void * uthread_mulmat(void *p)
{
	int i, j, k;
	int start_row, end_row;
	int start_col, end_col;
	unsigned int cpuid;
	struct timeval tv2, elapsed_runtime;

#define ptr ((uthread_arg_t *)p)

	i=0; j= 0; k=0;

	start_row = ptr->start_row;
	end_row = (ptr->start_row + PER_THREAD_ROWS);

#ifdef GT_GROUP_SPLIT
	start_col = ptr->start_col;
	end_col = (ptr->start_col + PER_THREAD_ROWS);
#else
	start_col = 0;
	end_col = SIZE;
#endif

#ifdef GT_THREADS
	cpuid = kthread_cpu_map[kthread_apic_id()]->cpuid;
	fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) started",ptr->tid, ptr->gid, cpuid);
#else
	fprintf(stderr, "\nThread(id:%d, group:%d) started",ptr->tid, ptr->gid);
#endif

	// for(i = start_row; i < end_row; i++)
	// 	for(j = start_col; j < end_col; j++)
	// 		for(k = 0; k < SIZE; k++)
	// 			ptr->_C->m[i][j] += ptr->_A->m[i][k] * ptr->_B->m[k][j];

	// CHANGED: each thread has its own matrix
	for(i = 0; i < ptr->matrix_size; i++)
		for(j = 0; j < ptr->matrix_size; j++)
			for(k = 0; k < ptr->matrix_size; k++)
				ptr->_C->m[i][j] += ptr->_A->m[i][k] * ptr->_B->m[k][j];

#ifdef GT_THREADS
	fprintf(stderr, "\nThread(id: %d , group: %d , cpu: %d ) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#else
	gettimeofday(&tv2,NULL);

	// NEW: records total runtime (ms)
	timersub(&tv2, &tv1, &elapsed_runtime);

	runtime[ptr->tid] = ((elapsed_runtime.tv_sec * 1000000) + (elapsed_runtime.tv_usec));

	fprintf(stderr, "\nThread(id: %d , group: %d ) finished (TIME : %lu s and %lu us)",
			ptr->tid, ptr->gid, elapsed_runtime.tv_sec, elapsed_runtime.tv_usec);
			// ptr->tid, ptr->gid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));

#endif

#undef ptr
	return 0;
}

matrix_t A, B, C;

// CHANGED
static void init_matrices(int matrix_size)
{
	generate_matrix(&A, 1, matrix_size);
	generate_matrix(&B, 1, matrix_size);
	generate_matrix(&C, 0, matrix_size);

	return;
}


uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];

int main(int argc, char* argv[])
{
	uthread_arg_t *uarg;
	int inx;

	// NEW
	int weight, matrix_size;

	if(argc > 2 || argc == 1)
	{
		fprintf(stderr, "Invalid command. Try: matrix <scheduler_type>\n");
		fprintf(stderr, "<scheduler_type>: 0 for default O(1) scheduler, 1 for credit based scheduler\n");
		exit(0);
	}
	else if(argc == 2)
	{
		scheduler_type = atoi(argv[1]);
		if(scheduler_type != 1 && scheduler_type != 0)
		{
			fprintf(stderr, "Invalid scheduler type. Use 0 for default O(1) scheduler, 1 for credit based scheduler\n");
			exit(0);
		}
	}
	else
	{
		fprintf(stderr, "EXIT: Unexpected Behavior\n");
		exit(0);
	}

	gtthread_app_init();

	gettimeofday(&tv1,NULL);


	// Default O(1)
	if (scheduler_type == 0)
	{
		init_matrices(SIZE);

		for(inx=0; inx<NUM_THREADS; inx++)
		{
			uarg = &uargs[inx];
			uarg->_A = &A;
			uarg->_B = &B;
			uarg->_C = &C;

			uarg->tid = inx;

			uarg->gid = (inx % NUM_GROUPS);

			uarg->start_row = (inx * PER_THREAD_ROWS);

			// NEW
			uarg->matrix_size = SIZE;

	#ifdef GT_GROUP_SPLIT
			/* Wanted to split the columns by groups !!! */
			uarg->start_col = (uarg->gid * PER_GROUP_COLS);
	#endif
			uthread_create(&utids[inx], uthread_mulmat, uarg, uarg->gid, 0);
		}
	}
	// Credit based scheduler
	else if (scheduler_type == 1)
	{
		for (inx = 0; inx < 4; inx++) {
			matrix_size = matrices_sizes[inx];
			init_matrices(matrix_size);

			int i, j;
			for (i = 0; i < 4; i++) {
				weight = weights[i];

				for (j = 0; j < 8; j++) {
					int id = 32 * inx + 8 * i + j;

					uarg = &uargs[id];
					uarg->_A = &A;
					uarg->_B = &B;
					uarg->_C = &C;
					uarg->tid = id;
					uarg->gid = (id % NUM_GROUPS);

			#ifdef GT_GROUP_SPLIT
                    /* Wanted to split the columns by groups !!! */
                    uarg->start_col = (uarg->gid * PER_GROUP_COLS);
			#endif
					uarg->start_row = 0;
                    uarg->weight = weight;
                    uarg->matrix_size = matrix_size;

                    fprintf(stderr, "Thread ID:%d, Weight:%d, Matrix Size:%d\n", uarg->tid, uarg->weight, uarg->matrix_size);
                    uthread_create(&utids[id], uthread_mulmat, uarg, uarg->gid, weight);
				}
			}
		}
	}

	gtthread_app_exit();

	fprintf(stderr, "Mission Accomplished!\n");


	// DATA OUTPUT

	if(scheduler_type == 1) {
		fprintf(stderr, "\nRUNTIME DATA\n");
		int x;

		for (x = 0; x < NUM_THREADS; x++) {
			fprintf(stderr, "tid %d : %d\n", x, runtime[x]);
		}

		fprintf(stderr, "\nEXECUTION TIME DATA\n");
		for (x = 0; x < NUM_THREADS; x++) {
			fprintf(stderr, "tid %d : %d\n", x, exe_time[x]);
		}
	}



	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
