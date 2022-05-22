#include <iostream>
#include <chrono>

class Solver
{
    int N;
    double* A;
    double* b;
    double* x;
    double* Ax_b;
    double denominator;
    double t;
    double eps;

    void FillA()
    {
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                if (i == j)
                {
                    A[i * N + j] = 2;
                }
                else
                {
                    A[i * N + j] = 1;
                }
            }
        }
    }

    void FillV(double* v, double value)
    {
        for (int i = 0; i < N; i++)
        {
            v[i] = value;
        }
    }

    double CalculateNorm(double* v)
    {
        double sum = 0;
        for (int i = 0; i < N; i++)
        {
            sum += v[i] * v[i];
        }
        return sqrt(sum);
    }

    bool StopCriterion()
    {
        double numerator = CalculateNorm(Ax_b);
        return numerator / denominator < eps;
    }

    void MultiplyVectorAndScalar(double* v, double s)
    {
        for (int i = 0; i < N; i++)
        {
            v[i] *= s;
        }
    }

    void Subtract(double* res, double* v)
    {
        for (int i = 0; i < N; i++)
            res[i] -= v[i];
    }

    void CalculateNextX()
    {
        MultiplyVectorAndScalar(Ax_b, t);
        Subtract(x, Ax_b);
    }

    void MultiplyMatrixAndVector(double* res, double* m, double* v)
    {
        for (int i = 0; i < N; i++)
        {
            res[i] = 0;
            for (int j = 0; j < N; j++)
            {
                res[i] += m[i * N + j] * v[j];
            }
        }
    }

public:
    Solver(int N, double t, double eps)
    {
        this->N = N;
        this->t = t;
        this->eps = eps;
        A = new double[N * N];
        b = new double[N];
        x = new double[N];
        Ax_b = new double[N];
        FillA();
        FillV(b, N + 1);
        FillV(x, 0);
        denominator = CalculateNorm(b);
    }

    ~Solver()
    {
        delete[] A;
        delete[] b;
        delete[] x;
        delete[] Ax_b;
    }

    void Run()
    {
        for (;;)
        {
            MultiplyMatrixAndVector(Ax_b, A, x);
            Subtract(Ax_b, b);
            if (StopCriterion())
                break;
            CalculateNextX();
        }
    }

    void Print() const
    {
        for (int i = 0; i < N; i++)
        {
            std::cout << x[i] << '\n';
        }
    }
};

int main() 
{
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    Solver solver(5000, 1e-5, 1e-10);
    solver.Run();

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

    //solver.Print();

    return 0;
}
