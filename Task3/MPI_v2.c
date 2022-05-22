/*
$ for i in 1 2 4 8 16 ; do echo $i ; mpirun -n $i ./main_v2 ; done
1
Time: 28.676426
2
Time: 14.730973
4
Time: 8.248193
8
Time: 4.037138
16
Time: 4.229946
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define ALLOC(N) (double*)malloc(N * sizeof(double))

//int MPE_Init_log(void);
//int MPE_Finish_log(const char *filename);

int N;
double* A;
double* A_sliced;
double* b;
double* b_sliced;
double* x;
double* Ax_b;
double* Ax_b_sliced;
double denominator;
double t;
double eps;
int size;
int rank;
int batch_size;

int A_sliced_size()
{
	if (rank < size - 1)
		return batch_size;
	else
		return N - batch_size * (size - 1);
}

void FillA()
{
	int i, j;
	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			A[i * N + j] = (i == j) ? 2 : 1;
		}
	}
}

void FillV(double* v, double value)
{
	int i;
	for (i = 0; i < N; i++)
	{
		v[i] = value;
	}
}

double CalculateNorm(double* v)
{
	double sum = 0;
	int i;
	for (i = 0; i < N; i++)
	{
		sum += v[i] * v[i];
	}
	return sqrt(sum);
}

int StopCriterion()
{
	int res;
	if (rank == 0)
	{
		double numerator = CalculateNorm(Ax_b);
		res = numerator / denominator < eps;
	}
	MPI_Bcast(&res, 1, MPI_INT, 0, MPI_COMM_WORLD);
	return res;
}

void MultiplyVectorAndScalar(double* v, double s)
{
	int i;
	for (i = 0; i < N; i++)
	{
		v[i] *= s;
	}
}

void Subtract(double* res, double* v)
{
	int i;
	for (i = 0; i < N; i++)
		res[i] -= v[i];
}

void CalculateNextX()
{
	if (rank != 0)
		return;
	MultiplyVectorAndScalar(Ax_b, t);
	Subtract(x, Ax_b);
}

void MultiplyAndSubtractThread()
{
	MPI_Bcast(x, N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	
	int i, j;
	for (i = 0; i < A_sliced_size(); i++)
	{
		Ax_b_sliced[i] = -b_sliced[i];
		for (j = 0; j < N; j++)
		{
			Ax_b_sliced[i] += A_sliced[i * N + j] * x[j];
		}
	}
}

void MultiplyMatrixAndVectorAndSubtract()
{
	MultiplyAndSubtractThread();
	MPI_Gather(Ax_b_sliced, batch_size, MPI_DOUBLE, Ax_b, batch_size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

Solver_Init(int N0, double t0, double eps0, int size0, int rank0)
{
	N = N0;
	t = t0;
	eps = eps0;
	size = size0;
	rank = rank0;

	batch_size = ((N / size) + ((N % size != 0) ? 1 : 0));
	int Nup = batch_size * size;

	x = ALLOC(N);
	Ax_b = ALLOC(Nup);

	if (rank == 0)
	{
		A = ALLOC(N * Nup);
		b = ALLOC(Nup);
		FillA();
		FillV(b, N + 1);
		FillV(x, 0);
		denominator = CalculateNorm(b);
	}
	else
	{
		A = NULL;
		b = NULL;
	}

	A_sliced = ALLOC(batch_size * N);
	b_sliced = ALLOC(batch_size);
	Ax_b_sliced = ALLOC(batch_size);

	MPI_Scatter(A, batch_size * N, MPI_DOUBLE, A_sliced, batch_size * N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Scatter(b, batch_size, MPI_DOUBLE, b_sliced, batch_size, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

Solver_Finish()
{
	if (A)
		free(A);
	if (b)
		free(b);
	if (x)
		free(x);
	if (Ax_b)
		free(Ax_b);

	free(A_sliced);
	free(b_sliced);
	free(Ax_b_sliced);
}

void Solver_Run()
{
	for (;;)
	{
		MultiplyMatrixAndVectorAndSubtract(); // Ax_b = A * x - b
		if (StopCriterion())
			break;
		CalculateNextX();
	}
}

void Solver_Print()
{
	if (rank != 0)
		return;

	int i;
	for (i = 0; i < N; i++)
	{
		printf("%lf\n", x[i]);
	}
}

int main(int argc, char *argv[])
{
	int size, rank;

	MPI_Init(&argc, &argv);
	//MPE_Init_log();
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	double start = MPI_Wtime();

	Solver_Init(3500, 1e-5, 1e-6, size, rank);
	Solver_Run();
	//Solver_Print();
	Solver_Finish();

	double end = MPI_Wtime();
	if (rank == 0)
		printf("Time: %lf\n", end - start);

	//MPE_Finish_log("log.slog");
	MPI_Finalize();
	return 0;
}
