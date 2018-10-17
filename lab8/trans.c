/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Student Name:Chen Zhiyang
 * Student ID:516030910347
 *
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
   int i,j,k;
   int tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7,tmp8;
   if(M==32&&N==32){

        for (k=0;k<32;k+=8){
            for (j=0;j<32;j+=8){
                for (i=k;i<k+8;i++){
                    tmp1 = A[i][j];
                    tmp2 = A[i][j+1];
                    tmp3 = A[i][j+2];
                    tmp4 = A[i][j+3];
                    tmp5 = A[i][j+4];
                    tmp6 = A[i][j+5];
                    tmp7 = A[i][j+6];
                    tmp8 = A[i][j+7];
                    B[j][i] = tmp1;
                    B[j+1][i] = tmp2;
                    B[j+2][i] = tmp3;
                    B[j+3][i] = tmp4;
                    B[j+4][i] = tmp5;
                    B[j+5][i] = tmp6;
                    B[j+6][i] = tmp7;
                    B[j+7][i] = tmp8;
                 }
            }

        }
   }


if (M == 64 && N == 64)
    {
    	for (j = 0; j < 64; j += 8) 
	    {
	        for (i = 0; i < 64; i+= 8) 
	        {
	        	for (k = i; k < i + 4; k++)
	        	{

	        		B[j][k] =A[k][j];
	        		B[j+1][k] = A[k][j+1];
	        		B[j+2][k] =A[k][j+2];
	        		B[j+3][k] = A[k][j+3];
	        		B[j][k+4] = A[k][j+4];
	        		B[j+1][k+4] = A[k][j+5];
	        		B[j+2][k+4] = A[k][j+6];
	        		B[j+3][k+4] = A[k][j+7];
	        	}

	        	for (k = j; k < j + 4; k++)
	        	{
	        	        tmp1 = B[k][i+4];
                                tmp2 = B[k][i+5];
                                tmp3 = B[k][i+6];
                                tmp4 = B[k][i+7];


	    
                                B[k][i+4] = A[i+4][k];
                                B[k][i+5] = A[i+5][k];
                                B[k][i+6] = A[i+6][k];
                                B[k][i+7] = A[i+7][k];

	        		tmp5 = A[i+4][k+4];
	        		tmp6 = A[i+5][k+4];
	        		tmp7 = A[i+6][k+4];
	        		tmp8 = A[i+7][k+4];

	        		B[k+4][i] = tmp1;
	        		B[k+4][i+1] = tmp2;
	        		B[k+4][i+2] = tmp3;
	        		B[k+4][i+3] = tmp4;
	        		B[k+4][i+4] = tmp5;
	        		B[k+4][i+5] = tmp6;
	        		B[k+4][i+6] = tmp7;
	        		B[k+4][i+7] = tmp8;
	        	}
	        }
	    } 
    }

   if (M == 61 && N == 67)
    {
    	for (j = 0; j < 61; j += 8) 
	    {
	        for (i = 0; i < 67; i++) {
                   for(tmp1=j;tmp1<j+8;tmp1++){
                         B[tmp1][i]=A[i][tmp1];

                      }
	        }
	    }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
   // registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

