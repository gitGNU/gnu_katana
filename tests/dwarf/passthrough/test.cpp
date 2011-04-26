#include <cstdio>

int foo()
{
  try
  {
    throw 2;
  }
  catch(int a)
  {
    printf("foo caught int %i\n",a);
  }
}


int main(int argc,char** argv)
{
  try
  {
    throw 1;
  }
  catch(int a)
  {
    printf("Caught int %i\n",a);
  }
  foo();
  return 0;
}
