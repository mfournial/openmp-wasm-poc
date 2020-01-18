int main()
{
  int i = -2;
  # pragma omp parallel
  {
    i++;  
  }
  return i;
}
