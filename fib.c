int printf(const char *, ...);

int fib(int n)
{
    if (n < 3) return n;
    return fib(n-2) + fib(n-1);
}

int main()
{
    return printf("%d\n", fib(34));
}
