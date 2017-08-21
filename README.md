# MulticoreParallelSimplex
// ----------------------------------------------------------------------------
/**
 * @file  parallel_simplex.cpp
 * @author Demétrios A. M. Coutinho
 * @email demetrios.coutinho@ifrn.edu.br
 * @author Samuel Xavier de Souza
 * @email samuel@dca.ufrn.edu.br
 * @date    08/2017
 * 
 * @brief This file contains a multicore parallel implementation of the standard
 *  simplex algorithm for solving linear programming problems using OpenMP.
 * @language: C++
 * 
 * @section Description
 *  The current implementation uses the standard simplex algorithm in tabular 
 *  form to parallelize. The LP(Linear Programming) problem needs to be modeled 
 *  in the standard form and can not have artificial variables. 
 * 
 *  Standard form:
 *  maximize z = c^t * x,
 *  Subject to Ax = b, 
 *             x >= 0
 *
 *  
 * @subsection Input Data
 *
 *  The input data is a file composed of the constraint matrix 'A', the vector of 
 *  coefficients of the objective function 'c' and the vector of independent values 'b', 
 *  as shown below. So, the independent values are in the last column and the objectives 
 *  values are in the last row.
 *
 *  | A  b|
 *  |-c  0|
 *
 * @subsection Compilation 
 *   To compile this file you need to run the following command:
 *
 *  g++ -Ofast simplexParallel.cpp -fopenmp -Wall -msse2 -fopt-info-vec-optimized  -ftree-vectorizer-verbose=2  -Wvector-operation-performance -o exec_name
 *
 * @subsection usage
 *   To run execute the command: 
 * 
 *   ./exec_name /PATH/name_of_input_file numb_of_threads chunk
 *
 *   chunk is a positive integer that specifies a chunk size of a chunk-sized block of loop
 *   iterations to give to each thread.
 *   The file's name needs to be like: number_of_constraintxnumber_of_variables.
 *   Example of a usage with a 2000x2000 LP problem, 16 threads and a chunk of 1000:
 * 
 *   ./exec_name 2000x2000 16 1000
 * 
 */
// ----------------------------------------------------------------------------
