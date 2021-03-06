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


#include <stdlib.h>
#include <cstdio>
#include <omp.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <cstring>
#include <string>

#include "time.h"


using namespace std;

double **tableau;
ofstream log_file;

struct Compare_Max {
    double val = 0;
    int index = -1;
};

struct Compare_Min {
    double val = HUGE_VAL;
    int index = -1;
};
#pragma omp declare reduction(minimo : struct Compare_Min : omp_out = omp_in.val < omp_out.val ? omp_in : omp_out)
#pragma omp declare reduction(maximo : struct Compare_Max : omp_out = omp_in.val > omp_out.val ? omp_in : omp_out)

/**
 * Alocate matrix
 * @param nL
 * @param nC
 * @return a double pointer to pointer
 */
double ** alocate_matrix(int nL, int nC) {

    double **tableau = new (nothrow) double *[nL];

    if (tableau == 0) {
        cerr << "Not possible to alocate matrix\n";
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nL; i++) {
        *(tableau + i) = new (nothrow) double[nC];
        if ((tableau + i) == 0) {
            cerr << "Not possible to alocate matrix \n";
            exit(EXIT_FAILURE);
        }
    }
    return tableau;
}

/**
 * Delete matrix
 * @param tableau
 * @param nL
 */
void delete_matrix(double **tableau, int nL) {

    if (tableau == 0) return;

    for (int i = 0; i < nL; i++) {

        delete [] tableau[i];
    }

    delete [] tableau;
}

/**
 * Calculate the time spent between a start and a end timespec structure.
 * @param start
 * @param end
 * @return 
 */
struct timespec My_diff(struct timespec start, struct timespec end) {
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

/**
 * Aux function to convert string to double
 * @param t
 * @param s
 * @param f
 * @return 
 */
template <class T>
bool from_string(T& t, const string& s, ios_base& (*f)(ios_base&)) {
    istringstream iss(s);
    return !(iss >> f >> t).fail();
}

/**
 * Function to convert the string line coefficients to double
 * @param s
 * @return 
 */
template <class T>
vector<T> string_to_vector(string s) {
    istringstream iss(s);
    vector<T> vec;
    do {
        string sub;
        iss >> sub;
        T numb;
        if (from_string<T>(numb, sub, dec)) {
            vec.push_back(numb);
        }
    } while (iss);
    return vec;
}

/**
 * Get the dimension of matrix from file name.
 * @param argv
 * @param nL
 * @param nC
 */
void get_dimension(char** argv, int &nL, int &nC) {
    int dimension[2] = {0, 0}, i = 0;

    char *pch = strtok(argv[1], "x/_");
    while (pch != NULL) {

        if (i == 1) {
            dimension[0] = atoi(pch);
        }
        if (i == 2) {
            dimension[1] = atoi(pch);
        }
        pch = strtok(NULL, "x/_");
        i++;
    }
    nL = dimension[0] + 1;
    nC = dimension[0] + dimension[1] + 1;
}

/**
 * Function to read from the file the information needed for simplex algorithm.
 * @param argv
 * @param nL
 * @param nC
 * @return 
 */
double ** read_data(char** argv, int& nL, int& nC) {

    ifstream file(argv[1]);

    log_file.open("log_cpp", ofstream::app);

    if (!file.is_open()) {
        cerr << "Error opening file";
        exit(1);
    }

    get_dimension(argv, nL, nC);

    log_file << "----" << nL << "x" << nC << "----" << endl << endl;

    double ** tableau = alocate_matrix(nL, nC);

    int lin = 0;
    string line;
    vector<int> sizes_col;
    while (getline(file, line)) {

        if (line == "") break;

        vector<double> contraint = string_to_vector<double>(line);
        copy(contraint.begin(), contraint.end(), tableau[lin]);
        lin++;
        sizes_col.push_back(contraint.size());
    }

    log_file << "lin " << lin << endl << "col:\n";

    for (uint i = 0; i < sizes_col.size(); i++) {
        log_file << i << " : " << sizes_col[i] << endl;
    }


    return tableau;
}

/**
 * Main function where is implemented the parallel simplex
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char** argv) {
    int numbThreads, constraintNumb, colNumb, ni = 0, chunk = 1;
    int count = 0, conta = 0;

    tableau = read_data(argv, constraintNumb, colNumb);

    constraintNumb--;
    colNumb--;

    from_string<int>(numbThreads, string(argv[2]), std::dec);

    omp_set_num_threads(numbThreads);
    //-----
    log_file << "numbThreads " << numbThreads << endl;

    ni = 0;

    struct timespec timeTotalInit, timeTotalEnd;

    from_string<int>(chunk, string(argv[3]), std::dec);
    log_file << "chunk " << chunk << endl;

    struct Compare_Max max;
    struct Compare_Min min;

    if (clock_gettime(CLOCK_REALTIME, &timeTotalInit)) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

#pragma omp parallel default(none) shared(min,max,chunk,numbThreads,count,tableau,conta,colNumb,constraintNumb,ni)
    {
        double pivot, pivot2, pivot3;
        int i, j;

#pragma omp for schedule(guided,chunk) reduction(maximo:max) 
        for (j = 0; j <= colNumb; j++)
            if (tableau[constraintNumb][j] < 0.0 && max.val < (-tableau[constraintNumb][j])) {
                max.val = -tableau[constraintNumb][j];
                max.index = j;
            }


#pragma omp single nowait
        max.val = 0;

        do {

#pragma omp for reduction(+:count),reduction(minimo:min) schedule(guided,chunk) //guided e dynamic testar chunck size e reduction <
            for (i = 0; i < constraintNumb; i++) {
                if (tableau[i][max.index] > 0.0) {
                    pivot = tableau[i][colNumb] / tableau[i][max.index];
                    if (min.val > pivot) {
                        min.val = pivot;
                        min.index = i;
                    }
                } else
                    count++;
            }

#pragma omp single nowait //testar com mais singles
            {
                if (count == constraintNumb) {
                    printf("Solução nao encontrada\n");
                    exit(1);
                } else
                    count = 0;
                conta = 0;
            }

            pivot = tableau[min.index][max.index];
            pivot3 = -tableau[constraintNumb][max.index];

#pragma omp barrier 
#pragma omp for 
            for (j = 0; j <= (colNumb); j++) {
                tableau[min.index][j] = tableau[min.index][j] / pivot;
            }

#pragma omp for nowait 
            for (i = 0; i < constraintNumb; i++) {
                if (i != min.index) {
                    pivot2 = -tableau[i][max.index];
#pragma GCC ivdep
                    for (j = 0; j <= colNumb; j++) {
                        tableau[i][j] = (pivot2 * tableau[min.index][j]) + tableau[i][j];
                    }
                }
            }

#pragma omp for reduction(+:conta),reduction(maximo:max) schedule(guided,chunk)
            for (j = 0; j <= colNumb; j++) {
                tableau[constraintNumb][j] = (pivot3 * tableau[min.index][j]) + tableau[constraintNumb][j];
                if (j < colNumb && tableau[constraintNumb][j] < 0.0) {
                    conta++;
                    if (max.val < (-tableau[constraintNumb][j])) {
                        max.val = -tableau[constraintNumb][j];
                        max.index = j;
                    }
                }
            }

#pragma omp single 
            {
                ni++;
                max.val = 0.0;
                min.val = HUGE_VAL;
            }
        } while (conta);
    }


    if (clock_gettime(CLOCK_REALTIME, &timeTotalEnd)) {
        perror("clock gettime");
        exit(EXIT_FAILURE);
    }

    double ONE_SECOND_IN_NANOSECONDS = 1000000000;
    struct timespec time = My_diff(timeTotalInit, timeTotalEnd);
    double processTime = (time.tv_sec + (double) time.tv_nsec / ONE_SECOND_IN_NANOSECONDS);

    printf("%f %f ", processTime / ni, processTime);
    printf("%d %f \n", ni, tableau[constraintNumb][colNumb]);

    delete_matrix(tableau, constraintNumb);
    log_file.close();
}
