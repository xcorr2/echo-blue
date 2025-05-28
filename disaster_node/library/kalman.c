
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "kalman.h"
#include <math.h>

// struct to define the paramters of the kalman filter and store matrices
typedef struct {
        double ndim;
        double** A;
        double** H;
        double** x_hat;
        double** cov;
        double** Q;
        double** R;
        int m;
        int n;
} Kalman;

// define a matrix by a pointer to the matrix as well as the number of rows and 
// columns
struct matrix {
    double** matrix;
    int rows;
    int cols;
};

// fifo for sending data to and from kalman filter thread
struct k_fifo kalman_us_fifo;
struct k_fifo kalman_rs_fifo;
struct k_fifo kalman_rssi_fifo;
struct k_fifo signal_fifo;

struct k_sem signal;

void create_filter();
    
/*
* print_matrix()
* given a matrix and the number of rows and columns, will print each cell. 
* Used for testing and debugging.
*/
void print_matrix(double** matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%6.2f ", matrix[i][j]);
        }
        printf("\n");
    }
}
    
/*
* identity()
* given a value ndim, will create a ndim by ndim size identity matrix.
*/   
double** identity(double ndim)
{
    // allocate space for matrix
    double** identity_matrix = (double**)k_malloc(sizeof(double*) * ndim);
    
    for (int i = 0; i < ndim; i++) {
        // allocate space for row
        identity_matrix[i] = (double*)k_malloc(sizeof(double) * ndim);
        for (int j = 0; j < ndim; j++) {
            // assign value to each cell in matrix.
            if (i == j) {
                // index is along diagonal of square matrix. Set as 1
                identity_matrix[i][j] = 1;
            } else {
                // index is not along diagonal. Set a 0
                identity_matrix[i][j] = 0;
            }
        }
    }    
    return identity_matrix;
}
    
/*
* allocate_matrix()
* given a number of rows and columns, will allocate the necessary space for the
* that matrix, initialising each element to 0.
*/
double** allocate_matrix(int rows, int cols) {
    double** matrix = (double**)k_malloc(rows * sizeof(double*));
    for (int i = 0; i < rows; i++) {
        // allocate space for columns per row, intialising value to 0
        matrix[i] = (double*)k_calloc(cols, sizeof(double));
    }
    return matrix;
}

/*
* free_matrix()
* given a matrix and number of rows, will free the allocated space for that 
* matrix.
*/
void free_matrix(double** matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        //printf("test: %f\n", matrix[0][0]);
        k_free(matrix[i]);
    }
    k_free(matrix);
}

/*
* matrix_mult()
* given two matrices of different sizes, will perform matrix multiplication on 
* them and return the result.
* Assumptions - matrices can be different sizes so long as the number of 
* columns in matrix a is equal to the number of rows in matrix b.asm
* Result size - the matrix returned will have as many rows as there are rows in
* matrix a, with as many columns as there are in matrix b
* REF: https://www.programiz.com/c-programming/examples/matrix-multiplication
*/
double** matrix_mult(double** a, double** b, int num_rows, int num_cols, int num_cols_2) 
{
    // allocate memory for new matrix
    double** result = allocate_matrix(num_rows, num_cols_2);
    for (int i = 0; i < num_rows; i++) {
        for (int j = 0; j < num_cols_2; j++) {
            for (int k = 0; k < num_cols; k++) {
                // add result of multiplcation to current cell
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}
    
/*
* matrix_addition()
* given two matrices a and b, will perform matrix addition on the two and 
* return the result. 
* Assumptions: the size of matrix a must equal the size of matrix b
*/
double** matrix_addition(double** a, double** b, double num_rows, double num_cols) 
{
    double** result = allocate_matrix(num_rows, num_cols);
    
    // Initializing elements of matrix mult to 0.
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            // add cell from matrix a with matrix b and store in result matrix
            result[row][col] = a[row][col] + b[row][col];
        }
    }
    return result;
}
    
/*
* matrix_subtraction()
* given two matrices a and b, will perform matrix subtraction and return the
* result.
* Assumptions: the size of matrix a must equal the size of matrix b
*/
double** matrix_subtraction(double** a, double** b, double num_rows, double num_cols) 
{
    double** result = allocate_matrix(num_rows, num_cols);
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            // subtract cell from matrix a with matrix b and store in result matrix
            result[row][col] = a[row][col] - b[row][col];
        }
    }
    return result;
}
    
/*
* transpose()
* given a matrix and its dimensions, will transpose the matrix and return
* the result. E.g, a matrix of size 2x4 as the input will return a matrix
* of size 4x2 with the same contents inside.
* REF: https://www.geeksforgeeks.org/c-transpose-matrix/
*/
double** transpose(double** a, double num_rows, double num_cols) 
{
    double** transposed_matrix = allocate_matrix(num_cols, num_rows);
    
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            // take the elements from the position (row,col) in input matrix 
            // and store in position (col,row) in result matrix
            transposed_matrix[col][row] = a[row][col];
        }
    }
    return transposed_matrix;
}
    
/*
* scale_matrix()
* given a matrix and its dimension, will multiply each cell by a value specified
* from inputs. E.g, with a scaler of 5 to a matrix of 2x2, each of the 4 cells
* will be multiplied by 5 individually.
*/
double** scale_matrix(double** a, double scaler, int num_rows, int num_cols) 
{
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            // scale matrix value by scaler
            a[row][col] = a[row][col] * scaler;
        }
    }
    return a;
}
    
/*
* inverse_matrix()
* given a matrix 'a', will compute the inverse and return the result.
* Assumptions: matrix must be a 2x2 matrix.
* E.g 
*    a = | a  b |   ->  inverse(a) = 1 / (a*d - b*c) * | d  -b |
*        | c  d |                                      | -c  a |
*/
double** inverse_matrix(double** a) 
{
    // compute the determinant (1 / (a*d - b*c))
    double determinant = 1 / (a[0][0] * a[1][1] - a[0][1] * a[1][0]);

    // compute and store the first cell ("a")
    double A = a[0][0] * determinant;
    // switch the elements "a" and "d"
    a[0][0] = a[1][1] * determinant;
    a[1][1] = A;

    // multiply b and c by determinant and -1
    a[0][1] = (-1) * determinant;
    a[1][0] = (-1) * determinant;
    
    return a;
}

/*
* determinant3x3()
* computes and returns the determinant of a 3x3 matrix gives each cells
* REF: https://www.semath.info/src/inverse-cofactor-ex4.html
*/
double determinant3x3 (double a, double b, double c, double d, double e, double f, double g, double h, double i) {
    return (a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g));
}

/*
* compute_3x3()
* given a 4x4 matrix a, will return a 3x3 matrix M_ij, where no
* cell in the matrix will contain a row or column from matrix A which
* is from row i or column j
* REF: https://www.semath.info/src/inverse-cofactor-ex4.html
*/
double** compute_3x3(double** A, int i, int j) 
{
    // create matrix
    double** three_x_three = allocate_matrix(3,3);

    // row2 is the index in the three by three matrix.
    // row is the index in the 4x4
    int row2 = 0;
    for (int row = 0; row < 4; row++) {
        // skip if row is equal to j
        if (row != j ){
            // col2 is index in 3x3 matrix
            // col is index in 4x4
            int col2 = 0;
            for (int col = 0; col < 4; col++) {
                // skip if column is equal to i
                if(col != i){
                    three_x_three[row2][col2++] = A[row][col];
                }
            }
            row2 += 1;
        }
    }
    return three_x_three;
}

/*
* inverse_4x4()
* given a 4x4 matrix, compute its inverse
* REF: https://www.semath.info/src/inverse-cofactor-ex4.html
*/
double** inverse_4x4(double** A) {
    // calculate the determinant of the 4x4 matrix
    // computed by calculating the determinant of matrices
    // M_11, M_21, M_31 and M_41 and summing results together
    double det = 0.0;
    for (int i = 0; i < 4; i++){
        // compute 3x3 matrix of M_i1
        double** three_x_three = compute_3x3(A, i, 0);
        // calculate the determinant
        double det3x3 = determinant3x3(
            three_x_three[0][0], three_x_three[0][1], three_x_three[0][2],
            three_x_three[1][0], three_x_three[1][1], three_x_three[1][2],
            three_x_three[2][0], three_x_three[2][1], three_x_three[2][2]
        );
        // add determinant to total. If odd number row (i), multiply by -1
        det += det3x3 * pow(-1, i) * A[0][i];
        free_matrix(three_x_three, 3);
    }

    // create a 4x4 matrix to store inverse
    double** inv = allocate_matrix(4, 4);
    double inv_det = 1.0 / det;

    // Inverse is calculated using 1 / determinant (inv_det).
    // use this value to calculate inverse of matrix A 
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            // compute 3x3 M_(row,col)
            double** three_x_three = compute_3x3(A, col, row);

            // compute determinant
            double det3x3 = determinant3x3(
                three_x_three[0][0], three_x_three[0][1], three_x_three[0][2],
                three_x_three[1][0], three_x_three[1][1], three_x_three[1][2],
                three_x_three[2][0], three_x_three[2][1], three_x_three[2][2]
            );
            // calculate new value for 4x4 matrix cell 
            inv[col][row] = pow(-1, row + col) * det3x3 * inv_det; 
            free_matrix(three_x_three, 3);
        }
    }

    return inv;
}
    
/*
* init_kalman()
* initialise a kalman filter stuct
* REF: https://thekalmanfilter.com/kalman-filter-explained-simply/
*/
void init_kalman(Kalman* filter, double ndim, double** x_init, double** cov_init, double meas_err_rssi_x, double meas_err_rssi_y, double meas_err_us_x, double meas_err_us_y, double proc_err, double dt, int m, int n) 
{
    // dimensions of filter (4x4)
    filter->ndim = ndim;
    // State transition matrix
    filter->A = (double**)k_malloc(sizeof(double*)*n);

    // create matrix A and add to struct
    double temp_A[4][4] = {
        {1, 0, dt, 0}, 
        {0, 1, 0, dt}, 
        {0, 0, 1, 0}, 
        {0, 0, 0, 1}};

    for (int i = 0; i < 4; i++) {
        filter->A[i] = (double*)k_malloc(sizeof(double)*4);
        for (int j = 0; j < 4; j++) {
            filter->A[i][j] = temp_A[i][j];
        }
    }

    // state-to-measurement matrix
    filter->H = (double**)k_malloc(sizeof(double*)*4);
    double temp_H[4][4] = {
        {1, 0, 0, 0}, 
        {0, 1, 0, 0},
        {1, 0, 0, 0},
        {0, 1, 0, 0}}; 

    for (int i = 0; i < 4; i++) {
        filter->H[i] = (double*)k_malloc(sizeof(double)*4);
        for (int j = 0; j < 4; j++) {
            filter->H[i][j] = temp_H[i][j];
        }
    }

    // current position
    filter->x_hat = x_init;
    // covariance matrix
    filter->cov = cov_init;
    // process noise matrix
    filter->Q = scale_matrix(identity(ndim), proc_err, ndim, ndim);
    // measurement error matrix
    filter->R = (double**)k_malloc(sizeof(double*)*4);
    double temp_r[4][4] = {
        {meas_err_us_x, 0, 0, 0},
        {0, meas_err_us_y, 0, 0},
        {0, 0, meas_err_rssi_x, 0},
        {0, 0, 0, meas_err_rssi_y}};

    for (int i = 0; i < 4; i++) {
        filter->R[i] = (double*)k_malloc(sizeof(double)*4);
        for (int j = 0; j < 4; j++) {
            filter->R[i][j] = temp_r[i][j];
        }
    }
    filter->m = m;
    filter->n = n;
}
   
/*
* update()
* given an observation with new position and velocity, update state matrices to
* predict new matrix
* REF: csse4011/tracking.py
*/
void update(double** obs, Kalman* filter) 
{
    // x_hat_est 4x1
    // 4x4 * 4x1 = 4x1
    double** x_hat_est = matrix_mult(filter->A, filter->x_hat, filter->n, filter->n, 1);

//    printf("test1\n");

    // cov_est 4x4
    // (4x4 * (4x4 * 4x4)) + 4x4
    // A*(cov*A^T) + Q
    
    double** AT = transpose(filter->A, filter->n, filter->n);
    double** cov_AT = matrix_mult(filter->cov, AT, 4, 4, 4);
    double** A_cov_AT = matrix_mult(filter->A, cov_AT, 4, 4, 4);
    double** cov_est = matrix_addition(A_cov_AT, filter->Q, 4, 4);
    free_matrix(AT, 4);
    //printf("test1.3\n");

    free_matrix(cov_AT, 4);
    free_matrix(A_cov_AT, 4);
    //printf("test2\n");



    //double** cov_est = matrix_addition(matrix_mult(filter->A, matrix_mult(filter->cov, transpose(filter->A, filter->n, filter->n), 4, 4, 4), filter->n, filter->n, 4), filter->Q, 4, 4);

    // error_x 4x1
    // 4x1 - (4x4 * 4x1)
    
    double** H_x_hat_est = matrix_mult(filter->H, x_hat_est, 4, 4, 1);
    //printf("test2.1\n");
    double** error_x =  matrix_subtraction(obs, H_x_hat_est, 4, 1);
     //   printf("test2.2\n");
    free_matrix(H_x_hat_est, 4);
     //   printf("test2.3\n");


    //double** error_x = matrix_subtraction(obs, matrix_mult(filter->H, x_hat_est, 4, 4, 1), 4, 1);
     //   printf("test3\n");


    // error_cov 4x4
    // 4x4 * (4x4 * 4x4) == 4x4
    // 4x4 + 4x4
    double** H_cov_est = matrix_mult(filter->H, cov_est, 4, 4, 4);
    double** HT = transpose(filter->H, filter->n, filter->n);
    double** H_cov_est_HT = matrix_mult(H_cov_est, HT, 4, 4, 4);
    double** error_cov = matrix_addition(H_cov_est_HT, filter->R, filter->n, filter->n);
    //k_free(H_cov_est[0]);
    //print_matrix(H_cov_est, 4, 4);
    //printf("%f\n", H_cov_est[0][0]);
    //printf("test\n");
    free_matrix(H_cov_est, 4);
    //free_matrix(HT, 4);
    free_matrix(H_cov_est_HT, 4);
      //  printf("test4\n");


    //double** error_cov = matrix_addition(matrix_mult(matrix_mult(filter->H, cov_est, 4, 4, 4), transpose(filter->H, filter->n, filter->n), filter->n, filter->n, filter->n), filter->R, filter->n, filter->n);

    // K 4x4
    // (4x4 * 4x4) * 4x4)
    double** cov_est_HT = matrix_mult(cov_est, HT, 4, 4, 4);
    double** inv_error_cov = inverse_4x4(error_cov);
    double** K = matrix_mult(cov_est_HT, inv_error_cov, 4, 4, 4);
    free_matrix(cov_est_HT, 4);
    free_matrix(inv_error_cov, 4);
    free_matrix(HT, 4);
     //   printf("test5\n");

    

    //double** K = matrix_mult(matrix_mult(cov_est, transpose(filter->H, 4, 4), 4, 4, 4), inverse_4x4(error_cov), 4, 4, 4);

    // x_hat 4x1
    // 4x1 + (4x4 * 4x1 = 4 * 1)
    
    double** K_error_x = matrix_mult(K, error_x, 4, 4, 1);
    free_matrix(filter->x_hat, 4);
    filter->x_hat =  matrix_addition(x_hat_est, K_error_x, 4, 1);
    free_matrix(K_error_x, 4);
       // printf("test6\n");

    
    //filter->x_hat = matrix_addition(x_hat_est, matrix_mult(K, error_x, 4, 4, 1), 4, 1);

    if (filter->ndim > 1) {
        // 4x4
        // (4x4 - 4x4 * 4x4) * 4x4
        
        double** K_H = matrix_mult(K, filter->H, filter->n, filter->n, filter->n);
        double** ndim_identity = identity(filter->ndim);
        double** ndim_min_K_H = matrix_subtraction(ndim_identity, K_H, 4, 4);
        free_matrix(filter->cov, 4);
        filter->cov = matrix_mult(ndim_min_K_H, cov_est, 4, 4, 4);
        free_matrix(K_H, 4);
        free_matrix(ndim_identity, 4);
        free_matrix(ndim_min_K_H, 4);

        //filter->cov = matrix_mult(matrix_subtraction(identity(filter->ndim), matrix_mult(K, filter->H, filter->n, filter->n, filter->n), 4, 4), cov_est, 4, 4, 4);
    } else {
        // create matrix of size 4x4 with all values set to 1
        double** all_ones = (double**)k_malloc(sizeof(double*) * 4);
        for (int i = 0; i < 4; i++) {
            all_ones[i] = (double*)k_malloc(sizeof(double)*4);
            all_ones[i][0] = 1;
            all_ones[i][1] = 1;
            all_ones[i][2] = 1;
            all_ones[i][3] = 1;
        }
        // ( 4x4 - 4x4 ) * 4x4
        double** ones_K = matrix_subtraction(all_ones, K, 4, 4);
        free_matrix(filter->cov, 4);
        filter->cov = matrix_mult(ones_K, cov_est, 4, 4, 4);
        //filter->cov = matrix_mult(matrix_subtraction(all_ones, K, 4, 4), cov_est, 4, 4, 4);
        free_matrix(ones_K, 4);
        free_matrix(all_ones, 4);
    }  
 //   printf("test7\n");
    free_matrix(x_hat_est, 4);
    free_matrix(cov_est, 4);
    free_matrix(error_x, 4);
    free_matrix(error_cov, 4);
    free_matrix(K, 4);
}

/*
* kalman_filter()
* thread function that handles creation of kalman filter as well as updating
*/    
void kalman_filter(int x, int y, int vx, int vy, int dt, int num_steps) {
    Kalman* filter = (Kalman*)k_malloc(sizeof(Kalman));
    // init dimensions
    int m = 2; //num measurements
    int n = 4; // num variables (x,y,vx,vy)
    int ndim = 4;

    // define error rates
    double proc_err = 0.3; // processing error
    double init_err = 10; // init position error
    double meas_err_rssi_x = 0.01; // ultrasonic x error
    double meas_err_us_x = 1000; // rssi x error
    double meas_err_rssi_y = 1000; // ultrasonic y error
    double meas_err_us_y = 0.01; // rssi y error

    double** obs = (double**)k_malloc(sizeof(double*)*4);
    obs[0] = (double*)k_malloc(sizeof(double));
    obs[1] = (double*)k_malloc(sizeof(double));
    obs[2] = (double*)k_malloc(sizeof(double));
    obs[3] = (double*)k_malloc(sizeof(double));

    double** x_init = (double**)k_malloc(sizeof(double*)*4);
    for (int i =0; i < 4; i++) {
        x_init[i] = (double*)k_malloc(sizeof(double)*1);
    }
    x_init[0][0] = 1;//init_x;
    x_init[1][0] = 1;//init_y;
    x_init[2][0] = 1;//init_vx;
    x_init[3][0] = 1;//init_vy; 
    double** cov_init = scale_matrix(identity(ndim), init_err, ndim, ndim);
    
    init_kalman(filter, ndim, x_init, cov_init, meas_err_rssi_x, meas_err_rssi_y, meas_err_us_x, meas_err_us_y, proc_err, dt, m, n);
    
    double** x_hat = (double**)k_malloc(sizeof(double*)*1);
    x_hat[0] = (double*)k_malloc(sizeof(double)*4);

    int flag = 0;
    int json_flag = 0;

    // start observations at 0,0 to be overwritten later with actual data
    obs[0][0] = 1;
    obs[1][0] = 1;
    obs[2][0] = 1;
    obs[3][0] = 1;
    //update(obs, filter);
    double orig_x = 0;
    double orig_y = 0;
    double x_diff = 0.3;
    double y_diff = 0.3;

    while(1) {
        //printf("%d\n",k_heap_stats_get());

        struct kalman_values* rx_data;
        // check for rssi data
        if (!k_fifo_is_empty(&kalman_rs_fifo)){
            // data recieved, limit any values to not be read if outside grid
            rx_data = k_fifo_get(&kalman_rs_fifo, K_FOREVER);
            if (rx_data->x <= 4){
                obs[0][0] = 1;//rx_data->x;
            }
            if (rx_data->y <= 4){
                obs[1][0] = rx_data->y;
            }
            // indicate new data has been read
            flag = 1;
            //printf("rs reading: x: %f, y: %f\n", obs[0][0], obs[1][0]);
            //printf("json flag 1: %d\n", json_flag);
        }
        // check for ultrasonic sensor data
        if (!k_fifo_is_empty(&kalman_us_fifo)){
            // data recieved, limit any values to not be read if outside grid
            rx_data = k_fifo_get(&kalman_us_fifo, K_FOREVER);
            if (rx_data->x <= 4){
                obs[2][0] = rx_data->x;
            }
            if (rx_data->y <= 4){
                obs[3][0] = 1;//rx_data->y;
            }
            flag = 1;
            //printf("us reading: x: %f, y: %f\n", obs[2][0], obs[3][0]);
        }
        // check if any new observations were made
        if (flag) {
            // new observation, update filter and reset flag
            flag = 0;
            //printf("obs:\n%f\n%f\n%f\n%f\n", obs[0][0], obs[1][0], obs[2][0], obs[3][0]);
            update(obs, filter);

            x_diff -= filter->x_hat[0][0] - orig_x;
            y_diff -= filter->x_hat[0][0] - orig_y;

            if (x_diff < 0 || y_diff < 0) {
                k_sem_give(&signal);
            }
            //printf("x: %f, y: %f\n", filter->x_hat[0][0], filter->x_hat[1][0]);
            k_free(rx_data);
        }
        k_msleep(500);
    }
}    

// initalise filter
void create_filter()
{
        kalman_filter(0,0,0,0,1,100);
}
