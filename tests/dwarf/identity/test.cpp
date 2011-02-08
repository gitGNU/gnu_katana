#include <cstdio>

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
}
